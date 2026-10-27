// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Rivers.h"
#include "RttrForeachPt.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "mapGenerator/Noise.h"
#include "mapGenerator/TextureHelper.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace rttr::mapGenerator {

namespace {
    /// A river stays within this many nodes of its route
    constexpr unsigned corridorRadius = 3;
    /// Sources must be at least this far from other rivers, otherwise rivers run side by side
    constexpr unsigned minSourceDistance = 6;
    constexpr unsigned numDirections = helpers::NumEnumValues_v<Direction>;
    /// Turns of a course in a row: 0 = no turn yet, 1/2 = one/two clockwise turns, 3/4 = one/two counterclockwise turns
    constexpr unsigned numTurnStates = 5;
    constexpr unsigned numStates = numDirections * numTurnStates;
    constexpr unsigned noState = std::numeric_limits<unsigned>::max();

    /// Turn state after turning by 60 degrees, numTurnStates if not allowed: A third turn in the same direction would
    /// circle around a node and make a pond instead of a river
    unsigned NextTurnState(unsigned state, bool clockwise)
    {
        if(clockwise)
            return state == 2 ? numTurnStates : (state == 1 ? 2 : 1);
        return state == 4 ? numTurnStates : (state == 3 ? 4 : 3);
    }

    /// Water already: sea or lake (all corners are water nodes) or an earlier river
    bool IsWaterTriangle(const Map& map, const Triangle& triangle, DescIdx<TerrainDesc> water)
    {
        if(map.textureMap.Get(triangle) == water)
            return true;
        for(const MapPoint& corner : GetTriangleEdges(triangle, map.size))
        {
            if(map.nodeKind[corner] != NodeKind::Water)
                return false;
        }
        return true;
    }

    /// The water of the edge from pt in direction dir shares a triangle side with existing water
    bool ConnectsToWater(const Map& map, const MapPoint& pt, Direction dir, DescIdx<TerrainDesc> water)
    {
        for(const Triangle& triangle : GetTriangles(pt, map.size, dir))
        {
            if(IsWaterTriangle(map, triangle, water))
                return true;
            for(const Triangle& neighbor : GetTriangleNeighbors(triangle, map.size))
            {
                if(IsWaterTriangle(map, neighbor, water))
                    return true;
            }
        }
        return false;
    }

    struct Course
    {
        /// Nodes from the source to the mouth (the last node is at the sea, a lake or another river)
        std::vector<MapPoint> nodes;
        /// directions[i] leads from nodes[i] to nodes[i + 1]
        std::vector<Direction> directions;
    };

