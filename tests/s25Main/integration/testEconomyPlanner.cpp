// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "EconomyStatistics.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "SerializedGameData.h"
#include "addons/const_addons.h"
#include "ai/AIPlayer.h"
#include "ai/aijh/AIConstruction.h"
#include "ai/aijh/AIPlayerJH.h"
#include "ai/civ/EconomyPlanner.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "buildings/nobUsual.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "civ/WareDistribution.h"
#include "factories/AIFactory.h"
#include "factories/BuildingFactory.h"
#include "figures/nofFarmhand.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "nodeObjs/noDeposit.h"
#include "nodeObjs/noGranite.h"
#include "nodeObjs/noTree.h"
#include "gameTypes/Inventory.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/ToolConsts.h"
#include "gameData/WorldDescription.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <cmath>
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
    desc.worker.look = "Stonemason";
    desc.workTime = 10;
    return desc;
}

/// Loam (ware 0) collected from deposits (map object 0) by the pit (workshop 0), clay (ware 1) made of loam by the
/// kiln (workshop 1). The technology "pottery" costs 5 clay (or `techCost`). `expanding`: the list of ai.lua
WorldDescription makeDesc(const std::vector<std::string>& expanding = {},
                          const decltype(TechnologyDesc::cost)& techCost = {{"Clay", 5}})
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
    pit.outputs = {{"Loam"}};
    desc.workshops.push_back(pit);
    WorkshopDesc kiln = makeWorkshop("Kiln");
    kiln.inputs = {InputDesc{{"Loam"}}};
    kiln.outputs = {{"Clay"}};
    desc.workshops.push_back(kiln);
    TechnologyDesc tech;
    tech.name = "pottery";
    tech.title = "Pottery";
    tech.cost = techCost;
    desc.technologies.add(tech);
    desc.aiExpandingBuildings = expanding;
    return desc;
}

constexpr GoodType LOAM = GoodType::Custom0;
/// Trees of a forest a woodcutter is placed at by the rules of the old AI (without a forester it needs a wood density
/// of 8 % in radius 7)
constexpr unsigned FOREST = 18;
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
    /// Let the game and the AI run
    void runAI(unsigned numGFs)
    {
        for(unsigned gf = 0; gf < numGFs; gf++)
        {
            em.ExecuteNextGF();
            ai->RunGF(em.GetCurrentGF(), gf % 5 == 0);
            if(gf % 5 == 4)
                executeCommands();
        }
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
    /// The construction costs of a ware alone of the orders that are not placed and not blocked (waiting for
    /// resources is no block): what the orders of the list plan of it (a blocked order counts for nothing)
    double costsOfUnblockedOrders(GoodType good)
    {
        double result = 0;
        for(const civai::Order* order : list())
        {
            if(!civai::EconomyPlanner::IsPlanned(*order))
                continue;
            for(const CostEntry& entry : BUILDING_COSTS[order->building])
            {
                if(entry.wares.size() == 1u && entry.wares.front() == good)
                    result += entry.amount;
            }
        }
        return result;
    }
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
    BOOST_TEST((kiln.cause == civai::OrderCause::Shortage));
    BOOST_TEST((kiln.forWare == CLAY));
    BOOST_TEST((kiln.usesWare == GoodType::Nothing));
    BOOST_TEST((kiln.state == civai::OrderState::Waiting));
    BOOST_TEST(kiln.reason.find("Eingang fehlt") == 0u);
    const civai::Order& pit = getOrder(PIT);
    BOOST_TEST(pit.parent == kiln.id);
    BOOST_TEST((pit.cause == civai::OrderCause::Input));
    BOOST_TEST((pit.kind == civai::OrderKind::Needed));
    BOOST_TEST((pit.forWare == LOAM));
    BOOST_TEST(findOrder(PIT) < findOrder(KILN));

    // Next step: the pit is placed. The kiln follows at once, beside the site of the pit (not once the pit works)
    step();
    BOOST_TEST((getOrder(PIT).state == civai::OrderState::Placed));
    const MapPoint pitPos = findSite(PIT);
    BOOST_TEST_REQUIRE(pitPos.isValid());
    if(!findSite(KILN).isValid())
        step();
    const MapPoint kilnPos = findSite(KILN);
    BOOST_TEST_REQUIRE(kilnPos.isValid());
    BOOST_TEST(world.CalcDistance(pitPos, kilnPos) <= 4u);
    // What the sites will make counts: the clay is not short anymore, no second kiln
    step();
    step();
    BOOST_TEST((ware(CLAY).state != civai::SupplyState::Shortage));
    unsigned numKilns = 0;
    for(const noBuildingSite* site : world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites())
        numKilns += site->GetBuildingType() == KILN ? 1u : 0u;
    BOOST_TEST(numKilns == 1u);
}

BOOST_FIXTURE_TEST_CASE(EnoughStockIsNoShortage, PlannerFixture)
{
    // The research asks for 5 clay, 40 are there: the forecast (stock minus requested, plus what will be made, minus
    // what will be used) is not negative. No shortage, no kiln
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(CLAY, 40), true);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    BOOST_TEST(ware(CLAY).openDemand == 5u);
    BOOST_TEST(ware(CLAY).forecast == 35.);
    BOOST_TEST((ware(CLAY).state != civai::SupplyState::Shortage));
    BOOST_TEST(findOrder(KILN) == -1);
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

BOOST_FIXTURE_TEST_CASE(SurplusOfANormalProductIsExpanding, PlannerFixture)
{
    // 20 loam and nobody uses it: surplus (more than the minimum stock of 8). Clay is normal (no research): no problem,
    // the kiln may only expand, it is in the list of ai.lua. No boards: it stays in the list
    startGame(makeDesc({"Kiln"}), 0);
    hq().AddToInventory(GoodCounts::make(LOAM, 20), true);
    startAI();
    step();
    BOOST_TEST((ware(LOAM).state == civai::SupplyState::Surplus));
    BOOST_TEST((ware(CLAY).state == civai::SupplyState::Normal));
    const civai::Order& kiln = getOrder(KILN);
    BOOST_TEST((kiln.kind == civai::OrderKind::Expanding));
    BOOST_TEST((kiln.forWare == CLAY));
    BOOST_TEST((kiln.usesWare == LOAM));
    BOOST_TEST_INFO(kiln.reason);
    BOOST_TEST(kiln.reason.find("Boards fehlt") == 0u);
    // With boards it is placed and leaves the list
    hq().AddToInventory(GoodCounts::make(GoodType::Boards, 2), true);
    step();
    BOOST_TEST(findSite(KILN).isValid());
}

BOOST_FIXTURE_TEST_CASE(SurplusOfANormalProductNotInTheList, PlannerFixture)
{
    // The same without the kiln in the list of ai.lua: nothing
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(LOAM, 20), true);
    startAI();
    step();
    BOOST_TEST((ware(LOAM).state == civai::SupplyState::Surplus));
    BOOST_TEST(findOrder(KILN) == -1);
}

BOOST_FIXTURE_TEST_CASE(SurplusIsUsedForAShortProduct, PlannerFixture)
{
    // Clay is short (research), loam in surplus: the needed kiln is the producer. The surplus is there for it: no pit
    // (no deposits anyway), the kiln is placed
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(LOAM, 20), true);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    const civai::Order& kiln = getOrder(KILN);
    BOOST_TEST((kiln.kind == civai::OrderKind::Needed));
    BOOST_TEST((kiln.cause == civai::OrderCause::Shortage));
    BOOST_TEST((kiln.state == civai::OrderState::Placed));
    BOOST_TEST(findSite(KILN).isValid());
}

BOOST_FIXTURE_TEST_CASE(ExpandOnlyWithTheBuildingsOfTheList, PlannerFixture)
{
    // Clay is in normal state (none there, none used, the research costs it). The kiln is not in the list of ai.lua:
    // no order
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(LOAM, 8), true);
    startAI();
    step();
    BOOST_TEST((ware(CLAY).state == civai::SupplyState::Normal));
    BOOST_TEST(findOrder(KILN) == -1);
    BOOST_TEST(!findSite(KILN).isValid());
}

