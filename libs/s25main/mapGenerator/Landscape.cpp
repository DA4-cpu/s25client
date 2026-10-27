// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Landscape.h"
#include "RttrForeachPt.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "mapGenerator/Algorithms.h"
#include "mapGenerator/Noise.h"
#include "mapGenerator/TextureHelper.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace rttr::mapGenerator {

namespace {
/// Size of the largest features in (visual) nodes for the given land mass
double GetFeatureSize(LandMass landMass, const MapExtent& size)
{
    const VisualPos visualSize = GetVisualSize(size);
    const double minDimension = std::min(visualSize.x, visualSize.y);
    switch(landMass)
    {
        case LandMass::Islands: return std::clamp(minDimension / 5., 8., 40.);
        case LandMass::Normal: return std::clamp(minDimension / 3., 12., 80.);
        case LandMass::Continents: return std::max(minDimension / 1.6, 16.);
    }
    return minDimension / 3.;
}

double SmoothStep(double edge0, double edge1, double x)
{
    const double t = std::clamp((x - edge0) / (edge1 - edge0), 0., 1.);
    return t * t * (3. - 2. * t);
}

/// Hills of land and mountains are at most this high (map height units) on large maps
constexpr double maxHillHeight = 12.;
/// Mountains rise at least this much above their foot ...
constexpr double minMountainHeight = 8.;
/// ... and at most this much
constexpr double maxMountainHeight = 45.;
/// Slope of mountains at their foot (height units per node) for flat and steep mountain ranges. Mines need the
/// south east neighbor at most 3 higher, so this stays below 3
constexpr double minMountainSlope = 0.8;
constexpr double maxMountainSlope = 2.4;
} // namespace

void CreateLandMasses(Map& map, RandomUtility& rnd, const MapSettings& settings)
{
    unsigned octaves = 4;
    double persistence = 0.5;
    switch(settings.roughness)
    {
        case Roughness::Smooth:
            octaves = 3;
            persistence = 0.35;
            break;
        case Roughness::Normal: break;
        case Roughness::Rough:
            octaves = 5;
            persistence = 0.6;
            break;
    }

    const auto noise =
      ToUniform(CreateFractalNoise(rnd, map.size, GetFeatureSize(settings.landMass, map.size), octaves, persistence));

    const double waterShare = std::min(settings.water, static_cast<unsigned short>(95)) / 100.;
    const double mountainShare = std::min(settings.mountains, static_cast<unsigned short>(95)) / 100.;
    // Rank thresholds: [0, waterLimit) = water, [waterLimit, mountainLimit) = land, rest mountains.
    // A triangle is only water if all its corners are water. So the share of water nodes must be a bit higher to get
    // the requested share of water area: Adjust the threshold a few times.
    double waterLimit = waterShare;
    if(waterShare > 0.)
    {
        const double numTriangles = 2. * map.size.x * map.size.y;
        for(int iteration = 0; iteration < 4; ++iteration)
        {
            unsigned numWaterTriangles = 0;
            RTTR_FOREACH_PT(MapPoint, map.size)
            {
                for(const bool rsu : {true, false})
                {
                    const auto corners = GetTriangleEdges(Triangle(rsu, pt), map.size);
                    if(noise[corners[0]] < waterLimit && noise[corners[1]] < waterLimit
                       && noise[corners[2]] < waterLimit)
                        ++numWaterTriangles;
                }
            }
            waterLimit = std::clamp(waterLimit + (waterShare - numWaterTriangles / numTriangles), 0., 0.98);
        }
    }
    const double mountainLimit = 1. - (1. - waterLimit) * mountainShare;

    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        const double value = noise[pt];
        if(value < waterLimit)
            map.nodeKind[pt] = NodeKind::Water;
        else if(value < mountainLimit)
            map.nodeKind[pt] = NodeKind::Land;
        else
            map.nodeKind[pt] = NodeKind::Mountain;
    }
}

