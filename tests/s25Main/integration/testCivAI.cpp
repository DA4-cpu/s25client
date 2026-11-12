// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "PointOutput.h"
#include "RttrForeachPt.h"
#include "addons/const_addons.h"
#include "ai/civ/CivAI.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "helpers/containerUtils.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "nodeObjs/noDeposit.h"
#include "nodeObjs/noGranite.h"
#include "nodeObjs/noTree.h"
#include "gameTypes/GameTypesOutput.h"
#include "gameTypes/Inventory.h"
#include "gameData/WorldDescription.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <memory>

// New AI of the civilization mode, step 1: production graph, map profile and which wares can be made
BOOST_AUTO_TEST_SUITE(CivAISuite)

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
/// kiln (workshop 1). With lockPit the pit needs the technology "loam"
WorldDescription makeDesc(bool lockPit = false)
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
    if(lockPit)
    {
        TechnologyDesc tech;
        tech.name = "loam";
        tech.title = "Loam";
        tech.unlockedBuildings = {"Pit"};
        desc.technologies.add(tech);
    }
    return desc;
}

/// Extras for the planner: the hut (workshop 2) is built of clay and makes nothing, the digger (workshop 3) collects
/// clay directly from clay banks (map object 1)
WorldDescription makePlannerDesc()
{
    WorldDescription desc = makeDesc();
    WorkshopDesc hut = makeWorkshop("Hut");
    hut.cost = {{{"Clay"}, 3}};
    desc.workshops.push_back(hut);
    MapObjectDesc bank;
    bank.name = "ClayBank";
    bank.title = "ClayBankTitle";
    bank.sizes = 2;
    bank.look = "Granite1";
    desc.mapObjects.push_back(bank);
    WorkshopDesc digger = makeWorkshop("Digger");
    digger.collectObject = "ClayBank";
    digger.collectRadius = 6;
    digger.output = {"Clay"};
    desc.workshops.push_back(digger);
    return desc;
}

constexpr GoodType LOAM = GoodType::Custom0;
const GoodType CLAY = getCustomGood(1);
const BuildingType PIT = getCustomBuilding(0);
const BuildingType KILN = getCustomBuilding(1);
const BuildingType HUT = getCustomBuilding(2);
const BuildingType DIGGER = getCustomBuilding(3);

struct CivAIFixture : public WorldWithGCExecution<1, 48, 48>
{
    void init(const WorldDescription& desc)
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
        world.GetPlayer(0).GetHQ()->AddToInventory(GoodCounts::make(GoodType::Boards, 10), true);
        ai = std::make_unique<civai::CivAI>(world, 0);
    }
    std::unique_ptr<civai::CivAI> ai;

    civai::WareState state(GoodType good) const { return ai->GetAnalysis()[good].state; }
    /// Put a map object (deposit, clay bank) in the territory near the HQ
    void placeObject(unsigned object, Position offset = Position(3, 3))
    {
        const MapPoint pt = world.MakeMapPoint(hqPos + offset);
        BOOST_TEST_REQUIRE(world.GetNode(pt).owner == 1u);
        world.DestroyNO(pt, false);
        world.SetNO(pt, new noDeposit(object, 1));
        ai->Refresh();
    }
    civai::ProductionPlan plan(const civai::Demand& demand) const
    {
        return civai::planProduction(world, 0, ai->GetGraph(), ai->GetMap(), demand);
    }
    static civai::Demand demandOf(GoodType good, double amount)
    {
        civai::Demand demand;
        demand.items.push_back({civai::DemandKind::Construction, {good}, amount});
        return demand;
    }
    int recipeIdx(BuildingType bld) const
    {
        const auto& recipes = ai->GetGraph().GetRecipes();
        for(unsigned i = 0; i < recipes.size(); ++i)
        {
            if(recipes[i].building == bld)
                return static_cast<int>(i);
        }
        return -1;
    }
    const civai::Recipe* findRecipe(BuildingType bld) const
    {
        for(const civai::Recipe& recipe : ai->GetGraph().GetRecipes())
        {
            if(recipe.building == bld)
                return &recipe;
        }
        return nullptr;
    }
};
} // namespace

