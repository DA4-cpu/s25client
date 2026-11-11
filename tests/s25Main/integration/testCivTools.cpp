// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BuildingRegister.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "SerializedGameData.h"
#include "Ware.h"
#include "addons/const_addons.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "factories/BuildingFactory.h"
#include "figures/nofBuildingWorker.h"
#include "lua/GameDataLoader.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "gameTypes/GameTypesOutput.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>

namespace bnw = boost::nowide;

// Tool reform of the civilization mode: Workers are recruited without tools, the tools are used in the buildings
BOOST_AUTO_TEST_SUITE(CivToolsSuite)

namespace {
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

ToolDesc makeTool(const std::string& ware, const std::string& material, unsigned durability, unsigned speed)
{
    ToolDesc desc;
    desc.ware = ware;
    desc.material = material;
    desc.durability = durability;
    desc.speed = speed;
    return desc;
}

/// Materials iron (priority 5) and copper (priority 3).
/// The woodcutter uses an axe: the built-in one (iron) or the new copper axe (ware 0).
/// The sawmill works slowly without tools and uses the new test saw (ware 1, copper, 10 work steps).
/// Replacements are ordered at once (the carriers are busy with the wood, so they take some time)
WorldDescription makeDesc()
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("CopperAxe", "Axe", "Axe"));
    desc.wares.push_back(makeWare("TestSaw", "", "Saw"));
    ToolMaterialDesc iron, copper;
    iron.name = "Iron";
    iron.title = "Eisen";
    iron.priority = 5;
    copper.name = "Copper";
    copper.title = "Kupfer";
    copper.priority = 3;
    desc.toolMaterials = {iron, copper};
    desc.tools = {makeTool("Axe", "Iron", 4, 100), makeTool("CopperAxe", "Copper", 2, 50),
                  makeTool("TestSaw", "Copper", 10, 100)};
    BuildingToolsDesc woodcutter, sawmill;
    woodcutter.baseSpeed = 0;
    woodcutter.tools = {{"Axe"}};
    sawmill.baseSpeed = 25;
    sawmill.tools = {{"TestSaw"}};
    desc.buildingTools = {{"Woodcutter", woodcutter}, {"Sawmill", sawmill}};
    desc.toolReorderPercent = 100;
    return desc;
}

constexpr GoodType COPPER_AXE = GoodType::Custom0;
const GoodType TEST_SAW = getCustomGood(1);

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

