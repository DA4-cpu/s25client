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
#include "lua/GameDataLoader.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "nodeObjs/noDeposit.h"
#include "nodeObjs/noGranite.h"
#include "nodeObjs/noTree.h"
#include "gameTypes/GameTypesOutput.h"
#include "gameTypes/Inventory.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
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
    BOOST_TEST(sawmill->nominalStepsPerMinute > 0.);
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
} // namespace

BOOST_AUTO_TEST_CASE(ExpandingBuildingsFromLua)
{
    const std::string workshop = R"(
rttr:AddWare{ name = "Loam", look = "Stones", transport = "Stones" }
rttr:AddWorkshop{ name = "Pit", size = "small", cost = { Boards = 2 }, look = "Quarry",
    worker = { tool = "none", look = "Stonemason" }, output = "Loam", workTime = 5 }
)";
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(workshop + R"(rttr:SetAIExpandingBuildings{ "Woodcutter", "Pit", "Barracks" })", desc));
    BOOST_TEST((desc.aiExpandingBuildings == std::vector<std::string>{"Woodcutter", "Pit", "Barracks"}));
    const civ::TechTree techTree(desc);
    BOOST_TEST(techTree.IsAIExpanding(BuildingType::Woodcutter));
    BOOST_TEST(techTree.IsAIExpanding(BuildingType::Custom0));
    BOOST_TEST(techTree.IsAIExpanding(BuildingType::Barracks));
    BOOST_TEST(!techTree.IsAIExpanding(BuildingType::Forester));
    BOOST_TEST(!techTree.IsAIExpanding(BuildingType::Well));

    for(const std::string& invalid :
        {std::string(R"(rttr:SetAIExpandingBuildings{ "Woodcutter", "Woodcutter" })"),
         std::string(R"(rttr:SetAIExpandingBuildings{ "Woodcutter", 3 })"),
         std::string(R"(rttr:SetAIExpandingBuildings{ Woodcutter = true })"),
         std::string(R"(rttr:SetAIExpandingBuildings{ })"), std::string(R"(rttr:SetAIExpandingBuildings{ "Woodcutter" }
rttr:SetAIExpandingBuildings{ "Quarry" })")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(workshop + invalid, invalidDesc));
    }
    WorldDescription unknownDesc;
    BOOST_TEST_REQUIRE(loadLua(workshop + R"(rttr:SetAIExpandingBuildings{ "Woodcuter" })", unknownDesc));
    BOOST_CHECK_THROW(civ::TechTree{unknownDesc}, GameDataError);
}

BOOST_AUTO_TEST_CASE(ExpandingBuildingsOfTheGameData)
{
    const civ::TechTree techTree = civ::loadDefaultTechTree();
    for(const BuildingType bld :
        {BuildingType::Woodcutter, BuildingType::Quarry, BuildingType::Sawmill, BuildingType::Armory,
         BuildingType::Mint, BuildingType::Barracks, BuildingType::Storehouse, BuildingType::HarborBuilding})
        BOOST_TEST(techTree.IsAIExpanding(bld));
    for(const BuildingType bld : {BuildingType::Forester, BuildingType::Well, BuildingType::Shipyard,
                                  BuildingType::Catapult, BuildingType::Farm, BuildingType::Mill})
        BOOST_TEST(!techTree.IsAIExpanding(bld));
    const auto copperworks = techTree.GetContent().FindBuilding("Copperworks");
    BOOST_TEST_REQUIRE(copperworks.has_value());
    BOOST_TEST(techTree.IsAIExpanding(*copperworks));
    // Basic economy of the user: woodcutter, quarry, sawmill, forester first; mines, smelters and smiths second
    for(const BuildingType bld :
        {BuildingType::Woodcutter, BuildingType::Quarry, BuildingType::Sawmill, BuildingType::Forester})
        BOOST_TEST(techTree.GetAIBasicEconomyLevel(bld) == 1u);
    for(const BuildingType bld : {BuildingType::IronMine, BuildingType::CoalMine, BuildingType::Charburner,
                                  BuildingType::Metalworks, BuildingType::Ironsmelter, BuildingType::Armory})
        BOOST_TEST(techTree.GetAIBasicEconomyLevel(bld) == 2u);
    BOOST_TEST(techTree.GetAIBasicEconomyLevel(*copperworks) == 2u);
    BOOST_TEST(techTree.GetAIBasicEconomyLevel(BuildingType::Brewery) == 0u);
    for(const GoodType good : {GoodType::Stones, GoodType::Wood, GoodType::Boards})
        BOOST_TEST(techTree.IsAIStartStockNoSurplus(good));
    BOOST_TEST(!techTree.IsAIStartStockNoSurplus(GoodType::IronOre));
}

BOOST_AUTO_TEST_CASE(BasicEconomyAndStartStockFromLua)
{
    const std::string workshop = R"(
rttr:AddWare{ name = "Loam", look = "Stones", transport = "Stones" }
rttr:AddWorkshop{ name = "Pit", size = "small", cost = { Boards = 2 }, look = "Quarry",
    worker = { tool = "none", look = "Stonemason" }, output = "Loam", workTime = 5 }
)";
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(workshop + R"(rttr:SetAIBasicEconomy{ { "Woodcutter", "Pit" }, { "IronMine" } }
rttr:SetAIStartStockNoSurplus{ "Wood", "Loam" })",
                               desc));
    BOOST_TEST(desc.aiBasicEconomy.size() == 2u);
    const civ::TechTree techTree(desc);
    BOOST_TEST(techTree.GetAIBasicEconomyLevel(BuildingType::Woodcutter) == 1u);
    BOOST_TEST(techTree.GetAIBasicEconomyLevel(BuildingType::Custom0) == 1u);
    BOOST_TEST(techTree.GetAIBasicEconomyLevel(BuildingType::IronMine) == 2u);
    BOOST_TEST(techTree.GetAIBasicEconomyLevel(BuildingType::Sawmill) == 0u);
    BOOST_TEST(techTree.IsAIStartStockNoSurplus(GoodType::Wood));
    BOOST_TEST(techTree.IsAIStartStockNoSurplus(GoodType::Custom0));
    BOOST_TEST(!techTree.IsAIStartStockNoSurplus(GoodType::Boards));

    for(const std::string& invalid :
        {std::string(R"(rttr:SetAIBasicEconomy{ { "Woodcutter" }, { "Woodcutter" } })"),
         std::string(R"(rttr:SetAIBasicEconomy{ "Woodcutter" })"), std::string(R"(rttr:SetAIBasicEconomy{ })"),
         std::string(R"(rttr:SetAIBasicEconomy{ { } })"), std::string(R"(rttr:SetAIStartStockNoSurplus{ "Wood", 2 })"),
         std::string(R"(rttr:SetAIStartStockNoSurplus{ "Wood", "Wood" })")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(workshop + invalid, invalidDesc));
    }
    WorldDescription unknownDesc;
    BOOST_TEST_REQUIRE(loadLua(workshop + R"(rttr:SetAIStartStockNoSurplus{ "Wod" })", unknownDesc));
    BOOST_CHECK_THROW(civ::TechTree{unknownDesc}, GameDataError);
}

BOOST_AUTO_TEST_SUITE_END()
