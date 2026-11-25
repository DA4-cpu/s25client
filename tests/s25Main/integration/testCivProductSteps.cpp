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
#include "civ/WareDistribution.h"
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
#include <optional>

namespace bnw = boost::nowide;

// Work steps per product (outputs = { { "CopperSword", steps = 1.5 } }) and the output and input ratios of tools
// (rttr:AddTool{ outputRatio, inputRatio }, as a number for everything or per tag)
BOOST_AUTO_TEST_SUITE(CivProductStepsSuite)

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

WareDesc makeWare(const std::string& name, const std::string& kind = "")
{
    WareDesc desc;
    desc.name = name;
    desc.title = name + "Title";
    if(!kind.empty())
        desc.kind = {kind};
    desc.look = "Boards";
    desc.transport = "Boards";
    return desc;
}

constexpr GoodType RAW = GoodType::Custom0;
const GoodType PIKE = getCustomGood(1);
const GoodType SABER = getCustomGood(2);
constexpr BuildingType SHOP = BuildingType::Custom0;

/// Raw (ware 0, kind "Metal") -> Pike (ware 1, kind "Weapon") and Saber (ware 2, kind "Weapon") with their work steps
/// (0: only the pike). Optionally the tongs as tool (speed 0, work speed of the shop 100)
WorldDescription makeDesc(unsigned outputRatio, unsigned pikeSteps, unsigned saberSteps,
                          std::optional<ToolDesc> tongs = std::nullopt)
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("Raw", "Metal"));
    desc.wares.push_back(makeWare("Pike", "Weapon"));
    desc.wares.push_back(makeWare("Saber", "Weapon"));
    WorkshopDesc shop;
    shop.name = "Shop";
    shop.title = "ShopTitle";
    shop.size = "small";
    shop.cost = {{{"Boards"}, 2}};
    shop.look = "Metalworks";
    shop.worker.title = "ShopWorker";
    shop.worker.look = "Metalworker";
    shop.inputs = {InputDesc{{"Raw"}}};
    shop.outputs = {{"Pike"}};
    shop.outputSteps = {pikeSteps};
    if(saberSteps > 0)
    {
        shop.outputs.push_back({"Saber"});
        shop.outputSteps.push_back(saberSteps);
    }
    shop.workTime = 40;
    shop.outputRatio = outputRatio;
    if(tongs)
    {
        ToolMaterialDesc iron;
        iron.name = "Iron";
        iron.title = "Iron";
        desc.toolMaterials = {iron};
        tongs->ware = "Tongs";
        tongs->material = "Iron";
        tongs->durability = 1000;
        tongs->speed = 0;
        desc.tools = {*tongs};
        BuildingToolsDesc shopTools;
        shopTools.baseSpeed = 100;
        shopTools.tools = {{"Tongs"}};
        shop.tools = shopTools;
    }
    desc.workshops.push_back(shop);
    return desc;
}

ToolDesc makeTongs(int outputRatio, std::vector<std::pair<std::string, int>> outputRatioByTag, int inputRatio = 0,
                   std::vector<std::pair<std::string, int>> inputRatioByTag = {})
{
    ToolDesc tongs;
    tongs.outputRatio = outputRatio;
    tongs.outputRatioByTag = std::move(outputRatioByTag);
    tongs.inputRatio = inputRatio;
    tongs.inputRatioByTag = std::move(inputRatioByTag);
    return tongs;
}