BOOST_FIXTURE_TEST_CASE(HarvestersOfDepositsExpandWithoutTheList, PlannerFixture)
{
    // The pit collects deposits (map objects of lua): like woodcutters and quarries it clears them, independent of the
    // stock and of the list of ai.lua
    startGame(makeDesc(), 0);
    placeDeposits();
    startAI();
    step();
    const civai::Order& pit = getOrder(PIT);
    BOOST_TEST((pit.kind == civai::OrderKind::Expanding));
    BOOST_TEST((pit.cause == civai::OrderCause::Deposits));
    hq().AddToInventory(GoodCounts::make(GoodType::Boards, 2), true);
    step();
    BOOST_TEST(findSite(PIT).isValid());
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

BOOST_FIXTURE_TEST_CASE(FullStorageAllowsOnlyNeededOrders, PlannerFixture)
{
    // The HQ holds 85 wares and has 80 (40 boards, 40 stones): more than 90 %. The pit (expanding) must wait, a
    // storehouse is ordered
    WorldDescription desc = makeDesc({"Pit"});
    desc.warehouseCapacity = {{"Headquarters", 85}};
    startGame(desc);
    placeDeposits();
    startAI();
    step();
    BOOST_TEST(planner()->IsStorageFull());
    const civai::Order& pit = getOrder(PIT);
    BOOST_TEST_INFO(pit.reason);
    BOOST_TEST(pit.reason.find("Lager voll (80 von 85)") == 0u);
    BOOST_TEST(!findSite(PIT).isValid());
    // Its place is searched by the rules of the old AI (no other warehouse within 15 of the center): none in this
    // small world
    const civai::Order& storehouse = getOrder(BuildingType::Storehouse);
    BOOST_TEST((storehouse.kind == civai::OrderKind::Needed));
    BOOST_TEST((storehouse.cause == civai::OrderCause::Storage));
}

BOOST_FIXTURE_TEST_CASE(NoPlaceOrdersAMilitaryBuilding, PlannerFixture)
{
    // The storage is full, the storehouse finds no place (15 away from the HQ is outside the territory): a needed
    // military building is ordered for it (Schritt 3). It is placed at the border once a soldier is free. Without a
    // problem one expanding military building is in the list too
    WorldDescription desc = makeDesc({"Pit"});
    desc.warehouseCapacity = {{"Headquarters", 85}};
    startGame(desc);
    hq().AddToInventory(PeopleCounts::make(Job::Private, 5), true);
    placeDeposits();
    startAI();
    step();
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    const civai::Order& storehouse = getOrder(BuildingType::Storehouse);
    BOOST_TEST_REQUIRE((storehouse.state == civai::OrderState::Blocked));
    BOOST_TEST(storehouse.reason.find("kein Bauplatz") == 0u);
    const unsigned storehouseId = storehouse.id;
    const auto isFor = [&](const civai::Order* order) {
        return order->cause == civai::OrderCause::Territory && order->parent == storehouseId;
    };
    const std::vector<const civai::Order*> orders = list();
    const auto military = std::find_if(orders.begin(), orders.end(), isFor);
    BOOST_TEST_REQUIRE((military != orders.end()));
    const civai::Order& militaryOrder = **military;
    BOOST_TEST((militaryOrder.kind == civai::OrderKind::Needed));
    BOOST_TEST((militaryOrder.territory == civai::TerritoryNeed::Place));
    BOOST_TEST(BuildingProperties::IsMilitary(militaryOrder.building));
    BOOST_TEST(helpers::contains_if(orders, [](const civai::Order* order) {
        return order->cause == civai::OrderCause::Territory && order->territory == civai::TerritoryNeed::Expansion
               && order->kind == civai::OrderKind::Expanding;
    }));
    step();
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    bool placed = false;
    for(const noBuildingSite* site : world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites())
        placed |= BuildingProperties::IsMilitary(site->GetBuildingType());
    BOOST_TEST(placed);
}

BOOST_FIXTURE_TEST_CASE(PlansWithAWarehouseFromLua, PlannerFixture)
{
    // A warehouse defined in lua (workshop 2) is no production building: the AI plans around it
    WorldDescription desc = makeDesc({"Pit"});
    WorkshopDesc shed;
    shed.name = "Shed";
    shed.title = "ShedTitle";
    shed.size = "small";
    shed.cost = {{{"Boards"}, 2}};
    shed.look = "Storehouse";
    shed.warehouseCapacity = 50;
    shed.warehouseWares = {{"Boards"}};
    desc.workshops.push_back(shed);
    startGame(desc);
    placeDeposits();
    BuildingFactory::CreateBuilding(world, getCustomBuilding(2), world.MakeMapPoint(hqPos + Position(-4, 2)), curPlayer,
                                    Nation::Romans);
    startAI();
    BOOST_TEST(!planner()->IsManaged(getCustomBuilding(2)));
    for(unsigned i = 0; i < 5; i++)
        step();
    runAI(600);
    BOOST_TEST(findSite(PIT).isValid());
    BOOST_TEST(findOrder(getCustomBuilding(2)) == -1);
}

BOOST_FIXTURE_TEST_CASE(BuildingLimitDependsOnTheLand, PlannerFixture)
{
    // Limit base 0 + 1 per area of 331 nodes (the land of the HQ): one pit
    WorldDescription desc = makeDesc({"Pit"});
    desc.aiBuildingLimitBase = 0;
    desc.aiBuildingLimitPerArea = 1;
    desc.aiBuildingLimitAreaSize = 331;
    startGame(desc);
    placeDeposits();
    placeDeposits(Position(-4, 4));
    startAI();
    BOOST_TEST(planner()->GetBuildingLimit() == 1u);
    step();
    BOOST_TEST_REQUIRE(findSite(PIT).isValid());
    step();
    const civai::Order& order = getOrder(PIT);
    BOOST_TEST_INFO(order.reason);
    BOOST_TEST(order.reason == "Höchstzahl erreicht: 1 von 1");
    BOOST_TEST(world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites().size() == 1u);
}

BOOST_FIXTURE_TEST_CASE(BuildingLimitCountsOnlyWholeAreas, PlannerFixture)
{
    // Less land than one area: only the base counts (no "at least 1 area")
    WorldDescription desc = makeDesc({"Pit"});
    desc.aiBuildingLimitBase = 2;
    desc.aiBuildingLimitPerArea = 3;
    desc.aiBuildingLimitAreaSize = 100000;
    startGame(desc);
    startAI();
    BOOST_TEST(planner()->GetBuildingLimit() == 2u);
}

BOOST_FIXTURE_TEST_CASE(OneExpandingOrderPerBuildingType, PlannerFixture)
{
    // The carver makes two knowledge media, both in normal state, and is in the list of ai.lua: one expanding order,
    // not one per medium
    WorldDescription desc = makeDesc({"Carver"});
    for(unsigned i = 0; i < 2; i++)
    {
        WareDesc ware = makeWare(i == 0 ? "CarvedL" : "CarvedI");
        ware.kind = {"Carved"};
        ware.knowledge[i] = 1;
        desc.wares.push_back(ware);
    }
    WorkshopDesc carver = makeWorkshop("Carver");
    carver.outputs = {{"Carved"}};
    carver.outputChoice = "knowledge";
    desc.workshops.push_back(carver);
    startGame(desc);
    startAI();
    step();
    // Placed right away (expanding orders leave the list): one site, not two
    unsigned numCarvers = 0;
    for(const noBuildingSite* site : world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites())
        numCarvers += site->GetBuildingType() == getCustomBuilding(2) ? 1 : 0;
    BOOST_TEST(numCarvers == 1u);
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
    // Exactly what is missing and why
    BOOST_TEST(pit.reason.find("Boards fehlt: 5 nötig, 3 frei (Lager 3") == 0u);
    // It waits for material (no block): its costs are planned. No trees: no way to more boards, both reasons are shown
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST(civai::EconomyPlanner::WaitsForResources(pit));
    BOOST_TEST(ware(GoodType::Boards).planned == costsOfUnblockedOrders(GoodType::Boards));
    BOOST_TEST(ware(GoodType::Boards).planned >= 5.);
    BOOST_TEST(pit.reason.find("kein Weg zu") != std::string::npos);
    // The sawmill for the boards is blocked (no way to wood): it counts for nothing (Nutzer, 2026-11-17), its stones
    // are not planned
    const civai::Order& sawmill = getOrder(BuildingType::Sawmill);
    BOOST_TEST((sawmill.state == civai::OrderState::Blocked));
    BOOST_TEST((sawmill.retry.kind == civai::RetryCondition::Kind::NoWay));
    BOOST_TEST(!civai::EconomyPlanner::IsPlanned(sawmill));
    BOOST_TEST(ware(GoodType::Stones).plannedByOrders == costsOfUnblockedOrders(GoodType::Stones));
    BOOST_TEST(ware(GoodType::Stones).plannedByOrders == 0.);
    // The 3 boards are kept for the pit: the old AI cannot use them for other buildings (e.g. military buildings)
    BOOST_TEST(!construction.HasMaterialFor(BuildingType::Woodcutter));
    BOOST_TEST(world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites().empty());
    // Enough boards: placed
    hq().AddToInventory(GoodCounts::make(GoodType::Boards, 2), true);
    step();
    BOOST_TEST((getOrder(PIT).state == civai::OrderState::Placed));
    BOOST_TEST(findSite(PIT).isValid());
}

BOOST_FIXTURE_TEST_CASE(ShortagesAreEvaluatedWithTheBuildingMaterialFactor, PlannerFixture)
{
    // The pit costs 8 boards, 7 are in stock: it waits. Planned: 12 boards (the pit, the kiln waiting for it and a
    // barracks waiting for soldiers). Forecast 7 - 12 = -5: shortage. The evaluation of shortages counts all
    // construction costs x BUILDING_MATERIAL_FACTOR (0.5): 7 - 6 = 1, no shortage, no sawmill (Nutzer, 2026-11-21).
    // The forecast itself stays (report, list, distribution of all wares)
    WorldDescription desc = makeDesc();
    desc.workshops[0].cost = {{{"Boards"}, 8}};
    startGame(desc, 7);
    placeDeposits();
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST_REQUIRE(civai::EconomyPlanner::WaitsForResources(getOrder(PIT)));
    const civai::WareStatus& boards = ware(GoodType::Boards);
    BOOST_TEST(boards.planned == costsOfUnblockedOrders(GoodType::Boards));
    BOOST_TEST(boards.planned == 12.);
    BOOST_TEST(std::abs(boards.forecast - (7. - boards.openDemand - boards.planned)) < 0.01);
    constexpr double factor = civai::EconomyPlanner::BUILDING_MATERIAL_FACTOR;
    BOOST_TEST(std::abs(boards.evaluated - (7. - factor * (boards.openDemand + boards.planned))) < 0.01);
    BOOST_TEST((boards.state == civai::SupplyState::Shortage));
    BOOST_TEST(!boards.evaluatedShortage);
    BOOST_TEST(findOrder(BuildingType::Sawmill) == -1);
    // The distribution of all wares counts the construction costs fully: its forecast is the one of the planner
    civ::updateDistribution(world, curPlayer);
    BOOST_TEST(world.GetPlayer(curPlayer).GetWareDistribution().GetNumber(GoodType::Boards)
               == std::llround(boards.forecast * 100));
}

BOOST_FIXTURE_TEST_CASE(PlannedDemandGoesToTheGame, PlannerFixture)
{
    // The pit waits for its boards: its construction costs are planned. The AI sends them to the game, the distribution
    // of all wares counts them fully: its forecast is the one of the planner (Nutzer, 2026-11-21)
    WorldDescription desc = makeDesc();
    desc.workshops[0].cost = {{{"Boards"}, 5}};
    startGame(desc, 3);
    placeDeposits();
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    step();
    BOOST_TEST_REQUIRE(civai::EconomyPlanner::WaitsForResources(getOrder(PIT)));
    civ::WareDistribution& distribution = world.GetPlayer(curPlayer).GetWareDistribution();
    unsigned boards = 0;
    for(const civ::PlannedDemand& demand : distribution.GetPlanned())
    {
        if(demand.wares == std::vector<GoodType>{GoodType::Boards})
            boards += demand.amount;
    }
    BOOST_TEST(boards == ware(GoodType::Boards).plannedByOrders);
    BOOST_TEST(boards >= 5u);
    BOOST_TEST(distribution.UsesForecast());
    civ::updateDistribution(world, curPlayer);
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST(distribution.GetNumber(GoodType::Boards) == std::llround(ware(GoodType::Boards).forecast * 100));
    BOOST_TEST(distribution.GetNumber(GoodType::Boards) < 0);
}

BOOST_FIXTURE_TEST_CASE(ClearingWoodcutterNeedsEnoughTrees, PlannerFixture)
{
    // Clearing woodcutters come where at least CLEARING_MIN_TREES trees are
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 2), true);
    placeTrees(Position(-5, 3), civai::EconomyPlanner::CLEARING_MIN_TREES - 1u);
    startAI();
    step();
    BOOST_TEST(findOrder(BuildingType::Woodcutter) == -1);
    BOOST_TEST(!findSite(BuildingType::Woodcutter).isValid());
}

