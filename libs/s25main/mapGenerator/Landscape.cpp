// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Landscape.h"
#include "RttrForeachPt.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "mapGenerator/Noise.h"
#include "mapGenerator/TextureHelper.h"
#include <algorithm>
#include <cmath>

namespace rttr::mapGenerator {

LandscapeLevels GetLandscapeLevels(uint8_t maxHeight)
{
    // The online generator keeps the land quite flat (about 10 height steps from the coast to the mountains) and
    // the mountains about 2-3 times as high.
    const auto landTop = static_cast<uint8_t>(std::clamp(maxHeight / 5, 3, 12));
    return LandscapeLevels{landTop, static_cast<uint8_t>(landTop + 2)};
}

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
} // namespace

LandscapeLevels CreateLandscape(Map& map, RandomUtility& rnd, const MapSettings& settings)
{
    const LandscapeLevels levels = GetLandscapeLevels(map.height.maximum);

    unsigned octaves = 4;
    double persistence = 0.5;
    unsigned heightJitter = 1;
    switch(settings.roughness)
    {
        case Roughness::Smooth:
            octaves = 3;
            persistence = 0.35;
            heightJitter = 0;
            break;
        case Roughness::Normal: break;
        case Roughness::Rough:
            octaves = 5;
            persistence = 0.6;
            heightJitter = 2;
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
    const unsigned maxMountainRise = map.height.maximum > levels.mountainLevel ?
                                       map.height.maximum - levels.mountainLevel :
                                       0u;
    const unsigned mountainRise = std::min(maxMountainRise, levels.landTop * 3u);

    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        const double value = noise[pt];
        if(value < waterLimit)
        {
            map.z[pt] = map.height.minimum;
        } else if(value < mountainLimit)
        {
            const double t = (value - waterLimit) / (mountainLimit - waterLimit);
            unsigned height = 1u + static_cast<unsigned>(t * levels.landTop);
            if(heightJitter > 0)
                height += rnd.RandomValue(0u, heightJitter);
            map.z[pt] = static_cast<uint8_t>(std::clamp(height, 1u, static_cast<unsigned>(levels.landTop)));
        } else
        {
            const double t = (value - mountainLimit) / std::max(1. - mountainLimit, 1e-9);
            unsigned height = levels.mountainLevel + static_cast<unsigned>(t * mountainRise);
            if(heightJitter > 0)
                height += rnd.RandomValue(0u, heightJitter);
            map.z[pt] = static_cast<uint8_t>(
              std::clamp(height, static_cast<unsigned>(levels.mountainLevel), static_cast<unsigned>(map.height.maximum)));
        }
    }
    return levels;
}

River CreateRiver(Map& map, RandomUtility& rnd, MapPoint source, unsigned maxLength)
{
    River river;
    auto& textures = map.textureMap;
    const auto water = textures.Find(IsWater);
    const uint8_t seaLevel = map.height.minimum;

    // Course of this river (to avoid flowing in circles)
    std::set<MapPoint, MapPointLess> course;
    MapPoint current = source;
    const auto setWater = [&](const MapPoint& pt, Direction dir) {
        for(const Triangle& triangle : GetTriangles(pt, map.size, dir))
        {
            textures.Set(triangle, water);
            for(const MapPoint& edge : GetTriangleEdges(triangle, map.size))
                river.insert(edge);
        }
    };

    for(unsigned step = 0; step < maxLength; ++step)
    {
        // Reached the sea level (water is textured later) -> done
        if(map.z[current] <= seaLevel)
            break;
        course.insert(current);
        map.riverCourse[current] = 1;

        // Find the lowest neighbors. Rivers dig through small bumps (up to 2 height steps, e.g. from the rough
        // surface) but end in a sink if the land around is higher
        constexpr unsigned maxBump = 2;
        std::vector<Direction> candidates;
        unsigned lowest = map.z[current] + maxBump;
        for(const auto dir : helpers::EnumRange<Direction>())
        {
            const MapPoint next = map.z.GetNeighbour(current, dir);
            if(helpers::contains(course, next) || map.z[next] > lowest)
                continue;
            if(map.z[next] < lowest)
            {
                lowest = map.z[next];
                candidates.clear();
            }
            candidates.push_back(dir);
        }

        if(candidates.empty())
        {
            // A sink: end in a small pond
            for(const auto dir : helpers::EnumRange<Direction>())
                setWater(current, dir);
            break;
        }

        const Direction dir = rnd.RandomItem(candidates);
        setWater(current, dir);
        const MapPoint next = map.z.GetNeighbour(current, dir);
        // Dig through the bump so the river flows downhill
        if(map.z[next] > map.z[current])
            map.z[next] = map.z[current];
        // Joined another river
        if(map.riverCourse[next])
            break;
        current = next;
    }

    // Rivers dig into the land a bit
    for(const MapPoint& pt : course)
    {
        if(map.z[pt] > seaLevel + 1)
            map.z[pt] = static_cast<uint8_t>(map.z[pt] - 1);
    }
    return river;
}

std::vector<River> CreateRivers(Map& map, RandomUtility& rnd, unsigned amount, const LandscapeLevels& levels)
{
    std::vector<River> rivers;
    if(amount == 0)
        return rivers;

    // Sources: upper part of the land, but not on the mountains
    const unsigned minSourceHeight = std::max(1u, levels.landTop * 2u / 3u);
    std::vector<MapPoint> sources;
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(map.z[pt] >= minSourceHeight && map.z[pt] < levels.mountainLevel)
            sources.push_back(pt);
    }
    if(sources.empty())
        return rivers;

    const unsigned numNodes = static_cast<unsigned>(map.size.x) * map.size.y;
    // At 100 % about one river per 1500 nodes
    const unsigned numRivers = std::max(1u, static_cast<unsigned>(std::lround(amount / 100. * numNodes / 1500.)));
    const unsigned maxLength = map.size.x + map.size.y;
    for(unsigned i = 0; i < numRivers; ++i)
    {
        const MapPoint source = sources[rnd.Index(sources.size())];
        if(map.riverCourse[source])
            continue;
        rivers.push_back(CreateRiver(map, rnd, source, maxLength));
    }
    return rivers;
}

} // namespace rttr::mapGenerator
