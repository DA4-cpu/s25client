// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "RttrForeachPt.h"
#include "commonDefines.h"
#include "lua/GameDataLoader.h"
#include "mapGenFixtures.h"
#include "mapGenerator/Landscape.h"
#include "mapGenerator/RandomMap.h"
#include "mapGenerator/TextureHelper.h"
#include <boost/test/unit_test.hpp>

using namespace rttr::mapGenerator;

namespace {
Map createLandscape(const WorldDescription& worldDesc, unsigned unevenness, unsigned seed)
{
    MapSettings settings;
    settings.size = MapExtent(128, 128);
    settings.water = 25;
    settings.mountains = 25;
    settings.unevenness = static_cast<unsigned short>(unevenness);
    Map map(settings.size, 2, worldDesc, DescIdx<LandscapeDesc>(3), GetMaximumHeight(settings.size));
    RandomUtility rnd(seed);
    CreateLandMasses(map, rnd, settings);
    CreateHeights(map, rnd, settings);
    return map;
}

/// Average of the largest height difference of each node of the given kind to its neighbors
double averageSlope(const Map& map, NodeKind kind)
{
    double sum = 0.;
    unsigned count = 0;
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(map.nodeKind[pt] != kind)
            continue;
        unsigned maxDiff = 0;
        for(const MapPoint& neighbor : map.z.GetNeighbours(pt))
            maxDiff = std::max(maxDiff, static_cast<unsigned>(absDiff(map.z[pt], map.z[neighbor])));
        sum += maxDiff;
        ++count;
    }
    return count ? sum / count : 0.;
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(LandscapeTests, MapGenFixture)

BOOST_AUTO_TEST_CASE(CreateHeights_water_land_and_mountains)
{
    const Map map = createLandscape(worldDesc, 50, 1);
    unsigned numWater = 0, numMountains = 0;
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        switch(map.nodeKind[pt])
        {
            case NodeKind::Water:
                ++numWater;
                BOOST_TEST_REQUIRE(map.z[pt] == map.height.minimum);
                break;
            case NodeKind::Land:
                BOOST_TEST_REQUIRE(map.z[pt] > map.height.minimum);
                BOOST_TEST_REQUIRE(map.mountainLift[pt] == 0u);
                break;
            case NodeKind::Mountain:
                ++numMountains;
                BOOST_TEST_REQUIRE(map.mountainLift[pt] >= 1u);
                break;
        }
        BOOST_TEST_REQUIRE(map.z[pt] <= map.height.maximum);
    }
    BOOST_TEST(numWater > 0u);
    BOOST_TEST(numMountains > 0u);
}

BOOST_AUTO_TEST_CASE(Mountains_are_never_lower_than_the_land_next_to_them)
{
    const Map map = createLandscape(worldDesc, 100, 2);
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(map.nodeKind[pt] != NodeKind::Mountain)
            continue;
        for(const MapPoint& neighbor : map.z.GetNeighbours(pt))
        {
            if(map.nodeKind[neighbor] == NodeKind::Land)
                BOOST_TEST_REQUIRE(map.z[pt] >= map.z[neighbor]);
        }
    }
}

BOOST_AUTO_TEST_CASE(Mines_are_possible_on_most_mountain_nodes)
{
    for(const unsigned unevenness : {0u, 50u, 100u})
    {
        const Map map = createLandscape(worldDesc, unevenness, 3);
        unsigned numMountains = 0, numMinable = 0;
        RTTR_FOREACH_PT(MapPoint, map.size)
        {
            if(map.nodeKind[pt] != NodeKind::Mountain)
                continue;
            ++numMountains;
            // Mines need the south east neighbor at most 3 higher (BQCalculator)
            if(map.z[map.z.GetNeighbour(pt, Direction::SouthEast)] <= map.z[pt] + 3)
                ++numMinable;
        }
        BOOST_TEST_REQUIRE(numMountains > 0u);
        BOOST_TEST_CONTEXT("Unevenness " << unevenness) { BOOST_TEST(numMinable >= numMountains * 95u / 100u); }
    }
}