void CreateHeights(Map& map, RandomUtility& rnd, const MapSettings& settings)
{
    const MapExtent size = map.size;
    // Small maps have a smaller maximum height, scale everything down there
    const double scale = std::clamp(map.height.maximum / 64., 0.4, 1.);
    const double unevenness = std::min(settings.unevenness, static_cast<unsigned short>(100)) / 100.;

    const auto distanceToWater = DistancesTo(size, [&map](const MapPoint& pt) { return map.nodeKind[pt] == NodeKind::Water; });
    const auto depthInMountains =
      DistancesTo(size, [&map](const MapPoint& pt) { return map.nodeKind[pt] != NodeKind::Mountain; });

    // Hills, and per region how hilly it is (flat plains vs. hilly land)
    const auto hills = ToUniform(CreateFractalNoise(rnd, size, 12., 3, 0.5));
    const auto hilliness = ToUniform(CreateFractalNoise(rnd, size, 40., 2, 0.5));
    // Per mountain range: steepness and height
    const auto steepness = ToUniform(CreateFractalNoise(rnd, size, 30., 2, 0.5));
    const auto mountainHeight = ToUniform(CreateFractalNoise(rnd, size, 30., 2, 0.5));
    // Detail on the mountains (ridges), roughly [-1, 1]
    const auto ridges = CreateFractalNoise(rnd, size, 8., 3, 0.5);

    // Random bumps of single nodes: none on flat maps, up to 2 on very uneven maps
    const unsigned jitter = unevenness >= 0.75 ? 2u : (unevenness >= 0.25 ? 1u : 0u);
    // Mountains get steeper (and flatter) with the unevenness, too
    const double slopeFactor = 0.6 + 0.8 * unevenness;

    RTTR_FOREACH_PT(MapPoint, size)
    {
        map.mountainLift[pt] = 0;
        if(map.nodeKind[pt] == NodeKind::Water)
        {
            map.z[pt] = map.height.minimum;
            continue;
        }
        const double coastDistance = std::min(distanceToWater[pt], 10u);
        // Flat beaches, then rising slowly
        const double coastRise = coastDistance * 0.4 * scale;
        const double region = SmoothStep(0.25, 0.85, hilliness[pt]);
        const double hillHeight =
          maxHillHeight * scale * unevenness * region * std::min(1., coastDistance / 6.) * hills[pt];
        double height = 1. + coastRise + hillHeight;

        if(map.nodeKind[pt] == NodeKind::Mountain)
        {
            const double depth = depthInMountains[pt];
            const double slope = (minMountainSlope + (maxMountainSlope - minMountainSlope) * steepness[pt]) * slopeFactor;
            const double cap =
              (minMountainHeight + (maxMountainHeight - minMountainHeight) * std::pow(mountainHeight[pt], 1.3)) * scale;
            // Rises with the slope at the foot and levels off towards the top
            double lift = cap * (1. - std::exp(-depth * slope / cap));
            lift += ridges[pt] * 0.1 * cap * std::min(1., depth / 3.);
            lift = std::max(lift, 1.);
            map.mountainLift[pt] = static_cast<uint8_t>(std::min(std::lround(lift), 250l));
            height += 1. + lift;
        }
        unsigned z = static_cast<unsigned>(std::lround(height));
        if(jitter > 0)
            z += rnd.RandomValue(0u, jitter);
        map.z[pt] = static_cast<uint8_t>(std::clamp(z, 1u, static_cast<unsigned>(map.height.maximum)));
    }

    // Mountains are never lower than the land next to them (hills and random bumps could make the foot lower)
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(map.nodeKind[pt] != NodeKind::Mountain)
            continue;
        for(const MapPoint& neighbor : map.z.GetNeighbours(pt))
        {
            if(map.nodeKind[neighbor] == NodeKind::Land && map.z[neighbor] >= map.z[pt])
                map.z[pt] = static_cast<uint8_t>(std::min<unsigned>(map.z[neighbor] + 1u, map.height.maximum));
        }
    }
}

