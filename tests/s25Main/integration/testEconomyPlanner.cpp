// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "addons/const_addons.h"
#include "ai/AIPlayer.h"
#include "ai/aijh/AIConstruction.h"
#include "ai/aijh/AIPlayerJH.h"
#include "ai/civ/EconomyPlanner.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "factories/AIFactory.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "nodeObjs/noDeposit.h"
#include "nodeObjs/noTree.h"
#include "gameTypes/Inventory.h"
#include "gameData/ToolConsts.h"
#include "gameData/WorldDescription.h"
#include <boost/test/unit_test.hpp>
#include <memory>

// Planner of the production buildings of the civilization mode: the priority list (DESIGN.md "Neufassung der
// Planung"). A small chain: the pit collects loam from deposits, the kiln makes clay of loam, the research costs clay
BOOST_AUTO_TEST_SUITE(EconomyPlannerSuite)

namespace {
WareDesc makeWare(const std::string& name)
{
    WareDesc desc;
    desc.name = name;
    desc.title = name + "Title";
    desc.look = "Stones";
    desc.transport = "Stones";
    return desc;
}

WorkshopDesc makeWorkshop(const std::string& name)
{
    WorkshopDesc desc;
    desc.name = name;
    desc.title = name + "Title";
    desc.size = "small";
    desc.cost = {{{"Boards"}, 2}};
    desc.look = "Quarry";
    desc.worker.title = name + "Worker";
    desc.worker.tool = "none";
    desc.worker.look = "Stonemason";
    desc.workTime = 10;
    return desc;
}

/// Loam (ware 0) collected from deposits (map object 0) by the pit (workshop 0), clay (ware 1) made of loam by the
/// kiln (workshop 1). The technology "pottery" costs 5 clay. `expanding`: the list of ai.lua
WorldDescription makeDesc(const std::vector<std::string>& expanding = {})
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("Loam"));
    desc.wares.push_back(makeWare("Clay"));
    MapObjectDesc deposit;
    deposit.name = "LoamDeposit";
    deposit.title = "LoamDepositTitle";
    deposit.sizes = 2;
    deposit.look = "Granite1";
    desc.mapObjects.push_back(deposit);
    WorkshopDesc pit = makeWorkshop("Pit");
    pit.collectObject = "LoamDeposit";
    pit.collectRadius = 6;
    pit.output = {"Loam"};
    desc.workshops.push_back(pit);
    WorkshopDesc kiln = makeWorkshop("Kiln");
    kiln.inputs = {{"Loam"}};
    kiln.output = {"Clay"};
    desc.workshops.push_back(kiln);
    TechnologyDesc tech;
    tech.name = "pottery";
    tech.title = "Pottery";
    tech.cost = {{"Clay", 5}};
    desc.technologies.add(tech);
    desc.aiExpandingBuildings = expanding;
    return desc;
}

constexpr GoodType LOAM = GoodType::Custom0;
const GoodType CLAY = getCustomGood(1);
const BuildingType PIT = getCustomBuilding(0);
const BuildingType KILN = getCustomBuilding(1);

struct PlannerFixture : public WorldWithGCExecution<1, 32, 32>
{
    std::unique_ptr<AIPlayer> ai;

