// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PointOutput.h"
#include "RttrForeachPt.h"
#include "mapGenFixtures.h"
#include "mapGenerator/Noise.h"
#include "mapGenerator/TextureHelper.h"
#include "mapGenerator/Zones.h"
#include "gameData/MapGenDesc.h"
#include <boost/test/unit_test.hpp>
#include <queue>
#include <set>

using namespace rttr::mapGenerator;

BOOST_AUTO_TEST_SUITE(ZonesTests)

namespace {
/// Number of connected areas of the given zone (on the wrapped map)
unsigned CountAreas(const NodeMapBase<uint8_t>& zones, uint8_t zone)
{
    NodeMapBase<uint8_t> visited;
    visited.Resize(zones.GetSize(), 0);
    unsigned numAreas = 0;
    RTTR_FOREACH_PT(MapPoint, zones.GetSize())
    {
        if(zones[pt] != zone || visited[pt])
            continue;
        ++numAreas;
        std::queue<MapPoint> todo;
        todo.push(pt);
        visited[pt] = 1;
        while(!todo.empty())
        {
            const MapPoint cur = todo.front();
            todo.pop();
            for(const MapPoint& neighbor : zones.GetNeighbours(cur))
            {
                if(zones[neighbor] == zone && !visited[neighbor])
                {
                    visited[neighbor] = 1;
                    todo.push(neighbor);
                }
            }
        }
    }
    return numAreas;
}
} // namespace

