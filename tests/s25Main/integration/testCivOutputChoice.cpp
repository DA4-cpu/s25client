// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BuildingRegister.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "PointOutput.h"
#include "RttrConfig.h"
#include "SerializedGameData.h"
#include "addons/const_addons.h"
#include "ai/AIPlayer.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "civ/WareDistribution.h"
#include "factories/AIFactory.h"
#include "factories/BuildingFactory.h"
#include "files.h"
#include "helpers/containerUtils.h"
#include "lua/GameDataLoader.h"
#include "postSystem/PostBox.h"
#include "postSystem/PostManager.h"
#include "postSystem/PostMsg.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "gameTypes/GameTypesOutput.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <iterator>
#include <memory>

namespace bnw = boost::nowide;

// Workshops with several products are steered by the distribution of all wares (Gesamtverteilung, DESIGN.md)
BOOST_AUTO_TEST_SUITE(CivOutputChoiceSuite)

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

WorkshopDesc makeWorkshop(const std::string& name, const std::string& input, std::vector<TagQuery> outputs)
{
    WorkshopDesc desc;
    desc.name = name;
    desc.title = name + "Title";
    desc.size = "small";
    desc.cost = {{{"Boards"}, 2}};
    desc.look = "Metalworks";
    desc.worker.title = name + "Worker";
    desc.worker.tool = "none";
    desc.worker.look = "Metalworker";
    desc.inputs = {{input}};
    desc.outputs = std::move(outputs);
    desc.workTime = 2;
    return desc;
}

WorldDescription makeDesc()
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("CopperAxe", "Axe", "Axe"));
    desc.wares.push_back(makeWare("CopperSaw", "Saw", "Saw"));
    desc.wares.push_back(makeWare("Jug", "Jug", "Water"));
    desc.wares.push_back(makeWare("Pot", "Pot", "Water"));
    desc.wares.push_back(makeWare("Brick", "Brick", "Stones"));
    desc.workshops.push_back(makeWorkshop("Copperworks", "Coal", {{"CopperAxe"}, {"CopperSaw"}}));
    desc.workshops.push_back(makeWorkshop("Pottery", "Stones", {{"Jug"}, {"Pot"}}));
    desc.workshops.push_back(makeWorkshop("Kiln", "Stones", {{"Brick"}}));
    return desc;
}

constexpr GoodType COPPER_AXE = GoodType::Custom0;
const GoodType COPPER_SAW = getCustomGood(1);
const GoodType JUG = getCustomGood(2);
const GoodType POT = getCustomGood(3);
const GoodType BRICK = getCustomGood(4);
constexpr BuildingType COPPERWORKS = BuildingType::Custom0;
const BuildingType POTTERY = getCustomBuilding(1);
const BuildingType KILN = getCustomBuilding(2);