BOOST_FIXTURE_TEST_CASE(ClearingWoodcutterDespiteWoodInStock, PlannerFixture)
{
    // Lots of wood in stock does not matter: the trees on the map do (and the woodcutter is no part of ai.lua here)
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(GoodType::Wood, 40), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 2), true);
    placeTrees(Position(-5, 3), FOREST);
    startAI();
    step();
    const MapPoint pos = findSite(BuildingType::Woodcutter);
    BOOST_TEST_REQUIRE(pos.isValid());
    BOOST_TEST(planner()->IsClearingWoodcutter(pos));
    // The mark is a game command, kept in the savegame
    BOOST_TEST(world.GetPlayer(curPlayer).IsClearingWoodcutter(pos));
}

BOOST_FIXTURE_TEST_CASE(QuarryAtFewStonePiles, PlannerFixture)
{
    // MIN_DEPOSITS_IN_REACH stone piles are enough for a quarry
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(GoodType::PickAxe, 2), true);
    for(unsigned i = 0; i < civai::EconomyPlanner::MIN_DEPOSITS_IN_REACH; ++i)
        placeObject(Position(-4, 4 + 2 * static_cast<int>(i)),
                    [](MapPoint) { return new noGranite(GraniteType::One, 5); });
    startAI();
    step();
    // Stones in stock (40) do not matter
    const MapPoint pos = findSite(BuildingType::Quarry);
    BOOST_TEST_REQUIRE(pos.isValid());
    BOOST_TEST(civai::EconomyPlanner::CountStonePiles(world.GetPlayer(curPlayer), pos, 8)
               >= civai::EconomyPlanner::MIN_DEPOSITS_IN_REACH);
}

BOOST_FIXTURE_TEST_CASE(NoOrdersForInputsOfABuildingThatCannotWork, PlannerFixture)
{
    // The glassworks needs loam and sand. Loam can be made (pit at the deposits), sand not: no pit for it (like no
    // well for a brewery without grain)
    WorldDescription desc = makeDesc();
    desc.wares.push_back(makeWare("Sand"));
    desc.wares.push_back(makeWare("Glass"));
    WorkshopDesc glassworks = makeWorkshop("Glassworks");
    glassworks.inputs = {InputDesc{{"Loam"}}, InputDesc{{"Sand"}}};
    glassworks.outputs = {{"Glass"}};
    desc.workshops.push_back(glassworks);
    TechnologyDesc tech;
    tech.name = "windows";
    tech.title = "Windows";
    tech.cost = {{"Glass", 2}};
    desc.technologies.add(tech);
    startGame(desc);
    placeDeposits();
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 1));
    step();
    step();
    const civai::Order& order = getOrder(getCustomBuilding(2));
    BOOST_TEST((order.state == civai::OrderState::Blocked));
    BOOST_TEST_INFO(order.reason);
    BOOST_TEST(order.reason.find("kein Weg zu SandTitle") != std::string::npos);
    // No order for its loam (the pit may come as harvester of the deposits, expanding)
    const unsigned glassworksId = order.id;
    for(const civai::Order* other : list())
    {
        BOOST_TEST(other->parent != glassworksId);
        if(other->building == PIT)
            BOOST_TEST((other->kind == civai::OrderKind::Expanding));
    }
}

namespace {
/// Clay is short (research): the kiln is needed, but the 8 loam do not suffice for it (it works fast). The brickworks
/// (expanding, slow) would get by with them and comes below the kiln
WorldDescription makeBrickworksDesc()
{
    WorldDescription desc = makeDesc({"Brickworks"});
    desc.wares.push_back(makeWare("Brick"));
    WorkshopDesc brickworks = makeWorkshop("Brickworks");
    brickworks.inputs = {InputDesc{{"Loam"}}};
    brickworks.outputs = {{"Brick"}};
    brickworks.workTime = 200;
    desc.workshops.push_back(brickworks);
    TechnologyDesc tech;
    tech.name = "walls";
    tech.title = "Walls";
    tech.cost = {{"Brick", 1}};
    desc.technologies.add(tech);
    return desc;
}
} // namespace

BOOST_FIXTURE_TEST_CASE(MissingInputIsKeptForTheOrderAbove, PlannerFixture)
{
    // Loam can be made (deposits): the kiln waits for it, the loam that is there is kept for the kiln
    startGame(makeBrickworksDesc());
    hq().AddToInventory(GoodCounts::make(LOAM, 8), true);
    placeDeposits();
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    const int kiln = findOrder(KILN);
    BOOST_TEST_REQUIRE(kiln >= 0);
    BOOST_TEST(list()[kiln]->inputsMissing == std::vector<GoodType>{LOAM});
    const civai::Order& order = getOrder(getCustomBuilding(2));
    BOOST_TEST_INFO(order.reason);
    BOOST_TEST(order.reason == "LoamTitle vorbehalten für Kiln");
    BOOST_TEST(!findSite(getCustomBuilding(2)).isValid());
}

