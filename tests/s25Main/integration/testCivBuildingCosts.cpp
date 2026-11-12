// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "PointOutput.h"
#include "RttrConfig.h"
#include "SerializedGameData.h"
#include "addons/const_addons.h"
#include "ai/AIPlayer.h"
#include "ai/aijh/AIConstruction.h"
#include "ai/aijh/AIPlayerJH.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "factories/AIFactory.h"
#include "files.h"
#include "lua/GameDataLoader.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "gameTypes/GameTypesOutput.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>

namespace bnw = boost::nowide;

// Construction costs by tags (civilization mode): rttr:SetBuildingCost and cost in rttr:AddWorkshop
BOOST_AUTO_TEST_SUITE(CivBuildingCostsSuite)

namespace {
bool loadLua(const std::string& content, WorldDescription& desc)
{
    rttr::test::TmpFolder tmp;
    {
        bnw::ofstream file(tmp / "default.lua");
        file << content;
    }
    GameDataLoader loader(desc, tmp);
    rttr::test::LogAccessor logAcc;
    return loader.Load();
}

WareDesc makeWare(const std::string& name, const std::string& kind, const std::string& look)
{
    WareDesc desc;
    desc.name = name;
    desc.title = name + "Title";
    desc.kind = kind;
    desc.look = look;
    desc.transport = look;
    return desc;
}

/// Leaf and needle boards (wares 0 and 1, kind "Bretter"). Woodcutter: 3 "Bretter". Sawmill: 1 needle board, then 1
/// leaf board
WorldDescription makeDesc()
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("LeafBoards", "Bretter", "Boards"));
    desc.wares.push_back(makeWare("NeedleBoards", "Bretter", "Boards"));
    desc.buildingCosts["Woodcutter"] = {{{"Bretter"}, 3}};
    desc.buildingCosts["Sawmill"] = {{{"NeedleBoards"}, 1}, {{"LeafBoards"}, 1}};
    return desc;
}

constexpr GoodType LEAF_BOARDS = GoodType::Custom0;
const GoodType NEEDLE_BOARDS = getCustomGood(1);