    /**
     * Course of a river along the edges of the map like in the original generator (CreateStream): Every step goes along
     * one edge and the 2 triangles beside the edge become water. The direction changes by 60 degrees at every step,
     * never straight on: Then the triangles of 2 steps share a side and the river is continuous and one triangle wide
     * (2 steps in the same direction would only touch at a corner). The course follows the route (downhill to the
     * water) as close as possible: Shortest way in the corridor around the route, uphill steps are expensive, a bit of
     * randomness (wobble) for natural bends.
     */
    Course FindCourse(const Map& map, const std::vector<MapPoint>& route, const NodeMapBase<uint8_t>& riverWater,
                      const std::vector<float>& wobble, DescIdx<TerrainDesc> water, std::vector<int>& localIdx)
    {
        // Corridor around the route. Nodes just outside of it may only be the mouth
        std::vector<MapPoint> corridor;
        std::vector<uint8_t> distance;
        for(const MapPoint& pt : route)
        {
            const unsigned idx = map.z.GetIdx(pt);
            if(localIdx[idx] < 0)
            {
                localIdx[idx] = static_cast<int>(corridor.size());
                corridor.push_back(pt);
                distance.push_back(0);
            }
        }
        for(unsigned i = 0; i < corridor.size(); ++i)
        {
            if(distance[i] > corridorRadius)
                continue;
            const MapPoint cur = corridor[i];
            const auto curDistance = distance[i];
            for(const MapPoint& neighbor : map.z.GetNeighbours(cur))
            {
                const unsigned idx = map.z.GetIdx(neighbor);
                if(localIdx[idx] < 0)
                {
                    localIdx[idx] = static_cast<int>(corridor.size());
                    corridor.push_back(neighbor);
                    distance.push_back(static_cast<uint8_t>(curDistance + 1));
                }
            }
        }

        const MapPoint source = route.front();
        const auto numLocal = static_cast<unsigned>(corridor.size());
        std::vector<float> cost(numLocal * numStates, std::numeric_limits<float>::max());
        std::vector<unsigned> parent(numLocal * numStates, noState);
        std::vector<uint8_t> isMouth(numLocal * numStates, 0);
        struct Entry
        {
            float cost;
            unsigned order;
            unsigned state;
            bool operator>(const Entry& other) const noexcept
            {
                return cost > other.cost || (cost == other.cost && order > other.order);
            }
        };
        std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
        unsigned order = 0;

        const auto step = [&](const MapPoint& from, float fromCost, unsigned fromState, Direction dir,
                              unsigned turnState) {
            const MapPoint next = map.z.GetNeighbour(from, dir);
            if(next == source)
                return;
            const unsigned idx = map.z.GetIdx(next);
            const int local = localIdx[idx];
            if(local < 0)
                return;
            const bool atWater = map.nodeKind[next] == NodeKind::Water || riverWater[next];
            const bool mouth = atWater && ConnectsToWater(map, from, dir, water);
            // Once in the water (coast of the sea or a lake) the river must not go back to the land
            if(!mouth && map.nodeKind[from] == NodeKind::Water && map.nodeKind[next] != NodeKind::Water)
                return;
            float stepCost = 1.f + 0.7f * distance[local] + wobble[idx];
            if(!mouth)
            {
                // Touching another river without joining it would make a blob
                if(riverWater[next] || distance[local] > corridorRadius)
                    return;
                if(helpers::contains_if(map.z.GetNeighbours(next),
                                        [&riverWater](const MapPoint& p) { return riverWater[p] != 0; }))
                    stepCost += 2.f;
            }
            if(map.z[next] > map.z[from])
                stepCost += 2.f * (map.z[next] - map.z[from]);
            const unsigned state =
              static_cast<unsigned>(local) * numStates + static_cast<unsigned>(dir) * numTurnStates + turnState;
            if(fromCost + stepCost < cost[state])
            {
                cost[state] = fromCost + stepCost;
                parent[state] = fromState;
                isMouth[state] = mouth ? 1 : 0;
                queue.push(Entry{cost[state], order++, state});
            }
        };

        for(const Direction dir : helpers::EnumRange<Direction>{})
            step(source, 0.f, noState, dir, 0);

        Course course;
        unsigned mouthState = noState;
        while(!queue.empty())
        {
            const Entry cur = queue.top();
            queue.pop();
            if(cur.cost > cost[cur.state])
                continue;
            if(isMouth[cur.state])
            {
                mouthState = cur.state;
                break;
            }
            const MapPoint pt = corridor[cur.state / numStates];
            const auto dir = static_cast<Direction>((cur.state % numStates) / numTurnStates);
            const unsigned turnState = cur.state % numTurnStates;
            for(const bool clockwise : {true, false})
            {
                const unsigned nextTurnState = NextTurnState(turnState, clockwise);
                if(nextTurnState < numTurnStates)
                    step(pt, cur.cost, cur.state, clockwise ? dir + 1u : dir - 1u, nextTurnState);
            }
        }

        if(mouthState != noState)
        {
            for(unsigned state = mouthState; state != noState; state = parent[state])
            {
                course.nodes.push_back(corridor[state / numStates]);
                course.directions.push_back(static_cast<Direction>((state % numStates) / numTurnStates));
            }
            course.nodes.push_back(source);
            std::reverse(course.nodes.begin(), course.nodes.end());
            std::reverse(course.directions.begin(), course.directions.end());

            // The course must not come back to itself (the shortest way rarely does): Every node once and no node next
            // to an earlier part of the course (3 steps turning by 60 degrees are always 2 nodes or more apart). The
            // mouth is not checked, the river might have run along the water before.
            const auto numCourseNodes = static_cast<int>(course.nodes.size());
            std::vector<int> stepOf(numLocal, -1);
            bool valid = true;
            for(int i = 0; i + 1 < numCourseNodes && valid; ++i)
            {
                int& curStep = stepOf[localIdx[map.z.GetIdx(course.nodes[i])]];
                valid = curStep < 0;
                curStep = i;
            }
            for(int i = 0; i + 1 < numCourseNodes && valid; ++i)
            {
                for(const MapPoint& neighbor : map.z.GetNeighbours(course.nodes[i]))
                {
                    const int local = localIdx[map.z.GetIdx(neighbor)];
                    if(local >= 0 && stepOf[local] >= 0 && std::abs(stepOf[local] - i) >= 3)
                        valid = false;
                }
            }
            if(!valid)
                course = Course();
        }

        for(const MapPoint& pt : corridor)
            localIdx[map.z.GetIdx(pt)] = -1;
        return course;
    }
} // namespace

