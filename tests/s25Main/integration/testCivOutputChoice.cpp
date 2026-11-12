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

// Workshops with several products (outputs) chosen like the metalworks (outputChoice = "tools") or by an own regulator
// (rttr:AddRegulator in distribution.lua)
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

WorkshopDesc makeWorkshop(const std::string& name, const std::string& input, std::vector<TagQuery> outputs,
                          const std::string& outputChoice)
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
    desc.outputChoice = outputChoice;
    desc.workTime = 2;
    return desc;
}

/// Copper axes and saws (wares 0 and 1) made from coal by the copperworks (workshop 0, outputChoice = "tools"),
/// jugs and pots (wares 2 and 3) made from stones by the pottery (workshop 1, regulator "Pottery": jugs 10, pots 0)
WorldDescription makeDesc()
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("CopperAxe", "Axe", "Axe"));
    desc.wares.push_back(makeWare("CopperSaw", "Saw", "Saw"));
    desc.wares.push_back(makeWare("Jug", "Jug", "Water"));
    desc.wares.push_back(makeWare("Pot", "Pot", "Water"));
    desc.workshops.push_back(makeWorkshop("Copperworks", "Coal", {{"CopperAxe"}, {"CopperSaw"}}, "tools"));
    desc.workshops.push_back(makeWorkshop("Pottery", "Stones", {{"Jug"}, {"Pot"}}, "Pottery"));
    RegulatorDesc regulator;
    regulator.name = "Pottery";
    regulator.title = "PotteryTitle";
    regulator.entries = {{{"Jug"}, "Jugs", 10}, {{"Pot"}, "Pots", 0}};
    desc.regulators.push_back(regulator);
    return desc;
}

constexpr GoodType COPPER_AXE = GoodType::Custom0;
const GoodType COPPER_SAW = getCustomGood(1);
const GoodType JUG = getCustomGood(2);
const GoodType POT = getCustomGood(3);
constexpr BuildingType COPPERWORKS = BuildingType::Custom0;
const BuildingType POTTERY = getCustomBuilding(1);