struct CostFixture : public WorldWithGCExecution1P
{
    CostFixture()
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(makeDesc()));
        civ::startGame(world);
        hq().AddToInventory(PeopleCounts::make(Job::Builder, 3), true);
    }

    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    unsigned inInventory(GoodType good) { return world.GetPlayer(0).GetInventory()[good]; }

    MapPoint placeSite(BuildingType bld, Position offset = Position(4, 2))
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + offset);
        this->SetBuildingSite(pos, bld);
        BOOST_TEST_REQUIRE(world.GetSpecObj<noBuildingSite>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        return pos;
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(CostsFromLua)
{
    const std::string base = R"(
rttr:AddWare{ name = "LeafBoards", kind = "Bretter", properties = { "Laub" }, look = "Boards", transport = "Boards" }
rttr:AddWare{ name = "NeedleBoards", kind = "Bretter", properties = { "Nadel" }, look = "Boards", transport = "Boards" }
rttr:SetBuildingCost{
    Woodcutter = { Stones = 1, Bretter = 2 },
    Sawmill = { { tags = "Stones", amount = 1 }, { tags = { "Bretter", "Nadel" }, amount = 2 } },
}
rttr:AddWorkshop{ name = "Shop", size = "small", cost = { { tags = "Bretter", amount = 3 } }, look = "Sawmill",
    worker = { tool = "none", look = "Carpenter" }, inputs = { "Stones" }, output = "Boards", workTime = 5 }
)";
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(base, desc));
    // Short form: alphabetical order
    const auto& woodcutterDesc = desc.buildingCosts.at("Woodcutter");
    BOOST_TEST_REQUIRE(woodcutterDesc.size() == 2u);
    BOOST_TEST((woodcutterDesc[0].tags == TagQuery{"Bretter"}));
    BOOST_TEST(woodcutterDesc[0].amount == 2u);
    BOOST_TEST((woodcutterDesc[1].tags == TagQuery{"Stones"}));

    const civ::Content content(desc);
    const BuildingCosts& woodcutter = content.GetBuildingCost(BuildingType::Woodcutter);
    BOOST_TEST_REQUIRE(woodcutter.size() == 2u);
    BOOST_TEST((woodcutter[0].wares == std::vector<GoodType>{LEAF_BOARDS, NEEDLE_BOARDS}));
    BOOST_TEST((woodcutter[1].wares == std::vector<GoodType>{GoodType::Stones}));
    // Long form: in this order
    const BuildingCosts& sawmill = content.GetBuildingCost(BuildingType::Sawmill);
    BOOST_TEST_REQUIRE(sawmill.size() == 2u);
    BOOST_TEST((sawmill[0].wares == std::vector<GoodType>{GoodType::Stones}));
    BOOST_TEST((sawmill[1].wares == std::vector<GoodType>{NEEDLE_BOARDS}));
    BOOST_TEST(sawmill[1].amount == 2u);
    // Not listed: boards and stones as before
    const BuildingCosts& farm = content.GetBuildingCost(BuildingType::Farm);
    BOOST_TEST_REQUIRE(farm.size() == 2u);
    BOOST_TEST((farm[0].wares == std::vector<GoodType>{GoodType::Boards}));
    BOOST_TEST(farm[0].amount == 3u);
    BOOST_TEST((farm[1].wares == std::vector<GoodType>{GoodType::Stones}));
    // Workshop
    const BuildingCosts& shop = content.GetBuildingCost(BuildingType::Custom0);
    BOOST_TEST_REQUIRE(shop.size() == 1u);
    BOOST_TEST((shop[0].wares == std::vector<GoodType>{LEAF_BOARDS, NEEDLE_BOARDS}));

    const auto replaced = [&base](const std::string& from, const std::string& to) {
        std::string result = base;
        const auto pos = result.find(from);
        BOOST_TEST_REQUIRE(pos != std::string::npos);
        return result.replace(pos, from.size(), to);
    };
    for(const std::string& invalid :
        {replaced("Stones = 1, Bretter = 2", "Stones = 0"), replaced("Stones = 1, Bretter = 2", "Stones = 32"),
         replaced("Stones = 1, Bretter = 2", "Stones = 20, Bretter = 12"),
         replaced("Stones = 1, Bretter = 2", "A = 1, B = 1, C = 1, D = 1, E = 1"),
         replaced("Stones = 1, Bretter = 2", "Stones = 1, { tags = \"Bretter\", amount = 1 }"),
         replaced("Stones = 1, Bretter = 2", ""),
         replaced("{ tags = \"Stones\", amount = 1 }", "{ tags = \"Stones\" }"),
         replaced("amount = 3", "amount = 3, phase = 1"), replaced("Woodcutter = {", "Woodcutter = 2, Farm = {"),
         base + "rttr:SetBuildingCost{ Woodcutter = { Boards = 1 } }"})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(invalid, invalidDesc));
    }
    for(const std::string& invalid :
        {replaced("Bretter = 2", "Planks = 2"), replaced("Woodcutter =", "HarborBuilding ="),
         replaced("Woodcutter =", "Headquarters ="), replaced("Woodcutter =", "Shop ="),
         replaced("Woodcutter =", "Wood =")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST_REQUIRE(loadLua(invalid, invalidDesc));
        BOOST_CHECK_THROW(civ::Content{invalidDesc}, GameDataError);
    }
}

BOOST_AUTO_TEST_CASE(ShippedCostsAreTheOriginalOnes)
{
    WorldDescription desc;
    GameDataLoader loader(desc, RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "civ");
    BOOST_TEST_REQUIRE(loader.Load());
    const civ::TechTree tree(desc);
    const civ::Content& content = tree.GetContent();
    BOOST_TEST_REQUIRE(content.IsFromLua());
    const civ::Content original;
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(isCustomBuilding(bld) || !BuildingProperties::IsValid(bld))
            continue;
        BOOST_TEST_INFO(static_cast<unsigned>(bld));
        const BuildingCosts& costs = content.GetBuildingCost(bld);
        const BuildingCosts& expected = original.GetBuildingCost(bld);
        BOOST_TEST_REQUIRE(costs.size() == expected.size());
        for(unsigned i = 0; i < costs.size(); ++i)
        {
            BOOST_TEST(costs[i].amount == expected[i].amount);
            BOOST_TEST((costs[i].wares.front() == expected[i].wares.front()));
        }
    }
}