struct ToolWorldFixture : public WorldWithGCExecution1P
{
    explicit ToolWorldFixture(const WorldDescription& desc = makeDesc())
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        // Some workers who came with their tools
        hq().AddToInventory(PeopleCounts::make(Job::Woodcutter, 2), true);
        hq().AddToInventory(PeopleCounts::make(Job::Helper, 10), true);
        civ::startGame(world);
    }

    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    unsigned countInHQ(GoodType good) { return hq().GetNumRealWares(good); }
    unsigned countInHQ(Job job) { return hq().GetNumRealFigures(job); }
    const Inventory& inventory() { return world.GetPlayer(0).GetInventory(); }

    /// Place a finished building connected to the HQ
    nobUsual& placeConnectedBuilding(BuildingType bld, Position offset = Position(4, 2))
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + offset);
        BuildingFactory::CreateBuilding(world, bld, pos, 0, Nation::Romans);
        auto* building = world.GetSpecObj<nobUsual>(pos);
        BOOST_TEST_REQUIRE(building);
        this->BuildRoadForBlds(pos, hqPos);
        return *building;
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(ToolsFromLua)
{
    const std::string base = R"(
rttr:AddWare{ name = "CopperScythe", kind = "Scythe", look = "Scythe", transport = "Scythe" }
rttr:AddWare{ name = "Plough", look = "Hammer", transport = "Hammer" }
rttr:AddToolMaterial{ name = "Iron", title = "Eisen", priority = 7 }
rttr:AddToolMaterial{ name = "Copper" }
rttr:AddTool{ ware = "Scythe", material = "Iron", durability = 100, speed = 100 }
rttr:AddTool{ ware = "CopperScythe", material = "Copper", durability = 40, speed = 60 }
rttr:AddTool{ ware = "Plough", material = "Iron", durability = 300, speed = 50 }
rttr:AddTool{ ware = "Saw", material = "Iron", durability = 80, speed = 100 }
rttr:SetToolReorder(20)
rttr:SetBuildingTools{
    Farm = { baseSpeed = 10, tools = { "Scythe", "Plough" } },
    Mill = { baseSpeed = 70 },
}
rttr:AddWorkshop{ name = "Joinery", size = "small", cost = { Boards = 2 }, look = "Sawmill",
    worker = { tool = "Saw", look = "Carpenter" }, inputs = { "Boards" }, output = "Stones", workTime = 5 }
rttr:AddWorkshop{ name = "Thresher", size = "small", cost = { Boards = 2 }, look = "Mill",
    worker = { tool = "none", look = "Miller" }, inputs = { "Grain" }, output = "Flour", workTime = 5,
    tools = { baseSpeed = 20, tools = { "Plough" } } }
)";
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(base, desc));
    const civ::Content content(desc);
    BOOST_TEST_REQUIRE(content.HasToolReform());
    BOOST_TEST_REQUIRE(content.GetToolMaterials().size() == 2u);
    BOOST_TEST(content.GetToolMaterials()[0].title == "Eisen");
    BOOST_TEST(content.GetToolMaterials()[0].defaultPriority == 7u);
    BOOST_TEST(content.GetToolMaterials()[1].title == "Copper");
    BOOST_TEST(content.GetToolMaterials()[1].defaultPriority == 5u);

    const GoodType copperScythe = GoodType::Custom0;
    const GoodType plough = getCustomGood(1);
    BOOST_TEST_REQUIRE(content.GetTool(GoodType::Scythe));
    BOOST_TEST(content.GetTool(GoodType::Scythe)->material == 0u);
    BOOST_TEST(content.GetTool(GoodType::Scythe)->durability == 100u);
    BOOST_TEST_REQUIRE(content.GetTool(copperScythe));
    BOOST_TEST(content.GetTool(copperScythe)->material == 1u);
    BOOST_TEST(content.GetTool(copperScythe)->speed == 60u);
    BOOST_TEST(!content.GetTool(GoodType::Axe));
    // 20% of the durability, at least 1
    BOOST_TEST(content.GetToolReorderSteps(GoodType::Scythe) == 20u);
    BOOST_TEST(content.GetToolReorderSteps(copperScythe) == 8u);

    // Farm: 2 slots, all scythes fit to the first one
    const civ::BuildingTools& farm = content.GetBuildingTools(BuildingType::Farm);
    BOOST_TEST(farm.baseSpeed == 10u);
    BOOST_TEST_REQUIRE(farm.slots.size() == 2u);
    BOOST_TEST((farm.slots[0].wares == (std::vector<GoodType>{GoodType::Scythe, copperScythe})));
    BOOST_TEST((farm.slots[1].wares == std::vector<GoodType>{plough}));
    // No tools, only a speed
    BOOST_TEST(content.GetBuildingTools(BuildingType::Mill).baseSpeed == 70u);
    BOOST_TEST(content.GetBuildingTools(BuildingType::Mill).slots.empty());
    // Not listed: The tool of the worker if it is a tool of the reform, otherwise normal speed
    const civ::BuildingTools& sawmill = content.GetBuildingTools(BuildingType::Sawmill);
    BOOST_TEST(sawmill.baseSpeed == 0u);
    BOOST_TEST_REQUIRE(sawmill.slots.size() == 1u);
    BOOST_TEST((sawmill.slots[0].wares == std::vector<GoodType>{GoodType::Saw}));
    BOOST_TEST(content.GetBuildingTools(BuildingType::Woodcutter).baseSpeed == 100u);
    BOOST_TEST(content.GetBuildingTools(BuildingType::Woodcutter).slots.empty());
    BOOST_TEST(content.GetBuildingTools(BuildingType::Brewery).baseSpeed == 100u);
    BOOST_TEST(content.GetBuildingTools(BuildingType::Barracks).slots.empty());
    // Workshops: the tool of the worker or their own tools
    const civ::BuildingTools& joinery = content.GetBuildingTools(BuildingType::Custom0);
    BOOST_TEST(joinery.baseSpeed == 0u);
    BOOST_TEST_REQUIRE(joinery.slots.size() == 1u);
    BOOST_TEST((joinery.slots[0].wares == std::vector<GoodType>{GoodType::Saw}));
    const civ::BuildingTools& thresher = content.GetBuildingTools(getCustomBuilding(1));
    BOOST_TEST(thresher.baseSpeed == 20u);
    BOOST_TEST_REQUIRE(thresher.slots.size() == 1u);
    BOOST_TEST((thresher.slots[0].wares == std::vector<GoodType>{plough}));

    // Jobs of buildings need no tool anymore, the others do
    {
        auto contentPtr = std::make_shared<const civ::Content>(desc);
        civ::setActiveContent(contentPtr);
        BOOST_TEST(contentPtr->IsRecruitedWithoutTool(Job::Woodcutter));
        BOOST_TEST(contentPtr->IsRecruitedWithoutTool(Job::Miller));
        BOOST_TEST(contentPtr->IsRecruitedWithoutTool(getCustomJob(0)));
        for(const Job job : {Job::Builder, Job::Planer, Job::Geologist, Job::Scout, Job::Private, Job::PackDonkey})
            BOOST_TEST(!contentPtr->IsRecruitedWithoutTool(job));
        civ::resetActiveContent(contentPtr.get());
    }

    const auto replaced = [&base](const std::string& from, const std::string& to) {
        std::string result = base;
        const auto pos = result.find(from);
        BOOST_TEST_REQUIRE(pos != std::string::npos);
        return result.replace(pos, from.size(), to);
    };
    for(const std::string& invalid :
        {replaced("priority = 7", "priority = 11"), replaced("durability = 100", "durability = 0"),
         replaced("speed = 60", "speed = 2000"), replaced("rttr:SetToolReorder(20)", "rttr:SetToolReorder(101)"),
         replaced("Mill = { baseSpeed = 70 }", "Mill = { baseSpeed = 0 }"),
         replaced("Mill = { baseSpeed = 70 }", "Mill = { speed = 70 }"),
         replaced("tools = { \"Scythe\", \"Plough\" }",
                  "tools = { \"Scythe\", \"Plough\", \"Saw\", \"Saw\", \"Saw\" }"),
         base + "\nrttr:AddToolMaterial{ name = \"Iron\" }",
         base + "\nrttr:AddTool{ ware = \"Saw\", material = \"Copper\", durability = 1, speed = 1 }",
         base + "\nrttr:SetBuildingTools{ Farm = { baseSpeed = 50 } }"})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(invalid, invalidDesc));
    }
    // Invalid content
    for(const std::string& invalid :
        {replaced("ware = \"Plough\"", "ware = \"Plow\""), replaced("material = \"Copper\"", "material = \"Bronze\""),
         replaced("tools = { \"Scythe\", \"Plough\" }", "tools = { \"Sickle\" }"),
         replaced("tools = { \"Scythe\", \"Plough\" }", "tools = { \"Scythe\", \"CopperScythe\" }"),
         replaced("Mill = { baseSpeed = 70 }", "Barracks = { baseSpeed = 70 }"),
         replaced("Mill = { baseSpeed = 70 }", "Joinery = { baseSpeed = 70 }"),
         replaced("rttr:AddToolMaterial{ name = \"Iron\", title = \"Eisen\", priority = 7 }\n"
                  "rttr:AddToolMaterial{ name = \"Copper\" }",
                  "")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        // Either the description or the content is invalid
        if(loadLua(invalid, invalidDesc))
            BOOST_CHECK_THROW(civ::Content{invalidDesc}, GameDataError);
    }
}