struct OutputChoiceFixture : public WorldWithGCExecution1P
{
    OutputChoiceFixture()
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        ggs.setSelection(AddonId::TOOL_ORDERING, 1);
        ggs.setSelection(AddonId::METALWORKSBEHAVIORONZERO, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(makeDesc()));
        civ::startGame(world);
        // No tool is wanted
        std::fill(toolSettings.begin(), toolSettings.end(), 0);
        std::fill(noOrders.begin(), noOrders.end(), 0);
        this->ChangeTools(toolSettings, noOrders.data());
    }

    ToolSettings toolSettings;
    helpers::EnumArray<int8_t, Tool> noOrders;

    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    unsigned countInHQ(GoodType good) { return hq().GetNumRealWares(good); }
    civ::PlayerResearch& research() { return world.GetPlayer(0).GetResearch(); }
    const civ::Content& content() { return world.GetTechTree().GetContent(); }

    /// Place a finished building connected to the HQ
    MapPoint placeConnectedBuilding(BuildingType bld)
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + Position(4, 2));
        BuildingFactory::CreateBuilding(world, bld, pos, 0, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<nobUsual>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        return pos;
    }

    void orderTools(Tool tool, int8_t amount)
    {
        helpers::EnumArray<int8_t, Tool> orders = noOrders;
        orders[tool] = amount;
        this->ChangeTools(toolSettings, orders.data());
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(OutputsAndRegulatorsFromLua)
{
    // The regulator may be defined after the workshop (distribution.lua comes after workshops.lua)
    const std::string base = R"(
rttr:AddWare{ name = "CopperAxe", kind = "Axe", look = "Axe", transport = "Axe" }
rttr:AddWare{ name = "CopperSaw", kind = "Saw", properties = { "Copper" }, look = "Saw", transport = "Saw" }
rttr:AddWare{ name = "Jug", kind = "Jug", look = "Water", transport = "Water" }
rttr:AddWare{ name = "Pot", kind = "Pot", properties = { "Clay" }, look = "Water", transport = "Water" }
rttr:AddWorkshop{ name = "Copperworks", size = "medium", cost = { Boards = 2 }, look = "Metalworks",
    worker = { tool = "Tongs", look = "Metalworker" }, inputs = { "IronOre", "Coal" },
    outputs = { "CopperAxe", { "Saw", "Copper" }, "Bow" }, outputChoice = "tools", workTime = 120 }
rttr:AddWorkshop{ name = "Pottery", size = "small", cost = { Boards = 2 }, look = "Bakery",
    worker = { tool = "none", look = "Baker" }, inputs = { "Stones" }, outputs = { "Jug", "Pot" },
    outputChoice = "Pottery", workTime = 40 }
rttr:AddWorkshop{ name = "Single", size = "small", cost = { Boards = 2 }, look = "Bakery",
    worker = { tool = "none", look = "Baker" }, inputs = { "Stones" }, outputs = { "Jug" }, workTime = 40 }
rttr:AddRegulator{ name = "Pottery", title = "Töpferei", entries = {
    { tags = "Jug", title = "Krüge", value = 8 },
    { tags = { "Pot", "Clay" }, value = 3 },
} }
)";
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(base, desc));
    BOOST_TEST_REQUIRE(desc.workshops.size() == 3u);
    BOOST_TEST_REQUIRE(desc.workshops[0].outputs.size() == 3u);
    BOOST_TEST(desc.workshops[0].output.empty());
    BOOST_TEST((desc.workshops[0].outputs[1] == TagQuery{"Saw", "Copper"}));
    BOOST_TEST(desc.workshops[0].outputChoice == "tools");
    BOOST_TEST_REQUIRE(desc.regulators.size() == 1u);
    BOOST_TEST(desc.regulators[0].title == "Töpferei");

    const civ::Content content(desc);
    // Tool types: built-in tools and wares with the identifier of a built-in tool as tag
    BOOST_TEST((content.GetToolTypeOf(GoodType::Saw) == Tool::Saw));
    BOOST_TEST((content.GetToolTypeOf(COPPER_AXE) == Tool::Axe));
    BOOST_TEST((content.GetToolTypeOf(COPPER_SAW) == Tool::Saw));
    BOOST_TEST(!content.GetToolTypeOf(JUG));
    BOOST_TEST(!content.GetToolTypeOf(GoodType::Boards));

    const civ::CustomBuilding& copperworks = *content.GetBuilding(COPPERWORKS);
    BOOST_TEST((copperworks.productChoice == civ::ProductChoice::Tools));
    BOOST_TEST((copperworks.productChoices == std::vector<GoodType>{COPPER_AXE, COPPER_SAW, GoodType::Bow}));
    BOOST_TEST((copperworks.productChoiceTools == std::vector<Tool>{Tool::Axe, Tool::Saw, Tool::Bow}));
    const auto* producedWare = boost::variant2::get_if<GoodType>(&copperworks.workDesc.producedWare);
    BOOST_TEST_REQUIRE(producedWare);
    BOOST_TEST((*producedWare == COPPER_AXE));

    BOOST_TEST_REQUIRE(content.GetRegulators().size() == 1u);
    const civ::Regulator& regulator = content.GetRegulators()[0];
    BOOST_TEST(regulator.name == "Pottery");
    BOOST_TEST_REQUIRE(regulator.entries.size() == 2u);
    BOOST_TEST(regulator.entries[0].title == "Krüge");
    BOOST_TEST(regulator.entries[0].defaultValue == 8u);
    // Without a title the first tag is shown
    BOOST_TEST(regulator.entries[1].title == "Pot");
    BOOST_TEST(regulator.entries[1].defaultValue == 3u);
    const civ::CustomBuilding& pottery = *content.GetBuilding(POTTERY);
    BOOST_TEST((pottery.productChoice == civ::ProductChoice::Regulator));
    BOOST_TEST(pottery.regulator == 0u);
    BOOST_TEST((pottery.productChoices == std::vector<GoodType>{JUG, POT}));
    BOOST_TEST((pottery.productChoiceEntries == std::vector<unsigned>{0, 1}));
    // A single entry needs no choice
    const civ::CustomBuilding& single = *content.GetBuilding(getCustomBuilding(2));
    BOOST_TEST((single.productChoice == civ::ProductChoice::None));
    BOOST_TEST((single.product == JUG));

    const auto replaced = [&base](const std::string& from, const std::string& to) {
        std::string result = base;
        const auto pos = result.find(from);
        BOOST_TEST_REQUIRE(pos != std::string::npos);
        return result.replace(pos, from.size(), to);
    };
    // Invalid lua definitions
    std::string nineEntries = "rttr:AddRegulator{ name = \"Many\", entries = {";
    for(unsigned i = 0; i < 9; ++i)
        nineEntries += " { tags = \"Jug\" },";
    nineEntries += " } }";
    for(const std::string& invalid :
        {replaced("outputs = { \"Jug\", \"Pot\" }", "output = \"Jug\", outputs = { \"Jug\", \"Pot\" }"),
         replaced("outputs = { \"Jug\", \"Pot\" }", "outputs = { }"), replaced("outputChoice = \"Pottery\", ", ""),
         replaced("value = 8", "value = 11"), replaced("name = \"Pottery\", title", "name = \"tools\", title"),
         replaced("value = 8 }", "value = 8, typo = 1 }"), base + nineEntries,
         base + "rttr:AddRegulator{ name = \"Pottery\", entries = { { tags = \"Jug\" } } }",
         base + "rttr:AddRegulator{ name = \"Empty\", entries = { } }"})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(invalid, invalidDesc));
    }
    // Invalid content
    for(const std::string& invalid : {// An entry of outputs matches 2 wares (Saw and CopperSaw)
                                      replaced("{ \"Saw\", \"Copper\" }", "\"Saw\""),
                                      // No tool type
                                      replaced("\"Bow\" }, outputChoice", "\"Jug\" }, outputChoice"),
                                      // Tool type twice (CopperAxe and Axe)
                                      replaced("\"Bow\" }, outputChoice", "\"Axe\" }, outputChoice"),
                                      // Ware twice
                                      replaced("\"Bow\" }, outputChoice", "\"CopperAxe\" }, outputChoice"),
                                      // Unknown regulator
                                      replaced("outputChoice = \"Pottery\"", "outputChoice = \"Potter\""),
                                      // Pot matches no entry
                                      replaced("{ \"Pot\", \"Clay\" }", "{ \"Pot\", \"Glass\" }"),
                                      // Pot matches 2 entries
                                      replaced("tags = \"Jug\"", "tags = \"Pot\""),
                                      // Entry matches no ware
                                      replaced("tags = \"Jug\"", "tags = \"Vase\"")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST_REQUIRE(loadLua(invalid, invalidDesc));
        BOOST_CHECK_THROW(civ::Content{invalidDesc}, GameDataError);
    }
}

