// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "PointOutput.h"
#include "SerializedGameData.h"
#include "addons/const_addons.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "factories/BuildingFactory.h"
#include "figures/nofBuildingWorker.h"
#include "lua/GameDataLoader.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "nodeObjs/noFlag.h"
#include "nodeObjs/noGranite.h"
#include "gameTypes/GameTypesOutput.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>

namespace bnw = boost::nowide;

// Output ratio of buildings (wares per work step, civilization mode): rttr:AddWorkshop{ outputRatio },
// rttr:AddTool{ outputRatio } and rttr:SetBuildingOutputRatio
BOOST_AUTO_TEST_SUITE(CivOutputRatioSuite)

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

WareDesc makeWare(const std::string& name)
{
    WareDesc desc;
    desc.name = name;
    desc.title = name + "Title";
    desc.look = "Boards";
    desc.transport = "Boards";
    return desc;
}

/// Raw (ware 0) -> Product (ware 1) with the given output ratio, optionally with the byproduct Bone (ware 2)
WorldDescription makeDesc(unsigned outputRatio, bool withByproduct = false)
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("Raw"));
    desc.wares.push_back(makeWare("Product"));
    desc.wares.push_back(makeWare("Bone"));
    WorkshopDesc shop;
    shop.name = "Shop";
    shop.title = "ShopTitle";
    shop.size = "small";
    shop.cost = {{"Boards", 2}};
    shop.look = "Sawmill";
    shop.worker.title = "ShopWorker";
    shop.worker.tool = "none";
    shop.worker.look = "Carpenter";
    shop.inputs = {{"Raw"}};
    shop.output = {"Product"};
    shop.workTime = 2;
    shop.outputRatio = outputRatio;
    desc.workshops.push_back(shop);
    if(withByproduct)
    {
        ByproductDesc byproduct;
        byproduct.building = "Shop";
        byproduct.ware = {"Bone"};
        desc.byproducts.push_back(byproduct);
    }
    return desc;
}

constexpr GoodType RAW = GoodType::Custom0;
const GoodType PRODUCT = getCustomGood(1);
const GoodType BONE = getCustomGood(2);
constexpr BuildingType SHOP = BuildingType::Custom0;