BOOST_FIXTURE_TEST_CASE(WithoutMaterialsNothingChanges, WorldWithGCExecution1P)
{
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    world.SetTechTree(std::make_unique<civ::TechTree>(WorldDescription()));
    civ::startGame(world);
    BOOST_TEST(!world.GetTechTree().GetContent().HasToolReform());
    nobBaseWarehouse& hq = *world.GetPlayer(0).GetHQ();
    hq.AddToInventory(PeopleCounts::make(Job::Woodcutter, 2), true);
    hq.AddToInventory(PeopleCounts::make(Job::Helper, 5), true);
    const unsigned numAxes = hq.GetNumRealWares(GoodType::Axe);
    civ::startNewGame(world);
    BOOST_TEST(hq.GetNumRealFigures(Job::Woodcutter) == 2u);
    BOOST_TEST(hq.GetNumRealWares(GoodType::Axe) == numAxes);
    BOOST_TEST(hq.CanRecruit(Job::Woodcutter) == (numAxes > 0u));
    BOOST_TEST(!hq.IsRecruitedWithoutTool(Job::Woodcutter));

    // Buildings have no tools and work at normal speed
    const MapPoint pos = world.MakeMapPoint(hqPos + Position(4, 2));
    BuildingFactory::CreateBuilding(world, BuildingType::Sawmill, pos, 0, Nation::Romans);
    auto* bld = world.GetSpecObj<nobUsual>(pos);
    BOOST_TEST_REQUIRE(bld);
    BOOST_TEST(bld->GetToolSlots().empty());
    BOOST_TEST(bld->GetWorkSpeed() == 100u);
    BOOST_TEST(bld->BeginWorkStep());
    BOOST_TEST(bld->ScaleWorkDuration(96) == 96u);
}

