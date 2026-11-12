// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "PointOutput.h"
#include "RttrForeachPt.h"
#include "addons/const_addons.h"
#include "ai/civ/CivAI.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "helpers/containerUtils.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "nodeObjs/noDeposit.h"
#include "nodeObjs/noGranite.h"
#include "gameTypes/GameTypesOutput.h"
#include "gameData/WorldDescription.h"
#include <boost/test/unit_test.hpp>
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

constexpr GoodType LOAM = GoodType::Custom0;
const GoodType CLAY = getCustomGood(1);

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

BOOST_AUTO_TEST_SUITE_END()