BOOST_FIXTURE_TEST_CASE(NoInputIsKeptForAnOrderThatCannotBeBuilt, PlannerFixture)
{
    // No loam can be made: the kiln cannot be built, it keeps no loam (otherwise it would block the brickworks for
    // good)
    startGame(makeBrickworksDesc());
    hq().AddToInventory(GoodCounts::make(LOAM, 8), true);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    const civai::Order& kiln = getOrder(KILN);
    BOOST_TEST((kiln.state == civai::OrderState::Blocked));
    BOOST_TEST_INFO(kiln.reason);
    BOOST_TEST(kiln.reason.find("kein Weg zu LoamTitle") == 0u);
    const int brickworks = findOrder(getCustomBuilding(2));
    if(brickworks >= 0)
    {
        BOOST_TEST_INFO(list()[brickworks]->reason);
        BOOST_TEST(list()[brickworks]->reason.find("vorbehalten") == std::string::npos);
    }
}

BOOST_FIXTURE_TEST_CASE(BasicEconomyComesFirst, PlannerFixture)
{
    // The research costs loam and clay. Loam is short first (pit), but the kiln is in the first level of the basic
    // economy: it comes first among the needed orders
    WorldDescription desc = makeDesc({}, {{"Loam", 5}, {"Clay", 5}});
    desc.aiBasicEconomy = {{"Kiln"}};
    startGame(desc);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    planner()->Step();
    const int kiln = findOrder(KILN), pit = findOrder(PIT);
    BOOST_TEST_REQUIRE(kiln >= 0);
    BOOST_TEST_REQUIRE(pit >= 0);
    BOOST_TEST(kiln < pit);
}

BOOST_FIXTURE_TEST_CASE(StartStockIsNoSurplus, PlannerFixture)
{
    WorldDescription desc = makeDesc();
    desc.aiStartStockNoSurplus = {{"Loam"}};
    startGame(desc);
    hq().AddToInventory(GoodCounts::make(LOAM, 20), true);
    startAI();
    step();
    BOOST_TEST(ware(LOAM).startStockLeft == 20u);
    BOOST_TEST((ware(LOAM).state == civai::SupplyState::Normal));
    BOOST_TEST(findOrder(KILN) == -1);
}

BOOST_FIXTURE_TEST_CASE(SitesPlanNoInputSpaces, PlannerFixture)
{
    startGame(makeDesc({"Kiln"}));
    hq().AddToInventory(GoodCounts::make(LOAM, 20), true);
    startAI();
    step();
    BOOST_TEST_REQUIRE(findSite(KILN).isValid());
    step();
    // The input spaces of the kiln sites are not planned (Nutzer, 2026-11-17): what the kilns will use counts as the
    // consumption of coming buildings (no kiln worked yet: by the work time)
    unsigned numSites = 0;
    for(const noBuildingSite* site : world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites())
        numSites += site->GetBuildingType() == KILN ? 1u : 0u;
    BOOST_TEST(numSites >= 1u);
    BOOST_TEST(ware(LOAM).planned == 0.);
    BOOST_TEST(ware(LOAM).free == 20.);
    BOOST_TEST(ware(LOAM).forecastConsumption > 0.);
    BOOST_TEST(ware(LOAM).inTransit == 0u);
}

BOOST_FIXTURE_TEST_CASE(SelfSolvingWoodcutterGetsTheBoards, PlannerFixture)
{
    // The research costs wood: a woodcutter is needed. 1 board, promised to a barracks site, and no boards made. The
    // woodcutter needs 2 and is part of the board chain (sawmill without wood): it is not blocked, the barracks site
    // (nothing built in yet) is cancelled for it
    startGame(makeDesc({}, {{"Wood", 5}}), 1);
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 2), true);
    placeTrees(Position(-5, 3), FOREST);
    startAI();
    // The site of the barracks is placed after the old AI ran (it would decide about it itself)
    MapPoint sitePos = MapPoint::Invalid();
    for(const Position offset : {Position(5, -3), Position(6, 2), Position(3, 5), Position(-3, -4)})
    {
        const MapPoint pt = world.MakeMapPoint(hqPos + offset);
        if(world.GetBQ(pt, curPlayer) >= BuildingQuality::Hut)
        {
            world.SetBuildingSite(BuildingType::Barracks, pt, curPlayer);
            sitePos = pt;
            break;
        }
    }
    BOOST_TEST_REQUIRE(sitePos.isValid());
    BOOST_TEST_REQUIRE(findSite(BuildingType::Barracks).isValid());
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    // The woodcutter for the wood of the research can work and makes what the boards lack (the sawmill has no wood):
    // priority
    step();
    {
        const civai::Order& order = getOrder(BuildingType::Woodcutter);
        BOOST_TEST((order.kind == civai::OrderKind::Needed));
        BOOST_TEST((order.cause == civai::OrderCause::Shortage));
        BOOST_TEST(!order.clearing);
        BOOST_TEST(order.selfSolving);
        BOOST_TEST_INFO(order.reason);
        BOOST_TEST(order.reason.find("Vorrang") == 0u);
        BOOST_TEST(order.reason.find("abgebrochen: Barracks") != std::string::npos);
        BOOST_TEST(!findSite(BuildingType::Barracks).isValid());
    }
    // The evaluation of shortages counts the construction costs with BUILDING_MATERIAL_FACTOR (0.5): at the start of
    // the step the board in stock covered half of the 2 boards of the barracks, no shortage of boards. Now the boards
    // of the woodcutter count: a sawmill, below the woodcutter. One woodcutter does not make what the research and the
    // sawmill need: the sawmill gets one more for its input
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST(findOrder(BuildingType::Woodcutter) < findOrder(BuildingType::Sawmill));
    const int sawmillId = static_cast<int>(getOrder(BuildingType::Sawmill).id);
    BOOST_TEST(helpers::contains_if(list(), [&](const civai::Order* other) {
        return other->building == BuildingType::Woodcutter && other->cause == civai::OrderCause::Input
               && static_cast<int>(other->parent) == sawmillId;
    }));
    // The board is free again, one more comes: the woodcutter gets them before anybody else
    hq().AddToInventory(GoodCounts::make(GoodType::Boards, 1), true);
    step();
    const MapPoint pos = findSite(BuildingType::Woodcutter);
    BOOST_TEST_REQUIRE(pos.isValid());
    BOOST_TEST(!planner()->IsClearingWoodcutter(pos));
}

BOOST_FIXTURE_TEST_CASE(ExpandingOrderNeverSolvesAProblem, PlannerFixture)
{
    // Without a need for wood: the clearing woodcutter is expanding. It lacks boards (1 of 2), but it solves no
    // problem: no priority, it waits at the end of the list. It waits for material (no block): its costs are
    // requested, the boards are short
    startGame(makeDesc(), 1);
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 2), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Wood, 40), true);
    placeTrees(Position(-5, 3), civai::EconomyPlanner::CLEARING_MIN_TREES + 2u);
    startAI();
    step();
    step();
    const civai::Order* clearing = nullptr;
    for(const civai::Order* order : list())
    {
        if(order->building == BuildingType::Woodcutter && order->clearing)
            clearing = order;
    }
    BOOST_TEST_REQUIRE(clearing);
    BOOST_TEST((clearing->kind == civai::OrderKind::Expanding));
    BOOST_TEST((clearing->cause == civai::OrderCause::Deposits));
    BOOST_TEST(!clearing->selfSolving);
    BOOST_TEST_INFO(clearing->reason);
    BOOST_TEST(clearing->reason.find("Boards fehlt") == 0u);
    BOOST_TEST(civai::EconomyPlanner::WaitsForResources(*clearing));
    BOOST_TEST(ware(GoodType::Boards).plannedByOrders == costsOfUnblockedOrders(GoodType::Boards));
    BOOST_TEST(ware(GoodType::Boards).plannedByOrders >= 2.);
    // All needed orders stand above it
    bool expandingSeen = false;
    for(const civai::Order* other : list())
    {
        if(other->kind == civai::OrderKind::Expanding)
            expandingSeen = true;
        else
            BOOST_TEST(!expandingSeen);
    }
}

BOOST_FIXTURE_TEST_CASE(ClearingMarkIsSaved, PlannerFixture)
{
    startGame(makeDesc());
    const MapPoint pt = world.MakeMapPoint(hqPos + Position(4, 2));
    world.SetBuildingSite(BuildingType::Woodcutter, pt, curPlayer);
    const MapPoint other = world.MakeMapPoint(hqPos + Position(-4, 2));
    // Only woodcutters are marked
    this->SetClearingWoodcutter(other, true);
    this->SetClearingWoodcutter(pt, true);
    BOOST_TEST(world.GetPlayer(curPlayer).GetClearingWoodcutters() == std::vector<MapPoint>{pt});

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    BOOST_TEST(world.GetPlayer(curPlayer).IsClearingWoodcutter(pt));

    this->SetClearingWoodcutter(pt, false);
    BOOST_TEST(world.GetPlayer(curPlayer).GetClearingWoodcutters().empty());
}

BOOST_FIXTURE_TEST_CASE(ClearingWoodcutterIsPlacedAtTheForest, PlannerFixture)
{
    startGame(makeDesc({"Woodcutter"}));
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 2), true);
    placeTrees(Position(-5, 3), FOREST);
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