BOOST_FIXTURE_TEST_CASE(StartWorkersBecomeHelpersAndTools, ToolWorldFixture)
{
    hq().AddToInventory(PeopleCounts::make(Job::Miller, 1), true);
    hq().AddToInventory(PeopleCounts::make(Job::Builder, 2), true);
    hq().AddToInventory(PeopleCounts::make(Job::Geologist, 1), true);
    const unsigned numWoodcutters = countInHQ(Job::Woodcutter);
    const unsigned numFishers = countInHQ(Job::Fisher);
    const unsigned numMillers = countInHQ(Job::Miller);
    const unsigned numBuilders = countInHQ(Job::Builder);
    const unsigned numGeologists = countInHQ(Job::Geologist);
    const unsigned numSoldiers = countInHQ(Job::Private);
    const unsigned numHelpers = countInHQ(Job::Helper);
    const unsigned numAxes = countInHQ(GoodType::Axe);
    const unsigned numRods = countInHQ(GoodType::RodAndLine);
    const unsigned numHammers = countInHQ(GoodType::Hammer);
    BOOST_TEST_REQUIRE(numWoodcutters >= 2u);

    civ::startNewGame(world);
    // Workers of buildings: helper + tool (if they had one)
    BOOST_TEST(countInHQ(Job::Woodcutter) == 0u);
    BOOST_TEST(countInHQ(Job::Fisher) == 0u);
    BOOST_TEST(countInHQ(Job::Miller) == 0u);
    BOOST_TEST(countInHQ(GoodType::Axe) == numAxes + numWoodcutters);
    BOOST_TEST(countInHQ(GoodType::RodAndLine) == numRods + numFishers);
    BOOST_TEST(countInHQ(Job::Helper) >= numHelpers + numWoodcutters + numFishers + numMillers);
    BOOST_TEST(hq().GetNumVisualFigures(Job::Woodcutter) == 0u);
    BOOST_TEST(hq().GetNumVisualFigures(Job::Helper) == countInHQ(Job::Helper));
    BOOST_TEST(hq().GetNumVisualWares(GoodType::Axe) == countInHQ(GoodType::Axe));
    // The others stay
    BOOST_TEST(countInHQ(Job::Builder) == numBuilders);
    BOOST_TEST(countInHQ(Job::Geologist) == numGeologists);
    BOOST_TEST(countInHQ(Job::Private) == numSoldiers);
    BOOST_TEST(countInHQ(GoodType::Hammer) == numHammers);
    // Same for the inventory of the player
    BOOST_TEST(inventory()[Job::Woodcutter] == 0u);
    BOOST_TEST(inventory()[Job::Helper] == countInHQ(Job::Helper));
    BOOST_TEST(inventory()[GoodType::Axe] == countInHQ(GoodType::Axe));
}

