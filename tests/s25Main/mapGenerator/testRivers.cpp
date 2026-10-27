// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "RttrForeachPt.h"
#include "mapGenFixtures.h"
#include "mapGenerator/Landscape.h"
#include "mapGenerator/RandomMap.h"
#include "mapGenerator/Rivers.h"
#include "mapGenerator/TextureHelper.h"
#include <boost/test/unit_test.hpp>

using namespace rttr::mapGenerator;

// LCOV_EXCL_START
BOOST_TEST_DONT_PRINT_LOG_VALUE(rttr::mapGenerator::NodeKind)
// LCOV_EXCL_STOP

namespace {
/// Map with sea in the west (x < 4), land rising to the east and a mountain ridge in the east
struct RiverFixture : MapGenFixture
{
    MapExtent size = MapExtent(64, 32);
    Map map;
    RiverFixture() : map(createMap(size))
    {
        RTTR_FOREACH_PT(MapPoint, size)
        {
            if(pt.x < 4)
            {
                map.nodeKind[pt] = NodeKind::Water;
                map.z[pt] = 0;
            } else
            {
                map.nodeKind[pt] = pt.x >= 50 ? NodeKind::Mountain : NodeKind::Land;
                map.z[pt] = static_cast<uint8_t>(1 + (pt.x - 4) / 2);
            }
        }
    }

    unsigned countCourse() const
    {
        return static_cast<unsigned>(std::count(map.riverCourse.begin(), map.riverCourse.end(), 1));
    }
};

/// Water texture (from a river, textures are not set otherwise)
bool hasWaterTexture(const Map& map, const Triangle& triangle)
{
    return map.textureMap.Get(triangle) == map.textureMap.Find(IsWater);
}

bool isSea(const Map& map, const Triangle& triangle)
{
    for(const MapPoint& corner : GetTriangleEdges(triangle, map.size))
    {
        if(map.nodeKind[corner] != NodeKind::Water)
            return false;
    }
    return true;
}

/// Number of river triangles (water texture, not the sea) which are not connected to the sea by water triangles
/// sharing a side: 0 = all rivers are continuous from the source to the sea
unsigned countDisconnectedRiverWater(const Map& map)
{
    const MapExtent size = map.size;
    const auto index = [&size](const Triangle& t) {
        return (static_cast<unsigned>(t.position.y) * size.x + t.position.x) * 2u + (t.rsu ? 1u : 0u);
    };
    const auto isWater = [&map](const Triangle& t) { return isSea(map, t) || hasWaterTexture(map, t); };
    std::vector<uint8_t> reached(static_cast<size_t>(size.x) * size.y * 2u, 0);
    std::vector<Triangle> todo;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        for(const bool rsu : {true, false})
        {
            const Triangle triangle(rsu, pt);
            if(isSea(map, triangle))
            {
                reached[index(triangle)] = 1;
                todo.push_back(triangle);
            }
        }
    }
    while(!todo.empty())
    {
        const Triangle cur = todo.back();
        todo.pop_back();
        for(const Triangle& neighbor : GetTriangleNeighbors(cur, size))
        {
            if(!reached[index(neighbor)] && isWater(neighbor))
            {
                reached[index(neighbor)] = 1;
                todo.push_back(neighbor);
            }
        }
    }
    unsigned numDisconnected = 0;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        for(const bool rsu : {true, false})
        {
            const Triangle triangle(rsu, pt);
            if(hasWaterTexture(map, triangle) && !reached[index(triangle)])
                ++numDisconnected;
        }
    }
    return numDisconnected;
}

/// Number of land nodes with more than the given number of water triangles around
unsigned countWideRiverNodes(const Map& map, unsigned maxWaterTriangles)
{
    unsigned result = 0;
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(map.nodeKind[pt] == NodeKind::Water)
            continue;
        unsigned numWater = 0;
        for(const Triangle& triangle : GetTriangles(pt, map.size))
            numWater += hasWaterTexture(map, triangle) ? 1 : 0;
        if(numWater > maxWaterTriangles)
            ++result;
    }
    return result;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(RiversTest, RiverFixture)

BOOST_AUTO_TEST_CASE(CreateRivers_creates_nothing_for_zero_amount)
{
    RandomUtility rnd(1);
    RiverSettings settings;
    settings.amount = 0;
    BOOST_TEST(CreateRivers(map, rnd, settings).empty());
    BOOST_TEST(countCourse() == 0u);
}

