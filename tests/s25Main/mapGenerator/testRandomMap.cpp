// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PointOutput.h"
#include "commonDefines.h"
#include "lua/GameDataLoader.h"
#include "RttrForeachPt.h"
#include "mapGenerator/RandomMap.h"
#include "mapGenerator/TextureHelper.h"
#include "gameData/MaxPlayers.h"
#include "libsiedler2/ArchivItem_Map.h"
#include "libsiedler2/ArchivItem_Map_Header.h"
#include "libsiedler2/libsiedler2.h"
#include "rttr/test/TmpFolder.hpp"
#include "rttr/test/random.hpp"
#include <boost/filesystem/operations.hpp>
#include <boost/test/unit_test.hpp>
#include <set>

using namespace rttr::mapGenerator;

BOOST_AUTO_TEST_SUITE(RandomMapTests)

static void ValidateMap(const Map& map, const MapExtent& size, unsigned players)
{
    BOOST_TEST_REQUIRE(map.players == players);
    BOOST_TEST(map.size == size);

    const auto validateHqPos = [&](const MapPoint hqPos) {
        // All textures around the point must allow a castle
        const auto neighbors = map.z.GetNeighbours(hqPos);
        for(const MapPoint pt : neighbors)
        {
            BOOST_TEST_REQUIRE(
              map.textureMap.All(pt, [](const TerrainDesc& terrain) { return terrain.GetBQ() == TerrainBQ::Castle; }),
              "At point " << pt);
        }
        // All direct neighbors must have the same height as the hqPos
        for(const MapPoint pt : neighbors)
            BOOST_TEST_REQUIRE(map.z[pt] == map.z[hqPos], "At point " << pt);

        const auto neighbors2 = map.z.GetPointsInRadiusWithCenter(hqPos, 2);

        const auto compareHeight = [&](const MapPoint& p1, const MapPoint& p2) { return map.z[p1] < map.z[p2]; };
        const MapPoint lowestPoint = *std::min_element(neighbors2.begin(), neighbors2.end(), compareHeight);
        const MapPoint highestPoint = *std::max_element(neighbors2.begin(), neighbors2.end(), compareHeight);
        // Height difference must be at most 2
        BOOST_TEST(absDiff(map.z[hqPos], map.z[lowestPoint]) <= 2);
        BOOST_TEST(absDiff(map.z[hqPos], map.z[highestPoint]) <= 2);
        // No objects around
        for(const MapPoint pt : neighbors2)
        {
            if(pt == hqPos) // This is the HQ
                continue;
            BOOST_TEST_REQUIRE(map.objectTypes[pt] == libsiedler2::OT_Empty, "At point " << pt);
        }
    };
    for(unsigned index = 0; index < players; index++)
    {
        BOOST_TEST_REQUIRE(map.hqPositions[index].isValid());
        BOOST_TEST_CONTEXT("Player " << index) { validateHqPos(map.hqPositions[index]); }
    }
}

static MapExtent getRandomMapSize(const unsigned minSize = 32, const unsigned maxSize = 80)
{
    auto size = rttr::test::randomPoint<MapExtent>(minSize, maxSize);
    // Need even size
    size.x &= ~1u;
    size.y &= ~1u;
    return size;
}

BOOST_AUTO_TEST_CASE(GenerateRandomMap_returns_valid_maps_for_all_land_masses)
{
    WorldDescription worldDesc;
    loadGameData(worldDesc);
    for(const LandMass landMass : {LandMass::Islands, LandMass::Normal, LandMass::Continents})
    {
        BOOST_TEST_CONTEXT("Land mass " << static_cast<int>(landMass))
        {
            RandomUtility rnd(static_cast<uint64_t>(landMass) + 10);
            MapSettings settings;
            settings.size = getRandomMapSize(64, 96);
            settings.numPlayers = rttr::test::randomValue(2u, 4u);
            settings.landMass = landMass;
            settings.water = landMass == LandMass::Islands ? 50 : 30;
            Map map = GenerateRandomMap(rnd, worldDesc, settings);
            ValidateMap(map, settings.size, settings.numPlayers);
        }
    }
}

