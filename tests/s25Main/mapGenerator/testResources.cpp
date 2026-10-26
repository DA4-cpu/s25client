// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "RttrForeachPt.h"
#include "mapGenFixtures.h"
#include "mapGenerator/Resources.h"
#include "mapGenerator/TextureHelper.h"
#include "mapGenerator/Zones.h"
#include "gameData/MapGenDesc.h"
#include <boost/test/unit_test.hpp>
#include <array>
#include <set>

using namespace rttr::mapGenerator;
using namespace libsiedler2;

// LCOV_EXCL_START
BOOST_TEST_DONT_PRINT_LOG_VALUE(libsiedler2::Animal)
// LCOV_EXCL_STOP

namespace {
MapResourceDesc makeRule(const std::string& object, double amount, MapGenTerrainFlags terrain)
{
    MapResourceDesc rule;
    rule.name = object;
    rule.object = object;
    rule.amount = amount;
    rule.terrain = terrain;
    return rule;
}

MapZoneDesc makeZone(const std::string& name, MapGenWeights trees, MapGenWeights animals)
{
    MapZoneDesc zone;
    zone.name = name;
    MapZonePackageDesc package;
    package.trees = std::move(trees);
    package.animals = std::move(animals);
    zone.packages.push_back(package);
    return zone;
}

bool isTree(uint8_t objectType)
{
    return objectType >= 0xC4 && objectType <= 0xC6;
}
} // namespace

struct ResourceTestFixture : MapGenFixture
{
    MapExtent size = MapExtent(32, 32);
    Map map;
    MapGenDesc desc;
    MapSettings settings;

    ResourceTestFixture() : map(createMap(size))
    {
        auto mountain = map.textureMap.Find(IsMinableMountain);
        auto water = map.textureMap.Find(IsWater);
        auto land = map.textureMap.Find(IsBuildableLand);

        // Columns: water (0-3), land (4-11), mountain (12-19), land (20-27), water (28-31)
        auto& textures = map.getTextures();
        RTTR_FOREACH_PT(MapPoint, size)
        {
            if(pt.x < 4 || pt.x >= 28)
                textures[pt] = TexturePair(water);
            else if(pt.x >= 12 && pt.x < 20)
                textures[pt] = TexturePair(mountain);
            else
                textures[pt] = TexturePair(land);
            map.temperature[pt] = 50;
            map.climate[pt] = 50;
        }
        // Two zones: left half and right half
        RTTR_FOREACH_PT(MapPoint, size)
        {
            map.zones[pt] = pt.x < 16 ? 0 : 1;
        }
        desc.zones.push_back(makeZone("Left", {{"Oak", 1}}, {{"Deer", 1}, {"Duck", 1}}));
        desc.zones.push_back(makeZone("Right", {{"Palm1", 1}}, {{"Fox", 1}}));
        ComputeHeightPercent(map);
        ComputeTerrainFlags(map, 2, 50.);
    }

    unsigned countObjects() const
    {
        return static_cast<unsigned>(helpers::count_if(map.objectTypes, [](const auto o) { return o != OT_Empty; }));
    }
};

BOOST_FIXTURE_TEST_SUITE(ResourcesTests, ResourceTestFixture)

BOOST_AUTO_TEST_CASE(PlaceResources_keeps_area_around_hqs_and_harbors_empty)
{
    RandomUtility rnd(0);
    const MapPoint hq(7, 7);
    const MapPoint harbor(24, 20);
    map.hqPositions.push_back(hq);
    map.harbors.push_back(Triangle(true, harbor));
    desc.resources.push_back(makeRule("trees", 100, toFlag(MapGenTerrain::Land)));

    PlaceResources(map, rnd, settings, desc);

    BOOST_TEST(countObjects() > 0u);
    for(const MapPoint center : {hq, harbor})
    {
        for(const MapPoint& pt : map.objectTypes.GetPointsInRadiusWithCenter(center, 5))
        {
            BOOST_TEST_REQUIRE(map.objectTypes[pt] == OT_Empty);
            BOOST_TEST_REQUIRE(map.objectInfos[pt] == OI_Empty);
        }
    }
}