BOOST_FIXTURE_TEST_CASE(GraphFromGameData, CivAIFixture)
{
    init(makeDesc());
    // Built-in buildings
    const civai::Recipe* sawmill = findRecipe(BuildingType::Sawmill);
    BOOST_TEST_REQUIRE(sawmill);
    BOOST_TEST((sawmill->inputs == std::vector<GoodType>{GoodType::Wood}));
    BOOST_TEST((sawmill->outputs == std::vector<GoodType>{GoodType::Boards}));
    BOOST_TEST((sawmill->source.kind == civai::SourceKind::None));
    BOOST_TEST(sawmill->waresPerMinute > 0.);
    const civai::Recipe* woodcutter = findRecipe(BuildingType::Woodcutter);
    BOOST_TEST_REQUIRE(woodcutter);
    BOOST_TEST((woodcutter->source.kind == civai::SourceKind::Trees));
    const civai::Recipe* mine = findRecipe(BuildingType::GraniteMine);
    BOOST_TEST_REQUIRE(mine);
    BOOST_TEST((mine->source.kind == civai::SourceKind::Ore));
    BOOST_TEST(mine->source.index == static_cast<unsigned>(ResourceType::Granite));
    BOOST_TEST((mine->outputs == std::vector<GoodType>{GoodType::Stones}));
    // The metalworks makes all tools
    BOOST_TEST_REQUIRE(findRecipe(BuildingType::Metalworks));
    BOOST_TEST(findRecipe(BuildingType::Metalworks)->outputs.size() == 12u);
    // Buildings of disabled addons (wine) and buildings without wares (forester) are not part of it
    BOOST_TEST(!findRecipe(BuildingType::Temple));
    BOOST_TEST(!findRecipe(BuildingType::Forester));
    // Stones: quarry and granite mine
    const auto& stoneProducers = ai->GetGraph().GetProducers(GoodType::Stones);
    BOOST_TEST(stoneProducers.size() == 2u);

    // Workshops from lua
    const civai::Recipe* pit = findRecipe(BuildingType::Custom0);
    BOOST_TEST_REQUIRE(pit);
    BOOST_TEST((pit->source.kind == civai::SourceKind::MapObject));
    BOOST_TEST(pit->source.index == 0u);
    BOOST_TEST((pit->outputs == std::vector<GoodType>{LOAM}));
    const civai::Recipe* kiln = findRecipe(getCustomBuilding(1));
    BOOST_TEST_REQUIRE(kiln);
    BOOST_TEST((kiln->inputs == std::vector<GoodType>{LOAM}));
    BOOST_TEST(ai->GetGraph().GetSourceName(pit->source) == "LoamDepositTitle");
}

