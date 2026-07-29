// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Rivers.h"
#include "mapGenerator/Terrain.h"
#include "mapGenerator/TextureHelper.h"
#include <algorithm>

namespace rttr::mapGenerator {

namespace {

/**
 * Direction candidates considered for the next step of a stream: continuing straight ahead or turning
 * by 60 degrees to either side of the last direction. Directions that would fold the stream back
 * towards where it came from ("excluded", see CreateStream) are never considered so the stream cannot
 * cross or double back on itself.
 */
std::vector<Direction> GetStreamDirectionCandidates(Direction lastDir, const std::vector<Direction>& excluded)
{
    std::vector<Direction> candidates;

    for(Direction dir : {lastDir, lastDir + 1, lastDir + 5})
    {
        if(!helpers::contains(excluded, dir))
        {
            candidates.push_back(dir);
        }
    }

    if(candidates.empty())
    {
        // Not expected to happen (the excluded directions can cover at most one of the three
        // candidates above), but keep the stream going rather than leaving it without any direction.
        candidates.push_back(lastDir);
    }

    return candidates;
}

/**
 * Picks the next direction for a stream out of the given candidates. Candidates leading to lower
 * terrain get a higher chance of being picked so the stream tends to flow downhill, but every
 * candidate keeps at least a small chance so the stream can still meander naturally and doesn't get
 * stuck forever against small bumps in the terrain.
 */
Direction ChooseDownhillDirection(RandomUtility& rnd, const Map& map, const MapPoint& currentNode,
                                   const std::vector<Direction>& candidates)
{
    std::vector<unsigned> heights;
    heights.reserve(candidates.size());

    unsigned highest = 0;
    for(Direction dir : candidates)
    {
        const auto neighbour = map.getTextures().GetNeighbour(currentNode, dir);
        const auto height = static_cast<unsigned>(map.z[neighbour]);
        heights.push_back(height);
        highest = std::max(highest, height);
    }

    // Weight = distance from the (locally) highest candidate, so the lowest candidate always has the
    // biggest chance to get picked while every candidate keeps a weight of at least 1.
    std::vector<unsigned> weights;
    weights.reserve(candidates.size());
    unsigned totalWeight = 0;
    for(unsigned height : heights)
    {
        const unsigned weight = highest - height + 1;
        weights.push_back(weight);
        totalWeight += weight;
    }

    unsigned roll = rnd.RandomValue(0u, totalWeight - 1);
    for(unsigned i = 0; i < candidates.size(); ++i)
    {
        if(roll < weights[i])
        {
            return candidates[i];
        }
        roll -= weights[i];
    }

    return candidates.back(); // unreachable
}

} // namespace

River CreateStream(RandomUtility& rnd, Map& map, const MapPoint& source, Direction direction, unsigned length,
                   unsigned splitRate)
{
    const MapExtent& size = map.size;

    River river;
    const std::vector<Direction> excluded{direction + 2, direction + 3, direction + 4};

    auto& textures = map.textureMap;
    const auto water = textures.Find(IsWater);

    MapPoint currentNode = source;
    Direction currentDir = direction;

    for(unsigned step = 0; step < length; ++step)
    {
        const auto& triangles = GetTriangles(currentNode, size, currentDir);

        for(const auto& triangle : triangles)
        {
            textures.Set(triangle, water);

            const auto& edges = GetTriangleEdges(triangle, size);
            for(const MapPoint& edge : edges)
            {
                river.insert(edge);
            }
        }

        if(map.z[currentNode] == map.height.minimum)
        {
            // Reached the sea (or another body of water at the map's minimum height) - stop here
            // instead of continuing to wander across the water. Note: we still fall through to the
            // height-carving step below instead of returning early, so the river bed of a stream that
            // reaches the sea quickly gets lowered just like that of any other stream.
            break;
        }

        const auto lastDir = currentDir;
        const auto candidates = GetStreamDirectionCandidates(lastDir, excluded);
        currentDir = ChooseDownhillDirection(rnd, map, currentNode, candidates);

        currentNode = map.getTextures().GetNeighbour(currentNode, lastDir);

        if(rnd.ByChance(splitRate))
        {
            auto splitDirection = direction + (rnd.ByChance(50) ? 5 : 1);
            auto anotherRivers = CreateStream(rnd, map, currentNode, splitDirection, length / 2, splitRate / 2);

            for(const MapPoint& node : anotherRivers)
            {
                river.insert(node);
            }
        }
    }

    const auto seaLevel = static_cast<unsigned>(map.height.minimum);
    auto& z = map.z;

    for(const MapPoint& node : river)
    {
        if(z[node] > 0)
        {
            z[node] = std::max(static_cast<unsigned>(z[node] - 1), seaLevel);
        } else
        {
            z[node] = seaLevel;
        }
    }

    return river;
}

} // namespace rttr::mapGenerator