BOOST_FIXTURE_TEST_CASE(WorkersNeedNoToolButBuildingsDo, ToolWorldFixture)
{
    civ::startNewGame(world);
    const unsigned numHelpers = countInHQ(Job::Helper);
    BOOST_TEST_REQUIRE(numHelpers > 0u);
    BOOST_TEST(hq().IsRecruitedWithoutTool(Job::Woodcutter));
    BOOST_TEST(hq().CanRecruit(Job::Woodcutter));
    BOOST_TEST(hq().CanRecruit(Job::Carpenter));
    // Builders and geologists still need their hammer
    BOOST_TEST(!hq().IsRecruitedWithoutTool(Job::Builder));
    BOOST_TEST(hq().CanRecruit(Job::Builder) == (countInHQ(GoodType::Hammer) > 0u));
    BOOST_TEST(hq().CanRecruit(Job::Geologist) == (countInHQ(GoodType::Hammer) > 0u));

    // The sawmill gets its carpenter although there is no saw at all
    BOOST_TEST_REQUIRE(countInHQ(TEST_SAW) == 0u);
    const unsigned numSaws = countInHQ(GoodType::Saw);
    hq().AddToInventory(GoodCounts::make(GoodType::Wood, 5), true);
    const nobUsual& sawmill = placeConnectedBuilding(BuildingType::Sawmill);
    BOOST_TEST_REQUIRE(sawmill.GetToolSlots().size() == 1u);
    RTTR_EXEC_TILL(1000, sawmill.HasWorker());
    BOOST_TEST(countInHQ(GoodType::Saw) == numSaws);
    BOOST_TEST(inventory()[Job::Carpenter] == 1u);
    BOOST_TEST(countInHQ(Job::Helper) < numHelpers);
    // Without its tool it works with its base speed (25%)
    BOOST_TEST((sawmill.GetToolSlots()[0].tool == GoodType::Nothing));
    BOOST_TEST(sawmill.GetWorkSpeed() == 25u);
    const unsigned numBoards = inventory()[GoodType::Boards];
    RTTR_EXEC_TILL(5000, inventory()[GoodType::Boards] > numBoards);
    BOOST_TEST(sawmill.ScaleWorkDuration(100) == 400u);

    // The woodcutter does not work at all without an axe (base speed 0)
    const unsigned numAxes = countInHQ(GoodType::Axe);
    this->SetToolMaterialPriorities({0, 0}); // order no tools
    const nobUsual& woodcutter = placeConnectedBuilding(BuildingType::Woodcutter, Position(-5, 2));
    RTTR_EXEC_TILL(1000, woodcutter.HasWorker());
    RTTR_SKIP_GFS(500);
    BOOST_TEST(woodcutter.GetWorkSpeed() == 0u);
    BOOST_TEST((woodcutter.GetToolSlots()[0].tool == GoodType::Nothing));
    BOOST_TEST(!woodcutter.GetToolSlots()[0].ordered);
    BOOST_TEST(woodcutter.GetNumMissingTools(GoodType::Axe) == 1u);
    BOOST_TEST(woodcutter.GetNumMissingTools(COPPER_AXE) == 1u);
    BOOST_TEST(woodcutter.GetNumMissingTools(GoodType::Saw) == 0u);
    BOOST_TEST((woodcutter.GetWorker()->GetState() == nofBuildingWorker::State::WaitingForWaresOrProductionStopped));
    BOOST_TEST(countInHQ(GoodType::Axe) == numAxes);
}