    /// Start the civilization mode with the game data. Construction material, helpers and builders in the HQ
    void startGame(const WorldDescription& desc, unsigned numBoards = 40)
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
        hq().AddToInventory(GoodCounts::make(GoodType::Boards, numBoards), true);
        hq().AddToInventory(GoodCounts::make(GoodType::Stones, 40), true);
        hq().AddToInventory(PeopleCounts::make(Job::Helper, 30), true);
        hq().AddToInventory(PeopleCounts::make(Job::Builder, 5), true);
    }
    /// Create the AI after the map is prepared (it scans the map at the start). The planner exists after the
    /// initialization of the AI, it did not plan yet
    void startAI()
    {
        world.InitAfterLoad();
        ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
        for(unsigned gf = 0; gf < 15; gf++)
        {
            em.ExecuteNextGF();
            ai->RunGF(em.GetCurrentGF(), gf % 5 == 0);
        }
        BOOST_TEST_REQUIRE(planner());
        executeCommands();
    }
    /// One planning step and the commands of the AI
    void step()
    {
        planner()->Step();
        executeCommands();
    }
    void executeCommands()
    {
        for(gc::GameCommandPtr& gc : ai->FetchGameCommands())
            gc->Execute(world, curPlayer);
    }

    nobBaseWarehouse& hq() { return *world.GetPlayer(curPlayer).GetHQ(); }
    civai::EconomyPlanner* planner() { return static_cast<AIJH::AIPlayerJH&>(*ai).GetEconomyPlanner(); }
    const civai::WareStatus& ware(GoodType good) { return planner()->GetWareStatus(good); }

    /// Map objects (deposits, trees) in the territory near the HQ
    void placeDeposits(Position offset = Position(4, 4))
    {
        for(const Position delta : {Position(0, 0), Position(1, 0), Position(0, 1)})
            placeObject(offset + delta, [](MapPoint) { return new noDeposit(0, 1); });
    }
    void placeTrees(Position offset, unsigned num)
    {
        static const std::vector<Position> deltas = {{0, 0}, {1, 0},  {0, 1},  {1, 1},  {-1, 0}, {0, -1},
                                                     {2, 0}, {0, 2},  {-1, 1}, {2, 1},  {1, 2},  {-1, -1},
                                                     {2, 2}, {-2, 0}, {0, -2}, {-2, 1}, {3, 0},  {0, 3}};
        BOOST_TEST_REQUIRE(num <= deltas.size());
        for(unsigned i = 0; i < num; i++)
            placeObject(offset + deltas[i], [](MapPoint pt) { return new noTree(pt, 0, 3); });
    }
    template<class T_Create>
    void placeObject(Position offset, const T_Create& create)
    {
        const MapPoint pt = world.MakeMapPoint(hqPos + offset);
        BOOST_TEST_REQUIRE(world.GetNode(pt).owner == curPlayer + 1u);
        world.DestroyNO(pt, false);
        world.SetNO(pt, create(pt));
        world.RecalcBQAroundPointBig(pt);
    }

    std::vector<const civai::Order*> list() { return planner()->GetList(); }
    /// Position of the order in the list, -1 if not there
    int findOrder(BuildingType bld)
    {
        const std::vector<const civai::Order*> orders = list();
        for(unsigned i = 0; i < orders.size(); i++)
        {
            if(orders[i]->building == bld)
                return static_cast<int>(i);
        }
        return -1;
    }
    const civai::Order& getOrder(BuildingType bld)
    {
        const int idx = findOrder(bld);
        BOOST_TEST_REQUIRE(idx >= 0);
        return *list()[idx];
    }
    MapPoint findSite(BuildingType bld)
    {
        for(const noBuildingSite* site : world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites())
        {
            if(site->GetBuildingType() == bld)
                return site->GetPos();
        }
        return MapPoint::Invalid();
    }
};
} // namespace

BOOST_FIXTURE_TEST_CASE(ShortageOrdersTheChainFromTheBottom, PlannerFixture)
{
    startGame(makeDesc());
    placeDeposits();
    startAI();
    // The research asks for 5 clay, none is there: shortage
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    BOOST_TEST((ware(CLAY).state == civai::SupplyState::Shortage));
    BOOST_TEST(ware(CLAY).openDemand == 5u);
    // The kiln makes clay of loam. Nobody makes loam: the pit is ordered for the kiln and comes above it
    const civai::Order& kiln = getOrder(KILN);
    BOOST_TEST((kiln.kind == civai::OrderKind::Needed));
    BOOST_TEST((kiln.forWare == CLAY));
    BOOST_TEST(!kiln.consumer);
    BOOST_TEST((kiln.state == civai::OrderState::Waiting));
    BOOST_TEST(kiln.reason.find("Eingang fehlt") == 0u);
    const civai::Order& pit = getOrder(PIT);
    BOOST_TEST(pit.parent == kiln.id);
    BOOST_TEST((pit.forWare == LOAM));
    BOOST_TEST(findOrder(PIT) < findOrder(KILN));

    // Next step: the pit is placed. The kiln waits until loam suffices
    step();
    BOOST_TEST((getOrder(PIT).state == civai::OrderState::Placed));
    BOOST_TEST(findSite(PIT).isValid());
    BOOST_TEST((getOrder(KILN).state == civai::OrderState::Waiting));
    BOOST_TEST(!findSite(KILN).isValid());
    // No second pit while the first starts up
    step();
    BOOST_TEST(world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites().size() == 1u);
}