BOOST_FIXTURE_TEST_CASE(KnowsWhatIsMissing, CivAIFixture)
{
    init(makeDesc());
    // No loam on the map: neither loam nor clay
    BOOST_TEST((state(LOAM) == civai::WareState::Unreachable));
    BOOST_TEST(ai->GetAnalysis()[LOAM].reason == "fehlt LoamDepositTitle");
    BOOST_TEST((state(CLAY) == civai::WareState::Unreachable));
    BOOST_TEST(ai->GetAnalysis()[CLAY].reason == "braucht LoamTitle");
    BOOST_TEST(ai->GetMap().GetTotal().GetAmount({civai::SourceKind::MapObject, 0}) == 0u);

    // Loam outside the territory: after expansion
    MapPoint farPt = MapPoint::Invalid();
    RTTR_FOREACH_PT(MapPoint, world.GetSize())
    {
        if(!farPt.isValid() && world.GetNode(pt).owner == 0u && world.GetNO(pt)->GetType() == NodalObjectType::Nothing)
            farPt = pt;
    }
    BOOST_TEST_REQUIRE(farPt.isValid());
    world.DestroyNO(farPt, false);
    world.SetNO(farPt, new noDeposit(0, 1));
    ai->Refresh();
    BOOST_TEST(ai->GetMap().GetTotal().GetAmount({civai::SourceKind::MapObject, 0}) == 2u);
    BOOST_TEST(ai->GetMap().GetTotalOwned().GetAmount({civai::SourceKind::MapObject, 0}) == 0u);
    BOOST_TEST((state(LOAM) == civai::WareState::NeedsExpansion));
    BOOST_TEST((state(CLAY) == civai::WareState::NeedsExpansion));

    // Loam in the territory: producible
    const MapPoint nearPt = world.MakeMapPoint(hqPos + Position(3, 3));
    BOOST_TEST_REQUIRE(world.GetNode(nearPt).owner == 1u);
    world.DestroyNO(nearPt, false);
    world.SetNO(nearPt, new noDeposit(0, 0));
    ai->Refresh();
    BOOST_TEST(ai->GetMap().GetTotalOwned().GetAmount({civai::SourceKind::MapObject, 0}) == 1u);
    BOOST_TEST((state(LOAM) == civai::WareState::Producible));
    BOOST_TEST((state(CLAY) == civai::WareState::Producible));
    BOOST_TEST(ai->GetReport().find("LoamTitle") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(StonesNeedPilesOrGranite, CivAIFixture)
{
    init(makeDesc());
    // The test world has no stone piles and no mountains
    const civai::ResourceProfile total = ai->GetMap().GetTotal();
    if(total.stones == 0 && total.resources[ResourceType::Granite] == 0)
    {
        BOOST_TEST((state(GoodType::Stones) == civai::WareState::Unreachable));
        BOOST_TEST(ai->GetAnalysis()[GoodType::Stones].reason.find("Steinhaufen") != std::string::npos);
    }
    // A stone pile in the territory
    const MapPoint pt = world.MakeMapPoint(hqPos + Position(-3, 3));
    world.DestroyNO(pt, false);
    world.SetNO(pt, new noGranite(GraniteType::One, 4));
    ai->Refresh();
    BOOST_TEST(ai->GetMap().GetTotalOwned().stones >= 5u);
    BOOST_TEST((state(GoodType::Stones) == civai::WareState::Producible));
}

BOOST_FIXTURE_TEST_CASE(LockedBuildingsNeedResearch, CivAIFixture)
{
    init(makeDesc(true));
    const MapPoint nearPt = world.MakeMapPoint(hqPos + Position(3, 3));
    world.DestroyNO(nearPt, false);
    world.SetNO(nearPt, new noDeposit(0, 1));
    ai->Refresh();
    BOOST_TEST((state(LOAM) == civai::WareState::NeedsResearch));
    BOOST_TEST((state(CLAY) == civai::WareState::NeedsResearch));
    // After the research
    civ::completeResearch(world, 0, 0);
    ai->Refresh();
    BOOST_TEST((state(LOAM) == civai::WareState::Producible));
}

BOOST_FIXTURE_TEST_CASE(UpdatesBitByBit, CivAIFixture)
{
    init(makeDesc());
    const MapPoint nearPt = world.MakeMapPoint(hqPos + Position(3, 3));
    world.DestroyNO(nearPt, false);
    world.SetNO(nearPt, new noDeposit(0, 1));
    // The AI scans a few regions per NWF: after one round over all regions it knows the deposit
    const unsigned numRounds =
      (ai->GetMap().GetNumRegions() + civai::CivAI::REGIONS_PER_NWF - 1) / civai::CivAI::REGIONS_PER_NWF;
    for(unsigned i = 0; i <= numRounds; ++i)
        ai->RunGF(i + 1, true);
    BOOST_TEST(ai->GetMap().GetTotalOwned().GetAmount({civai::SourceKind::MapObject, 0}) == 2u);
    BOOST_TEST(ai->GetTimes().numUpdates == numRounds + 1u);
}

// Step 2: plan of the production buildings

BOOST_FIXTURE_TEST_CASE(PlansChainBackwards, CivAIFixture)
{
    init(makeDesc());
    placeObject(0);
    const civai::ProductionPlan result = plan(demandOf(CLAY, 30));
    BOOST_TEST(result.recipe[CLAY] == recipeIdx(KILN));
    BOOST_TEST(result.recipe[LOAM] == recipeIdx(PIT));
    BOOST_TEST(result.demandPerMinute[CLAY] > 0.);
    // The kiln needs one loam per clay
    BOOST_TEST(result.needPerMinute[LOAM] == result.needPerMinute[CLAY], boost::test_tools::tolerance(1e-9));
    BOOST_TEST(result.wanted[KILN] >= 1u);
    BOOST_TEST(result.wanted[PIT] >= 1u);
    BOOST_TEST((result.mainWare[PIT] == LOAM));
    // No helpers in the warehouse: only 2 new buildings at once, but each step of the chain gets one
    BOOST_TEST(world.GetPlayer(0).GetInventory().people[Job::Helper] == 0u);
    BOOST_TEST(result.wanted[PIT] + result.wanted[KILN] == 2u);
    // Never more building sites than allowed at once
    BOOST_TEST(result.wanted[PIT] <= 2u);
    BOOST_TEST(result.unmet.empty());
    BOOST_TEST(result.GetReport().find("Kiln") != std::string::npos);
    // The planner decides about all production buildings, not about warehouses and military buildings
    BOOST_TEST(result.managed[KILN]);
    BOOST_TEST(result.managed[BuildingType::Sawmill]);
    BOOST_TEST(result.managed[BuildingType::Forester]);
    BOOST_TEST(!result.managed[BuildingType::Storehouse]);
    BOOST_TEST(!result.managed[BuildingType::Barracks]);
    BOOST_TEST(!result.managed[BuildingType::Shipyard]);

    // No demand: nothing wanted
    const civai::ProductionPlan idle = plan(civai::Demand{});
    BOOST_TEST(idle.wanted[KILN] == 0u);
    BOOST_TEST(idle.wanted[PIT] == 0u);
}

BOOST_FIXTURE_TEST_CASE(NoSourceNoBuildings, CivAIFixture)
{
    init(makeDesc());
    const civai::ProductionPlan result = plan(demandOf(CLAY, 30));
    BOOST_TEST(result.recipe[CLAY] == -1);
    BOOST_TEST(result.wanted[KILN] == 0u);
    BOOST_TEST(result.wanted[PIT] == 0u);
    BOOST_TEST_REQUIRE(result.unmet.size() == 1u);
    BOOST_TEST(result.unmet.front() == "ClayTitle (Bau)");
}

BOOST_FIXTURE_TEST_CASE(ChoosesWayBySource, CivAIFixture)
{
    init(makePlannerDesc());
    world.GetPlayer(0).GetHQ()->AddToInventory(PeopleCounts::make(Job::Helper, 10), true);
    // Only loam deposits: clay from loam
    placeObject(0);
    BOOST_TEST(plan(demandOf(CLAY, 30)).recipe[CLAY] == recipeIdx(KILN));
    // Clay banks as well: digging clay directly is cheaper than the chain
    placeObject(1, Position(-3, 3));
    const civai::ProductionPlan little = plan(demandOf(CLAY, 2));
    BOOST_TEST(little.recipe[CLAY] == recipeIdx(DIGGER));
    BOOST_TEST(little.wanted[DIGGER] == 1u);
    BOOST_TEST(little.wanted[KILN] == 0u);
    BOOST_TEST(little.wanted[PIT] == 0u);
    // A few pieces of clay bank are enough for one digger only: the rest of a large demand takes the chain
    const civai::ProductionPlan much = plan(demandOf(CLAY, 60));
    BOOST_TEST(much.work[DIGGER] == 1., boost::test_tools::tolerance(1e-9));
    BOOST_TEST(much.wanted[DIGGER] == 1u);
    BOOST_TEST(much.wanted[KILN] >= 1u);
    BOOST_TEST(much.wanted[PIT] >= 1u);
}

BOOST_FIXTURE_TEST_CASE(ByproductIsAWay, CivAIFixture)
{
    // The pit brings clay as byproduct of loam: that is cheaper than the kiln
    WorldDescription desc = makeDesc();
    ByproductDesc byproduct;
    byproduct.building = "Pit";
    byproduct.ware = {"Clay"};
    desc.byproducts.push_back(byproduct);
    init(desc);
    placeObject(0);
    const civai::Recipe* pit = findRecipe(PIT);
    BOOST_TEST_REQUIRE(pit);
    BOOST_TEST((pit->byproducts == std::vector<GoodType>{CLAY}));
    BOOST_TEST(pit->byproductAmounts == std::vector<unsigned>{1u});
    const civai::ProductionPlan result = plan(demandOf(CLAY, 10));
    BOOST_TEST(result.recipe[CLAY] == recipeIdx(PIT));
    BOOST_TEST(result.wanted[PIT] >= 1u);
    BOOST_TEST(result.wanted[KILN] == 0u);
    // Loam is wanted as well: its pits bring enough clay, the kiln is not needed either
    civai::Demand both = demandOf(LOAM, 10);
    both.items.push_back({civai::DemandKind::Construction, {CLAY}, 10});
    BOOST_TEST(plan(both).wanted[KILN] == 0u);
}

BOOST_FIXTURE_TEST_CASE(DemandFromBuildingSite, CivAIFixture)
{
    init(makePlannerDesc());
    placeObject(0);
    const MapPoint sitePos = world.MakeMapPoint(hqPos + Position(4, 2));
    this->SetBuildingSite(sitePos, HUT);
    BOOST_TEST_REQUIRE(world.GetSpecObj<noBuildingSite>(sitePos));
    const civai::Demand demand = civai::collectDemand(world, 0);
    const auto isClay = [](const civai::DemandItem& item) {
        return item.kind == civai::DemandKind::Construction && item.wares == std::vector<GoodType>{CLAY};
    };
    const auto it = std::find_if(demand.items.begin(), demand.items.end(), isClay);
    BOOST_TEST_REQUIRE((it != demand.items.end()));
    BOOST_TEST(it->amount == 3.);
    // No clay in stock: the planner wants the chain
    const civai::ProductionPlan result = plan(demand);
    BOOST_TEST(result.wanted[KILN] >= 1u);
    BOOST_TEST(result.wanted[PIT] >= 1u);
    // Clay in stock: nothing needed for the site
    world.GetPlayer(0).GetHQ()->AddToInventory(GoodCounts::make(CLAY, 3), true);
    const std::vector<civai::DemandItem> left =
      civai::applyStock(civai::collectDemand(world, 0).items, world.GetPlayer(0).GetInventory());
    BOOST_TEST(std::none_of(left.begin(), left.end(), isClay));
}

BOOST_AUTO_TEST_CASE(StockIsSharedByPriority)
{
    Inventory inventory;
    inventory.Add(GoodType::Boards, 5);
    std::vector<civai::DemandItem> items;
    items.push_back({civai::DemandKind::Reserve, {GoodType::Boards}, 15});
    items.push_back({civai::DemandKind::Construction, {GoodType::Boards}, 3});
    items.push_back({civai::DemandKind::Tools, {GoodType::Axe}, 1});
    const std::vector<civai::DemandItem> left = civai::applyStock(items, inventory);
    // The building site gets the boards first, the reserve the rest
    BOOST_TEST_REQUIRE(left.size() == 2u);
    BOOST_TEST((left[0].kind == civai::DemandKind::Tools));
    BOOST_TEST((left[1].kind == civai::DemandKind::Reserve));
    BOOST_TEST(left[1].amount == 13.);
}

BOOST_FIXTURE_TEST_CASE(ForestersForWoodcutters, CivAIFixture)
{
    init(makeDesc());
    // The sawmill is built of stones as well
    world.GetPlayer(0).GetHQ()->AddToInventory(GoodCounts::make(GoodType::Stones, 10), true);
    world.GetPlayer(0).GetHQ()->AddToInventory(PeopleCounts::make(Job::Helper, 10), true);
    // Some trees in the territory
    for(const Position offset : {Position(-4, -2), Position(-2, -4), Position(2, -4)})
    {
        const MapPoint pt = world.MakeMapPoint(hqPos + offset);
        world.DestroyNO(pt, false);
        world.SetNO(pt, new noTree(pt, 0, 3));
    }
    ai->Refresh();
    const civai::ProductionPlan result = plan(demandOf(GoodType::Boards, 60));
    BOOST_TEST(result.recipe[GoodType::Boards] == recipeIdx(BuildingType::Sawmill));
    BOOST_TEST(result.wanted[BuildingType::Sawmill] >= 1u);
    BOOST_TEST(result.wanted[BuildingType::Woodcutter] >= 1u);
    BOOST_TEST(result.wanted[BuildingType::Forester] >= 1u);
    BOOST_TEST((result.mainWare[BuildingType::Forester] == GoodType::Wood));
}

BOOST_FIXTURE_TEST_CASE(BuildingsForKnowledge, CivAIFixture)
{
    WorldDescription desc = makeDesc();
    TechnologyDesc tech;
    tech.name = "writing";
    tech.title = "Writing";
    tech.knowledge = {0, 30, 0};
    desc.technologies.add(tech);
    desc.buildingKnowledge["Woodcutter"] = {0, 2, 0};
    desc.buildingKnowledge["Quarry"] = {0, 4, 0};
    init(desc);
    world.GetPlayer(0).GetHQ()->AddToInventory(PeopleCounts::make(Job::Helper, 10), true);
    const MapPoint treePt = world.MakeMapPoint(hqPos + Position(-4, -2));
    world.DestroyNO(treePt, false);
    world.SetNO(treePt, new noTree(treePt, 0, 3));
    const MapPoint stonePt = world.MakeMapPoint(hqPos + Position(-3, 3));
    world.DestroyNO(stonePt, false);
    world.SetNO(stonePt, new noGranite(GraniteType::One, 4));
    ai->Refresh();
    BOOST_TEST_REQUIRE(civ::setResearch(world, 0, 0));

    BOOST_TEST(civai::collectDemand(world, 0).knowledge[civ::KnowledgeType::Industry] == 300);
    // Only the knowledge (no construction material wanted)
    civai::Demand demand;
    demand.knowledge = civai::collectDemand(world, 0).knowledge;
    const civai::ProductionPlan result = plan(demand);
    // The quarry gives the cheapest knowledge point (2 boards for 4 IWP), at most 2 building sites at once, then
    // woodcutters
    BOOST_TEST(result.forKnowledge[BuildingType::Quarry] == 2u);
    BOOST_TEST(result.wanted[BuildingType::Quarry] == 2u);
    BOOST_TEST(result.forKnowledge[BuildingType::Woodcutter] == 2u);
    BOOST_TEST(result.GetReport().find("für Wissen") != std::string::npos);
    // The rest would have to come from knowledge media, there are none
    BOOST_TEST(helpers::contains(result.unmet, std::string("Wissen IWP (Forschung)")));
}

BOOST_FIXTURE_TEST_CASE(PlansInRunGF, CivAIFixture)
{
    init(makeDesc());
    BOOST_TEST(!ai->GetPlan());
    for(unsigned gf = 1; gf <= civai::CivAI::PLAN_INTERVAL; ++gf)
        ai->RunGF(gf, false);
    BOOST_TEST_REQUIRE(ai->GetPlan());
    BOOST_TEST(ai->GetTimes().planUs > 0.);
}

BOOST_AUTO_TEST_SUITE_END()