struct RatioFixture : public WorldWithGCExecution1P
{
    void init(const WorldDescription& desc)
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
    }

    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    unsigned countInHQ(GoodType good) { return hq().GetNumRealWares(good); }
    unsigned inInventory(GoodType good) { return world.GetPlayer(0).GetInventory()[good]; }

    /// Place a finished building connected to the HQ
    MapPoint placeConnectedBuilding(BuildingType bld, Position offset = Position(4, 2))
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + offset);
        BuildingFactory::CreateBuilding(world, bld, pos, 0, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<nobUsual>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        return pos;
    }

    /// Let the shop process the given number of raw wares and return the number of products
    unsigned produce(unsigned numRaw)
    {
        hq().AddToInventory(GoodCounts::make(RAW, numRaw), true);
        const MapPoint bldPos = placeConnectedBuilding(SHOP);
        RTTR_EXEC_TILL(20000, inInventory(RAW) == 0u && !world.GetSpecObj<nobUsual>(bldPos)->is_working
                                && world.GetSpecObj<nobUsual>(bldPos)->GetWorker()
                                && world.GetSpecObj<nobUsual>(bldPos)->GetWorker()->GetState()
                                     == nofBuildingWorker::State::WaitingForWaresOrProductionStopped);
        RTTR_SKIP_GFS(500);
        return inInventory(PRODUCT);
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(OutputRatioFromLua)
{
    const std::string base = R"(
rttr:AddWare{ name = "Raw", look = "Wood", transport = "Wood" }
rttr:AddWare{ name = "Product", look = "Boards", transport = "Boards" }
rttr:AddWare{ name = "CopperSaw", kind = "Saw", look = "Saw", transport = "Saw" }
rttr:AddToolMaterial{ name = "Iron" }
rttr:AddTool{ ware = "Saw", material = "Iron", durability = 100, speed = 100, outputRatio = 0.5 }
rttr:AddTool{ ware = "CopperSaw", material = "Iron", durability = 100, speed = 50 }
rttr:AddWorkshop{ name = "Shop", size = "small", cost = { Boards = 2 }, look = "Sawmill",
    worker = { tool = "Saw", look = "Carpenter" }, inputs = { "Raw" }, output = "Product", workTime = 5,
    outputRatio = 1.25 }
rttr:AddWorkshop{ name = "Plain", size = "small", cost = { Boards = 2 }, look = "Sawmill",
    worker = { tool = "none", look = "Carpenter" }, inputs = { "Raw" }, output = "Product", workTime = 5 }
rttr:SetBuildingOutputRatio{ Woodcutter = 2, Quarry = 0.25 }
)";
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(base, desc));
    BOOST_TEST(desc.workshops[0].outputRatio == 125u);
    BOOST_TEST(desc.workshops[1].outputRatio == 100u);
    BOOST_TEST(desc.tools[0].outputRatio == 50u);
    BOOST_TEST(desc.tools[1].outputRatio == 0u);
    BOOST_TEST(desc.buildingOutputRatios.at("Woodcutter") == 200u);
    const civ::Content content(desc);
    BOOST_TEST(content.GetOutputRatio(SHOP) == 125u);
    BOOST_TEST(content.GetOutputRatio(getCustomBuilding(1)) == 100u);
    BOOST_TEST(content.GetOutputRatio(BuildingType::Woodcutter) == 200u);
    BOOST_TEST(content.GetOutputRatio(BuildingType::Quarry) == 25u);
    BOOST_TEST(content.GetOutputRatio(BuildingType::Sawmill) == 100u);
    BOOST_TEST_REQUIRE(content.GetTool(GoodType::Saw));
    BOOST_TEST(content.GetTool(GoodType::Saw)->outputRatio == 50u);

    const auto replaced = [&base](const std::string& from, const std::string& to) {
        std::string result = base;
        const auto pos = result.find(from);
        BOOST_TEST_REQUIRE(pos != std::string::npos);
        return result.replace(pos, from.size(), to);
    };
    for(const std::string& invalid :
        {replaced("outputRatio = 1.25", "outputRatio = 8.5"), replaced("outputRatio = 1.25", "outputRatio = -1"),
         replaced("outputRatio = 1.25", "outputRatio = \"much\""), replaced("outputRatio = 0.5 }", "outputRatio = 9 }"),
         replaced("Quarry = 0.25", "Quarry = \"low\""), base + "rttr:SetBuildingOutputRatio{ Woodcutter = 1 }",
         base
           + "rttr:AddWorkshop{ name = \"Lib\", size = \"small\", look = \"Sawmill\", worker = { tool = \"none\", "
             "look = \"Carpenter\" }, library = { capacity = 5 }, outputRatio = 2 }"})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(invalid, invalidDesc));
    }
    // Workshops get it in rttr:AddWorkshop, only built-in buildings with a worker here
    for(const std::string& invalid :
        {replaced("Quarry = 0.25", "Shop = 0.25"), replaced("Quarry = 0.25", "Barracks = 2"),
         replaced("Quarry = 0.25", "Quary = 2")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST_REQUIRE(loadLua(invalid, invalidDesc));
        BOOST_CHECK_THROW(civ::Content{invalidDesc}, GameDataError);
    }
}

BOOST_FIXTURE_TEST_CASE(HalfRatioMakesEverySecondStep, RatioFixture)
{
    init(makeDesc(50));
    // 5 steps: 0, 1, 0, 1, 0 -> 2 products, half a product left over
    BOOST_TEST(produce(5) == 2u);
    BOOST_TEST(countInHQ(RAW) == 0u);
}

BOOST_FIXTURE_TEST_CASE(RatioAboveOneMakesSeveralWares, RatioFixture)
{
    init(makeDesc(250));
    // 4 steps with 2.5: 2, 3, 2, 3
    BOOST_TEST(produce(4) == 10u);
    RTTR_EXEC_TILL(2000, countInHQ(PRODUCT) == 10u);
}

BOOST_FIXTURE_TEST_CASE(ZeroRatioDoesNotWork, RatioFixture)
{
    init(makeDesc(0));
    hq().AddToInventory(GoodCounts::make(RAW, 3), true);
    const MapPoint bldPos = placeConnectedBuilding(SHOP);
    const nobUsual& bld = *world.GetSpecObj<nobUsual>(bldPos);
    RTTR_EXEC_TILL(3000, bld.GetNumWares(0) == 3u);
    RTTR_SKIP_GFS(1000);
    // The inputs are not wasted
    BOOST_TEST(bld.GetNumWares(0) == 3u);
    BOOST_TEST(bld.GetOutputRatio() == 0u);
    BOOST_TEST(inInventory(PRODUCT) == 0u);
}

BOOST_FIXTURE_TEST_CASE(MoreWaresThanAFlagHolds, RatioFixture)
{
    // 8 products and 8 byproducts per step: the worker puts them at the flag in 2 rounds
    init(makeDesc(800, true));
    BOOST_TEST(produce(1) == 8u);
    BOOST_TEST(inInventory(BONE) == 8u);
    RTTR_EXEC_TILL(4000, countInHQ(PRODUCT) == 8u && countInHQ(BONE) == 8u);
}