BOOST_AUTO_TEST_CASE(GenerateZones_creates_each_zone_exactly_once_as_connected_area)
{
    for(const MapExtent size : {MapExtent(32, 32), MapExtent(64, 128), MapExtent(160, 96)})
    {
        for(const unsigned numZones : {1u, 2u, 7u})
        {
            RandomUtility rnd(size.x * 1000 + numZones);
            const auto zones = GenerateZones(rnd, size, numZones);
            BOOST_TEST_REQUIRE(zones.GetSize() == size);
            std::vector<unsigned> numNodes(numZones, 0);
            RTTR_FOREACH_PT(MapPoint, size)
            {
                BOOST_TEST_REQUIRE(zones[pt] < numZones);
                ++numNodes[zones[pt]];
            }
            for(unsigned zone = 0; zone < numZones; ++zone)
            {
                BOOST_TEST_CONTEXT("Size " << size.x << "x" << size.y << ", zone " << zone << "/" << numZones)
                {
                    BOOST_TEST(numNodes[zone] > 0u);
                    BOOST_TEST(CountAreas(zones, static_cast<uint8_t>(zone)) == 1u);
                    // Roughly equal sizes: no zone has less than a quarter of the average
                    BOOST_TEST(numNodes[zone] * numZones * 4 > size.x * size.y);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(GenerateZones_is_deterministic)
{
    RandomUtility rnd1(42), rnd2(42);
    const auto zones1 = GenerateZones(rnd1, MapExtent(64, 64), 7);
    const auto zones2 = GenerateZones(rnd2, MapExtent(64, 64), 7);
    BOOST_TEST(std::equal(zones1.begin(), zones1.end(), zones2.begin()));
}

BOOST_AUTO_TEST_CASE(FractalNoise_wraps_around_the_map)
{
    RandomUtility rnd(7);
    const MapExtent size(64, 32);
    const auto noise = CreateFractalNoise(rnd, size, 16., 3, 0.5);
    // Neighbors across the map border differ as little as neighbors inside the map
    double maxInnerDiff = 0., maxBorderDiff = 0.;
    for(MapCoord y = 0; y < size.y; ++y)
    {
        for(MapCoord x = 0; x + 1 < size.x; ++x)
            maxInnerDiff = std::max(maxInnerDiff, std::abs(noise[MapPoint(x, y)] - noise[MapPoint(x + 1, y)]));
        maxBorderDiff = std::max(maxBorderDiff, std::abs(noise[MapPoint(size.x - 1, y)] - noise[MapPoint(0, y)]));
    }
    BOOST_TEST(maxBorderDiff <= maxInnerDiff * 1.5);
    const auto uniform = ToUniform(noise);
    BOOST_TEST(*std::min_element(uniform.begin(), uniform.end()) == 0.);
    BOOST_TEST(*std::max_element(uniform.begin(), uniform.end()) < 1.);
}

BOOST_FIXTURE_TEST_CASE(ComputeTerrainFlags_distinguishes_fresh_and_salt_water, MapGenFixture)
{
    // Big sea on the left half, a small lake on the right half, land in between
    const MapExtent size(40, 40);
    auto map = createMap(size);
    const auto water = map.textureMap.Find(IsWater);
    const auto land = map.textureMap.Find(IsBuildableLand);
    const auto mountain = map.textureMap.Find(IsMinableMountain);
    RTTR_FOREACH_PT(MapPoint, size)
    {
        map.getTextures()[pt] = TexturePair(land);
        if(pt.x < 16)
            map.getTextures()[pt] = TexturePair(water);
        else if(pt.x >= 28 && pt.x < 31 && pt.y >= 18 && pt.y < 21)
            map.getTextures()[pt] = TexturePair(water);
        else if(pt.x >= 34 && pt.x < 38)
            map.getTextures()[pt] = TexturePair(mountain);
    }
    // Lakes up to 2 % of the map (32 nodes) are fresh water
    ComputeTerrainFlags(map, 3, 2.);

    const auto has = [&map](const MapPoint& pt, MapGenTerrain terrain) {
        return (map.terrainFlags[pt] & toFlag(terrain)) != 0;
    };
    BOOST_TEST(has(MapPoint(5, 5), MapGenTerrain::Saltwater));
    BOOST_TEST(!has(MapPoint(5, 5), MapGenTerrain::Freshwater));
    BOOST_TEST(has(MapPoint(29, 19), MapGenTerrain::Freshwater));
    BOOST_TEST(!has(MapPoint(29, 19), MapGenTerrain::Saltwater));
    // Land far from water
    BOOST_TEST(has(MapPoint(23, 5), MapGenTerrain::Land));
    BOOST_TEST(!has(MapPoint(23, 5), MapGenTerrain::NearSalt));
    // Shore of the sea (the node at x=16 touches water triangles)
    BOOST_TEST(has(MapPoint(16, 5), MapGenTerrain::ShoreSalt));
    BOOST_TEST(!has(MapPoint(16, 5), MapGenTerrain::Land));
    BOOST_TEST(has(MapPoint(18, 5), MapGenTerrain::NearSalt));
    BOOST_TEST(has(MapPoint(18, 5), MapGenTerrain::Land));
    // Near the lake
    BOOST_TEST(has(MapPoint(26, 19), MapGenTerrain::NearFresh));
    BOOST_TEST(!has(MapPoint(26, 19), MapGenTerrain::NearSalt));
    // Mountains
    BOOST_TEST(has(MapPoint(36, 5), MapGenTerrain::Mountain));
    BOOST_TEST(!has(MapPoint(36, 5), MapGenTerrain::Land));

    // With a smaller limit the lake is salt water, too
    ComputeTerrainFlags(map, 3, 0.);
    BOOST_TEST(has(MapPoint(29, 19), MapGenTerrain::Saltwater));
}

BOOST_FIXTURE_TEST_CASE(ComputeTerrainFlags_marks_rivers, MapGenFixture)
{
    const MapExtent size(32, 32);
    auto map = createMap(size);
    const auto land = map.textureMap.Find(IsBuildableLand);
    RTTR_FOREACH_PT(MapPoint, size)
    {
        map.getTextures()[pt] = TexturePair(land);
    }
    const auto water = map.textureMap.Find(IsWater);
    for(MapCoord x = 5; x < 20; ++x)
    {
        const MapPoint pt(x, 10);
        map.riverCourse[pt] = 1;
        for(const Triangle& triangle : GetTriangles(pt, size, Direction::East))
            map.textureMap.Set(triangle, water);
    }
    ComputeTerrainFlags(map, 2, 2.);
    const auto has = [&map](const MapPoint& pt, MapGenTerrain terrain) {
        return (map.terrainFlags[pt] & toFlag(terrain)) != 0;
    };
    BOOST_TEST(has(MapPoint(10, 10), MapGenTerrain::River));
    BOOST_TEST(has(MapPoint(10, 10), MapGenTerrain::Freshwater));
    BOOST_TEST(has(MapPoint(10, 11), MapGenTerrain::ShoreRiver));
    BOOST_TEST(has(MapPoint(10, 11), MapGenTerrain::ShoreFresh));
    BOOST_TEST(has(MapPoint(10, 12), MapGenTerrain::NearRiver));
    BOOST_TEST(has(MapPoint(10, 12), MapGenTerrain::NearFresh));
    BOOST_TEST(!has(MapPoint(10, 20), MapGenTerrain::NearRiver));
    BOOST_TEST(has(MapPoint(10, 20), MapGenTerrain::Land));
}

BOOST_FIXTURE_TEST_CASE(HeightPercent_and_cooling, MapGenFixture)
{
    auto map = createMap(MapExtent(16, 16));
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        map.z[pt] = static_cast<uint8_t>(pt.x * 2); // 0 .. 30
        map.temperature[pt] = 80;
    }
    ComputeHeightPercent(map);
    BOOST_TEST(map.heightPercent[MapPoint(0, 3)] == 0u);
    BOOST_TEST(map.heightPercent[MapPoint(15, 3)] == 100u);
    BOOST_TEST(map.heightPercent[MapPoint(5, 3)] == 33u);
    ApplyHeightCooling(map, 30);
    BOOST_TEST(map.temperature[MapPoint(0, 3)] == 80u);
    BOOST_TEST(map.temperature[MapPoint(15, 3)] == 50u);
    BOOST_TEST(map.temperature[MapPoint(5, 3)] == 71u);
}

BOOST_AUTO_TEST_SUITE_END()