BOOST_AUTO_TEST_CASE(Unevenness_changes_the_slopes)
{
    const Map flat = createLandscape(worldDesc, 0, 4);
    const Map normal = createLandscape(worldDesc, 50, 4);
    const Map uneven = createLandscape(worldDesc, 100, 4);
    BOOST_TEST(averageSlope(flat, NodeKind::Land) < averageSlope(normal, NodeKind::Land));
    BOOST_TEST(averageSlope(normal, NodeKind::Land) < averageSlope(uneven, NodeKind::Land));
    BOOST_TEST(averageSlope(flat, NodeKind::Mountain) < averageSlope(uneven, NodeKind::Mountain));
    // Flat land is much flatter than uneven land (it only rises towards the inland and the mountains)
    BOOST_TEST(averageSlope(flat, NodeKind::Land) * 2. < averageSlope(uneven, NodeKind::Land));
    BOOST_TEST(averageSlope(uneven, NodeKind::Land) > 1.5);
}

BOOST_AUTO_TEST_CASE(Land_has_flat_and_hilly_regions)
{
    // With a medium unevenness some regions are plains and some are hilly
    const Map map = createLandscape(worldDesc, 50, 5);
    std::vector<unsigned> localRange;
    for(MapCoord y = 4; y + 4 < map.size.y; y += 8)
    {
        for(MapCoord x = 4; x + 4 < map.size.x; x += 8)
        {
            const MapPoint center(x, y);
            if(map.nodeKind[center] != NodeKind::Land)
                continue;
            unsigned minZ = 255, maxZ = 0;
            bool onlyLand = true;
            for(const MapPoint& pt : map.z.GetPointsInRadiusWithCenter(center, 3))
            {
                onlyLand &= map.nodeKind[pt] == NodeKind::Land;
                minZ = std::min<unsigned>(minZ, map.z[pt]);
                maxZ = std::max<unsigned>(maxZ, map.z[pt]);
            }
            if(onlyLand)
                localRange.push_back(maxZ - minZ);
        }
    }
    BOOST_TEST_REQUIRE(localRange.size() > 10u);
    const unsigned minRange = *std::min_element(localRange.begin(), localRange.end());
    const unsigned maxRange = *std::max_element(localRange.begin(), localRange.end());
    BOOST_TEST(minRange <= 3u);
    BOOST_TEST(maxRange >= 5u);
}

BOOST_AUTO_TEST_CASE(Mountain_factor_reduces_mountains_only_there)
{
    MapSettings settings;
    settings.size = MapExtent(128, 128);
    settings.water = 20;
    settings.mountains = 30;
    const auto countMountains = [&](const Map& map, bool left) {
        unsigned result = 0;
        RTTR_FOREACH_PT(MapPoint, map.size)
        {
            if((pt.x < 64) == left && map.nodeKind[pt] == NodeKind::Mountain)
                ++result;
        }
        return result;
    };
    Map normal(settings.size, 2, worldDesc, DescIdx<LandscapeDesc>(3), GetMaximumHeight(settings.size));
    RandomUtility rnd1(9);
    CreateLandMasses(normal, rnd1, settings);

    // Half as many mountains in the left half (same random numbers, so the same noise)
    Map reduced(settings.size, 2, worldDesc, DescIdx<LandscapeDesc>(3), GetMaximumHeight(settings.size));
    NodeMapBase<double> factor;
    factor.Resize(settings.size, 1.);
    RTTR_FOREACH_PT(MapPoint, settings.size)
    {
        if(pt.x < 64)
            factor[pt] = 0.5;
    }
    RandomUtility rnd2(9);
    CreateLandMasses(reduced, rnd2, settings, factor);
    BOOST_TEST_REQUIRE(countMountains(normal, true) > 100u);
    BOOST_TEST(countMountains(reduced, true) < countMountains(normal, true) * 0.7);
    BOOST_TEST(countMountains(reduced, true) > countMountains(normal, true) * 0.3);
    // The other half and the water are unchanged
    BOOST_TEST(countMountains(reduced, false) == countMountains(normal, false));
    RTTR_FOREACH_PT(MapPoint, settings.size)
    {
        BOOST_TEST_REQUIRE((reduced.nodeKind[pt] == NodeKind::Water) == (normal.nodeKind[pt] == NodeKind::Water));
        // Mountains only get fewer, they never appear somewhere else
        if(reduced.nodeKind[pt] == NodeKind::Mountain)
            BOOST_TEST_REQUIRE((normal.nodeKind[pt] == NodeKind::Mountain));
    }
}