BOOST_FIXTURE_TEST_CASE(SiteTakesAnyMatchingWare, CostFixture)
{
    // Only new boards: 1 leaf, 2 needle
    hq().AddToInventory(GoodCounts::make(LEAF_BOARDS, 1), true);
    hq().AddToInventory(GoodCounts::make(NEEDLE_BOARDS, 2), true);
    const unsigned numBoards = inInventory(GoodType::Boards);
    const MapPoint pos = placeSite(BuildingType::Woodcutter);
    auto* site = world.GetSpecObj<noBuildingSite>(pos);
    BOOST_TEST_REQUIRE(site->GetMaterials().size() == 1u);
    BOOST_TEST(site->GetMaterials()[0].ordered.size() == 3u);
    BOOST_TEST(site->CalcDistributionPoints(GoodType::Boards) == 0u);
    RTTR_EXEC_TILL(5000, world.GetSpecObj<nobUsual>(pos) != nullptr);
    BOOST_TEST(inInventory(LEAF_BOARDS) == 0u);
    BOOST_TEST(inInventory(NEEDLE_BOARDS) == 0u);
    // The built-in boards are no "Bretter" here
    BOOST_TEST(inInventory(GoodType::Boards) == numBoards);
}

BOOST_FIXTURE_TEST_CASE(MaterialsAreUsedInOrder, CostFixture)
{
    // Sawmill: first a needle board, then a leaf board. Without the needle board the builder waits although the leaf
    // board is there
    hq().AddToInventory(GoodCounts::make(LEAF_BOARDS, 1), true);
    const MapPoint pos = placeSite(BuildingType::Sawmill);
    const auto* site = world.GetSpecObj<noBuildingSite>(pos);
    BOOST_TEST_REQUIRE(site->GetMaterials().size() == 2u);
    RTTR_EXEC_TILL(3000, site->GetMaterials()[1].delivered.size() == 1u);
    RTTR_SKIP_GFS(1000);
    BOOST_TEST(site->GetBuildProgress(false) == 0u);
    BOOST_TEST(site->GetMaterials()[0].used == 0u);
    hq().AddToInventory(GoodCounts::make(NEEDLE_BOARDS, 1), true);
    world.GetPlayer(0).FindMaterialForBuildingSites();
    RTTR_EXEC_TILL(5000, world.GetSpecObj<nobUsual>(pos) != nullptr);
    BOOST_TEST(inInventory(LEAF_BOARDS) == 0u);
    BOOST_TEST(inInventory(NEEDLE_BOARDS) == 0u);
}

BOOST_FIXTURE_TEST_CASE(SerializeBuildingSite, CostFixture)
{
    hq().AddToInventory(GoodCounts::make(LEAF_BOARDS, 1), true);
    hq().AddToInventory(GoodCounts::make(NEEDLE_BOARDS, 1), true);
    const MapPoint pos = placeSite(BuildingType::Woodcutter);
    RTTR_EXEC_TILL(3000, world.GetSpecObj<noBuildingSite>(pos)->GetMaterials()[0].ordered.empty());
    // One board is still missing
    const auto materialsBefore = world.GetSpecObj<noBuildingSite>(pos)->GetMaterials();

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    const auto* site = world.GetSpecObj<noBuildingSite>(pos);
    BOOST_TEST_REQUIRE(site);
    BOOST_TEST_REQUIRE(site->GetMaterials().size() == 1u);
    BOOST_TEST((site->GetMaterials()[0].delivered == materialsBefore[0].delivered));
    BOOST_TEST(site->GetMaterials()[0].used == materialsBefore[0].used);
    BOOST_TEST(site->GetNumUsed(LEAF_BOARDS) == 2u);
    // The last board arrives later
    hq().AddToInventory(GoodCounts::make(NEEDLE_BOARDS, 1), true);
    world.GetPlayer(0).FindMaterialForBuildingSites();
    RTTR_EXEC_TILL(5000, world.GetSpecObj<nobUsual>(pos) != nullptr);
}

BOOST_FIXTURE_TEST_CASE(AIKnowsTheMaterial, CostFixture)
{
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    AIJH::AIConstruction& construction = static_cast<AIJH::AIPlayerJH&>(*ai).GetConstruction();
    BOOST_TEST(!construction.HasMaterialFor(BuildingType::Woodcutter));
    hq().AddToInventory(GoodCounts::make(LEAF_BOARDS, 2), true);
    hq().AddToInventory(GoodCounts::make(NEEDLE_BOARDS, 1), true);
    BOOST_TEST(construction.HasMaterialFor(BuildingType::Woodcutter));
    // A site takes it
    placeSite(BuildingType::Woodcutter);
    const auto free = construction.GetFreeConstructionMaterial();
    BOOST_TEST(free[LEAF_BOARDS] + free[NEEDLE_BOARDS] == 0);
    BOOST_TEST(!construction.HasMaterialFor(BuildingType::Woodcutter));
}

BOOST_AUTO_TEST_SUITE_END()
