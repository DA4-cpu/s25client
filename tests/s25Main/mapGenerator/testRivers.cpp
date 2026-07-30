// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenFixtures.h"
#include "mapGenerator/Algorithms.h"
#include "mapGenerator/Rivers.h"
#include "mapGenerator/TextureHelper.h"
#include "mapGenerator/Triangles.h"
#include <boost/test/unit_test.hpp>

using namespace rttr::mapGenerator;

class RiverFixture : public MapGenFixture
{
public:
    Map map;
    RandomUtility rnd;
    RiverFixture() : map(createMap(MapExtent(8, 10))), rnd(0) { map.z.Resize(map.size, map.height.maximum); }

    /// A distance-to-sea field for maps with no sea at all: every entry is "unreachable". Streams fall
    /// back to pure (unguaranteed) height-based meandering in that case - good enough for tests that
    /// only care about invariants unrelated to actually reaching the sea.
    NodeMapBase<unsigned> noSea() const
    {
        NodeMapBase<unsigned> distances;
        distances.Resize(map.size, static_cast<unsigned>(-1));
        return distances;
    }

    NodeMapBase<unsigned> distanceToSeaField() const
    {
        return DistancesTo(map.size, [this](const MapPoint& pt) { return map.z[pt] == map.height.minimum; });
    }
};

BOOST_FIXTURE_TEST_SUITE(RiversTest, RiverFixture)

BOOST_AUTO_TEST_CASE(CreateStream_marks_both_triangles_of_every_visited_node)
{
    // Regression test for a gap bug: a stream used to only mark the 2 triangles pointing in its
    // current direction of travel, which - depending on direction and row parity - sometimes missed
    // one or even both of a node's own triangles, especially right after the stream turned. This
    // showed up as small notches of land poking into an otherwise straight river. Now every node the
    // stream visits gets its whole tile (both triangles) marked, regardless of direction, so this must
    // hold even for a single, deterministic first step.
    const auto distanceToSea = noSea();
    const MapPoint source(3, 3);

    CreateStream(rnd, map, distanceToSea, source, Direction::East, 1);

    BOOST_TEST_REQUIRE(map.textureMap.Check(Triangle(true, source), IsWater));
    BOOST_TEST_REQUIRE(map.textureMap.Check(Triangle(false, source), IsWater));
}

BOOST_AUTO_TEST_CASE(CreateStream_returns_only_connected_nodes)
{
    const auto distanceToSea = noSea();
    const MapPoint source(3, 2);
    const unsigned length = 7;

    for(const auto d : helpers::enumRange<Direction>())
    {
        auto river = CreateStream(rnd, map, distanceToSea, source, d, length);

        auto containedByRiver = [&river](const MapPoint& pt) { return helpers::contains(river, pt); };

        for(const MapPoint& pt : river)
        {
            BOOST_TEST_REQUIRE(helpers::contains_if(map.z.GetNeighbours(pt), containedByRiver));
        }
    }
}

BOOST_AUTO_TEST_CASE(CreateStream_returns_only_nodes_covered_by_water)
{
    auto land = map.textureMap.Find(IsBuildableLand);
    map.getTextures().Resize(map.size, land);
    const auto distanceToSea = noSea();
    const MapPoint source(3, 2);
    const unsigned length = 7;

    for(const auto d : helpers::enumRange<Direction>())
    {
        auto river = CreateStream(rnd, map, distanceToSea, source, d, length);

        for(const MapPoint& pt : river)
        {
            BOOST_TEST_REQUIRE(map.textureMap.Any(pt, IsWater));
        }
    }
}

BOOST_AUTO_TEST_CASE(CreateStream_reduces_height_of_river_nodes)
{
    NodeMapBase<uint8_t> originalZ;
    originalZ.Resize(map.size);
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        originalZ[pt] = map.z[pt];
    }

    const auto distanceToSea = noSea();
    const MapPoint source(4, 1);
    const unsigned length = 6;

    for(const auto d : helpers::enumRange<Direction>())
    {
        auto river = CreateStream(rnd, map, distanceToSea, source, d, length);

        for(const MapPoint& pt : river)
        {
            BOOST_TEST_REQUIRE(map.z[pt] < originalZ[pt]);
        }
    }
}