BOOST_FIXTURE_TEST_CASE(ToolsWearOutAndAreReplaced, ToolWorldFixture)
{
    civ::startNewGame(world);
    hq().AddToInventory(GoodCounts::make(TEST_SAW, 2), true);
    const nobUsual& sawmill = placeConnectedBuilding(BuildingType::Sawmill);
    const nobUsual::ToolSlot& slot = sawmill.GetToolSlots()[0];
    // The building orders its tool. In the building it is not part of the inventory anymore
    RTTR_EXEC_TILL(2000, slot.tool == TEST_SAW);
    BOOST_TEST(slot.stepsLeft == 10u);
    hq().AddToInventory(GoodCounts::make(GoodType::Wood, 40), true);
    BOOST_TEST((slot.spare == GoodType::Nothing));
    BOOST_TEST(countInHQ(TEST_SAW) == 1u);
    BOOST_TEST(inventory()[TEST_SAW] == 1u);
    BOOST_TEST(sawmill.GetWorkSpeed() == 125u);
    RTTR_EXEC_TILL(1000, sawmill.HasWorker());

    // The replacement is ordered with the first work step and waits in the building
    RTTR_EXEC_TILL(6000, slot.spare == TEST_SAW);
    BOOST_TEST((slot.tool == TEST_SAW));
    BOOST_TEST(slot.stepsLeft < 10u);
    BOOST_TEST(slot.stepsLeft > 0u);
    BOOST_TEST(!slot.ordered);
    BOOST_TEST(countInHQ(TEST_SAW) == 0u);
    BOOST_TEST(inventory()[TEST_SAW] == 0u);
    BOOST_TEST(sawmill.GetWorkSpeed() == 125u);
    BOOST_TEST(sawmill.GetNumMissingTools(TEST_SAW) == 0u);
    // Steps are faster with the tool
    BOOST_TEST(sawmill.ScaleWorkDuration(100) == 80u);

    // The tool breaks: The replacement is used
    RTTR_EXEC_TILL(8000, slot.spare == GoodType::Nothing);
    BOOST_TEST((slot.tool == TEST_SAW));
    BOOST_TEST(slot.stepsLeft == 10u);
    // No more saws: After the last one the sawmill works with its base speed again
    RTTR_EXEC_TILL(12000, slot.tool == GoodType::Nothing);
    BOOST_TEST(sawmill.GetWorkSpeed() == 25u);
    BOOST_TEST(sawmill.GetNumMissingTools(TEST_SAW) == 1u);
    BOOST_TEST(!slot.ordered);
    // 2 saws with 10 steps each
    BOOST_TEST(inventory()[GoodType::Boards] >= 19u);

    // A new saw is fetched as soon as there is one
    hq().AddToInventory(GoodCounts::make(TEST_SAW, 1), true);
    RTTR_EXEC_TILL(6000, slot.tool == TEST_SAW);
    BOOST_TEST(sawmill.GetWorkSpeed() == 125u);
}

BOOST_FIXTURE_TEST_CASE(MaterialPriorities, ToolWorldFixture)
{
    civ::startNewGame(world);
    const civ::Content& content = world.GetTechTree().GetContent();
    const civ::PlayerResearch& research = world.GetPlayer(0).GetResearch();
    using Priorities = civ::PlayerResearch::ToolMaterialPriorities;
    BOOST_TEST(research.GetToolMaterialPriorities(content) == (Priorities{5, 3}), boost::test_tools::per_element());
    BOOST_TEST_REQUIRE(countInHQ(GoodType::Axe) >= 2u);
    hq().AddToInventory(GoodCounts::make(COPPER_AXE, 2), true);

    // Default: iron before copper
    const nobUsual& woodcutter1 = placeConnectedBuilding(BuildingType::Woodcutter);
    RTTR_EXEC_TILL(1000, woodcutter1.GetToolSlots()[0].tool != GoodType::Nothing);
    BOOST_TEST((woodcutter1.GetToolSlots()[0].tool == GoodType::Axe));
    BOOST_TEST(woodcutter1.GetWorkSpeed() == 100u);

    // Wrong number of values: ignored
    this->SetToolMaterialPriorities({1, 2, 3});
    BOOST_TEST(research.GetToolMaterialPriorities(content) == (Priorities{5, 3}), boost::test_tools::per_element());
    // Copper before iron, values above 10 are limited
    this->SetToolMaterialPriorities({2, 12});
    BOOST_TEST(research.GetToolMaterialPriorities(content) == (Priorities{2, 10}), boost::test_tools::per_element());
    const nobUsual& woodcutter2 = placeConnectedBuilding(BuildingType::Woodcutter, Position(-5, 2));
    RTTR_EXEC_TILL(1000, woodcutter2.GetToolSlots()[0].tool != GoodType::Nothing);
    BOOST_TEST((woodcutter2.GetToolSlots()[0].tool == COPPER_AXE));
    BOOST_TEST(woodcutter2.GetWorkSpeed() == 50u);
}