BOOST_FIXTURE_TEST_CASE(NeededOrderReplacesTheExpandingOne, PlannerFixture)
{
    // The pit is ordered as harvester of the deposits (expanding) and then for the kiln (needed, clay is short): the
    // needed one replaces the expanding one (same building and ware). Needed orders ignore the limit per building type
    WorldDescription desc = makeDesc();
    desc.aiBuildingLimitBase = 0;
    desc.aiBuildingLimitPerArea = 0;
    startGame(desc);
    placeDeposits();
    startAI();
    BOOST_TEST(planner()->GetBuildingLimit() == 0u);
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    unsigned numPits = 0;
    for(const civai::Order* order : list())
    {
        if(order->building != PIT)
            continue;
        ++numPits;
        BOOST_TEST((order->kind == civai::OrderKind::Needed));
        BOOST_TEST((order->cause == civai::OrderCause::Input));
    }
    BOOST_TEST(numPits == 1u);
    // Placed although the limit is 0
    step();
    BOOST_TEST(findSite(PIT).isValid());
    BOOST_TEST((getOrder(PIT).state == civai::OrderState::Placed));
}

namespace {
/// The research asks for `amount` clay: amount / BALANCE_MINUTES per minute are missing (to be there within one balance
/// length).
/// `loam` in stock. Returns the number of kilns ordered for it
unsigned numKilnsForClay(PlannerFixture& fixture, const unsigned amount, const unsigned loam)
{
    fixture.startGame(makeDesc({}, {{"Clay", amount}}));
    fixture.hq().AddToInventory(GoodCounts::make(LOAM, loam), true);
    fixture.startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(fixture.world, fixture.curPlayer, 0));
    fixture.planner()->Step();
    unsigned numKilns = 0;
    for(const civai::Order* order : fixture.list())
    {
        if(order->building == KILN)
        {
            ++numKilns;
            BOOST_TEST((order->cause == civai::OrderCause::Shortage));
        }
    }
    return numKilns;
}
} // namespace

BOOST_FIXTURE_TEST_CASE(OneProducerForASmallDeficit, PlannerFixture)
{
    // 1 clay per minute: one kiln makes more than that by its work time (loam in surplus: its input suffices)
    BOOST_TEST(numKilnsForClay(*this, civai::EconomyPlanner::BALANCE_MINUTES, 100) == 1u);
}

BOOST_FIXTURE_TEST_CASE(ProducersByTheDeficit, PlannerFixture)
{
    // 10 clay per minute: more than one kiln makes by its work time
    BOOST_TEST(numKilnsForClay(*this, 10 * civai::EconomyPlanner::BALANCE_MINUTES, 100) > 1u);
}

BOOST_FIXTURE_TEST_CASE(OrdersOfATypeAreLimited, PlannerFixture)
{
    // Very much clay per minute: not more than MAX_ORDERS_PER_TYPE kilns at the same time
    const unsigned numKilns = numKilnsForClay(*this, 5000, 5000);
    BOOST_TEST(numKilns > 1u);
    BOOST_TEST(numKilns <= civai::EconomyPlanner::MAX_ORDERS_PER_TYPE);
}

BOOST_FIXTURE_TEST_CASE(OneProducerWaitsForItsInputs, PlannerFixture)
{
    // The same without loam: the input of the kiln does not suffice. One kiln waits for it (the loam is ordered for
    // it), more once it suffices
    BOOST_TEST(numKilnsForClay(*this, 50, 0) == 1u);
}

namespace {
/// Finished woodcutter near the trees placed before. Returns its position
MapPoint placeWoodcutter(PlannerFixture& fixture)
{
    for(const Position offset : {Position(-3, 5), Position(-6, 6), Position(-7, 1), Position(-2, 1), Position(-4, -1)})
    {
        const MapPoint pt = fixture.world.MakeMapPoint(fixture.hqPos + offset);
        if(fixture.world.GetBQ(pt, fixture.curPlayer) >= BuildingQuality::Hut)
        {
            BuildingFactory::CreateBuilding(fixture.world, BuildingType::Woodcutter, pt, fixture.curPlayer,
                                            Nation::Romans);
            return pt;
        }
    }
    return MapPoint::Invalid();
}
} // namespace

BOOST_FIXTURE_TEST_CASE(RenewerWhenTheSourceGetsFew, PlannerFixture)
{
    // A woodcutter for good with fewer than RENEW_BELOW trees in reach gets a forester near it
    startGame(makeDesc());
    placeTrees(Position(-5, 3), 5);
    const MapPoint pos = placeWoodcutter(*this);
    BOOST_TEST_REQUIRE(pos.isValid());
    const unsigned radius = nofFarmhand::GetWorkRadius(Job::Woodcutter);
    BOOST_TEST_REQUIRE(civai::EconomyPlanner::CountTrees(world.GetPlayer(curPlayer), pos, radius)
                       < civai::EconomyPlanner::RENEW_BELOW);
    startAI();
    planner()->Step();
    const civai::Order& forester = getOrder(BuildingType::Forester);
    BOOST_TEST((forester.cause == civai::OrderCause::Renewal));
    BOOST_TEST((forester.kind == civai::OrderKind::Needed));
    BOOST_TEST((forester.near == pos));
    // One per woodcutter at a time
    planner()->Step();
    unsigned numForesters = 0;
    for(const civai::Order* order : list())
        numForesters += order->building == BuildingType::Forester ? 1u : 0u;
    BOOST_TEST(numForesters == 1u);
}

BOOST_FIXTURE_TEST_CASE(NoRenewerWithEnoughOfTheSource, PlannerFixture)
{
    startGame(makeDesc());
    placeTrees(Position(-5, 3), 18);
    const MapPoint pos = placeWoodcutter(*this);
    BOOST_TEST_REQUIRE(pos.isValid());
    const unsigned radius = nofFarmhand::GetWorkRadius(Job::Woodcutter);
    BOOST_TEST_REQUIRE(civai::EconomyPlanner::CountTrees(world.GetPlayer(curPlayer), pos, radius)
                       >= civai::EconomyPlanner::RENEW_BELOW);
    startAI();
    step();
    BOOST_TEST(findOrder(BuildingType::Forester) == -1);
}

BOOST_FIXTURE_TEST_CASE(SoldiersAreAWare, PlannerFixture)
{
    // A barracks asks for soldiers, the warehouses have none: soldiers are short. Swords and shields are there, beer
    // not: the beer is needed for the soldiers (the brewery), not swords or shields
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(GoodType::Sword, 10), true);
    hq().AddToInventory(GoodCounts::make(GoodType::ShieldRomans, 10), true);
    MapPoint barracksPos = MapPoint::Invalid();
    for(const Position offset : {Position(5, -3), Position(6, 2), Position(3, 5), Position(-3, -4)})
    {
        const MapPoint pt = world.MakeMapPoint(hqPos + offset);
        if(world.GetBQ(pt, curPlayer) >= BuildingQuality::Hut)
        {
            BuildingFactory::CreateBuilding(world, BuildingType::Barracks, pt, curPlayer, Nation::Romans);
            barracksPos = pt;
            break;
        }
    }
    BOOST_TEST_REQUIRE(barracksPos.isValid());
    startAI();
    planner()->Step();
    const civai::WareStatus& soldiers = planner()->GetSoldierStatus();
    BOOST_TEST_INFO("Soldiers: stock " << soldiers.stock << ", open " << soldiers.openDemand);
    BOOST_TEST((soldiers.state == civai::SupplyState::Shortage));
    const civai::Order& brewery = getOrder(BuildingType::Brewery);
    BOOST_TEST((brewery.cause == civai::OrderCause::Soldiers));
    BOOST_TEST((brewery.forWare == GoodType::Beer));
    BOOST_TEST(findOrder(BuildingType::Armory) == -1);
}

BOOST_FIXTURE_TEST_CASE(ToolMakersByTheirCapacity, PlannerFixture)
{
    // Tools are missing (the stock of each kind) and nobody makes them: tool makers by their capacity, counted once
    // over all their products (Regel A), not one per tool
    ggs.setSelection(AddonId::TOOL_ORDERING, 1);
    startGame(makeDesc());
    startAI();
    step();
    step();
    unsigned numMakers = 0;
    for(const civai::Order* order : list())
    {
        if(order->building != BuildingType::Metalworks)
            continue;
        ++numMakers;
        BOOST_TEST((order->cause == civai::OrderCause::Shortage));
        BOOST_TEST((order->kind == civai::OrderKind::Needed));
    }
    BOOST_TEST(numMakers >= 1u);
    BOOST_TEST(numMakers <= civai::EconomyPlanner::MAX_ORDERS_PER_TYPE);
}