struct StepsFixture : public WorldWithGCExecution1P
{
    void init(const WorldDescription& desc)
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
        // Values from the number of wares, nothing stops the shop
        this->SetWareDistribution(false, {});
        for(const GoodType good : {PIKE, SABER})
        {
            civ::WareSetting setting = world.GetPlayer(0).GetWareDistribution().GetSetting(good);
            setting.autoPause = false;
            setting.paused = false;
            setting.autoValue = false;
            setting.manualValue = 500;
            this->SetWareDistribution(false, {{good, setting}});
        }
    }

    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    unsigned inInventory(GoodType good) { return world.GetPlayer(0).GetInventory()[good]; }

    /// Only this product is chosen (value 1000, the other one 0)
    void onlyChoose(GoodType good)
    {
        for(const GoodType other : {PIKE, SABER})
        {
            civ::WareSetting setting = world.GetPlayer(0).GetWareDistribution().GetSetting(other);
            setting.manualValue = other == good ? 1000 : 0;
            this->SetWareDistribution(false, {{other, setting}});
        }
        RTTR_SKIP_GFS(1);
    }

    /// Place the finished shop connected to the HQ and wait for its worker (and its tool if given)
    nobUsual& placeShop(bool withTongs = false)
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + Position(4, 2));
        BuildingFactory::CreateBuilding(world, SHOP, pos, 0, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<nobUsual>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        nobUsual& shop = *world.GetSpecObj<nobUsual>(pos);
        RTTR_EXEC_TILL(2000, shop.HasWorker());
        if(withTongs)
        {
            hq().AddToInventory(GoodCounts::make(GoodType::Tongs, 1), true);
            RTTR_EXEC_TILL(3000, shop.GetToolSlots()[0].tool == GoodType::Tongs);
        }
        return shop;
    }

    /// Work steps with the given raw wares until the shop waits for wares again
    void work(nobUsual& shop, unsigned numRaw)
    {
        hq().AddToInventory(GoodCounts::make(RAW, numRaw), true);
        RTTR_EXEC_TILL(20000, inInventory(RAW) == 0u && shop.GetNumWares(0) == 0u && !shop.is_working
                                && shop.GetWorker()->GetState()
                                     == nofBuildingWorker::State::WaitingForWaresOrProductionStopped);
        RTTR_SKIP_GFS(500);
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(StepsAndToolRatiosFromLua)
{
    const std::string base = R"(
rttr:AddWare{ name = "Raw", kind = "Metal", look = "IronOre", transport = "IronOre" }
rttr:AddWare{ name = "Pike", kind = "Weapon", look = "Boards", transport = "Boards" }
rttr:AddWare{ name = "Saber", kind = "Weapon", properties = { "Long" }, look = "Boards", transport = "Boards" }
rttr:AddToolMaterial{ name = "Iron" }
rttr:AddTool{ ware = "Tongs", material = "Iron", durability = 100, speed = 100, outputRatio = { Saber = 0.5 },
    inputRatio = -0.2 }
rttr:AddTool{ ware = "Hammer", material = "Iron", durability = 100, speed = 100, outputRatio = -0.25,
    inputRatio = { Coal = -0.25, Metal = 0.5 } }
rttr:AddWorkshop{ name = "Shop", size = "small", cost = { Boards = 2 }, look = "Metalworks",
    worker = { look = "Metalworker" }, inputs = { "Raw" }, outputs = { "Pike", { "Weapon", "Long", steps = 1.5 } },
    workTime = 100 }
rttr:AddWorkshop{ name = "Smithy", size = "small", cost = { Boards = 2 }, look = "Metalworks",
    worker = { look = "Metalworker" }, inputs = { "Raw" }, outputs = { { "Weapon", steps = 2 } }, workTime = 100 }
)";
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(base, desc));
    BOOST_TEST_REQUIRE(desc.workshops.size() == 2u);
    BOOST_TEST((desc.workshops[0].outputs[1] == TagQuery{"Weapon", "Long"}));
    BOOST_TEST((desc.workshops[0].outputSteps == std::vector<unsigned>{100, 150}));
    BOOST_TEST_REQUIRE(desc.tools.size() == 2u);
    BOOST_TEST(desc.tools[0].outputRatio == 0);
    BOOST_TEST((desc.tools[0].outputRatioByTag == std::vector<std::pair<std::string, int>>{{"Saber", 50}}));
    BOOST_TEST(desc.tools[0].inputRatio == -20);
    BOOST_TEST(desc.tools[1].outputRatio == -25);
    BOOST_TEST(
      (desc.tools[1].inputRatioByTag == std::vector<std::pair<std::string, int>>{{"Coal", -25}, {"Metal", 50}}));

    const civ::Content content(desc);
    const civ::CustomBuilding& shop = *content.GetBuilding(SHOP);
    BOOST_TEST(shop.GetProductSteps(PIKE) == 100u);
    BOOST_TEST(shop.GetProductSteps(SABER) == 150u);
    // An entry matching several wares: each is a product with the steps of the entry
    const civ::CustomBuilding& smithy = *content.GetBuilding(getCustomBuilding(1));
    BOOST_TEST((smithy.productChoices == std::vector<GoodType>{PIKE, SABER}));
    BOOST_TEST(smithy.GetProductSteps(PIKE) == 200u);
    BOOST_TEST(smithy.GetProductSteps(SABER) == 200u);
    const civ::ToolInfo& tongs = *content.GetTool(GoodType::Tongs);
    BOOST_TEST(tongs.GetOutputBonus(SABER) == 50);
    BOOST_TEST(tongs.GetOutputBonus(PIKE) == 0);
    BOOST_TEST(tongs.GetOutputBonus(std::nullopt) == 0);
    BOOST_TEST(tongs.GetBestOutputBonus() == 50);
    BOOST_TEST(tongs.GetInputBonus({RAW}) == -20);
    const civ::ToolInfo& hammer = *content.GetTool(GoodType::Hammer);
    BOOST_TEST(hammer.GetOutputBonus(SABER) == -25);
    BOOST_TEST(hammer.GetInputBonus({GoodType::Coal}) == -25);
    BOOST_TEST(hammer.GetInputBonus({RAW}) == 50);
    // Only if all wares of the input have the tag
    BOOST_TEST(hammer.GetInputBonus({RAW, GoodType::Coal}) == 0);

    const auto replaced = [&base](const std::string& from, const std::string& to) {
        std::string result = base;
        const auto pos = result.find(from);
        BOOST_TEST_REQUIRE(pos != std::string::npos);
        return result.replace(pos, from.size(), to);
    };
    for(const std::string& invalid :
        {replaced("steps = 1.5", "steps = 0"), replaced("steps = 1.5", "steps = 100"),
         replaced("steps = 1.5", "steps = \"many\""), replaced("steps = 1.5", "steps = 0.001"),
         replaced("{ \"Weapon\", \"Long\", steps = 1.5 }", "{ steps = 1.5 }"),
         replaced("steps = 1.5", "steps = 1.5, amount = 2"),
         replaced("outputRatio = { Saber = 0.5 }", "outputRatio = {}"),
         replaced("outputRatio = { Saber = 0.5 }", "outputRatio = { Saber = \"much\" }"),
         replaced("outputRatio = { Saber = 0.5 }", "outputRatio = { 0.5 }"),
         replaced("outputRatio = -0.25", "outputRatio = -9"), replaced("inputRatio = -0.2", "inputRatio = 100"),
         replaced("inputRatio = -0.2", "inputRatio = \"less\"")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(invalid, invalidDesc));
    }
    // A tag no ware has
    WorldDescription unknownTag;
    BOOST_TEST_REQUIRE(loadLua(replaced("Coal = -0.25", "Cole = -0.25"), unknownTag));
    BOOST_CHECK_THROW(civ::Content{unknownTag}, GameDataError);
}