std::vector<River> CreateRivers(Map& map, RandomUtility& rnd, const RiverSettings& settings)
{
    std::vector<River> rivers;
    const MapExtent size = map.size;
    const unsigned numNodes = static_cast<unsigned>(size.x) * size.y;
    if(settings.amount == 0)
        return rivers;

    // 1) Route from every land node to the water: priority flood from all water nodes. The priority is the height
    // (with some smooth noise so rivers meander), but never lower than the priority of the node it was reached from,
    // so basins are flowed through over their lowest pass instead of ending there.
    const auto meander = CreateFractalNoise(rnd, size, 6., 2, 0.5);
    constexpr int noDownstream = -1;
    std::vector<int> downstream(numNodes, noDownstream);
    std::vector<uint8_t> visited(numNodes, 0);
    struct Entry
    {
        double priority;
        unsigned order;
        unsigned idx;
        bool operator>(const Entry& other) const noexcept
        {
            return priority > other.priority || (priority == other.priority && order > other.order);
        }
    };
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
    unsigned order = 0;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(map.nodeKind[pt] == NodeKind::Water)
        {
            const unsigned idx = map.z.GetIdx(pt);
            visited[idx] = 1;
            queue.push(Entry{-1., order++, idx});
        }
    }
    if(queue.empty())
        return rivers; // No sea or lake to flow to
    while(!queue.empty())
    {
        const Entry cur = queue.top();
        queue.pop();
        const MapPoint pt(static_cast<MapCoord>(cur.idx % size.x), static_cast<MapCoord>(cur.idx / size.x));
        for(const MapPoint& neighbor : map.z.GetNeighbours(pt))
        {
            const unsigned nIdx = map.z.GetIdx(neighbor);
            if(visited[nIdx])
                continue;
            visited[nIdx] = 1;
            downstream[nIdx] = static_cast<int>(cur.idx);
            const double priority = std::max(cur.priority, map.z[neighbor] + 1.5 * meander[neighbor]);
            queue.push(Entry{priority, order++, nIdx});
        }
    }

    // 2) Sources: high nodes, in random order
    const uint8_t maxHeight = *std::max_element(map.z.begin(), map.z.end());
    const unsigned minSourceHeight = std::max(1u, maxHeight * settings.minHeight / 100u);
    std::vector<MapPoint> sources;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(map.nodeKind[pt] != NodeKind::Water && map.z[pt] >= minSourceHeight)
            sources.push_back(pt);
    }
    for(unsigned i = sources.size(); i > 1; --i)
        std::swap(sources[i - 1], sources[rnd.Index(i)]);

    std::vector<float> wobble(numNodes);
    for(float& value : wobble)
        value = static_cast<float>(rnd.RandomDouble(0., 0.6));

    const auto water = map.textureMap.Find(IsWater);
    const unsigned numRivers =
      std::max(1u, static_cast<unsigned>(std::lround(settings.amount / 100. * numNodes / 2000.)));
    // Corners of the water triangles of all rivers
    NodeMapBase<uint8_t> riverWater;
    riverWater.Resize(size, 0);
    // Next node downstream on the course of a river
    std::vector<int> nextOnCourse(numNodes, noDownstream);
    std::vector<int> localIdx(numNodes, -1);
    const auto isRiverWater = [&riverWater](const MapPoint& p) { return riverWater[p] != 0; };
    for(const MapPoint& source : sources)
    {
        if(rivers.size() >= numRivers)
            break;
        if(helpers::contains_if(map.z.GetPointsInRadiusWithCenter(source, minSourceDistance), isRiverWater))
            continue;

        // Route: follow the downstream neighbors until the water or another river
        std::vector<MapPoint> route;
        int idx = static_cast<int>(map.z.GetIdx(source));
        while(idx != noDownstream)
        {
            const MapPoint pt(static_cast<MapCoord>(idx % size.x), static_cast<MapCoord>(idx / size.x));
            route.push_back(pt);
            if(map.nodeKind[pt] == NodeKind::Water || riverWater[pt])
                break;
            idx = downstream[idx];
        }
        if(route.size() < std::max(2u, settings.minLength / 2))
            continue;

        const Course course = FindCourse(map, route, riverWater, wobble, water, localIdx);
        if(course.directions.size() < std::max(2u, settings.minLength))
            continue;

        // Carve: never rise downstream. The mouth is the sea, a lake or another river
        const auto& nodes = course.nodes;
        const MapPoint mouth = nodes.back();
        for(unsigned i = 1; i + 1 < nodes.size(); ++i)
            map.z[nodes[i]] = std::min(map.z[nodes[i]], map.z[nodes[i - 1]]);
        const MapPoint lastNode = nodes[nodes.size() - 2];
        if(map.riverCourse[mouth] && map.z[mouth] > map.z[lastNode])
        {
            // Joins a river higher up: Lower that river from here on
            map.z[mouth] = map.z[lastNode];
            for(int cur = static_cast<int>(map.z.GetIdx(mouth)); nextOnCourse[cur] != noDownstream;
                cur = nextOnCourse[cur])
            {
                const auto next = static_cast<unsigned>(nextOnCourse[cur]);
                if(map.z[next] <= map.z[static_cast<unsigned>(cur)])
                    break;
                map.z[next] = map.z[static_cast<unsigned>(cur)];
            }
        }

        River river;
        for(unsigned i = 0; i < course.directions.size(); ++i)
        {
            for(const Triangle& triangle : GetTriangles(nodes[i], size, course.directions[i]))
            {
                map.textureMap.Set(triangle, water);
                for(const MapPoint& corner : GetTriangleEdges(triangle, size))
                {
                    river.insert(corner);
                    riverWater[corner] = 1;
                }
            }
            if(map.nodeKind[nodes[i]] == NodeKind::Water)
                continue; // Coast of the sea or a lake
            map.riverCourse[nodes[i]] = 1;
            if(map.nodeKind[nodes[i + 1]] != NodeKind::Water)
                nextOnCourse[map.z.GetIdx(nodes[i])] = static_cast<int>(map.z.GetIdx(nodes[i + 1]));
        }
        rivers.push_back(std::move(river));
    }
    return rivers;
}

} // namespace rttr::mapGenerator