BOOST_FIXTURE_TEST_CASE(GroupsOfWaresAConsumerTakes, PlannerFixture)
{
    // The mixer takes sand or gravel (any of them): one group of both. Sand and gravel alone are no group a consumer
    // takes; the group has the stock of both
    WorldDescription desc = makeDesc();
    desc.wares.push_back(makeWare("Sand"));
    desc.wares.back().kind = {"Aggregate"};
    desc.wares.push_back(makeWare("Gravel"));
    desc.wares.back().kind = {"Aggregate"};
    desc.wares.push_back(makeWare("Concrete"));
    WorkshopDesc mixer = makeWorkshop("Mixer");
    // One input for both
    mixer.inputs = {InputDesc{{"Aggregate"}}};
    mixer.outputs = {{"Concrete"}};
    desc.workshops.push_back(mixer);
    startGame(desc);
    const GoodType sand = getCustomGood(2), gravel = getCustomGood(3);
    hq().AddToInventory(GoodCounts::make(sand, 5), true);
    hq().AddToInventory(GoodCounts::make(gravel, 6), true);
    startAI();
    step();
    const civai::WareGroup* both = planner()->FindGroup({sand, gravel});
    BOOST_TEST_REQUIRE(both);
    BOOST_TEST(both->demanded);
    BOOST_TEST(both->status.stock == 11u);
    const civai::WareGroup* sandAlone = planner()->FindGroup({sand});
    BOOST_TEST_REQUIRE(sandAlone);
    BOOST_TEST(!sandAlone->demanded);
    BOOST_TEST(sandAlone->status.stock == 5u);
}

BOOST_FIXTURE_TEST_CASE(NoOrdersForConstructionMaterial, PlannerFixture)
{
    // The pit (loam) and the mixer (mortar) lack boards, none are in stock. No orders are made for their construction
    // material (Nutzer, 2026-11-17): they wait for it, their costs are planned, the boards are short, one needed
    // sawmill is there for the shortage
    WorldDescription desc = makeDesc({}, {{"Loam", 5}, {"Mortar", 5}});
    desc.wares.push_back(makeWare("Mortar"));
    WorkshopDesc mixer = makeWorkshop("Mixer");
    mixer.outputs = {{"Mortar"}};
    desc.workshops.push_back(mixer);
    const BuildingType MIXER = getCustomBuilding(2);
    startGame(desc, 0);
    placeDeposits();
    placeTrees(Position(-5, 3), civai::EconomyPlanner::RENEW_BELOW + 1u);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    // The costs of the orders count from the step after they were made
    step();
    for(unsigned i = 0; i < 3; i++)
    {
        step();
        const unsigned pitId = getOrder(PIT).id;
        const unsigned mixerId = getOrder(MIXER).id;
        unsigned numSawmills = 0;
        for(const civai::Order* order : list())
        {
            BOOST_TEST(order->parent != pitId);
            BOOST_TEST(order->parent != mixerId);
            BOOST_TEST(!helpers::contains(order->alsoFor, pitId));
            BOOST_TEST(!helpers::contains(order->alsoFor, mixerId));
            if(order->building == BuildingType::Sawmill)
            {
                ++numSawmills;
                BOOST_TEST((order->cause == civai::OrderCause::Shortage));
                BOOST_TEST((order->kind == civai::OrderKind::Needed));
            }
        }
        BOOST_TEST(numSawmills == 1u);
        BOOST_TEST((ware(GoodType::Boards).state == civai::SupplyState::Shortage));
        BOOST_TEST(civai::EconomyPlanner::WaitsForResources(getOrder(PIT)));
        BOOST_TEST_INFO(planner()->GetReport());
        BOOST_TEST(ware(GoodType::Boards).plannedByOrders == costsOfUnblockedOrders(GoodType::Boards));
    }
}

namespace {
/// The hut costs 2 "walls" (loam or clay) and 2 "roof" (clay or straw), it makes hutwork. The research costs 5 hutwork
WorldDescription makeHutDesc()
{
    WorldDescription desc = makeDesc({}, {{"Hutwork", 5}});
    desc.wares[0].kind = {"Wall"};
    desc.wares[1].kind = {"Wall"};
    desc.wares[1].properties = {"Roof"};
    desc.wares.push_back(makeWare("Straw"));
    desc.wares.back().kind = {"Roof"};
    desc.wares.push_back(makeWare("Hutwork"));
    WorkshopDesc hut = makeWorkshop("Hut");
    hut.cost = {{{"Wall"}, 2}, {{"Roof"}, 2}};
    hut.outputs = {{"Hutwork"}};
    desc.workshops.push_back(hut);
    return desc;
}
const BuildingType HUT = getCustomBuilding(2);
const GoodType STRAW = getCustomGood(2);
} // namespace

BOOST_FIXTURE_TEST_CASE(GroupsSharingWaresCountTheStockOnce, PlannerFixture)
{
    // 3 clay are there: walls and roof alone would have enough (3 of 2), together they lack one. The material check
    // takes the entries one after another: the hut lacks material. It waits for it, its costs are planned and shared
    // out: the walls get 2 clay, the roof 1 and lacks one. There is no union of the two groups (Nutzer, 2026-11-17:
    // only groups a consumer takes)
    startGame(makeHutDesc());
    hq().AddToInventory(GoodCounts::make(CLAY, 3), true);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    const civai::Order& order = getOrder(HUT);
    BOOST_TEST_REQUIRE((order.cause == civai::OrderCause::Shortage));
    BOOST_TEST_REQUIRE(planner()->FindGroup({LOAM, CLAY}));
    BOOST_TEST_REQUIRE(planner()->FindGroup({CLAY, STRAW}));
    BOOST_TEST(!planner()->FindGroup({LOAM, CLAY, STRAW}));
    BOOST_TEST(!findSite(HUT).isValid());
    BOOST_TEST(order.reason.find("fehlt: 2 nötig, 1 frei") != std::string::npos);
    BOOST_TEST(civai::EconomyPlanner::WaitsForResources(order));
    BOOST_TEST(ware(CLAY).planned == 3.);
    BOOST_TEST(ware(CLAY).forecast == 0.);
    BOOST_TEST(planner()->FindGroup({LOAM, CLAY})->status.forecast == 0.);
    BOOST_TEST(planner()->FindGroup({CLAY, STRAW})->status.forecast == -1.);
}

BOOST_FIXTURE_TEST_CASE(EntriesAreSharedOutAmongTheirWares, PlannerFixture)
{
    // A hut site without a road asks for all of its costs: 2 walls (loam or clay), 2 roof (clay or straw). Each entry
    // takes its wares, the ones fewer other entries take first: the walls get 2 of the 3 clay, the roof the last one,
    // it lacks one. The clay is given to the entries, not free for anyone else (no surplus)
    startGame(makeHutDesc());
    hq().AddToInventory(GoodCounts::make(CLAY, 3), true);
    startAI();
    bool sitePlaced = false;
    for(const Position offset : {Position(5, -3), Position(6, 2), Position(3, 5), Position(-3, -4)})
    {
        const MapPoint pt = world.MakeMapPoint(hqPos + offset);
        if(world.GetBQ(pt, curPlayer) >= BuildingQuality::Hut)
        {
            world.SetBuildingSite(HUT, pt, curPlayer);
            sitePlaced = true;
            break;
        }
    }
    BOOST_TEST_REQUIRE(sitePlaced);
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    const civai::WareGroup* walls = planner()->FindGroup({LOAM, CLAY});
    const civai::WareGroup* roof = planner()->FindGroup({CLAY, STRAW});
    BOOST_TEST_REQUIRE(walls);
    BOOST_TEST_REQUIRE(roof);
    BOOST_TEST(!planner()->FindGroup({LOAM, CLAY, STRAW}));
    BOOST_TEST(ware(CLAY).openDemand == 3.);
    BOOST_TEST(ware(CLAY).forecast == 0.);
    BOOST_TEST((ware(CLAY).state == civai::SupplyState::Normal));
    BOOST_TEST(walls->status.forecast == 0.);
    BOOST_TEST(roof->status.forecast == -1.);
    BOOST_TEST(roof->status.missing == 1.);
    BOOST_TEST((roof->status.state == civai::SupplyState::Shortage));
    BOOST_TEST((walls->status.state == civai::SupplyState::Normal));
    // 20 clay: 16 left after both entries, 16 is the surplus of the clay (not 20)
    hq().AddToInventory(GoodCounts::make(CLAY, 17), true);
    step();
    // The groups are found again: new groups may have been added in the step
    walls = planner()->FindGroup({LOAM, CLAY});
    roof = planner()->FindGroup({CLAY, STRAW});
    BOOST_TEST_REQUIRE(walls);
    BOOST_TEST_REQUIRE(roof);
    BOOST_TEST(ware(CLAY).forecast == 16.);
    BOOST_TEST(walls->status.forecast == 16.);
    BOOST_TEST(roof->status.forecast == 16.);
    BOOST_TEST(roof->status.missing == 0.);
}