void MarkLakes(Map& map, unsigned maxSize)
{
    map.lakes.Resize(map.size, 0);
    NodeMapBase<uint8_t> visited;
    visited.Resize(map.size, 0);
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(map.nodeKind[pt] != NodeKind::Water || visited[pt])
            continue;
        std::vector<MapPoint> body{pt};
        visited[pt] = 1;
        for(unsigned i = 0; i < body.size(); ++i)
        {
            for(const MapPoint& neighbor : map.z.GetNeighbours(body[i]))
            {
                if(map.nodeKind[neighbor] == NodeKind::Water && !visited[neighbor])
                {
                    visited[neighbor] = 1;
                    body.push_back(neighbor);
                }
            }
        }
        if(body.size() <= maxSize)
        {
            for(const MapPoint& node : body)
                map.lakes[node] = 1;
        }
    }
}

NodeMapBase<uint8_t> GetTextureLevels(const Map& map, uint8_t mountainLevel)
{
    NodeMapBase<uint8_t> levels;
    levels.Resize(map.size, 0);
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        switch(map.nodeKind[pt])
        {
            case NodeKind::Water: levels[pt] = 0; break;
            case NodeKind::Land: levels[pt] = 1; break;
            case NodeKind::Mountain:
                levels[pt] = static_cast<uint8_t>(std::min(255u, mountainLevel + 0u + map.mountainLift[pt]));
                break;
        }
    }
    return levels;
}

std::vector<MapPoint> PlaceMountainBuildingSites(Map& map, unsigned spacing)
{
    std::vector<MapPoint> sites;
    if(spacing < 2)
        return sites;
    auto& textures = map.textureMap;
    const auto buildableTextures = textures.FindAll(IsBuildableMountain);
    if(buildableTextures.empty())
        return sites;

    // A site needs mountains all around (2 nodes), otherwise the flag or the building could end up at the water
    const auto isSuitable = [&](const MapPoint& pt) {
        for(const MapPoint& p : map.z.GetPointsInRadiusWithCenter(pt, 2))
        {
            if(!textures.All(p, IsMinableMountain) || map.riverCourse[p])
                return false;
        }
        return true;
    };
    NodeMapBase<uint8_t> isSite;
    isSite.Resize(map.size, 0);

    // Regular grid: every `spacing` nodes, every second grid row shifted by half the spacing
    unsigned gridRow = 0;
    for(unsigned y = spacing / 2; y < map.size.y; y += spacing, ++gridRow)
    {
        const unsigned offset = (gridRow % 2) ? spacing / 2 : 0;
        for(unsigned x = offset; x < map.size.x; x += spacing)
        {
            const MapPoint gridPt(static_cast<MapCoord>(x), static_cast<MapCoord>(y));
            if(map.nodeKind[gridPt] != NodeKind::Mountain)
                continue;
            // The grid point itself or the closest suitable node nearby
            MapPoint site = MapPoint::Invalid();
            if(isSuitable(gridPt))
                site = gridPt;
            else
            {
                for(const MapPoint& p : map.z.GetPointsInRadius(gridPt, std::max(1u, spacing / 3)))
                {
                    if(isSuitable(p))
                    {
                        site = p;
                        break;
                    }
                }
            }
            if(!site.isValid())
                continue;
            // Flattening a site changes the heights in radius 2, so sites must be further apart
            if(helpers::contains_if(map.z.GetPointsInRadiusWithCenter(site, 4),
                                    [&isSite](const MapPoint& p) { return isSite[p] != 0; }))
                continue;

            // Texture of the mountain type matching the temperature (like the mountain textures)
            DescIdx<TerrainDesc> texture = buildableTextures.front();
            int bestDistance = std::numeric_limits<int>::max();
            for(const auto candidate : buildableTextures)
            {
                const int distance = std::abs(static_cast<int>(textures.GetDesc(candidate).temperature)
                                              - static_cast<int>(map.temperature[site]));
                if(distance < bestDistance)
                {
                    bestDistance = distance;
                    texture = candidate;
                }
            }
            for(const Triangle& triangle : GetTriangles(site, map.size))
                textures.Set(triangle, texture);
            FlattenForCastleBuilding(map.z, site);
            isSite[site] = 1;
            sites.push_back(site);
        }
    }
    return sites;
}

} // namespace rttr::mapGenerator