BOOST_FIXTURE_TEST_CASE(ProductCostingTwoSteps, StepsFixture)
{
    init(makeDesc(100, 200, 0));
    nobUsual& shop = placeShop();
    // 5 steps: every step takes its raw ware, every second one makes a pike. 1 saved step is left
    work(shop, 5);
    BOOST_TEST(inInventory(PIKE) == 2u);
    BOOST_TEST(shop.GetSavedSteps(PIKE) == 100u);
    BOOST_TEST(!shop.GetChosenProduct());
}

BOOST_FIXTURE_TEST_CASE(EachProductSavesOnItsOwn, StepsFixture)
{
    // Pike 1 step, saber 1.5 steps (Nutzer, 2026-11-25)
    init(makeDesc(100, 100, 150));
    nobUsual& shop = placeShop();
    // Step 1 on the saber: 1 saved for it, no saber
    onlyChoose(SABER);
    work(shop, 1);
    BOOST_TEST(inInventory(SABER) == 0u);
    BOOST_TEST(shop.GetSavedSteps(SABER) == 100u);
    // Step 2 on the pike: a pike, the saber keeps its saved step
    onlyChoose(PIKE);
    work(shop, 1);
    BOOST_TEST(inInventory(PIKE) == 1u);
    BOOST_TEST(shop.GetSavedSteps(PIKE) == 0u);
    BOOST_TEST(shop.GetSavedSteps(SABER) == 100u);
    // Step 3 on the saber: 2 saved, a saber, half a step left for the saber
    onlyChoose(SABER);
    work(shop, 1);
    BOOST_TEST(inInventory(SABER) == 1u);
    BOOST_TEST(shop.GetSavedSteps(SABER) == 50u);
    BOOST_TEST(inInventory(PIKE) == 1u);
}