struct OutputChoiceFixture : public WorldWithGCExecution1P
{
    OutputChoiceFixture()
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(makeDesc()));
        civ::startGame(world);
        // Values from the number of wares (stock + on the way - open): the forecast counts what a new building would
        // make by its work time, the tests check the choice
        this->SetWareDistribution(false, {});
    }
    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    unsigned countInHQ(GoodType good) { return hq().GetNumRealWares(good); }
    const civ::WareDistribution& distribution() { return world.GetPlayer(0).GetWareDistribution(); }
    /// Place a finished building connected to the HQ
    MapPoint placeConnectedBuilding(BuildingType bld, Position offset = Position(4, 2))
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + offset);
        BuildingFactory::CreateBuilding(world, bld, pos, 0, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<nobUsual>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        return pos;
    }
    /// Set the value of the ware by the player (0..1000)
    void setValue(GoodType good, uint16_t value)
    {
        civ::WareSetting setting = distribution().GetSetting(good);
        setting.autoValue = false;
        setting.manualValue = value;
        this->SetWareDistribution(distribution().UsesForecast(), {{good, setting}});
    }
    void setPaused(GoodType good, bool autoPause, bool paused)
    {
        civ::WareSetting setting = distribution().GetSetting(good);
        setting.autoPause = autoPause;
        setting.paused = paused;
        this->SetWareDistribution(distribution().UsesForecast(), {{good, setting}});
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(SeveralProductsFromLua)
{
    const std::string base = R"(
rttr:AddWare{ name = "CopperAxe", kind = "Axe", look = "Axe", transport = "Axe" }
rttr:AddWare{ name = "CopperSaw", kind = "Saw", properties = { "Copper" }, look = "Saw", transport = "Saw" }
rttr:AddWorkshop{ name = "Copperworks", size = "medium", cost = { Boards = 2 }, look = "Metalworks",
    worker = { tool = "Tongs", look = "Metalworker" }, inputs = { "IronOre", "Coal" },
    outputs = { "CopperAxe", { "Saw", "Copper" }, "Bow" }, workTime = 120 }
rttr:AddWorkshop{ name = "Single", size = "small", cost = { Boards = 2 }, look = "Bakery",
    worker = { tool = "none", look = "Baker" }, inputs = { "Stones" }, outputs = { "CopperAxe" }, workTime = 40 }
)";
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(base, desc));
    BOOST_TEST_REQUIRE(desc.workshops.size() == 2u);
    BOOST_TEST((desc.workshops[0].outputs[1] == TagQuery{"Saw", "Copper"}));
    BOOST_TEST(desc.workshops[0].outputChoice.empty());

    const civ::Content content(desc);
    BOOST_TEST((content.GetToolTypeOf(COPPER_AXE) == Tool::Axe));
    const civ::CustomBuilding& copperworks = *content.GetBuilding(COPPERWORKS);
    BOOST_TEST((copperworks.productChoice == civ::ProductChoice::Distribution));
    BOOST_TEST((copperworks.productChoices == std::vector<GoodType>{COPPER_AXE, COPPER_SAW, GoodType::Bow}));
    const civ::CustomBuilding& single = *content.GetBuilding(getCustomBuilding(1));
    BOOST_TEST((single.productChoice == civ::ProductChoice::None));

    const auto replaced = [&base](const std::string& from, const std::string& to) {
        std::string result = base;
        const auto pos = result.find(from);
        BOOST_TEST_REQUIRE(pos != std::string::npos);
        return result.replace(pos, from.size(), to);
    };
    // Only "knowledge" is left as outputChoice, own regulators are gone
    for(const std::string& invalid :
        {replaced("\"Bow\" }, workTime = 120", "\"Bow\" }, outputChoice = \"tools\", workTime = 120"),
         replaced("\"Bow\" }, workTime = 120", "\"Bow\" }, outputChoice = \"Pottery\", workTime = 120"),
         base + "rttr:AddRegulator{ name = \"Pottery\", entries = { { tags = \"Jug\" } } }"})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(invalid, invalidDesc));
    }
    // A ware twice
    WorldDescription twice;
    BOOST_TEST_REQUIRE(loadLua(replaced("\"Bow\" }, workTime", "\"CopperAxe\" }, workTime"), twice));
    BOOST_CHECK_THROW(civ::Content{twice}, GameDataError);
}

BOOST_AUTO_TEST_CASE(ShippedDistributionLuaIsValid)
{
    const auto civFolder = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "civ";
    WorldDescription shipped;
    {
        GameDataLoader loader(shipped, civFolder);
        BOOST_TEST_REQUIRE(loader.Load());
    }
    const civ::TechTree tree(shipped);
    const civ::Content& shippedContent = tree.GetContent();
    BOOST_TEST_REQUIRE(shippedContent.HasToolReform());
    BOOST_TEST(shippedContent.GetToolMaterials()[0].defaultPriority == shipped.toolMaterialPriorities.at("Iron"));
    const auto copperworks = shippedContent.FindBuilding("Copperworks");
    BOOST_TEST_REQUIRE(copperworks.has_value());
    BOOST_TEST((shippedContent.GetBuilding(*copperworks)->productChoice == civ::ProductChoice::Distribution));
}

