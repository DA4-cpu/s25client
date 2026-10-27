// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Zones.h"
#include "RttrForeachPt.h"
#include "mapGenerator/Algorithms.h"
#include "mapGenerator/Noise.h"
#include "mapGenerator/TextureHelper.h"
#include "gameData/MapGenDesc.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>

namespace rttr::mapGenerator {

NodeMapBase<uint8_t> GenerateZones(RandomUtility& rnd, const MapExtent& size, unsigned numZones)
{
    NodeMapBase<uint8_t> zones;
    zones.Resize(size, 0);
    if(numZones <= 1)
        return zones;

    // Seed points far apart from each other: For each new seed take the best of some random candidates
    std::vector<MapPoint> seeds{rnd.Point(size)};
    while(seeds.size() < numZones)
    {
        MapPoint best = MapPoint::Invalid();
        double bestDistance = -1.;
        for(unsigned candidate = 0; candidate < 64; ++candidate)
        {
            const MapPoint pt = rnd.Point(size);
            double distance = std::numeric_limits<double>::max();
            for(const MapPoint& seed : seeds)
                distance = std::min(distance, GetVisualDistance(pt, seed, size));
            if(distance > bestDistance)
            {
                bestDistance = distance;
                best = pt;
            }
        }
        seeds.push_back(best);
    }

    // Which zone grows from which seed
    std::vector<uint8_t> zoneOfSeed(numZones);
    std::iota(zoneOfSeed.begin(), zoneOfSeed.end(), static_cast<uint8_t>(0));
    for(unsigned i = numZones - 1; i > 0; --i)
        std::swap(zoneOfSeed[i], zoneOfSeed[rnd.Index(i + 1)]);

    // Grow all zones at once (shortest paths from the seeds). Costs vary smoothly, so the borders get wavy.
    // Each node belongs to the seed of its shortest path, so every zone is connected.
    const VisualPos visualSize = GetVisualSize(size);
    const double featureSize = std::sqrt(visualSize.x * visualSize.y / numZones) / 2.;
    const auto costNoise = ToUniform(CreateFractalNoise(rnd, size, featureSize, 3, 0.5));

    NodeMapBase<double> distances;
    distances.Resize(size, std::numeric_limits<double>::max());
    using Entry = std::pair<double, unsigned>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
    for(unsigned i = 0; i < numZones; ++i)
    {
        const unsigned idx = distances.GetIdx(seeds[i]);
        distances[idx] = 0.;
        zones[idx] = zoneOfSeed[i];
        queue.emplace(0., idx);
    }
    while(!queue.empty())
    {
        const auto [distance, idx] = queue.top();
        queue.pop();
        if(distance > distances[idx])
            continue;
        const MapPoint pt(static_cast<MapCoord>(idx % size.x), static_cast<MapCoord>(idx / size.x));
        for(const MapPoint& neighbor : distances.GetNeighbours(pt))
        {
            const unsigned nIdx = distances.GetIdx(neighbor);
            const double cost =
              GetVisualDistance(pt, neighbor, size) * (1. + 3. * (costNoise[idx] + costNoise[nIdx]) / 2.);
            if(distance + cost < distances[nIdx])
            {
                distances[nIdx] = distance + cost;
                zones[nIdx] = zones[idx];
                queue.emplace(distance + cost, nIdx);
            }
        }
    }
    return zones;
}

void ComputeHeightPercent(Map& map)
{
    const uint8_t seaLevel = map.height.minimum;
    const uint8_t maxHeight = *std::max_element(map.z.begin(), map.z.end());
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(map.z[pt] <= seaLevel || maxHeight <= seaLevel)
            map.heightPercent[pt] = 0;
        else
            map.heightPercent[pt] = static_cast<uint8_t>((map.z[pt] - seaLevel) * 100u / (maxHeight - seaLevel));
    }
}

void ApplyHeightCooling(Map& map, unsigned cooling)
{
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        const unsigned decrease = cooling * map.heightPercent[pt] / 100u;
        map.temperature[pt] = static_cast<uint8_t>(map.temperature[pt] > decrease ? map.temperature[pt] - decrease : 0);
    }
}

void ApplyWaterHumidity(Map& map, unsigned maxDistance, unsigned humidity)
{
    if(humidity == 0)
        return;
    std::vector<MapPoint> freshWater;
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(map.lakes[pt] || map.riverCourse[pt])
            freshWater.push_back(pt);
    }
    if(freshWater.empty())
        return;
    const auto distance = DistancesTo(freshWater, map.size);
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(distance[pt] > maxDistance)
            continue;
        const unsigned increase = humidity * (maxDistance + 1u - distance[pt]) / (maxDistance + 1u);
        map.climate[pt] = static_cast<uint8_t>(std::min(100u, map.climate[pt] + increase));
    }
}