BOOST_FIXTURE_TEST_CASE(ChosenAnewForEveryStep, StepsFixture)
{
    // Equal values: both products are chosen, the saved steps of a product do not keep it chosen
    init(makeDesc(50, 100, 100));
    nobUsual& shop = placeShop();
    work(shop, 20);
    // 20 steps with 0.5 each: everything saved is in the products (1 step each) or still saved for a product
    BOOST_TEST(inInventory(PIKE) > 0u);
    BOOST_TEST(inInventory(SABER) > 0u);
    BOOST_TEST((inInventory(PIKE) + inInventory(SABER)) * 100u + shop.GetSavedSteps(PIKE) + shop.GetSavedSteps(SABER)
               == 20u * 50u);
}

BOOST_FIXTURE_TEST_CASE(RatioAboveTheCostMakesSeveralOfTheProduct, StepsFixture)
{
    init(makeDesc(250, 100, 100));
    nobUsual& shop = placeShop();
    onlyChoose(SABER);
    // 2.5 saved for the saber: 2 sabers in the step, half a step left for the saber
    work(shop, 1);
    BOOST_TEST(inInventory(SABER) == 2u);
    BOOST_TEST(inInventory(PIKE) == 0u);
    BOOST_TEST(shop.GetSavedSteps(SABER) == 50u);
}

BOOST_FIXTURE_TEST_CASE(OrderIsProcessedWhenTheProductIsMade, StepsFixture)
{
    // Saber 2 steps: the order stays open while the steps only save for it
    init(makeDesc(100, 100, 200));
    nobUsual& shop = placeShop();
    onlyChoose(PIKE);
    this->ChangeWareOrders(SABER, 1);
    const civ::WareDistribution& distribution = world.GetPlayer(0).GetWareDistribution();
    BOOST_TEST_REQUIRE(distribution.GetOrders(SABER) == 1u);
    work(shop, 1);
    BOOST_TEST(shop.GetSavedSteps(SABER) == 100u);
    BOOST_TEST(distribution.GetOrders(SABER) == 1u);
    work(shop, 1);
    BOOST_TEST(inInventory(SABER) == 1u);
    BOOST_TEST(distribution.GetOrders(SABER) == 0u);
    // Then the values count again
    work(shop, 1);
    BOOST_TEST(inInventory(PIKE) == 1u);
}

BOOST_FIXTURE_TEST_CASE(ToolBonusForATag, StepsFixture)
{
    // The tongs give +0.5 for pikes only
    init(makeDesc(100, 100, 0, makeTongs(0, {{"Pike", 50}})));
    nobUsual& shop = placeShop(true);
    BOOST_TEST(shop.GetOutputRatio() == 100u);
    BOOST_TEST(shop.GetOutputRatio(PIKE) == 150u);
    BOOST_TEST(shop.GetOutputRatio(SABER) == 100u);
    // 1.5 per step: 1, 2, 1, 2
    work(shop, 4);
    BOOST_TEST(inInventory(PIKE) == 6u);
}

BOOST_FIXTURE_TEST_CASE(ToolBonusForAllProducts, StepsFixture)
{
    init(makeDesc(100, 100, 0, makeTongs(100, {{"Saber", 100}})));
    nobUsual& shop = placeShop(true);
    BOOST_TEST(shop.GetOutputRatio(PIKE) == 200u);
    BOOST_TEST(shop.GetOutputRatio(SABER) == 300u);
    work(shop, 3);
    BOOST_TEST(inInventory(PIKE) == 6u);
}