BOOST_AUTO_TEST_CASE(GenerateRandomMap_for_all_roughness_values_returns_valid_map)
{
    WorldDescription worldDesc;
    loadGameData(worldDesc);
    for(const Roughness roughness : {Roughness::Smooth, Roughness::Normal, Roughness::Rough})
    {
        RandomUtility rnd(static_cast<uint64_t>(roughness) + 20);
        MapSettings settings;
        settings.size = MapExtent(64, 64);
        settings.numPlayers = 2;
        settings.roughness = roughness;
        Map map = GenerateRandomMap(rnd, worldDesc, settings);
        ValidateMap(map, settings.size, settings.numPlayers);
    }
}

BOOST_AUTO_TEST_CASE(GenerateRandomMap_returns_valid_map_with_max_players)
{
    RandomUtility rnd(0);
    WorldDescription worldDesc;
    loadGameData(worldDesc);
    MapSettings settings;

    settings.size = getRandomMapSize(64, 80);
    settings.numPlayers = MAX_PLAYERS;
    settings.water = 10;
    settings.mountains = 10;

    Map map = GenerateRandomMap(rnd, worldDesc, settings);
    ValidateMap(map, settings.size, settings.numPlayers);
}

BOOST_AUTO_TEST_CASE(GenerateRandomMap_has_requested_water_and_mountain_share)
{
    RandomUtility rnd(5);
    WorldDescription worldDesc;
    loadGameData(worldDesc);
    MapSettings settings;
    settings.size = MapExtent(128, 128);
    settings.numPlayers = 2;
    settings.water = 40;
    settings.mountains = 20;
    settings.rivers = 0;

    Map map = GenerateRandomMap(rnd, worldDesc, settings);
    // Count triangles
    unsigned numWater = 0, numMountain = 0, numLand = 0;
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        for(const bool rsu : {true, false})
        {
            const TerrainDesc& desc = map.textureMap.GetDesc(rsu ? map.getTextures()[pt].rsu : map.getTextures()[pt].lsd);
            if(desc.kind == TerrainKind::Water)
                ++numWater;
            else if(IsMountainOrSnowOrLava(desc))
                ++numMountain;
            else
                ++numLand;
        }
    }
    const double numTriangles = 2. * map.size.x * map.size.y;
    BOOST_TEST(numWater / numTriangles == 0.4, boost::test_tools::tolerance(0.1));
    BOOST_TEST(numMountain / (numMountain + numLand) == 0.2, boost::test_tools::tolerance(0.25));
}

BOOST_AUTO_TEST_CASE(GenerateRandomMap_creates_all_zones_and_places_objects)
{
    RandomUtility rnd(6);
    WorldDescription worldDesc;
    loadGameData(worldDesc);
    BOOST_TEST_REQUIRE(worldDesc.mapGen.zones.size() == 7u);
    BOOST_TEST_REQUIRE(!worldDesc.mapGen.resources.empty());
    MapSettings settings;
    settings.size = MapExtent(96, 96);
    settings.numPlayers = 2;

    Map map = GenerateRandomMap(rnd, worldDesc, settings);
    std::set<unsigned> zones(map.zones.begin(), map.zones.end());
    BOOST_TEST(zones.size() == 7u);
    unsigned numTrees = 0, numStones = 0, numAnimals = 0, numMineRes = 0;
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        const auto objectType = map.objectTypes[pt];
        if(objectType >= 0xC4 && objectType <= 0xC6)
            ++numTrees;
        else if(objectType == libsiedler2::OT_Stone1 || objectType == libsiedler2::OT_Stone2)
            ++numStones;
        if(map.animals[pt] != libsiedler2::Animal::None)
            ++numAnimals;
        if(map.resources[pt] > libsiedler2::R_Coal && map.resources[pt] < libsiedler2::R_Granite + 8)
            ++numMineRes;
    }
    BOOST_TEST(numTrees > 0u);
    BOOST_TEST(numStones > 0u);
    BOOST_TEST(numAnimals > 0u);
    BOOST_TEST(numMineRes > 0u);
}