BOOST_AUTO_TEST_CASE(PlaceResources_uses_terrain_and_zone_packages)
{
    RandomUtility rnd(1);
    desc.resources.push_back(makeRule("trees", 100, toFlag(MapGenTerrain::Land)));
    PlaceResources(map, rnd, settings, desc);
    const TreeCode oak = GetTreeCode("Oak");
    const TreeCode palm = GetTreeCode("Palm1");
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(!isTree(map.objectTypes[pt]))
            continue;
        // Only on land
        BOOST_TEST_REQUIRE((map.terrainFlags[pt] & toFlag(MapGenTerrain::Land)) != 0);
        // Species from the package of the zone
        const TreeCode expected = map.zones[pt] == 0 ? oak : palm;
        BOOST_TEST_REQUIRE(map.objectTypes[pt] == expected.type);
        BOOST_TEST_REQUIRE((map.objectInfos[pt] & 0xF0) == expected.index);
    }
    // Amount 100 % on land: (almost) all land nodes have a tree
    unsigned numLand = 0, numTrees = 0;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(map.terrainFlags[pt] & toFlag(MapGenTerrain::Land))
        {
            ++numLand;
            numTrees += isTree(map.objectTypes[pt]) ? 1 : 0;
        }
    }
    BOOST_TEST(numTrees == numLand);
}

BOOST_AUTO_TEST_CASE(PlaceResources_amount_climate_zones_and_global_factor)
{
    MapResourceDesc rule = makeRule("stones", 0, toFlag(MapGenTerrain::Land));
    rule.amountSlider = "stonePiles";
    rule.zones = {"Right"};
    desc.resources.push_back(rule);

    const auto countStones = [this](unsigned stonePiles, unsigned resourceAmount, unsigned seed) {
        RandomUtility rnd(seed);
        map.objectTypes.Resize(size, OT_Empty);
        map.objectInfos.Resize(size, OI_Empty);
        settings.stonePiles = static_cast<unsigned short>(stonePiles);
        settings.resourceAmount = static_cast<unsigned short>(resourceAmount);
        PlaceResources(map, rnd, settings, desc);
        unsigned numStones = 0;
        RTTR_FOREACH_PT(MapPoint, size)
        {
            if(map.objectTypes[pt] == OT_Stone1 || map.objectTypes[pt] == OT_Stone2)
            {
                BOOST_TEST_REQUIRE(map.zones[pt] == 1u); // Only in zone "Right"
                ++numStones;
            }
        }
        return numStones;
    };
    BOOST_TEST(countStones(0, 100, 1) == 0u);
    unsigned numLandRight = 0;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(map.zones[pt] == 1 && (map.terrainFlags[pt] & toFlag(MapGenTerrain::Land)))
            ++numLandRight;
    }
    BOOST_TEST_REQUIRE(numLandRight > 100u);
    const unsigned half = countStones(50, 100, 1);
    BOOST_TEST(half > numLandRight / 4);
    BOOST_TEST(half < numLandRight * 3 / 4);
    BOOST_TEST(countStones(100, 100, 1) == numLandRight);
    BOOST_TEST(countStones(50, 200, 1) == numLandRight);
    BOOST_TEST(countStones(100, 0, 1) == 0u);

    // Climate limits and weights
    desc.resources.back().climate.temperature = MapGenRange{60, 100};
    BOOST_TEST(countStones(100, 100, 2) == 0u); // temperature is 50 everywhere
    desc.resources.back().climate.temperature = MapGenRange{};
    desc.resources.back().weights.push_back(MapResourceWeight{MapGenClimate{}, 0.});
    BOOST_TEST(countStones(100, 100, 2) == 0u);
}

BOOST_AUTO_TEST_CASE(PlaceResources_places_clusters)
{
    MapResourceDesc rule = makeRule("trees", 30, toFlag(MapGenTerrain::Land) | toFlag(MapGenTerrain::Mountain));
    rule.cluster = 6;
    rule.density = 100;
    desc.resources.push_back(rule);
    RandomUtility rnd(3);
    PlaceResources(map, rnd, settings, desc);
    // Trees in clusters have more tree neighbors than randomly scattered trees (30 % -> ~1.8 of 6)
    unsigned numTrees = 0, numTreeNeighbors = 0;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(!isTree(map.objectTypes[pt]))
            continue;
        ++numTrees;
        for(const MapPoint& neighbor : map.objectTypes.GetNeighbours(pt))
            numTreeNeighbors += isTree(map.objectTypes[neighbor]) ? 1 : 0;
    }
    BOOST_TEST_REQUIRE(numTrees > 0u);
    BOOST_TEST(numTreeNeighbors > numTrees * 3);
}