BOOST_AUTO_TEST_CASE(CreateRivers_flows_from_high_sources_to_the_water)
{
    RandomUtility rnd(2);
    RiverSettings settings;
    settings.amount = 100;
    settings.minHeight = 60;
    settings.minLength = 10;
    const auto rivers = CreateRivers(map, rnd, settings);
    BOOST_TEST_REQUIRE(rivers.size() == 1u);
    const uint8_t maxHeight = *std::max_element(map.z.begin(), map.z.end());

    // Continuous: every river triangle is connected to the sea, no land in between
    BOOST_TEST(countDisconnectedRiverWater(map) == 0u);
    // One triangle wide like in the original generator: The course nodes have 4 water triangles (2 at the source),
    // the nodes beside the course less
    BOOST_TEST(countWideRiverNodes(map, 4) == 0u);

    // The course nodes form a chain from the sea upwards: Follow them from the sea
    NodeMapBase<uint8_t> reached;
    reached.Resize(size, 0);
    std::vector<MapPoint> todo;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(map.riverCourse[pt])
        {
            BOOST_TEST_REQUIRE(map.nodeKind[pt] != NodeKind::Water);
            BOOST_TEST_REQUIRE(helpers::contains_if(GetTriangles(pt, size),
                                                    [this](const Triangle& t) { return hasWaterTexture(map, t); }));
            if(helpers::contains_if(map.z.GetNeighbours(pt),
                                    [this](const MapPoint& p) { return map.nodeKind[p] == NodeKind::Water; }))
            {
                reached[pt] = 1;
                todo.push_back(pt);
            }
        }
    }
    BOOST_TEST_REQUIRE(todo.size() == 1u);
    unsigned maxCourseHeight = 0;
    while(!todo.empty())
    {
        const MapPoint pt = todo.back();
        todo.pop_back();
        maxCourseHeight = std::max<unsigned>(maxCourseHeight, map.z[pt]);
        unsigned numUpstream = 0;
        for(const MapPoint& neighbor : map.z.GetNeighbours(pt))
        {
            if(map.riverCourse[neighbor] && !reached[neighbor])
            {
                // Rivers never rise downstream
                BOOST_TEST_REQUIRE(map.z[neighbor] >= map.z[pt]);
                reached[neighbor] = 1;
                todo.push_back(neighbor);
                ++numUpstream;
            }
        }
        // A single chain: The course doesn't touch itself
        BOOST_TEST_REQUIRE(numUpstream <= 1u);
    }
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(map.riverCourse[pt])
            BOOST_TEST_REQUIRE(reached[pt]);
    }
    // Sources are high up
    BOOST_TEST(maxCourseHeight >= maxHeight * 60u / 100u);
    // Long rivers: about from the mountains to the sea
    BOOST_TEST(countCourse() >= 40u);
}

BOOST_AUTO_TEST_CASE(CreateRivers_turns_at_every_step)
{
    RandomUtility rnd(3);
    RiverSettings settings;
    settings.amount = 30;
    settings.minHeight = 80;
    settings.minLength = 10;
    BOOST_TEST_REQUIRE(CreateRivers(map, rnd, settings).size() == 1u);
    // Like in the original generator 2 steps in the same direction never follow each other: then the water of the
    // 2 steps would only touch at a corner. So course nodes never lie on a straight line of 3 nodes.
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(!map.riverCourse[pt])
            continue;
        for(const Direction dir : {Direction::West, Direction::NorthWest, Direction::NorthEast})
        {
            BOOST_TEST_REQUIRE(
              !(map.riverCourse[map.z.GetNeighbour(pt, dir)] && map.riverCourse[map.z.GetNeighbour(pt, dir + 3u)]));
        }
    }
}

BOOST_AUTO_TEST_CASE(CreateRivers_needs_water_to_flow_to)
{
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(map.nodeKind[pt] == NodeKind::Water)
        {
            map.nodeKind[pt] = NodeKind::Land;
            map.z[pt] = 1;
        }
    }
    RandomUtility rnd(4);
    RiverSettings settings;
    settings.amount = 100;
    BOOST_TEST(CreateRivers(map, rnd, settings).empty());
}

BOOST_AUTO_TEST_CASE(CreateRivers_ignores_short_rivers)
{
    RandomUtility rnd(5);
    RiverSettings settings;
    settings.amount = 100;
    settings.minHeight = 0;
    settings.minLength = 200; // Longer than possible
    BOOST_TEST(CreateRivers(map, rnd, settings).empty());
    BOOST_TEST(countCourse() == 0u);
}

BOOST_AUTO_TEST_CASE(Rivers_on_generated_landscapes_are_continuous_and_narrow)
{
    for(const unsigned seed : {11u, 12u, 13u})
    {
        MapSettings settings;
        settings.size = MapExtent(128, 128);
        settings.water = 25;
        settings.mountains = 25;
        settings.rivers = 100;
        Map genMap(settings.size, 2, worldDesc, DescIdx<LandscapeDesc>(3), GetMaximumHeight(settings.size));
        RandomUtility rnd(seed);
        CreateLandMasses(genMap, rnd, settings);
        CreateHeights(genMap, rnd, settings);
        RiverSettings riverSettings;
        riverSettings.amount = 100;
        const auto rivers = CreateRivers(genMap, rnd, riverSettings);
        BOOST_TEST_CONTEXT("Seed " << seed)
        {
            // At 100 % about one river per 2000 nodes
            BOOST_TEST(rivers.size() >= 5u);
            BOOST_TEST(countDisconnectedRiverWater(genMap) == 0u);
            // Only where rivers join, nodes may be surrounded by more water
            BOOST_TEST(countWideRiverNodes(genMap, 4) <= rivers.size());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