BOOST_FIXTURE_TEST_CASE(NoWayShowsTheReason, PlannerFixture)
{
    // No deposits: the kiln cannot get loam
    startGame(makeDesc());
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    const civai::Order& kiln = getOrder(KILN);
    BOOST_TEST((kiln.state == civai::OrderState::Blocked));
    BOOST_TEST_INFO(kiln.reason);
    BOOST_TEST(kiln.reason.find("kein Weg zu") == 0u);
    BOOST_TEST(kiln.reason.find("nicht im Gebiet: LoamDeposit") != std::string::npos);
    BOOST_TEST(findOrder(PIT) == -1);
    // Nothing changed: it stays blocked and nothing is placed
    step();
    BOOST_TEST((getOrder(KILN).state == civai::OrderState::Blocked));
    BOOST_TEST(world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites().empty());
}

BOOST_FIXTURE_TEST_CASE(SurplusOrdersAConsumer, PlannerFixture)
{
    startGame(makeDesc());
    placeDeposits();
    // 20 loam and nobody uses it: surplus (more than the minimum stock of 8)
    hq().AddToInventory(GoodCounts::make(LOAM, 20), true);
    startAI();
    step();
    BOOST_TEST((ware(LOAM).state == civai::SupplyState::Surplus));
    const civai::Order& kiln = getOrder(KILN);
    BOOST_TEST(kiln.consumer);
    BOOST_TEST((kiln.forWare == LOAM));
    BOOST_TEST((kiln.state == civai::OrderState::Placed));
    BOOST_TEST(findSite(KILN).isValid());
    // Nothing else: loam is not short
    BOOST_TEST(findOrder(PIT) == -1);
}

BOOST_FIXTURE_TEST_CASE(SurplusIsThereForItsConsumer, PlannerFixture)
{
    // No deposits: nobody can make loam. The 20 loam in stock would not keep a kiln busy for 10 minutes, but they are
    // a surplus and the kiln is ordered to use them
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(LOAM, 20), true);
    startAI();
    step();
    BOOST_TEST((ware(LOAM).state == civai::SupplyState::Surplus));
    BOOST_TEST((getOrder(KILN).state == civai::OrderState::Placed));
    BOOST_TEST(findSite(KILN).isValid());
}

BOOST_FIXTURE_TEST_CASE(ExpandOnlyWithTheBuildingsOfTheList, PlannerFixture)
{
    // Loam is in normal state (none there, none used). The pit is not in the list of ai.lua: no order
    startGame(makeDesc());
    placeDeposits();
    startAI();
    step();
    BOOST_TEST((ware(LOAM).state == civai::SupplyState::Normal));
    BOOST_TEST(findOrder(PIT) == -1);
    BOOST_TEST(!findSite(PIT).isValid());
}

BOOST_FIXTURE_TEST_CASE(ExpandingOrdersArePlacedAndLeaveTheList, PlannerFixture)
{
    startGame(makeDesc({"Pit"}));
    placeDeposits();
    startAI();
    step();
    BOOST_TEST(findSite(PIT).isValid());
    // Expanding orders have no start-up time
    BOOST_TEST(findOrder(PIT) == -1);
}