BOOST_AUTO_TEST_CASE(GenerateRandomMap_same_seed_gives_same_map)
{
    WorldDescription worldDesc;
    loadGameData(worldDesc);
    MapSettings settings;
    settings.size = MapExtent(64, 64);
    settings.numPlayers = 3;
    RandomUtility rnd1(1234), rnd2(1234);
    const Map map1 = GenerateRandomMap(rnd1, worldDesc, settings);
    const Map map2 = GenerateRandomMap(rnd2, worldDesc, settings);
    BOOST_TEST(std::equal(map1.z.begin(), map1.z.end(), map2.z.begin()));
    BOOST_TEST(std::equal(map1.objectTypes.begin(), map1.objectTypes.end(), map2.objectTypes.begin()));
    BOOST_TEST(std::equal(map1.resources.begin(), map1.resources.end(), map2.resources.begin()));
    BOOST_TEST(map1.hqPositions == map2.hqPositions, boost::test_tools::per_element());
}

BOOST_AUTO_TEST_CASE(GenerateRandomMap_returns_valid_mapfile_with_max_players)
{
    rttr::test::TmpFolder tmpFolder;
    const auto mapPath = tmpFolder.get() / "map.wld";

    MapSettings settings;
    settings.size = MapExtent(64, 64);
    settings.numPlayers = MAX_PLAYERS;
    settings.water = 10;
    settings.seed = 77;

    const uint32_t seed = CreateRandomMap(mapPath, settings);
    BOOST_TEST(seed == 77u);
    libsiedler2::Archiv archive;
    BOOST_TEST_REQUIRE(libsiedler2::Load(mapPath, archive) == 0);
    const auto* map = dynamic_cast<const libsiedler2::ArchivItem_Map*>(archive[0]);
    BOOST_TEST_REQUIRE(map);
    const auto& mapHeader = map->getHeader();
    BOOST_TEST(mapHeader.getWidth() == settings.size.x);
    BOOST_TEST(mapHeader.getHeight() == settings.size.y);
    BOOST_TEST(mapHeader.getNumPlayers() == settings.numPlayers);
    BOOST_TEST(mapHeader.getName() == "Zufall 77");
    // Must have unique HQ positions
    std::vector<MapPoint> hqs;
    for(unsigned i = 0; i < std::min(unsigned(mapHeader.maxPlayers), settings.numPlayers); i++)
    {
        Point<uint16_t> hqPos;
        mapHeader.getPlayerHQ(i, hqPos.x, hqPos.y);
        BOOST_TEST(!helpers::contains(hqs, MapPoint(hqPos)));
        hqs.emplace_back(hqPos);
    }
    // Preview image next to the map
    BOOST_TEST(boost::filesystem::exists(GetPreviewImagePath(mapPath)));
    BOOST_TEST(GetPreviewImagePath(mapPath).filename() == "map_Vorschau.bmp");
}

BOOST_AUTO_TEST_CASE(CreateRandomMap_uses_a_random_seed_if_none_is_set)
{
    rttr::test::TmpFolder tmpFolder;
    MapSettings settings;
    settings.size = MapExtent(32, 32);
    settings.numPlayers = 2;
    settings.seed = 0;
    const uint32_t seed1 = CreateRandomMap(tmpFolder.get() / "map1.swd", settings);
    const uint32_t seed2 = CreateRandomMap(tmpFolder.get() / "map2.swd", settings);
    BOOST_TEST(seed1 != 0u);
    BOOST_TEST(seed2 != 0u);
    BOOST_TEST(seed1 != seed2);
}

BOOST_AUTO_TEST_SUITE_END()