BOOST_AUTO_TEST_CASE(PlaceResources_places_animals_by_habitat)
{
    desc.resources.push_back(makeRule("animals", 100,
                                      toFlag(MapGenTerrain::Land) | toFlag(MapGenTerrain::Saltwater)
                                        | toFlag(MapGenTerrain::Freshwater)));
    RandomUtility rnd(4);
    PlaceResources(map, rnd, settings, desc);
    unsigned numDucks = 0, numDeer = 0, numFoxes = 0;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        const Animal animal = map.animals[pt];
        if(animal == Animal::None)
            continue;
        if(map.textureMap.All(pt, IsWater))
        {
            // Only zone "Left" has ducks
            BOOST_TEST_REQUIRE(animal == Animal::Duck);
            BOOST_TEST_REQUIRE(map.zones[pt] == 0u);
            ++numDucks;
        } else
        {
            BOOST_TEST_REQUIRE(animal != Animal::Duck);
            if(animal == Animal::Deer)
                ++numDeer;
            else if(animal == Animal::Fox)
                ++numFoxes;
        }
    }
    BOOST_TEST(numDucks > 0u);
    BOOST_TEST(numDeer > 0u);
    BOOST_TEST(numFoxes > 0u);
}

BOOST_AUTO_TEST_CASE(PlaceResources_fish_groundwater_and_mines)
{
    for(const char* object : {"coal", "iron", "gold", "granite"})
    {
        MapResourceDesc rule = makeRule(object, 0, toFlag(MapGenTerrain::Mountain));
        rule.amountSlider = object;
        rule.cluster = 6;
        desc.resources.push_back(rule);
    }
    desc.resources.push_back(
      makeRule("fish", 100, toFlag(MapGenTerrain::Saltwater) | toFlag(MapGenTerrain::Freshwater)));
    desc.resources.push_back(makeRule("groundwater", 100, toFlag(MapGenTerrain::Land)));
    RandomUtility rnd(5);
    PlaceResources(map, rnd, settings, desc);

    unsigned numMountainWithRes = 0, numMountainWithoutRes = 0;
    std::array<unsigned, 4> numPerType{};
    RTTR_FOREACH_PT(MapPoint, size)
    {
        const uint8_t resource = map.resources[pt];
        if(map.textureMap.All(pt, IsMinableMountain))
        {
            if(resource == R_None)
                ++numMountainWithoutRes;
            else
            {
                ++numMountainWithRes;
                BOOST_TEST_REQUIRE(resource > R_Coal);
                BOOST_TEST_REQUIRE(resource < R_Granite + 8);
                BOOST_TEST_REQUIRE((resource & 7) != 0); // amount 1..7
                ++numPerType[(resource - R_Coal) / 8];
            }
        } else if(map.textureMap.All(pt, IsWater))
            BOOST_TEST_REQUIRE(resource == R_Fish);
        else if(map.terrainFlags[pt] & toFlag(MapGenTerrain::Land))
            BOOST_TEST_REQUIRE(resource == R_Water);
        else
            BOOST_TEST_REQUIRE(resource == R_None);
    }
    BOOST_TEST(numMountainWithRes > 2u * numMountainWithoutRes);
    // Coal (40) and iron (36) are much more common than gold (9)
    BOOST_TEST(numPerType[0] > numPerType[2]);
    BOOST_TEST(numPerType[1] > numPerType[2]);
}

BOOST_AUTO_TEST_CASE(GetTreeCode_and_GetAnimalCode_know_all_lua_names)
{
    std::set<std::pair<uint8_t, uint8_t>> codes;
    for(const std::string& name : getMapGenTreeNames())
    {
        const TreeCode code = GetTreeCode(name);
        BOOST_TEST(codes.insert(std::make_pair(code.type, code.index)).second);
    }
    for(const std::string& name : getMapGenAnimalNames())
        BOOST_TEST(GetAnimalCode(name) != Animal::None);
    BOOST_CHECK_THROW(GetTreeCode("Mahagoni"), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()
