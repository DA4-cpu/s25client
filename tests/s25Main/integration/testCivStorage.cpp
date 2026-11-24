// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FindWhConditions.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "SerializedGameData.h"
#include "Ware.h"
#include "addons/const_addons.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "buildings/nobStorehouse.h"
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
#include "gameData/BuildingProperties.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/format.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>

// Civilization mode: capacity of the warehouses (storage.lua) and warehouses defined in lua
BOOST_AUTO_TEST_SUITE(CivStorageSuite)

namespace {
bool loadLua(const std::string& content, WorldDescription& desc)
{
    rttr::test::TmpFolder tmp;
    {
        boost::nowide::ofstream file(tmp / "default.lua");
        file << content;
    }
    GameDataLoader loader(desc, tmp);
    rttr::test::LogAccessor logAcc;
    return loader.Load();
}

/// The shed (workshop 0) is a warehouse for boards only
WorldDescription makeShedDesc(unsigned capacity = 5)
{
    WorldDescription desc;
    WorkshopDesc shed;
    shed.name = "Shed";
    shed.title = "ShedTitle";
    shed.size = "small";
    shed.cost = {{{"Boards"}, 2}};
    shed.look = "Storehouse";
    shed.warehouseCapacity = capacity;
    shed.warehouseWares = {{"Boards"}};
    desc.workshops.push_back(shed);
    return desc;
}
const BuildingType SHED = getCustomBuilding(0);

struct StorageFixture : public WorldWithGCExecution<1, 32, 32>
{
    void init(const WorldDescription& desc)
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
        BOOST_TEST_REQUIRE(hq().GetLoad() == 0u);
    }
    nobBaseWarehouse& hq() { return *world.GetPlayer(curPlayer).GetHQ(); }
    GamePlayer& player() { return world.GetPlayer(curPlayer); }

    MapPoint placeConnectedBuilding(BuildingType bld, Position offset)
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + offset);
        BuildingFactory::CreateBuilding(world, bld, pos, curPlayer, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<noBuilding>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        return pos;
    }
    /// A sawmill with its carpenter and wood for `numBoards` boards
    nobUsual& placeSawmill(unsigned numBoards)
    {
        hq().AddToInventory(GoodCounts::make(GoodType::Wood, numBoards), true);
        hq().AddToInventory(PeopleCounts::make(Job::Carpenter, 1), true);
        return *world.GetSpecObj<nobUsual>(placeConnectedBuilding(BuildingType::Sawmill, Position(4, 2)));
    }
    /// Wares lying at a flag without goal
    unsigned numWaiting()
    {
        unsigned result = 0;
        for(const Ware* ware : player().GetWares())
            result += ware->IsLostWare() && ware->IsWaitingAtFlag() ? 1 : 0;
        return result;
    }
    unsigned numProduced()
    {
        return player().GetEconomyStatistics().GetTotalGained(GoodType::Boards, WareSource::Production);
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(CapacityOfTheGameData)
{
    const civ::TechTree techTree = civ::loadDefaultTechTree();
    const civ::Content& content = techTree.GetContent();
    BOOST_TEST(content.GetWarehouseCapacity(BuildingType::Headquarters) == 700u);
    BOOST_TEST(content.GetWarehouseCapacity(BuildingType::HarborBuilding) == 500u);
    BOOST_TEST(content.GetWarehouseCapacity(BuildingType::Storehouse) == 400u);
    BOOST_TEST(content.GetFlagWorkLimit() == 7u);
    BOOST_TEST(techTree.GetAIStorageFullPercent() == 90u);
    // Without game data: no limits
    BOOST_TEST(civ::Content().GetWarehouseCapacity(BuildingType::Headquarters) == 0u);
    BOOST_TEST(civ::Content().GetFlagWorkLimit() == 8u);
}

BOOST_AUTO_TEST_CASE(CapacityFromLua)
{
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(R"(rttr:SetWarehouseCapacity{ Headquarters = 50, Storehouse = 20 }
rttr:SetFlagWorkLimit(6)
rttr:SetAIStorageFull(80))",
                               desc));
    const civ::TechTree techTree(desc);
    BOOST_TEST(techTree.GetContent().GetWarehouseCapacity(BuildingType::Headquarters) == 50u);
    BOOST_TEST(techTree.GetContent().GetWarehouseCapacity(BuildingType::Storehouse) == 20u);
    BOOST_TEST(techTree.GetContent().GetWarehouseCapacity(BuildingType::HarborBuilding) == 0u);
    BOOST_TEST(techTree.GetContent().GetFlagWorkLimit() == 6u);
    BOOST_TEST(techTree.GetAIStorageFullPercent() == 80u);
    for(const std::string& invalid :
        {std::string("rttr:SetWarehouseCapacity{ Headquarters = 0 }"),
         std::string("rttr:SetWarehouseCapacity{ Headquarters = \"many\" }"),
         std::string("rttr:SetWarehouseCapacity{ }"), std::string("rttr:SetWarehouseCapacity{ 5 }"),
         std::string("rttr:SetFlagWorkLimit(0)"), std::string("rttr:SetFlagWorkLimit(9)"),
         std::string("rttr:SetAIStorageFull(0)"), std::string("rttr:SetAIStorageFull(101)")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(invalid, invalidDesc));
    }
    // Only warehouses have a capacity
    WorldDescription noWarehouse;
    BOOST_TEST_REQUIRE(loadLua("rttr:SetWarehouseCapacity{ Woodcutter = 5 }", noWarehouse));
    BOOST_CHECK_THROW(civ::TechTree{noWarehouse}, GameDataError);
}

