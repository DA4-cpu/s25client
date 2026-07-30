// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Rivers.h"
#include "mapGenerator/Terrain.h"
#include "mapGenerator/TextureHelper.h"
#include "mapGenerator/Triangles.h"
#include <algorithm>
#include <array>

namespace rttr::mapGenerator {

namespace {

struct DirectionCandidate
{
    Direction dir;
    MapPoint neighbour;
    unsigned distanceToSea;
    unsigned height;
};

/**
 * Picks the next direction for a stream out of all 6 neighbors of "currentNode". Only neighbors that get the
 * stream strictly closer to the sea are ever considered - by construction of "distanceToSea" (a plain grid BFS
 * distance field), a node with a nonzero distance D always has at least one neighbor with distance D-1, so this
 * set is never empty and every single step is guaranteed to reduce the remaining distance to the sea by exactly
 * 1. "cameFrom" (the direction the stream just arrived from) is excluded from that set to avoid immediately
 * reversing, unless it is the only neighbor that actually provides that progress, in which case it is allowed
 * anyway rather than leaving the stream stuck. Among multiple, equally-close candidates, lower terrain height is
 * preferred, purely for a more natural-looking meander - this never overrides the progress guarantee above.
 */
Direction ChooseDirectionTowardsSea(RandomUtility& rnd, const Map& map, const NodeMapBase<unsigned>& distanceToSea,
                                     const MapPoint& currentNode, Direction cameFrom)
{
    std::vector<DirectionCandidate> all;
    all.reserve(6);
    for(Direction dir : helpers::EnumRange<Direction>())
    {
        const auto neighbour = map.getTextures().GetNeighbour(currentNode, dir);
        all.push_back(
          DirectionCandidate{dir, neighbour, distanceToSea[neighbour], static_cast<unsigned>(map.z[neighbour])});
    }

    const unsigned bestDistance =
      std::min_element(all.begin(), all.end(),
                        [](const auto& a, const auto& b) { return a.distanceToSea < b.distanceToSea; })
        ->distanceToSea;

    std::vector<DirectionCandidate> candidates;
    for(const auto& candidate : all)
    {
        if(candidate.distanceToSea == bestDistance && candidate.dir != cameFrom)
        {
            candidates.push_back(candidate);
        }
    }
    if(candidates.empty())
    {
        // The only progress-making neighbor is the one we just came from - take it anyway rather than
        // getting stuck; this still reduces the distance to the sea by exactly 1, same as any other step.
        for(const auto& candidate : all)
        {
            if(candidate.distanceToSea == bestDistance)
            {
                candidates.push_back(candidate);
            }
        }
    }

    unsigned highest = 0;
    for(const auto& candidate : candidates)
    {
        highest = std::max(highest, candidate.height);
    }

    // Weight = distance from the (locally) highest candidate's height, so the lowest one always has the
    // biggest chance to get picked while every candidate keeps a weight of at least 1 - this only ever
    // chooses among candidates that already make equal progress towards the sea, so it can't undermine
    // the guarantee above, it just adds some natural-looking variation to the path taken.
    unsigned totalWeight = 0;
    std::vector<unsigned> weights;
    weights.reserve(candidates.size());
    for(const auto& candidate : candidates)
    {
        const unsigned weight = highest - candidate.height + 1;
        weights.push_back(weight);
        totalWeight += weight;
    }

    unsigned roll = rnd.RandomValue(0u, totalWeight - 1);
    for(unsigned idx = 0; idx < candidates.size(); ++idx)
    {
        if(roll < weights[idx])
        {
            return candidates[idx].dir;
        }
        roll -= weights[idx];
    }

    return candidates.back().dir; // unreachable
}

} // namespace

River CreateStream(RandomUtility& rnd, Map& map, const NodeMapBase<unsigned>& distanceToSea, const MapPoint& source,
                   Direction direction, unsigned length, unsigned splitRate)
{
    const MapExtent& size = map.size;

    River river;

    auto& textures = map.textureMap;
    const auto water = textures.Find(IsWater);

    MapPoint currentNode = source;
    // Nothing yet - treat the stream as if it had just arrived from the opposite of its initial
    // heading, so it won't immediately reverse out of the gate on its very first step either.
    Direction cameFrom = direction + 3;

    for(unsigned step = 0; step < length; ++step)
    {
        // Mark both of the current node's own triangles as water, i.e. its whole tile - not just the
        // 2 triangles pointing in some specific direction. Which 2 triangles "point forward" depends
        // on both the direction and the row's parity in this hex grid, and for some combinations they
        // don't include either of the node's own triangles at all. Whenever the stream then turned,
        // this could leave one (or both) of a node's triangles unset, showing up as small notches of
        // land poking into an otherwise straight stretch of river. Setting a node's own tile directly
        // instead sidesteps that entirely: every node the stream visits becomes a solid, fully
        // water-covered tile, independent of the direction it was entered or left in.
        const std::array<Triangle, 2> tile{Triangle(true, currentNode), Triangle(false, currentNode)};

        for(const auto& triangle : tile)
        {
            textures.Set(triangle, water);

            const auto& edges = GetTriangleEdges(triangle, size);
            for(const MapPoint& edge : edges)
            {
                river.insert(edge);
            }
        }

        if(distanceToSea[currentNode] == 0 || map.z[currentNode] == map.height.minimum)
        {
            // Reached the sea (or another body of water at the map's minimum height) - stop here
            // instead of continuing to wander across the water. Note: we still fall through to the
            // height-carving step below instead of returning early, so the river bed of a stream that
            // reaches the sea quickly gets lowered just like that of any other stream.
            break;
        }

        const auto chosenDir = ChooseDirectionTowardsSea(rnd, map, distanceToSea, currentNode, cameFrom);
        cameFrom = chosenDir + 3;
        currentNode = map.getTextures().GetNeighbour(currentNode, chosenDir);

        if(rnd.ByChance(splitRate))
        {
            auto splitDirection = direction + (rnd.ByChance(50) ? 5 : 1);
            auto anotherRivers =
              CreateStream(rnd, map, distanceToSea, currentNode, splitDirection, length / 2, splitRate / 2);

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