BOOST_FIXTURE_TEST_CASE(WorkerBringsToolsBackFromDemolishedBuilding, ToolWorldFixture)
{
    civ::startNewGame(world);
    const unsigned numAxes = countInHQ(GoodType::Axe);
    BOOST_TEST_REQUIRE(numAxes >= 1u);
    const nobUsual& woodcutter = placeConnectedBuilding(BuildingType::Woodcutter);
    const MapPoint bldPos = woodcutter.GetPos();
    RTTR_EXEC_TILL(1000, woodcutter.HasWorker() && woodcutter.GetToolSlots()[0].tool == GoodType::Axe);
    // Tools in buildings are not part of the inventory (the replacement was ordered at once, too)
    BOOST_TEST(countInHQ(GoodType::Axe) < numAxes);
    BOOST_TEST(inventory()[GoodType::Axe] < numAxes);
    BOOST_TEST(inventory()[Job::Woodcutter] == 1u);
    const unsigned numHelpers = inventory()[Job::Helper];

    // The axe is still good (at least half of its durability): The worker takes it and the replacement home and is a
    // helper again
    BOOST_TEST_REQUIRE(woodcutter.GetToolSlots()[0].stepsLeft >= 2u);
    this->DestroyBuilding(bldPos);
    RTTR_EXEC_TILL(2000, countInHQ(GoodType::Axe) == numAxes);
    BOOST_TEST(inventory()[GoodType::Axe] == numAxes);
    BOOST_TEST(hq().GetNumVisualWares(GoodType::Axe) == numAxes);
    BOOST_TEST(inventory()[Job::Woodcutter] == 0u);
    BOOST_TEST(countInHQ(Job::Woodcutter) == 0u);
    BOOST_TEST(hq().GetNumVisualFigures(Job::Woodcutter) == 0u);
    BOOST_TEST(inventory()[Job::Helper] > numHelpers);
}

BOOST_FIXTURE_TEST_CASE(SerializeTools, ToolWorldFixture)
{
    civ::startNewGame(world);
    hq().AddToInventory(GoodCounts::make(TEST_SAW, 3), true);
    this->SetToolMaterialPriorities({4, 9});
    const MapPoint bldPos = placeConnectedBuilding(BuildingType::Sawmill).GetPos();
    RTTR_EXEC_TILL(2000, world.GetSpecObj<nobUsual>(bldPos)->GetToolSlots()[0].tool == TEST_SAW);
    hq().AddToInventory(GoodCounts::make(GoodType::Wood, 40), true);
    // Save while the replacement is on its way
    RTTR_EXEC_TILL(8000, world.GetSpecObj<nobUsual>(bldPos)->GetToolSlots()[0].tool == TEST_SAW
                           && world.GetSpecObj<nobUsual>(bldPos)->GetToolSlots()[0].ordered);
    const nobUsual::ToolSlot slotBefore = world.GetSpecObj<nobUsual>(bldPos)->GetToolSlots()[0];
    const unsigned speedBefore = world.GetSpecObj<nobUsual>(bldPos)->ScaleWorkDuration(100);

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    const nobUsual* sawmill = world.GetSpecObj<nobUsual>(bldPos);
    BOOST_TEST_REQUIRE(sawmill);
    BOOST_TEST_REQUIRE(sawmill->GetToolSlots().size() == 1u);
    const nobUsual::ToolSlot& slot = sawmill->GetToolSlots()[0];
    BOOST_TEST((slot.tool == slotBefore.tool));
    BOOST_TEST(slot.stepsLeft == slotBefore.stepsLeft);
    BOOST_TEST((slot.spare == slotBefore.spare));
    BOOST_TEST_REQUIRE(slot.ordered);
    BOOST_TEST((slot.ordered->type == TEST_SAW));
    BOOST_TEST(sawmill->ScaleWorkDuration(100) == speedBefore);
    using Priorities = civ::PlayerResearch::ToolMaterialPriorities;
    BOOST_TEST(world.GetPlayer(0).GetResearch().GetToolMaterialPriorities(world.GetTechTree().GetContent())
                 == (Priorities{4, 9}),
               boost::test_tools::per_element());
    // It goes on: The replacement arrives and all 3 saws are used up
    RTTR_EXEC_TILL(6000, world.GetSpecObj<nobUsual>(bldPos)->GetToolSlots()[0].spare == TEST_SAW);
    RTTR_EXEC_TILL(30000, world.GetSpecObj<nobUsual>(bldPos)->GetToolSlots()[0].tool == GoodType::Nothing);
    BOOST_TEST(world.GetPlayer(0).GetInventory()[TEST_SAW] == 0u);
}

BOOST_AUTO_TEST_SUITE_END()