BOOST_FIXTURE_TEST_CASE(ProductWithoutYieldIsNotChosen, StepsFixture)
{
    // The tongs take the whole ratio of the saber: only pikes
    init(makeDesc(100, 100, 100, makeTongs(0, {{"Saber", -100}})));
    nobUsual& shop = placeShop(true);
    BOOST_TEST(shop.GetOutputRatio(SABER) == 0u);
    onlyChoose(SABER);
    work(shop, 6);
    BOOST_TEST(inInventory(PIKE) == 6u);
    BOOST_TEST(inInventory(SABER) == 0u);
}

BOOST_FIXTURE_TEST_CASE(NegativeToolRatioStopsTheWork, StepsFixture)
{
    init(makeDesc(100, 100, 0, makeTongs(-100, {})));
    nobUsual& shop = placeShop(true);
    BOOST_TEST(shop.GetOutputRatio() == 0u);
    hq().AddToInventory(GoodCounts::make(RAW, 3), true);
    RTTR_EXEC_TILL(3000, shop.GetNumWares(0) == 3u);
    RTTR_SKIP_GFS(1000);
    // The inputs are not wasted
    BOOST_TEST(shop.GetNumWares(0) == 3u);
    BOOST_TEST(inInventory(PIKE) == 0u);
}

BOOST_FIXTURE_TEST_CASE(ToolInputRatioForAllInputs, StepsFixture)
{
    // 0.5 less raw per step: 1, 0, 1, 0
    init(makeDesc(100, 100, 0, makeTongs(0, {}, -50)));
    nobUsual& shop = placeShop(true);
    BOOST_TEST(shop.GetInputAmount(0) == 50u);
    work(shop, 2);
    BOOST_TEST(inInventory(PIKE) == 4u);
}

BOOST_FIXTURE_TEST_CASE(ToolInputRatioForATag, StepsFixture)
{
    // The input of raw (tag Metal) takes 1 more per step, the tag Coal does not concern it
    init(makeDesc(100, 100, 0, makeTongs(0, {}, 0, {{"Metal", 100}, {"Coal", -50}})));
    nobUsual& shop = placeShop(true);
    BOOST_TEST(shop.GetInputAmount(0) == 200u);
    work(shop, 6);
    BOOST_TEST(inInventory(PIKE) == 3u);
}

BOOST_FIXTURE_TEST_CASE(ToolInputRatioDownToZero, StepsFixture)
{
    // No raw needed at all: the shop works without wares
    init(makeDesc(100, 100, 0, makeTongs(0, {}, -100)));
    nobUsual& shop = placeShop(true);
    BOOST_TEST(shop.GetInputAmount(0) == 0u);
    RTTR_EXEC_TILL(3000, inInventory(PIKE) >= 2u);
}

BOOST_FIXTURE_TEST_CASE(SerializeTheSavedSteps, StepsFixture)
{
    init(makeDesc(100, 100, 300));
    nobUsual& shop = placeShop();
    onlyChoose(SABER);
    work(shop, 1);
    BOOST_TEST(shop.GetSavedSteps(SABER) == 100u);
    // During a step: the chosen product is saved, too
    hq().AddToInventory(GoodCounts::make(RAW, 1), true);
    RTTR_EXEC_TILL(8000, shop.is_working);
    BOOST_TEST((shop.GetChosenProduct() == SABER));
    const MapPoint shopPos = shop.GetPos();

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    nobUsual& loaded = *world.GetSpecObj<nobUsual>(shopPos);
    BOOST_TEST(loaded.GetSavedSteps(SABER) == 100u);
    BOOST_TEST((loaded.GetChosenProduct() == SABER));
    work(loaded, 1);
    BOOST_TEST(inInventory(SABER) == 1u);
    BOOST_TEST(inInventory(PIKE) == 0u);
    BOOST_TEST(loaded.GetSavedSteps(SABER) == 0u);
}

BOOST_AUTO_TEST_SUITE_END()