BOOST_AUTO_TEST_CASE(CreateStream_still_lowers_its_bed_when_it_reaches_the_sea_immediately)
{
    // Regression test: a stream used to `return` as soon as it reached sea level, skipping the
    // height-carving step below entirely whenever that happened on its very first step. Now it always
    // falls through to carve its bed, no matter how quickly it reaches the sea.
    const MapPoint source(3, 3);
    map.z.Resize(map.size, map.height.maximum);
    map.z[source] = map.height.minimum;
    const auto distanceToSea = distanceToSeaField();

    const auto river = CreateStream(rnd, map, distanceToSea, source, Direction::East, 20);

    BOOST_TEST_REQUIRE(!river.empty());
    for(const MapPoint& pt : river)
    {
        BOOST_TEST_REQUIRE(map.z[pt] <= map.height.maximum - 1);
    }
}

BOOST_AUTO_TEST_CASE(CreateStream_always_reaches_the_sea)
{
    // The core guarantee this whole algorithm is built around: previously, a stream's very first
    // (essentially arbitrary) heading fixed a 120-degree cone of directions it could never leave for
    // its entire length. If the actual sea happened to lie outside that cone, the stream was doomed to
    // wander the wrong general region of the map for its whole length, regardless of terrain - showing
    // up as rivers that petered out on dry land or stopped just short of the water. Now it always
    // reaches an actual sea node, given a length budget of at least "distance to sea" + 1 (see
    // CreateStream's docs for why the "+1"), regardless of which of the 6 directions it starts in.
    map.z.Resize(map.size, map.height.maximum);
    for(unsigned y = 0; y < map.size.y; ++y)
    {
        for(unsigned x = 0; x < 3; ++x)
        {
            map.z[MapPoint(x, y)] = map.height.minimum;
        }
    }
    const auto distanceToSea = distanceToSeaField();

    const MapPoint source(6, 5);
    BOOST_TEST_REQUIRE(distanceToSea[source] != static_cast<unsigned>(-1));

    for(const auto d : helpers::enumRange<Direction>())
    {
        const unsigned length = distanceToSea[source] + 1;
        const auto river = CreateStream(rnd, map, distanceToSea, source, d, length);

        const bool reachedSea = helpers::contains_if(
          river, [this](const MapPoint& pt) { return map.z[pt] == map.height.minimum; });
        BOOST_TEST_REQUIRE(reachedSea);
    }
}

BOOST_AUTO_TEST_CASE(CreateStream_prefers_lower_terrain_among_equally_short_paths)
{
    // Among several directions that are all equally good in terms of actual progress towards the sea,
    // the stream should still prefer lower terrain, purely for a more natural-looking meander - this
    // must never override the progress guarantee tested above, only break ties between equally good
    // options.
    auto land = map.textureMap.Find(IsBuildableLand);
    map.getTextures().Resize(map.size, land);
    map.z.Resize(map.size, map.height.maximum);

    const MapPoint source(3, 4);
    // NorthEast's reverse (SouthWest) doesn't touch either candidate below, so it can't skew the
    // "avoid immediately reversing" rule towards or against either of them.
    const Direction initialDirection = Direction::NorthEast;
    const MapPoint lowNeighbour = map.z.GetNeighbour(source, Direction::East);
    const MapPoint highNeighbour = map.z.GetNeighbour(source, Direction::West);

    // Two separate "sea" points, one reachable via each neighbor, both exactly 2 hex-steps from
    // source, so distanceToSea ties between the two neighbors from source's point of view.
    map.z[map.z.GetNeighbour(lowNeighbour, Direction::East)] = map.height.minimum;
    map.z[map.z.GetNeighbour(highNeighbour, Direction::West)] = map.height.minimum;
    map.z[lowNeighbour] = map.height.minimum + 5;

    const auto distanceToSea = distanceToSeaField();

    // Sanity-check our own setup: the two candidates really do tie, and no other neighbor of "source"
    // ties with them (which would dilute the test).
    BOOST_TEST_REQUIRE(distanceToSea[lowNeighbour] == distanceToSea[highNeighbour]);
    for(Direction dir : {Direction::NorthEast, Direction::SouthEast, Direction::SouthWest, Direction::NorthWest})
    {
        BOOST_TEST_REQUIRE(distanceToSea[map.z.GetNeighbour(source, dir)] > distanceToSea[lowNeighbour]);
    }

    unsigned pickedLow = 0;
    const unsigned trials = 30;
    for(unsigned trial = 0; trial < trials; ++trial)
    {
        map.getTextures().Resize(map.size, land);
        CreateStream(rnd, map, distanceToSea, source, initialDirection, 2);

        const bool lowVisited = map.textureMap.Check(Triangle(true, lowNeighbour), IsWater)
                                 && map.textureMap.Check(Triangle(false, lowNeighbour), IsWater);
        if(lowVisited)
        {
            ++pickedLow;
        }
    }

    BOOST_TEST_REQUIRE(pickedLow > trials / 2);
}

BOOST_AUTO_TEST_SUITE_END()
