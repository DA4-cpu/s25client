// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/HeadQuarters.h"
#include "helpers/containerUtils.h"
#include "mapGenerator/Algorithms.h"
#include "mapGenerator/TextureHelper.h"
#include "RttrForeachPt.h"
#include <queue>

namespace rttr::mapGenerator {

std::vector<MapPoint> FindLargestConnectedArea(const Map& map)
{
    std::set<MapPoint, MapPointLess> visited;
    std::vector<MapPoint> connectedArea;

    auto partiallyBuildable = [&map](const MapPoint& pt) { return map.textureMap.Any(pt, IsBuildableLand); };

    auto partiallyConnected = [&map, &partiallyBuildable](const MapPoint& pt) {
        return helpers::contains_if(map.getTextures().GetNeighbours(pt), partiallyBuildable);
    };

    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(visited.insert(pt).second)
        {
            auto area = Collect(map.getTextures(), pt, partiallyConnected);

            visited.insert(area.begin(), area.end());

            if(area.size() > connectedArea.size())
                connectedArea = area;
        }
    }

    // remove rivers & water around the coast which have been
    // added before due to allowing tiny rivers to be part of
    // connected areas

    helpers::erase_if(connectedArea, [&map](const MapPoint& pt) { return map.textureMap.Any(pt, IsWater); });

    return connectedArea;
}

void PlaceHeadquarters(Map& map, RandomUtility& rnd, int number, MountainDistance distance, int retries)
{
    auto maxRetries = retries;
    auto success = false;

    auto area = FindLargestConnectedArea(map);

    while(!success && retries > 0)
    {
        success = true;

        for(int index = 0; index < number; index++)
        {
            auto possiblePositions = FindHqPositions(map, area, distance);

            if(possiblePositions.empty())
            {
                map.hqPositions.clear();
                success = false;
                break;
            }

            auto hq = retries == maxRetries ? possiblePositions.front() : rnd.RandomItem(possiblePositions);

            map.hqPositions.push_back(hq);
        }

        retries--;
    }

    if(!success)
        throw std::runtime_error("Could not find any valid HQ position!");
    for(const MapPoint hqPos : map.hqPositions)
        FlattenForCastleBuilding(map.z, hqPos);
}

std::vector<std::vector<MapPoint>> FindConnectedAreas(const Map& map)
{
    NodeMapBase<uint8_t> partiallyBuildable;
    partiallyBuildable.Resize(map.size, 0);
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        partiallyBuildable[pt] = map.textureMap.Any(pt, IsBuildableLand) ? 1 : 0;
    }
    // Same as in FindLargestConnectedArea: A node belongs to an area if a neighbor is partially buildable, so tiny
    // rivers do not split areas
    const auto partiallyConnected = [&](const MapPoint& pt) {
        return helpers::contains_if(map.getTextures().GetNeighbours(pt),
                                    [&](const MapPoint& neighbor) { return partiallyBuildable[neighbor] != 0; });
    };

    NodeMapBase<uint8_t> visited;
    visited.Resize(map.size, 0);
    std::vector<std::vector<MapPoint>> areas;
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(visited[pt] || !partiallyConnected(pt))
            continue;
        std::vector<MapPoint> area;
        std::queue<MapPoint> todo;
        todo.push(pt);
        visited[pt] = 1;
        while(!todo.empty())
        {
            const MapPoint cur = todo.front();
            todo.pop();
            area.push_back(cur);
            for(const MapPoint& neighbor : map.getTextures().GetNeighbours(cur))
            {
                if(!visited[neighbor] && partiallyConnected(neighbor))
                {
                    visited[neighbor] = 1;
                    todo.push(neighbor);
                }
            }
        }
        helpers::erase_if(area, [&map](const MapPoint& p) { return map.textureMap.Any(p, IsWater); });
        if(!area.empty())
            areas.push_back(std::move(area));
    }
    std::stable_sort(areas.begin(), areas.end(),
                     [](const auto& a, const auto& b) { return a.size() > b.size(); });
    return areas;
}

void PlaceHeadquartersOnLandmasses(Map& map, RandomUtility& rnd, unsigned number, MountainDistance distance,
                                   int retries)
{
    const auto areas = FindConnectedAreas(map);
    if(areas.empty())
        throw std::runtime_error("Could not find any valid HQ position!");

    // An HQ needs some land around it to be playable
    const size_t minAreaSize = std::min<size_t>(300, areas.front().size() / 2);

    for(int attempt = 0; attempt < retries; ++attempt)
    {
        map.hqPositions.clear();
        std::vector<unsigned> playersInArea(areas.size(), 0);
        bool success = true;
        for(unsigned player = 0; player < number && success; ++player)
        {
            // Areas ordered by space per player
            std::vector<unsigned> order;
            for(unsigned i = 0; i < areas.size(); ++i)
            {
                if(areas[i].size() >= minAreaSize)
                    order.push_back(i);
            }
            std::stable_sort(order.begin(), order.end(), [&](unsigned a, unsigned b) {
                return areas[a].size() * (playersInArea[b] + 1) > areas[b].size() * (playersInArea[a] + 1);
            });
            bool placed = false;
            for(const unsigned areaIdx : order)
            {
                const auto positions = FindHqPositions(map, areas[areaIdx], distance);
                if(positions.empty())
                    continue;
                // First attempt: best positions. Later attempts: random among the better half
                const size_t numCandidates = attempt == 0 ? 1 : std::max<size_t>(1, positions.size() / 2);
                map.hqPositions.push_back(positions[rnd.Index(numCandidates)]);
                ++playersInArea[areaIdx];
                placed = true;
                break;
            }
            success = placed;
        }
        if(success)
        {
            for(const MapPoint hqPos : map.hqPositions)
                FlattenForCastleBuilding(map.z, hqPos);
            return;
        }
    }
    map.hqPositions.clear();
    throw std::runtime_error("Could not find any valid HQ position!");
}

} // namespace rttr::mapGenerator