BOOST_FIXTURE_TEST_CASE(ExpandingSiteIsNoSolution, PlannerFixture)
{
    // A clearing woodcutter is placed (expanding). Then the research needs lots of wood: the clearing site does not
    // count as coming (an expanding order never solves a problem), a needed woodcutter is ordered
    startGame(makeDesc({}, {{"Wood", 200}}));
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 3), true);
    placeTrees(Position(-5, 3), FOREST);
    startAI();
    step();
    const MapPoint clearingPos = findSite(BuildingType::Woodcutter);
    BOOST_TEST_REQUIRE(clearingPos.isValid());
    BOOST_TEST_REQUIRE(planner()->IsClearingWoodcutter(clearingPos));
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    BOOST_TEST_REQUIRE((ware(GoodType::Wood).state == civai::SupplyState::Shortage));
    const bool needed = helpers::contains_if(list(), [](const civai::Order* order) {
        return order->building == BuildingType::Woodcutter && order->kind == civai::OrderKind::Needed
               && !order->clearing;
    });
    BOOST_TEST(needed);
}

BOOST_FIXTURE_TEST_CASE(HarvesterBesideItsRenewer, PlannerFixture)
{
    // Wood is short, no place has enough trees for a woodcutter for good (fewer than RENEW_BELOW, and too few for a
    // clearing one): a forester is ordered for it first, the woodcutter is placed beside its site
    startGame(makeDesc({}, {{"Wood", 5}}));
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 2), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Shovel, 2), true);
    placeTrees(Position(-5, 3), 4);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    const civai::Order& woodcutter = getOrder(BuildingType::Woodcutter);
    const unsigned woodcutterId = woodcutter.id;
    BOOST_TEST((woodcutter.kind == civai::OrderKind::Needed));
    BOOST_TEST((woodcutter.state == civai::OrderState::Waiting));
    BOOST_TEST_INFO(woodcutter.reason);
    BOOST_TEST(woodcutter.reason.find("kein Bauplatz mit") == 0u);
    const civai::Order& forester = getOrder(BuildingType::Forester);
    BOOST_TEST((forester.cause == civai::OrderCause::Renewal));
    BOOST_TEST(forester.parent == woodcutterId);
    BOOST_TEST(findOrder(BuildingType::Forester) < findOrder(BuildingType::Woodcutter));
    // The forester is placed in the same step (orders made while the list is worked on are worked on in the same
    // step), the woodcutter waits for its site. Then the woodcutter is placed beside the site
    const MapPoint foresterPos = findSite(BuildingType::Forester);
    BOOST_TEST_REQUIRE(foresterPos.isValid());
    BOOST_TEST(!findSite(BuildingType::Woodcutter).isValid());
    step();
    const MapPoint woodcutterPos = findSite(BuildingType::Woodcutter);
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST_REQUIRE(woodcutterPos.isValid());
    BOOST_TEST(world.CalcDistance(woodcutterPos, foresterPos) <= nofFarmhand::GetWorkRadius(Job::Forester));
    BOOST_TEST(!planner()->IsClearingWoodcutter(woodcutterPos));
    // The forester renews around the woodcutter now: no second one for it
    step();
    const auto isRenewerOf = [&](const civai::Order* order) {
        return order->building == BuildingType::Forester && order->near == woodcutterPos;
    };
    const std::vector<const civai::Order*> orders = list();
    BOOST_TEST(std::count_if(orders.begin(), orders.end(), isRenewerOf) == 1);
}

BOOST_FIXTURE_TEST_CASE(ToolsOfComingBuildingsAreAskedFor, PlannerFixture)
{
    // Wood is needed, a woodcutter is ordered. One axe is there: the woodcutter is placed. Its worker is recruited
    // with an axe: the site asks for it (planned) before the building is finished, with the ones the player keeps in
    // stock. They are ordered
    ggs.setSelection(AddonId::TOOL_ORDERING, 1);
    startGame(makeDesc({}, {{"Wood", 5}}));
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 1), true);
    placeTrees(Position(-5, 3), civai::EconomyPlanner::RENEW_BELOW + 1u);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST_REQUIRE(findSite(BuildingType::Woodcutter).isValid());
    step();
    const civai::WareGroup* axes = planner()->FindGroup({GoodType::Axe});
    BOOST_TEST_REQUIRE(axes);
    BOOST_TEST(axes->status.planned >= 1u + civai::EconomyPlanner::TOOLS_IN_STOCK);
    BOOST_TEST((axes->status.state == civai::SupplyState::Shortage));
}

BOOST_FIXTURE_TEST_CASE(ToolsOfWaitingOrdersAreAskedFor, PlannerFixture)
{
    // No axe: the woodcutter waits for its tool. Waiting for resources is no block: its axe is planned with the ones
    // kept in stock. They are ordered
    ggs.setSelection(AddonId::TOOL_ORDERING, 1);
    startGame(makeDesc({}, {{"Wood", 5}}));
    placeTrees(Position(-5, 3), civai::EconomyPlanner::RENEW_BELOW + 1u);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    const civai::Order& woodcutter = getOrder(BuildingType::Woodcutter);
    BOOST_TEST(civai::EconomyPlanner::WaitsForResources(woodcutter));
    const civai::WareGroup* axes = planner()->FindGroup({GoodType::Axe});
    BOOST_TEST_REQUIRE(axes);
    BOOST_TEST(axes->status.planned == 1u + civai::EconomyPlanner::TOOLS_IN_STOCK);
    BOOST_TEST(axes->status.plannedByOrders == 1.);
    BOOST_TEST((axes->status.state == civai::SupplyState::Shortage));
}

namespace {
/// makeDesc with the tool reform: the woodcutter works slowly without its axe (baseSpeed 25) or not at all (0)
WorldDescription makeToolDesc(const decltype(TechnologyDesc::cost)& techCost, const unsigned baseSpeed = 25)
{
    WorldDescription desc = makeDesc({}, techCost);
    ToolMaterialDesc iron;
    iron.name = "Iron";
    iron.title = "Eisen";
    desc.toolMaterials = {iron};
    desc.toolMaterialPriorities = {{"Iron", 5}};
    ToolDesc axe;
    axe.ware = "Axe";
    axe.material = "Iron";
    axe.durability = 100;
    axe.speed = 100;
    desc.tools = {axe};
    BuildingToolsDesc woodcutter;
    woodcutter.baseSpeed = baseSpeed;
    woodcutter.tools = {{"Axe"}};
    desc.buildingTools = {{"Woodcutter", woodcutter}};
    return desc;
}
} // namespace

BOOST_FIXTURE_TEST_CASE(SlowBuildingDoesNotWaitForItsTool, PlannerFixture)
{
    // Wood is needed. The woodcutter works slowly without its axe (base speed 25): it does not wait for it (Nutzer,
    // 2026-11-18). The axe is still asked for: no axe and no maker (no iron in the territory) -> the tool maker is
    // short and ordered
    startGame(makeToolDesc({{"Wood", 5}}));
    placeTrees(Position(-5, 3), FOREST);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST(findSite(BuildingType::Woodcutter).isValid());
    const civai::WareGroup* axes = planner()->FindGroup({GoodType::Axe});
    BOOST_TEST_REQUIRE(axes);
    BOOST_TEST(axes->status.planned >= 1.);
    BOOST_TEST(findOrder(BuildingType::Metalworks) >= 0);
}

BOOST_FIXTURE_TEST_CASE(ToolForecastDecidesForBuildingsNotWorkingWithout, PlannerFixture)
{
    // The woodcutter does not work without its axe (base speed 0): no axe and no maker -> the forecast of the axes does
    // not allow it, it waits. With an axe in stock it is placed
    startGame(makeToolDesc({{"Wood", 5}}, 0));
    placeTrees(Position(-5, 3), FOREST);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    step();
    const civai::Order& order = getOrder(BuildingType::Woodcutter);
    BOOST_TEST((order.state == civai::OrderState::Blocked));
    BOOST_TEST_INFO(order.reason);
    BOOST_TEST(order.reason.find("Werkzeug fehlt: Axe") == 0u);
    BOOST_TEST(civai::EconomyPlanner::WaitsForResources(order));
    BOOST_TEST(!findSite(BuildingType::Woodcutter).isValid());
    // The tool maker is short: it is ordered and shows why it cannot come
    BOOST_TEST(findOrder(BuildingType::Metalworks) >= 0);
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 1), true);
    step();
    BOOST_TEST(findSite(BuildingType::Woodcutter).isValid());
}