BOOST_FIXTURE_TEST_CASE(ToolsAddToTheRatio, RatioFixture)
{
    WorldDescription desc = makeDesc(50);
    ToolMaterialDesc iron;
    iron.name = "Iron";
    iron.title = "Iron";
    desc.toolMaterials = {iron};
    ToolDesc tongs;
    tongs.ware = "Tongs";
    tongs.material = "Iron";
    tongs.durability = 100;
    tongs.speed = 0;
    tongs.outputRatio = 100;
    desc.tools = {tongs};
    BuildingToolsDesc shopTools;
    shopTools.baseSpeed = 100;
    shopTools.tools = {{"Tongs"}};
    desc.workshops[0].tools = shopTools;
    init(desc);

    const MapPoint bldPos = placeConnectedBuilding(SHOP);
    const nobUsual& bld = *world.GetSpecObj<nobUsual>(bldPos);
    RTTR_EXEC_TILL(2000, bld.HasWorker());
    BOOST_TEST(bld.GetOutputRatio() == 50u);
    // With the tongs: 0.5 + 1 = 1.5
    hq().AddToInventory(GoodCounts::make(GoodType::Tongs, 1), true);
    RTTR_EXEC_TILL(3000, bld.GetToolSlots()[0].tool == GoodType::Tongs);
    BOOST_TEST(bld.GetOutputRatio() == 150u);
    hq().AddToInventory(GoodCounts::make(RAW, 4), true);
    RTTR_EXEC_TILL(20000,
                   inInventory(RAW) == 0u && !bld.is_working
                     && bld.GetWorker()->GetState() == nofBuildingWorker::State::WaitingForWaresOrProductionStopped);
    RTTR_SKIP_GFS(500);
    // 1, 2, 1, 2
    BOOST_TEST(inInventory(PRODUCT) == 6u);
}

BOOST_FIXTURE_TEST_CASE(BuiltinBuildingWithRatio, RatioFixture)
{
    WorldDescription desc = makeDesc(100);
    desc.buildingOutputRatios = {{"Sawmill", 200}};
    init(desc);
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    const unsigned numBoards = inInventory(GoodType::Boards);
    hq().AddToInventory(GoodCounts::make(GoodType::Wood, 3), true);
    const unsigned numWood = inInventory(GoodType::Wood);
    placeConnectedBuilding(BuildingType::Sawmill);
    RTTR_EXEC_TILL(20000, inInventory(GoodType::Boards) >= numBoards + 2u * numWood);
    RTTR_SKIP_GFS(1000);
    BOOST_TEST(inInventory(GoodType::Boards) == numBoards + 2u * numWood);
}

BOOST_FIXTURE_TEST_CASE(StonemasonTakesAPiecePerStone, RatioFixture)
{
    WorldDescription desc = makeDesc(100);
    desc.buildingOutputRatios = {{"Quarry", 200}};
    init(desc);
    hq().AddToInventory(GoodCounts::make(GoodType::PickAxe, 1), true);
    // A pile with 5 stones (state 4): 2 + 2 + 1, then it is gone
    const MapPoint pilePos = world.MakeMapPoint(hqPos + Position(8, 0));
    world.DestroyNO(pilePos, false);
    world.SetNO(pilePos, new noGranite(GraniteType::One, 4));
    const unsigned numStones = inInventory(GoodType::Stones);
    placeConnectedBuilding(BuildingType::Quarry, Position(6, 2));
    RTTR_EXEC_TILL(20000, world.GetNO(pilePos)->GetType() != NodalObjectType::Granite);
    RTTR_SKIP_GFS(1000);
    BOOST_TEST(inInventory(GoodType::Stones) == numStones + 5u);
}

BOOST_FIXTURE_TEST_CASE(SerializeOutputRatio, RatioFixture)
{
    init(makeDesc(150, true));
    hq().AddToInventory(GoodCounts::make(RAW, 1), true);
    const MapPoint bldPos = placeConnectedBuilding(SHOP);
    // After the first step (1 product, half one left over) the next step gives 2
    RTTR_EXEC_TILL(8000, inInventory(PRODUCT) == 1u);
    hq().AddToInventory(GoodCounts::make(RAW, 1), true);
    RTTR_EXEC_TILL(8000, world.GetSpecObj<nobUsual>(bldPos)->is_working);

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    RTTR_EXEC_TILL(8000, inInventory(PRODUCT) == 3u);
    RTTR_SKIP_GFS(500);
    BOOST_TEST(inInventory(PRODUCT) == 3u);
    BOOST_TEST(inInventory(BONE) == 3u);
}

BOOST_AUTO_TEST_SUITE_END()