BOOST_FIXTURE_TEST_CASE(FullWarehouseTakesNoMoreWares, StorageFixture)
{
    WorldDescription desc;
    desc.warehouseCapacity = {{"Headquarters", 3}};
    init(desc);
    BOOST_TEST(hq().GetCapacity() == 3u);
    // Wares added directly are stored also beyond the capacity. Then nothing more is accepted
    nobUsual& sawmill = placeSawmill(4);
    BOOST_TEST(hq().GetLoad() >= 3u);
    BOOST_TEST(!hq().CanStoreWare(GoodType::Boards));
    // The sawmill gets the wood and makes 4 boards: 3 go to the HQ (wares on their way count), the 4th waits at the
    // flag of the sawmill
    RTTR_EXEC_TILL(8000, numProduced() == 4u);
    RTTR_SKIP_GFS(500);
    BOOST_TEST(hq().GetNumRealWares(GoodType::Boards) == 3u);
    BOOST_TEST(hq().GetLoad() == 3u);
    BOOST_TEST(numWaiting() == 1u);
    BOOST_TEST(sawmill.GetFlag()->GetNumWares() == 1u);
    // Still no space
    player().FindGoalsForWaitingWares();
    RTTR_SKIP_GFS(300);
    BOOST_TEST(numWaiting() == 1u);
    // Space again: the ware comes
    BOOST_TEST(hq().TakeWares(GoodType::Boards, 1, WareUse::Lost) == 1u);
    player().FindGoalsForWaitingWares();
    BOOST_TEST(numWaiting() == 0u);
    RTTR_EXEC_TILL(500, hq().GetNumRealWares(GoodType::Boards) == 3u);
}

BOOST_FIXTURE_TEST_CASE(WaitingWareGoesToABuildingThatNeedsIt, StorageFixture)
{
    WorldDescription desc;
    desc.warehouseCapacity = {{"Headquarters", 1}};
    init(desc);
    placeSawmill(2);
    RTTR_EXEC_TILL(8000, numProduced() == 2u);
    RTTR_SKIP_GFS(500);
    BOOST_TEST_REQUIRE(numWaiting() == 1u);
    // No boards in the HQ, a building site needs boards: it gets the waiting one
    hq().TakeWares(GoodType::Boards, 1, WareUse::Lost);
    hq().AddToInventory(PeopleCounts::make(Job::Builder, 1), true);
    const MapPoint sitePos = world.MakeMapPoint(hqPos + Position(-4, 2));
    world.SetBuildingSite(BuildingType::Woodcutter, sitePos, curPlayer);
    this->BuildRoadForBlds(sitePos, hqPos);
    player().FindGoalsForWaitingWares();
    BOOST_TEST(numWaiting() == 0u);
    bool toSite = false;
    for(const Ware* ware : player().GetWares())
        toSite |= ware->type == GoodType::Boards && ware->GetGoal() == world.GetSpecObj<noBuildingSite>(sitePos);
    BOOST_TEST(toSite);
}

BOOST_FIXTURE_TEST_CASE(WorkStopsBeforeTheFlagIsFull, StorageFixture)
{
    // Only 2 wares of a worker may lie at his flag. The HQ takes one board, then it is full
    WorldDescription desc;
    desc.warehouseCapacity = {{"Headquarters", 1}};
    desc.flagWorkLimit = 2;
    init(desc);
    nobUsual& sawmill = placeSawmill(6);
    RTTR_SKIP_GFS(10000);
    BOOST_TEST(hq().GetNumRealWares(GoodType::Boards) == 1u);
    BOOST_TEST(sawmill.GetFlag()->GetNumWares() == 2u);
    // The carpenter waits with the 4th board in his hands, the rest of the wood is not used
    BOOST_TEST(numProduced() == 3u);
    BOOST_TEST_REQUIRE(sawmill.GetWorker());
    BOOST_TEST((sawmill.GetWorker()->GetState() == nofBuildingWorker::State::WaitForWareSpace));
}