BOOST_FIXTURE_TEST_CASE(NeededOrderKeepsItsMaterial, PlannerFixture)
{
    WorldDescription desc = makeDesc();
    desc.workshops[0].cost = {{{"Boards"}, 5}};
    // 3 boards: not enough for the pit
    startGame(desc, 3);
    placeDeposits();
    startAI();
    AIJH::AIConstruction& construction = static_cast<AIJH::AIPlayerJH&>(*ai).GetConstruction();
    BOOST_TEST_REQUIRE(construction.HasMaterialFor(BuildingType::Woodcutter));
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    step();
    const civai::Order& pit = getOrder(PIT);
    BOOST_TEST((pit.state == civai::OrderState::Blocked));
    BOOST_TEST_INFO(pit.reason);
    BOOST_TEST(pit.reason.find("Baumaterial fehlt") == 0u);
    // No trees: no way to more boards, both reasons are shown
    BOOST_TEST(pit.reason.find("kein Weg zu") != std::string::npos);
    // The 3 boards are kept for the pit: the old AI cannot use them for other buildings (e.g. military buildings)
    BOOST_TEST(!construction.HasMaterialFor(BuildingType::Woodcutter));
    BOOST_TEST(world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites().empty());
    // Enough boards: placed
    hq().AddToInventory(GoodCounts::make(GoodType::Boards, 2), true);
    step();
    BOOST_TEST((getOrder(PIT).state == civai::OrderState::Placed));
    BOOST_TEST(findSite(PIT).isValid());
}

BOOST_FIXTURE_TEST_CASE(ClearingWoodcutterNeedsEnoughTrees, PlannerFixture)
{
    // Woodcutters expand only where at least CLEARING_MIN_TREES trees are in reach
    startGame(makeDesc({"Woodcutter"}));
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 2), true);
    placeTrees(Position(-5, 3), civai::EconomyPlanner::CLEARING_MIN_TREES - 1u);
    startAI();
    step();
    BOOST_TEST(!findSite(BuildingType::Woodcutter).isValid());
    const civai::Order& order = getOrder(BuildingType::Woodcutter);
    BOOST_TEST((order.kind == civai::OrderKind::Expanding));
    BOOST_TEST(order.clearing);
    BOOST_TEST((order.state == civai::OrderState::Blocked));
    BOOST_TEST(order.reason == "kein Bauplatz");
}

BOOST_FIXTURE_TEST_CASE(ClearingWoodcutterIsPlacedAtTheForest, PlannerFixture)
{
    startGame(makeDesc({"Woodcutter"}));
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 2), true);
    placeTrees(Position(-5, 3), civai::EconomyPlanner::CLEARING_MIN_TREES + 2u);
    startAI();
    step();
    const MapPoint pos = findSite(BuildingType::Woodcutter);
    BOOST_TEST_REQUIRE(pos.isValid());
    BOOST_TEST(planner()->IsClearingWoodcutter(pos));
    BOOST_TEST(civai::EconomyPlanner::CountTrees(world.GetPlayer(curPlayer), pos, 6)
               >= civai::EconomyPlanner::CLEARING_MIN_TREES);
}

BOOST_FIXTURE_TEST_CASE(CountsTrees, PlannerFixture)
{
    startGame(makeDesc());
    const MapPoint center = world.MakeMapPoint(hqPos + Position(-5, 3));
    const unsigned before = civai::EconomyPlanner::CountTrees(world.GetPlayer(curPlayer), center, 2);
    placeTrees(Position(-5, 3), 4);
    BOOST_TEST(civai::EconomyPlanner::CountTrees(world.GetPlayer(curPlayer), center, 2) == before + 4u);
}

BOOST_FIXTURE_TEST_CASE(OrdersOneToolOfEachKind, PlannerFixture)
{
    ggs.setSelection(AddonId::TOOL_ORDERING, 1);
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(GoodType::Hammer, 3), true);
    startAI();
    step();
    const GamePlayer& player = world.GetPlayer(curPlayer);
    for(const auto tool : helpers::enumRange<Tool>())
    {
        BOOST_TEST_INFO(static_cast<unsigned>(tool));
        // One more than asked for (nothing is asked for)
        if(tool == Tool::Hammer)
            BOOST_TEST(player.GetToolsOrdered(tool) == 0u);
        else
            BOOST_TEST(player.GetToolsOrdered(tool) == 1u);
        // The regulator of the distribution stays at 0
        BOOST_TEST(player.GetToolPriority(tool) == 0u);
    }
    // Ordered already: not again
    step();
    for(const auto tool : helpers::enumRange<Tool>())
        BOOST_TEST(player.GetToolsOrdered(tool) <= 1u);
}

BOOST_AUTO_TEST_SUITE_END()