BOOST_FIXTURE_TEST_CASE(ValuesFromTheNumbers, OutputChoiceFixture)
{
    // Number of wares: the ware with the lowest number gets the highest value, the highest number 0
    BOOST_TEST(!distribution().UsesForecast());
    hq().AddToInventory(GoodCounts::make(JUG, 6), true);
    hq().AddToInventory(GoodCounts::make(POT, 2), true);
    placeConnectedBuilding(POTTERY);
    civ::updateDistribution(world, 0);
    BOOST_TEST((distribution().GetState(JUG) == civ::WareDistState::Valued));
    BOOST_TEST((distribution().GetState(POT) == civ::WareDistState::Valued));
    BOOST_TEST(distribution().GetNumber(JUG) == 6 * civ::AMOUNT_UNIT);
    BOOST_TEST(distribution().GetNumber(POT) == 2 * civ::AMOUNT_UNIT);
    BOOST_TEST(distribution().GetValue(POT) == civ::DISTRIBUTION_SCALE);
    BOOST_TEST(distribution().GetValue(JUG) == 0u);
    // No building makes it: not valued
    BOOST_TEST((distribution().GetState(BRICK) == civ::WareDistState::NoBuilding));
    // More than 8 (no consumption): surplus, not valued any more. Alone in the comparison: the highest value
    hq().AddToInventory(GoodCounts::make(JUG, 3), true);
    civ::updateDistribution(world, 0);
    BOOST_TEST((distribution().GetState(JUG) == civ::WareDistState::Surplus));
    BOOST_TEST(distribution().IsUnwanted(JUG));
    BOOST_TEST(distribution().GetValue(POT) == civ::DISTRIBUTION_SCALE);
}

BOOST_FIXTURE_TEST_CASE(BuildingChoosesByTheValues, OutputChoiceFixture)
{
    // Valued only once a finished building makes them
    placeConnectedBuilding(POTTERY);
    setValue(JUG, 1000);
    setValue(POT, 0);
    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 4), true);
    // Highest value 100 %, lowest 0 %
    RTTR_EXEC_TILL(8000, countInHQ(JUG) == 4u);
    BOOST_TEST(countInHQ(POT) == 0u);
    setValue(JUG, 200);
    setValue(POT, 900);
    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 3), true);
    RTTR_EXEC_TILL(8000, countInHQ(POT) == 3u);
    BOOST_TEST(countInHQ(JUG) == 4u);
    // All equal: equally distributed
    setValue(JUG, 500);
    setValue(POT, 500);
    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 12), true);
    RTTR_EXEC_TILL(20000, countInHQ(JUG) + countInHQ(POT) == 4u + 3u + 12u);
    BOOST_TEST(countInHQ(JUG) > 4u);
    BOOST_TEST(countInHQ(POT) > 3u);
}

BOOST_FIXTURE_TEST_CASE(OrdersFirst, OutputChoiceFixture)
{
    placeConnectedBuilding(POTTERY);
    setValue(JUG, 1000);
    setValue(POT, 0);
    PostBox& postbox = world.GetPostMgr().AddPostBox(0);
    postbox.Clear();
    this->ChangeWareOrders(POT, 2);
    BOOST_TEST(distribution().GetOrders(POT) == 2u);
    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 4), true);
    RTTR_EXEC_TILL(8000, countInHQ(POT) == 2u);
    BOOST_TEST(distribution().GetOrders(POT) == 0u);
    BOOST_TEST_REQUIRE(postbox.GetNumMsgs() == 1u);
    BOOST_TEST((postbox.GetMsg(0)->GetCategory() == PostCategory::Economy));
    RTTR_EXEC_TILL(8000, countInHQ(JUG) == 2u);
    BOOST_TEST(countInHQ(POT) == 2u);
    // Orders below 0 are ignored
    this->ChangeWareOrders(POT, -5);
    BOOST_TEST(distribution().GetOrders(POT) == 0u);
}