BOOST_AUTO_TEST_CASE(GetMountainFactors_by_climate_with_smooth_transitions)
{
    Map map(MapExtent(64, 64), 2, worldDesc, DescIdx<LandscapeDesc>(3), 64);
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        map.temperature[pt] = 80;
        map.climate[pt] = pt.x < 32 ? 80 : 20; // Humid on the left
    }
    MapClimateDesc climate;
    BOOST_TEST(GetMountainFactors(map, climate)[MapPoint(10, 10)] == 1.);
    MapResourceWeight weight;
    weight.climate.temperature.minimum = 60;
    weight.climate.humidity.minimum = 60;
    weight.factor = 0.5;
    climate.mountainWeights.push_back(weight);
    BOOST_TEST(climate.getMountainFactor(80, 80) == 0.5);
    BOOST_TEST(climate.getMountainFactor(80, 20) == 1.);
    BOOST_TEST(climate.getMountainFactor(20, 80) == 1.);
    const auto factors = GetMountainFactors(map, climate);
    BOOST_TEST(factors[MapPoint(16, 10)] == 0.5, boost::test_tools::tolerance(1e-9));
    BOOST_TEST(factors[MapPoint(48, 10)] == 1., boost::test_tools::tolerance(1e-9));
    // In between near the border
    BOOST_TEST(factors[MapPoint(31, 10)] > 0.5);
    BOOST_TEST(factors[MapPoint(31, 10)] < 1.);
    BOOST_TEST(factors[MapPoint(32, 10)] > 0.5);
    BOOST_TEST(factors[MapPoint(32, 10)] < 1.);
}

BOOST_AUTO_TEST_CASE(PlaceMountainBuildingSites_places_flat_sites_on_a_grid)
{
    Map map = createLandscape(worldDesc, 100, 6);
    const auto mountain = map.textureMap.Find(IsMinableMountain);
    const auto land = map.textureMap.Find(IsBuildableLand);
    const auto water = map.textureMap.Find(IsWater);
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        switch(map.nodeKind[pt])
        {
            case NodeKind::Water: map.getTextures()[pt] = TexturePair(water); break;
            case NodeKind::Land: map.getTextures()[pt] = TexturePair(land); break;
            case NodeKind::Mountain: map.getTextures()[pt] = TexturePair(mountain); break;
        }
    }
    unsigned numMountainNodes = 0;
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        if(map.textureMap.All(pt, IsMinableMountain))
            ++numMountainNodes;
    }

    const auto sites = PlaceMountainBuildingSites(map, 6);
    // About one site per 6x6 nodes of mountains (less at the edges)
    BOOST_TEST(sites.size() > numMountainNodes / 36u / 3u);
    BOOST_TEST(sites.size() <= numMountainNodes / 36u + 1u);
    for(const MapPoint& site : sites)
    {
        BOOST_TEST_REQUIRE(map.textureMap.All(site, IsBuildableMountain));
        // Mountains around (mines and the flag)
        for(const MapPoint& p : map.z.GetNeighbours(site))
            BOOST_TEST_REQUIRE(map.textureMap.Any(p, IsMinableMountain));
        // Castle building quality by altitude (BQCalculator): flag at most 1 higher, radius 2 at most 2 different
        BOOST_TEST_REQUIRE(map.z[map.z.GetNeighbour(site, Direction::SouthEast)] <= map.z[site] + 1);
        for(const MapPoint& p : map.z.GetPointsInRadius(site, 2))
            BOOST_TEST_REQUIRE(absDiff(map.z[p], map.z[site]) <= 2u);
        // Far enough apart
        for(const MapPoint& other : sites)
        {
            if(other != site)
                BOOST_TEST_REQUIRE(map.z.CalcDistance(site, other) > 4u);
        }
    }
    BOOST_TEST(PlaceMountainBuildingSites(map, 0).empty());
}

BOOST_AUTO_TEST_CASE(GetTextureLevels_by_kind_and_lift)
{
    Map map = createLandscape(worldDesc, 50, 7);
    const auto levels = GetTextureLevels(map, 2);
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        switch(map.nodeKind[pt])
        {
            case NodeKind::Water: BOOST_TEST_REQUIRE(levels[pt] == 0u); break;
            case NodeKind::Land: BOOST_TEST_REQUIRE(levels[pt] == 1u); break;
            case NodeKind::Mountain: BOOST_TEST_REQUIRE(levels[pt] == 2u + map.mountainLift[pt]); break;
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