BOOST_FIXTURE_TEST_CASE(AToolOfAnySlotLetsItWork, PlannerFixture)
{
    // Base speed 0, slots for an axe and a saw: a saw in stock lets the woodcutter work, it does not wait for the axe.
    // The axe is still asked for
    WorldDescription desc = makeToolDesc({{"Wood", 5}}, 0);
    ToolDesc saw = desc.tools.front();
    saw.ware = "Saw";
    desc.tools.push_back(saw);
    desc.buildingTools["Woodcutter"].tools = {TagQuery{"Axe"}, TagQuery{"Saw"}};
    startGame(desc);
    placeTrees(Position(-5, 3), FOREST);
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST(findSite(BuildingType::Woodcutter).isValid());
    const civai::WareGroup* axes = planner()->FindGroup({GoodType::Axe});
    BOOST_TEST_REQUIRE(axes);
    BOOST_TEST(axes->status.planned >= 1.);
}

BOOST_FIXTURE_TEST_CASE(WaitingConsumersCountInTheForecast, PlannerFixture)
{
    // A kiln without loam (and without its worker yet): what it would use by its work time counts. Loam is short, a pit
    // is ordered for the shortage (no order "steht still")
    startGame(makeDesc());
    placeDeposits();
    MapPoint kilnPos = MapPoint::Invalid();
    for(const Position offset : {Position(5, -3), Position(6, 2), Position(3, 5), Position(-3, -4)})
    {
        const MapPoint pt = world.MakeMapPoint(hqPos + offset);
        if(world.GetBQ(pt, curPlayer) >= BuildingQuality::Hut)
        {
            BuildingFactory::CreateBuilding(world, KILN, pt, curPlayer, Nation::Romans);
            kilnPos = pt;
            break;
        }
    }
    BOOST_TEST_REQUIRE(kilnPos.isValid());
    startAI();
    step();
    BOOST_TEST(ware(LOAM).forecastConsumption > 0.);
    // The pits ordered for the shortage are placed (they are in the forecast now)
    const civai::Order& pit = getOrder(PIT);
    BOOST_TEST((pit.cause == civai::OrderCause::Shortage));
    BOOST_TEST((pit.kind == civai::OrderKind::Needed));
    BOOST_TEST((pit.state == civai::OrderState::Placed));
}

BOOST_FIXTURE_TEST_CASE(StoppedBuildingsCountWithTheirFullWork, PlannerFixture)
{
    // A kiln stopped for longer than a balance length (by the player; by the distribution of all wares the same, Nutzer
    // 2026-11-22). The research asks for clay: the stopped kiln counts with what it makes by its work time (none of its
    // minutes ran), no other kiln comes. What it would use does not count: no needed pit for its loam
    startGame(makeDesc());
    placeDeposits();
    MapPoint kilnPos = MapPoint::Invalid();
    for(const Position offset : {Position(5, -3), Position(6, 2), Position(3, 5), Position(-3, -4)})
    {
        const MapPoint pt = world.MakeMapPoint(hqPos + offset);
        if(world.GetBQ(pt, curPlayer) >= BuildingQuality::Hut)
        {
            BuildingFactory::CreateBuilding(world, KILN, pt, curPlayer, Nation::Romans);
            kilnPos = pt;
            break;
        }
    }
    BOOST_TEST_REQUIRE(kilnPos.isValid());
    world.GetSpecObj<nobUsual>(kilnPos)->SetProductionEnabled(false);
    const unsigned windowGFs = civai::EconomyPlanner::BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    for(unsigned i = 0; i < windowGFs + 600; ++i)
    {
        em.ExecuteNextGF();
        civ::runDistributions(world, em.GetCurrentGF());
    }
    startAI();
    BOOST_TEST_REQUIRE(civ::setResearch(world, curPlayer, 0));
    step();
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST(ware(CLAY).forecastProduction >= 5.);
    BOOST_TEST(ware(LOAM).forecastConsumption == 0.);
    BOOST_TEST(findOrder(KILN) == -1);
    // A pit comes at most as a harvester of the deposits (expanding), not for the loam of the kiln
    BOOST_TEST(!helpers::contains_if(list(), [](const civai::Order* order) {
        return order->building == PIT && order->kind == civai::OrderKind::Needed;
    }));
}

BOOST_FIXTURE_TEST_CASE(ExpandingOrderGetsExpandingSolutions, PlannerFixture)
{
    // The kiln (list of ai.lua) expands, clay is normal. Its input loam does not suffice: the order for the loam is
    // expanding too (no rights). No boards: nothing is placed
    startGame(makeDesc({"Kiln"}), 0);
    placeDeposits();
    startAI();
    step();
    const civai::Order& kiln = getOrder(KILN);
    BOOST_TEST((kiln.kind == civai::OrderKind::Expanding));
    BOOST_TEST_INFO(kiln.reason);
    BOOST_TEST(kiln.reason.find("Eingang fehlt") == 0u);
    bool pitForKiln = false;
    for(const civai::Order* order : list())
    {
        if(order->parent != kiln.id && !helpers::contains(order->alsoFor, kiln.id))
            continue;
        BOOST_TEST((order->kind == civai::OrderKind::Expanding));
        pitForKiln |= order->building == PIT;
    }
    BOOST_TEST(pitForKiln);
}

BOOST_FIXTURE_TEST_CASE(HarvestersShareTheUnitsOfTheirSource, PlannerFixture)
{
    // 3 deposits of 2 wares each: 6 units. The first pit may come (no other one reaches them: 1 unit suffices). A
    // second one would share them: 3 units each, fewer than UNITS_PER_HARVESTER (Nutzer, 2026-11-18). It gets no place
    startGame(makeDesc(), 40);
    placeDeposits();
    startAI();
    step();
    BOOST_TEST_REQUIRE(findSite(PIT).isValid());
    for(unsigned i = 0; i < 4; i++)
        step();
    BOOST_TEST_INFO(planner()->GetReport());
    unsigned numPits = 0;
    for(const noBuildingSite* site : world.GetPlayer(curPlayer).GetBuildingRegister().GetBuildingSites())
        numPits += site->GetBuildingType() == PIT ? 1u : 0u;
    BOOST_TEST(numPits == 1u);
}

BOOST_FIXTURE_TEST_CASE(NoHarvesterWhereItsWorkerCannotWalk, PlannerFixture)
{
    // One deposit, surrounded by stone piles: in reach, but its worker cannot walk to it (like nofFarmhand: a path of
    // at most 20). No pit is placed for it
    startGame(makeDesc(), 40);
    const Position center(4, 4);
    placeObject(center, [](MapPoint) { return new noDeposit(0, 1); });
    for(const auto dir : helpers::EnumRange<Direction>{})
    {
        const MapPoint pt = world.GetNeighbour(world.MakeMapPoint(hqPos + center), dir);
        world.DestroyNO(pt, false);
        world.SetNO(pt, new noGranite(GraniteType::One, 5));
        world.RecalcBQAroundPointBig(pt);
    }
    startAI();
    for(unsigned i = 0; i < 3; i++)
        step();
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST(!findSite(PIT).isValid());
}

BOOST_FIXTURE_TEST_CASE(ClearingWoodcutterWithFewTreesGetsTheForester, PlannerFixture)
{
    // A woodcutter for good without trees gets a forester ordered. A clearing woodcutter with fewer than
    // CLEARING_UPGRADE_BELOW trees becomes one for good and gets that forester (Nutzer, 2026-11-18); the other one
    // waits for the next forester
    startGame(makeDesc());
    hq().AddToInventory(GoodCounts::make(GoodType::Shovel, 3), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Axe, 3), true);
    std::vector<MapPoint> places;
    for(const Position offset : {Position(5, -3), Position(-5, 3), Position(6, 2), Position(-3, -4), Position(3, 5)})
    {
        const MapPoint pt = world.MakeMapPoint(hqPos + offset);
        if(world.GetBQ(pt, curPlayer) >= BuildingQuality::Hut
           && std::none_of(places.begin(), places.end(),
                           [&](MapPoint other) { return world.CalcDistance(pt, other) < 4; }))
            places.push_back(pt);
    }
    BOOST_TEST_REQUIRE(places.size() >= 2u);
    const MapPoint forGood = places[0];
    const MapPoint clearing = places[1];
    BuildingFactory::CreateBuilding(world, BuildingType::Woodcutter, forGood, curPlayer, Nation::Romans);
    BuildingFactory::CreateBuilding(world, BuildingType::Woodcutter, clearing, curPlayer, Nation::Romans);
    this->SetClearingWoodcutter(clearing, true);
    startAI();
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST(!world.GetPlayer(curPlayer).IsClearingWoodcutter(clearing));
    BOOST_TEST(!planner()->IsClearingWoodcutter(clearing));
    const auto renewerOf = [&](const MapPoint pt) {
        const std::vector<const civai::Order*> orders = list();
        return std::count_if(orders.begin(), orders.end(), [&](const civai::Order* order) {
            return order->cause == civai::OrderCause::Renewal && order->near == pt;
        });
    };
    BOOST_TEST(renewerOf(clearing) == 1);
    // The other one gets the next forester
    step();
    BOOST_TEST_INFO(planner()->GetReport());
    BOOST_TEST(renewerOf(forGood) == 1);
}

BOOST_AUTO_TEST_SUITE_END()