BOOST_FIXTURE_TEST_CASE(SurplusStopsTheBuilding, OutputChoiceFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 10), true);
    hq().AddToInventory(GoodCounts::make(BRICK, 9), true);
    const MapPoint pos = placeConnectedBuilding(KILN);
    const nobUsual& kiln = *world.GetSpecObj<nobUsual>(pos);
    RTTR_EXEC_TILL(2000, kiln.HasWorker());
    // 9 bricks, no consumption: surplus, the kiln stops and orders no stones
    civ::updateDistribution(world, 0);
    BOOST_TEST((distribution().GetState(BRICK) == civ::WareDistState::Surplus));
    BOOST_TEST(kiln.IsProductionStopped());
    BOOST_TEST(kiln.IsPausedByDistribution());
    BOOST_TEST(!kiln.IsProductionDisabled());
    RTTR_SKIP_GFS(1000);
    BOOST_TEST(countInHQ(BRICK) == 9u);
    // An order lets it work: it starts at once, the order is processed when the step starts
    this->ChangeWareOrders(BRICK, 1);
    BOOST_TEST(distribution().GetOrders(BRICK) == 0u);
    RTTR_EXEC_TILL(4000, countInHQ(BRICK) == 10u);
    BOOST_TEST(kiln.IsProductionStopped());
    // Stopped by the player: no surplus needed, it stays stopped
    setPaused(BRICK, false, true);
    BOOST_TEST((distribution().GetState(BRICK) == civ::WareDistState::Paused));
    BOOST_TEST(kiln.IsProductionStopped());
    // By the player, running: it works again
    setPaused(BRICK, false, false);
    BOOST_TEST(!kiln.IsProductionStopped());
    RTTR_EXEC_TILL(4000, countInHQ(BRICK) == 11u);
}

BOOST_FIXTURE_TEST_CASE(SurplusByTheNumberOfWares, OutputChoiceFixture)
{
    // Forecast mode: the values come from the forecast, the surplus from the number of wares (Nutzer, 2026-11-21). The
    // new kiln counts with what it will make: 5 bricks and its production are more than 8, it still works
    this->SetWareDistribution(true, {});
    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 10), true);
    hq().AddToInventory(GoodCounts::make(BRICK, 5), true);
    const MapPoint pos = placeConnectedBuilding(KILN);
    const nobUsual& kiln = *world.GetSpecObj<nobUsual>(pos);
    RTTR_EXEC_TILL(2000, kiln.HasWorker());
    civ::updateDistribution(world, 0);
    BOOST_TEST(distribution().GetNumber(BRICK) > 8 * civ::AMOUNT_UNIT);
    BOOST_TEST((distribution().GetState(BRICK) == civ::WareDistState::Valued));
    BOOST_TEST(!kiln.IsProductionStopped());
    // 9 bricks: surplus, the kiln stops
    hq().AddToInventory(GoodCounts::make(BRICK, 4), true);
    civ::updateDistribution(world, 0);
    BOOST_TEST((distribution().GetState(BRICK) == civ::WareDistState::Surplus));
    BOOST_TEST(kiln.IsProductionStopped());
}

BOOST_FIXTURE_TEST_CASE(PlannedDemandCountsInTheForecast, OutputChoiceFixture)
{
    // The planned demand of the AI ("geplant") counts in the forecast, not in the number of wares. Entries without an
    // amount are dropped
    hq().AddToInventory(GoodCounts::make(BRICK, 6), true);
    std::vector<civ::PlannedDemand> planned(2);
    planned[0].wares = {BRICK};
    planned[0].amount = 4;
    planned[1].wares = {POT};
    planned[1].amount = 0;
    this->SetPlannedDemand(planned);
    BOOST_TEST_REQUIRE(distribution().GetPlanned().size() == 1u);
    civ::updateDistribution(world, 0);
    BOOST_TEST(distribution().GetNumber(BRICK) == 6 * civ::AMOUNT_UNIT);
    this->SetWareDistribution(true, {});
    BOOST_TEST(distribution().GetNumber(BRICK) == 2 * civ::AMOUNT_UNIT);
    this->SetPlannedDemand({});
    civ::updateDistribution(world, 0);
    BOOST_TEST(distribution().GetNumber(BRICK) == 6 * civ::AMOUNT_UNIT);
}