void ComputeTerrainFlags(Map& map, unsigned nearWaterDistance)
{
    const MapExtent size = map.size;
    const auto& textures = map.textureMap;

    NodeMapBase<uint8_t> isWater; // All triangles water
    isWater.Resize(size, 0);
    RTTR_FOREACH_PT(MapPoint, size)
    {
        isWater[pt] = textures.All(pt, IsWater) ? 1 : 0;
    }

    // Lakes are fresh water, the sea salt water (see MarkLakes). Water from rivers on land nodes is fresh water, too
    const auto isFresh = [&map](const MapPoint& pt) { return map.nodeKind[pt] != NodeKind::Water || map.lakes[pt]; };

    std::vector<MapPoint> freshNodes, saltNodes, riverNodes;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(map.riverCourse[pt])
        {
            riverNodes.push_back(pt);
            freshNodes.push_back(pt);
        } else if(isWater[pt])
        {
            if(isFresh(pt))
                freshNodes.push_back(pt);
            else
                saltNodes.push_back(pt);
        }
    }
    const unsigned unreachable = std::numeric_limits<unsigned>::max();
    const auto distanceTo = [&](const std::vector<MapPoint>& nodes) {
        if(nodes.empty())
        {
            NodeMapBase<unsigned> result;
            result.Resize(size, unreachable);
            return result;
        }
        return DistancesTo(nodes, size);
    };
    const auto isNear = [unreachable](unsigned distance, unsigned maxDistance) {
        return distance != unreachable && distance <= maxDistance;
    };

    // Shore: Nodes touching water. Which water is decided by the closest water (nodes at the coast often have no
    // neighbor completely covered by water, so look 2 nodes far)
    NodeMapBase<uint16_t> flags;
    flags.Resize(size, 0);
    {
        const auto freshDistance = distanceTo(freshNodes);
        const auto saltDistance = distanceTo(saltNodes);
        const auto riverDistance = distanceTo(riverNodes);
        RTTR_FOREACH_PT(MapPoint, size)
        {
            MapGenTerrainFlags& curFlags = flags[pt];
            if(map.riverCourse[pt])
            {
                curFlags |= toFlag(MapGenTerrain::River) | toFlag(MapGenTerrain::Freshwater);
                continue;
            }
            if(isWater[pt])
            {
                curFlags |= toFlag(isFresh(pt) ? MapGenTerrain::Freshwater : MapGenTerrain::Saltwater);
                continue;
            }
            if(!textures.Any(pt, IsWater))
            {
                if(textures.Any(pt, IsMountainOrSnowOrLava))
                    curFlags |= toFlag(MapGenTerrain::Mountain);
                else if(textures.All(pt, IsLand))
                    curFlags |= toFlag(MapGenTerrain::Land);
                continue;
            }
            if(isNear(riverDistance[pt], 1))
                curFlags |= toFlag(MapGenTerrain::ShoreRiver) | toFlag(MapGenTerrain::ShoreFresh);
            if(isNear(freshDistance[pt], 2))
                curFlags |= toFlag(MapGenTerrain::ShoreFresh);
            if(isNear(saltDistance[pt], 2))
                curFlags |= toFlag(MapGenTerrain::ShoreSalt);
        }
    }

    // Near: at most nearWaterDistance nodes from the water or its shore
    std::vector<MapPoint> nearFreshSources(freshNodes), nearSaltSources(saltNodes), nearRiverSources(riverNodes);
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(flags[pt] & toFlag(MapGenTerrain::ShoreFresh))
            nearFreshSources.push_back(pt);
        if(flags[pt] & toFlag(MapGenTerrain::ShoreSalt))
            nearSaltSources.push_back(pt);
        if(flags[pt] & toFlag(MapGenTerrain::ShoreRiver))
            nearRiverSources.push_back(pt);
    }
    const auto nearFresh = distanceTo(nearFreshSources);
    const auto nearSalt = distanceTo(nearSaltSources);
    const auto nearRiver = distanceTo(nearRiverSources);
    RTTR_FOREACH_PT(MapPoint, size)
    {
        MapGenTerrainFlags curFlags = flags[pt];
        if(!isWater[pt] && !map.riverCourse[pt])
        {
            if(isNear(nearFresh[pt], nearWaterDistance))
                curFlags |= toFlag(MapGenTerrain::NearFresh);
            if(isNear(nearSalt[pt], nearWaterDistance))
                curFlags |= toFlag(MapGenTerrain::NearSalt);
            if(isNear(nearRiver[pt], nearWaterDistance))
                curFlags |= toFlag(MapGenTerrain::NearRiver);
        }
        map.terrainFlags[pt] = curFlags;
    }
}

} // namespace rttr::mapGenerator