BOOST_AUTO_TEST_CASE(ShippedDistributionLuaIsValid)
{
    // The example in distribution.lua works when uncommented, the shipped values work with the other civ files
    const auto civFolder = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "civ";
    bnw::ifstream in(civFolder / "distribution.lua");
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto start = content.find("\nrttr:", content.find("Beispiel (auskommentiert"));
    const auto end = content.find("]]", start);
    BOOST_TEST_REQUIRE(start != std::string::npos);
    BOOST_TEST_REQUIRE(end != std::string::npos);
    rttr::test::TmpFolder tmp;
    {
        bnw::ofstream file(tmp / "default.lua");
        file << content.substr(start, end - start);
    }
    WorldDescription desc;
    {
        GameDataLoader loader(desc, tmp);
        BOOST_TEST_REQUIRE(loader.Load());
    }
    const civ::Content exampleContent(desc);
    BOOST_TEST_REQUIRE(exampleContent.GetRegulators().size() == 1u);
    BOOST_TEST((exampleContent.GetBuilding(BuildingType::Custom0)->productChoice == civ::ProductChoice::Regulator));

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
    BOOST_TEST((shippedContent.GetBuilding(*copperworks)->productChoice == civ::ProductChoice::Tools));
}

BOOST_FIXTURE_TEST_CASE(ToolWorkshopFollowsOrdersAndRegulator, OutputChoiceFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Coal, 10), true);
    const MapPoint bldPos = placeConnectedBuilding(COPPERWORKS);
    const nobUsual& bld = *world.GetSpecObj<nobUsual>(bldPos);
    RTTR_EXEC_TILL(2000, bld.HasWorker());
    RTTR_EXEC_TILL(3000, bld.GetNumWares(0) > 0u);
    // No order and all priorities 0 (with METALWORKSBEHAVIORONZERO): nothing is made
    RTTR_SKIP_GFS(500);
    BOOST_TEST(!bld.is_working);
    BOOST_TEST(countInHQ(COPPER_AXE) + countInHQ(COPPER_SAW) == 0u);

    // Ordered saws are made, the order is used up and the player gets the message
    PostBox& postbox = world.GetPostMgr().AddPostBox(0);
    postbox.Clear();
    orderTools(Tool::Saw, 2);
    BOOST_TEST(world.GetPlayer(0).GetToolsOrdered(Tool::Saw) == 2u);
    RTTR_EXEC_TILL(3000, countInHQ(COPPER_SAW) == 2u);
    BOOST_TEST(world.GetPlayer(0).GetToolsOrdered(Tool::Saw) == 0u);
    BOOST_TEST(countInHQ(COPPER_AXE) == 0u);
    BOOST_TEST(countInHQ(GoodType::Saw) == 0u);
    BOOST_TEST_REQUIRE(postbox.GetNumMsgs() == 1u);
    BOOST_TEST((postbox.GetMsg(0)->GetCategory() == PostCategory::Economy));
    RTTR_SKIP_GFS(500);
    BOOST_TEST(countInHQ(COPPER_SAW) == 2u);

    // Tools it can not make are not taken from the orders
    orderTools(Tool::Bow, 1);
    RTTR_SKIP_GFS(500);
    BOOST_TEST(world.GetPlayer(0).GetToolsOrdered(Tool::Bow) == 1u);
    BOOST_TEST(countInHQ(COPPER_AXE) + countInHQ(COPPER_SAW) == 2u);

    // With a priority for axes it makes axes
    toolSettings[Tool::Axe] = 10;
    this->ChangeTools(toolSettings, noOrders.data());
    RTTR_EXEC_TILL(3000, countInHQ(COPPER_AXE) >= 2u);
    BOOST_TEST(countInHQ(COPPER_SAW) == 2u);
    BOOST_TEST(world.GetPlayer(0).GetToolsOrdered(Tool::Bow) == 1u);
}