BOOST_AUTO_TEST_CASE(WarehouseFromLua)
{
    const std::string shed =
      R"(rttr:AddWorkshop{ name = "Shed", size = "small", cost = { Boards = 2 }, look = "Storehouse",
    warehouse = { wares = { "Boards", "Stones" }, capacity = 80 }%s })";
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua((boost::format(shed) % "").str(), desc));
    const civ::Content content(desc);
    BOOST_TEST(content.IsCustomWarehouse(SHED));
    BOOST_TEST(content.GetWarehouseCapacity(SHED) == 80u);
    BOOST_TEST(content.WarehouseStores(SHED, GoodType::Boards));
    BOOST_TEST(content.WarehouseStores(SHED, GoodType::Stones));
    BOOST_TEST(!content.WarehouseStores(SHED, GoodType::Wood));
    // Built-in warehouses store everything
    BOOST_TEST(content.WarehouseStores(BuildingType::Storehouse, GoodType::Wood));
    BOOST_TEST(!content.IsCustomWarehouse(BuildingType::Storehouse));

    for(const std::string& extra :
        {std::string(R"(, worker = { look = "Helper" })"), std::string(R"(, output = "Boards")"),
         std::string(R"(, inputs = { "Wood" })"), std::string(", workTime = 5")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(extra);
        BOOST_TEST(!loadLua((boost::format(shed) % extra).str(), invalidDesc));
    }
    for(const std::string& warehouse :
        {std::string("{ wares = { \"Boards\" } }"), std::string("{ capacity = 5 }"),
         std::string("{ wares = { }, capacity = 5 }"), std::string("{ wares = { \"Boards\" }, capacity = 0 }")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(warehouse);
        BOOST_TEST(!loadLua(R"(rttr:AddWorkshop{ name = "Shed", size = "small", cost = { Boards = 2 },
    look = "Storehouse", warehouse = )"
                              + warehouse + " }",
                            invalidDesc));
    }
    WorldDescription unknownWare;
    BOOST_TEST_REQUIRE(loadLua(R"(rttr:AddWorkshop{ name = "Shed", size = "small", cost = { Boards = 2 },
    look = "Storehouse", warehouse = { wares = { "Bords" }, capacity = 5 } })",
                               unknownWare));
    BOOST_CHECK_THROW(civ::Content{unknownWare}, GameDataError);
}

BOOST_FIXTURE_TEST_CASE(LuaWarehouseStoresOnlyItsWares, StorageFixture)
{
    WorldDescription desc = makeShedDesc(5);
    desc.warehouseCapacity = {{"Headquarters", 1}};
    init(desc);
    BOOST_TEST(BuildingProperties::IsWareHouse(SHED));
    BOOST_TEST(!BuildingProperties::IsUsual(SHED));
    const MapPoint shedPos = placeConnectedBuilding(SHED, Position(-4, 2));
    auto* shed = world.GetSpecObj<nobStorehouse>(shedPos);
    BOOST_TEST_REQUIRE(shed);
    BOOST_TEST(player().GetBuildingRegister().GetStorehouses().size() == 2u);
    BOOST_TEST(shed->GetCapacity() == 5u);
    BOOST_TEST(FW::AcceptsWare(GoodType::Boards)(*shed));
    BOOST_TEST(!FW::AcceptsWare(GoodType::Stones)(*shed));
    BOOST_TEST(!FW::AcceptsFigure(Job::Helper)(*shed));
    BOOST_TEST(!FW::AcceptsFigure(Job::Carpenter)(*shed));

    // 4 boards: one fits into the HQ, the others go to the shed. No helpers are born in the shed
    placeSawmill(4);
    RTTR_EXEC_TILL(10000, numProduced() == 4u);
    RTTR_SKIP_GFS(1000);
    BOOST_TEST(numWaiting() == 0u);
    BOOST_TEST(hq().GetNumRealWares(GoodType::Boards) + shed->GetNumRealWares(GoodType::Boards) == 4u);
    BOOST_TEST(shed->GetNumRealWares(GoodType::Boards) >= 3u);
    BOOST_TEST(shed->GetNumRealFigures(Job::Helper) == 0u);
    // A building site gets its boards from the warehouses, also from the shed (the HQ has only one)
    hq().AddToInventory(PeopleCounts::make(Job::Builder, 1), true);
    const MapPoint sitePos = world.MakeMapPoint(hqPos + Position(-2, 5));
    world.SetBuildingSite(BuildingType::Woodcutter, sitePos, curPlayer);
    this->BuildRoadForBlds(sitePos, hqPos);
    RTTR_EXEC_TILL(4000, hq().GetNumRealWares(GoodType::Boards) + shed->GetNumRealWares(GoodType::Boards) == 2u);
    const unsigned inShed = shed->GetNumRealWares(GoodType::Boards);
    BOOST_TEST(inShed <= 2u);

    // Saved and loaded: still a warehouse with its boards
    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    shed = world.GetSpecObj<nobStorehouse>(shedPos);
    BOOST_TEST_REQUIRE(shed);
    BOOST_TEST((shed->GetBuildingType() == SHED));
    BOOST_TEST(shed->GetNumRealWares(GoodType::Boards) == inShed);
    BOOST_TEST(player().GetBuildingRegister().GetStorehouses().size() == 2u);
}

BOOST_AUTO_TEST_SUITE_END()