BOOST_FIXTURE_TEST_CASE(NotWithoutCivMode, OutputChoiceFixture)
{
    ggs.setSelection(AddonId::CIVILIZATION, 0);
    this->SetWareDistribution(true, {});
    this->ChangeWareOrders(JUG, 3);
    std::vector<civ::PlannedDemand> planned(1);
    planned[0].wares = {JUG};
    planned[0].amount = 2;
    this->SetPlannedDemand(planned);
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    BOOST_TEST(!distribution().UsesForecast());
    BOOST_TEST(distribution().GetOrders(JUG) == 0u);
    BOOST_TEST(distribution().GetPlanned().empty());
}

BOOST_FIXTURE_TEST_CASE(SerializeTheDistribution, OutputChoiceFixture)
{
    setValue(POT, 700);
    setPaused(JUG, false, true);
    this->ChangeWareOrders(POT, 3);
    std::vector<civ::PlannedDemand> planned(1);
    planned[0].wares = {POT, JUG};
    planned[0].amount = 7;
    this->SetPlannedDemand(planned);
    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 2), true);
    const MapPoint pos = placeConnectedBuilding(POTTERY);
    RTTR_EXEC_TILL(3000, world.GetSpecObj<nobUsual>(pos)->is_working);
    BOOST_TEST_REQUIRE(distribution().GetOrders(POT) == 2u);

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    BOOST_TEST(!distribution().UsesForecast());
    BOOST_TEST(!distribution().GetSetting(POT).autoValue);
    BOOST_TEST(distribution().GetSetting(POT).manualValue == 700u);
    BOOST_TEST(!distribution().GetSetting(JUG).autoPause);
    BOOST_TEST(distribution().GetSetting(JUG).paused);
    BOOST_TEST(distribution().GetOrders(POT) == 2u);
    BOOST_TEST_REQUIRE(distribution().GetPlanned().size() == 1u);
    BOOST_TEST((distribution().GetPlanned()[0].wares == std::vector<GoodType>{JUG, POT}));
    BOOST_TEST(distribution().GetPlanned()[0].amount == 7u);
    // The pot being made comes out
    RTTR_EXEC_TILL(1000, countInHQ(POT) == 1u);
    const nobUsual& pottery = *world.GetSpecObj<nobUsual>(pos);
    BOOST_TEST(pottery.GetFinishedGF() < em.GetCurrentGF());
    BOOST_TEST(pottery.GetWorkerSinceGF() != nobUsual::NO_GF);
}

namespace {
using BiggerCivWorldFixture = WorldWithGCExecution<1, 24, 22>;
} // namespace

BOOST_FIXTURE_TEST_CASE(AIBuildsToolWorkshop, BiggerCivWorldFixture)
{
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    // The research costs a copper saw: the planner of the new AI wants the copperworks
    WorldDescription desc = makeDesc();
    TechnologyDesc tech;
    tech.name = "carpentry";
    tech.title = "Carpentry";
    tech.cost = {{"CopperSaw", 1}};
    desc.technologies.add(tech);
    world.SetTechTree(std::make_unique<civ::TechTree>(desc));
    civ::startGame(world);
    addStartResources();
    // Its input in stock (it cannot be made here)
    world.GetPlayer(curPlayer).GetHQ()->AddToInventory(GoodCounts::make(GoodType::Coal, 30), true);
    world.InitAfterLoad();
    const BuildingRegister& buildings = world.GetPlayer(curPlayer).GetBuildingRegister();
    const auto hasCopperworks = [&buildings]() {
        return !buildings.GetBuildings(COPPERWORKS).empty()
               || helpers::contains_if(buildings.GetBuildingSites(), [](const noBuildingSite* site) {
                      return site->GetBuildingType() == COPPERWORKS;
                  });
    };
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    for(unsigned gf = 0; gf < 12000 && !hasCopperworks();)
    {
        std::vector<gc::GameCommandPtr> aiGcs = ai->FetchGameCommands();
        for(unsigned i = 0; i < 5; i++, gf++)
        {
            em.ExecuteNextGF();
            ai->RunGF(em.GetCurrentGF(), i == 0);
        }
        for(gc::GameCommandPtr& gc : aiGcs)
            gc->Execute(world, curPlayer);
    }
    BOOST_TEST(hasCopperworks());
}

BOOST_AUTO_TEST_SUITE_END()