BOOST_FIXTURE_TEST_CASE(RegulatorChoosesProduct, OutputChoiceFixture)
{
    // Default from lua: jugs 10, pots 0
    using Values = civ::PlayerResearch::RegulatorValues;
    BOOST_TEST(research().GetRegulatorValues(content(), 0) == (Values{10, 0}), boost::test_tools::per_element());
    BOOST_TEST(research().GetRegulatorValues(content(), 1).empty());

    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 6), true);
    const unsigned numStones = countInHQ(GoodType::Stones);
    placeConnectedBuilding(POTTERY);
    RTTR_EXEC_TILL(8000, countInHQ(JUG) == numStones);
    BOOST_TEST(countInHQ(POT) == 0u);

    // Invalid commands are ignored, values above 10 are limited
    this->SetRegulatorValues(0, {1});
    this->SetRegulatorValues(1, {1, 1});
    BOOST_TEST(research().GetRegulatorValues(content(), 0) == (Values{10, 0}), boost::test_tools::per_element());
    this->SetRegulatorValues(0, {0, 50});
    BOOST_TEST(research().GetRegulatorValues(content(), 0) == (Values{0, 10}), boost::test_tools::per_element());

    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 4), true);
    RTTR_EXEC_TILL(8000, countInHQ(POT) == 4u);
    BOOST_TEST(countInHQ(JUG) == numStones);

    // All 0: equally distributed
    this->SetRegulatorValues(0, {0, 0});
    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 12), true);
    RTTR_EXEC_TILL(20000, countInHQ(JUG) + countInHQ(POT) == numStones + 4u + 12u);
    BOOST_TEST(countInHQ(JUG) > numStones);
    BOOST_TEST(countInHQ(POT) > 4u);
}

BOOST_FIXTURE_TEST_CASE(RegulatorIgnoredWithoutCivMode, OutputChoiceFixture)
{
    ggs.setSelection(AddonId::CIVILIZATION, 0);
    this->SetRegulatorValues(0, {1, 2});
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    using Values = civ::PlayerResearch::RegulatorValues;
    BOOST_TEST(research().GetRegulatorValues(content(), 0) == (Values{10, 0}), boost::test_tools::per_element());
}

BOOST_FIXTURE_TEST_CASE(SerializeRegulatorAndChosenTool, OutputChoiceFixture)
{
    this->SetRegulatorValues(0, {3, 7});
    hq().AddToInventory(GoodCounts::make(GoodType::Coal, 2), true);
    const MapPoint bldPos = placeConnectedBuilding(COPPERWORKS);
    RTTR_EXEC_TILL(3000, world.GetSpecObj<nobUsual>(bldPos)->GetNumWares(0) > 0u);
    // Save while the ordered saw is being made (the order is already taken)
    orderTools(Tool::Saw, 1);
    RTTR_EXEC_TILL(1000, world.GetSpecObj<nobUsual>(bldPos)->is_working);
    BOOST_TEST_REQUIRE(world.GetPlayer(0).GetToolsOrdered(Tool::Saw) == 0u);

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    using Values = civ::PlayerResearch::RegulatorValues;
    BOOST_TEST(research().GetRegulatorValues(content(), 0) == (Values{3, 7}), boost::test_tools::per_element());
    RTTR_EXEC_TILL(1000, countInHQ(COPPER_SAW) == 1u);
    BOOST_TEST(countInHQ(COPPER_AXE) == 0u);
    // Nothing more is made
    RTTR_SKIP_GFS(500);
    BOOST_TEST(countInHQ(COPPER_SAW) == 1u);
    BOOST_TEST(countInHQ(GoodType::Coal) + world.GetSpecObj<nobUsual>(bldPos)->GetNumWares(0) == 1u);
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
