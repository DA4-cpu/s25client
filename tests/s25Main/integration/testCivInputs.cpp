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
#include "factories/BuildingFactory.h"
#include "figures/nofBuildingWorker.h"
#include "lua/GameDataLoader.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "gameTypes/GameTypesOutput.h"
#include "gameData/BuildingConsts.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>

namespace bnw = boost::nowide;

// Inputs of workshops (civilization mode, rttr:AddWorkshop{ inputs }): a place for every ware of a tag query, also
// mixed, with an amount per work step and own places (storage)
BOOST_AUTO_TEST_SUITE(CivInputsSuite)

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

/// Wares: Raw (0), Product (1), Fish2 and Meat2 (2, 3, both with the tag "Food2"). The shop makes Product from the
/// given inputs
WorldDescription makeDesc(std::vector<InputDesc> inputs, unsigned storage = 6)
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("Raw"));
    desc.wares.push_back(makeWare("Product"));
    desc.wares.push_back(makeWare("Fish2", "Food2"));
    desc.wares.push_back(makeWare("Meat2", "Food2"));
    WorkshopDesc shop;
    shop.name = "Shop";
    shop.title = "ShopTitle";
    shop.size = "small";
    shop.cost = {{{"Boards"}, 2}};
    shop.look = "Sawmill";
    shop.worker.title = "ShopWorker";
    shop.worker.look = "Carpenter";
    shop.inputs = std::move(inputs);
    shop.storage = storage;
    shop.outputs = {{"Product"}};
    shop.workTime = 40;
    desc.workshops.push_back(shop);
    return desc;
}

constexpr GoodType RAW = GoodType::Custom0;
const GoodType PRODUCT = getCustomGood(1);
const GoodType FISH2 = getCustomGood(2);
const GoodType MEAT2 = getCustomGood(3);
constexpr BuildingType SHOP = BuildingType::Custom0;

struct InputFixture : public WorldWithGCExecution1P
{
    void init(const WorldDescription& desc)
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
    }

    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    unsigned inInventory(GoodType good) { return world.GetPlayer(0).GetInventory()[good]; }

    MapPoint placeConnectedBuilding()
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + Position(4, 2));
        BuildingFactory::CreateBuilding(world, SHOP, pos, 0, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<nobUsual>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        return pos;
    }
    const nobUsual& shop(const MapPoint pos) { return *world.GetSpecObj<nobUsual>(pos); }
    bool isWaiting(const MapPoint pos)
    {
        const nobUsual& bld = shop(pos);
        return !bld.is_working && bld.GetWorker()
               && bld.GetWorker()->GetState() == nofBuildingWorker::State::WaitingForWaresOrProductionStopped;
    }

    /// Give the wares, let the shop work until it used all of them and waits, and return the number of products
    unsigned produce(const std::vector<std::pair<GoodType, unsigned>>& wares)
    {
        for(const auto& [good, num] : wares)
            hq().AddToInventory(GoodCounts::make(good, num), true);
        const MapPoint bldPos = placeConnectedBuilding();
        RTTR_EXEC_TILL(4000, shop(bldPos).HasWorker());
        // The inventory counts the wares until they are used (also in the building and on their way)
        RTTR_EXEC_TILL(30000, numInInventory(wares) == 0u && isWaiting(bldPos));
        RTTR_SKIP_GFS(1000);
        return inInventory(PRODUCT);
    }
    unsigned numInInventory(const std::vector<std::pair<GoodType, unsigned>>& wares)
    {
        unsigned result = 0;
        for(const auto& entry : wares)
            result += inInventory(entry.first);
        return result;
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(InputsFromLua)
{
    const std::string base = R"(
rttr:AddWare{ name = "Raw", look = "Wood", transport = "Wood" }
rttr:AddWare{ name = "Product", look = "Boards", transport = "Boards" }
rttr:AddWare{ name = "Fish2", kind = "Food2", look = "Fish", transport = "Fish" }
rttr:AddWare{ name = "Meat2", kind = "Food2", look = "Meat", transport = "Meat" }
rttr:AddWorkshop{ name = "Shop", size = "small", cost = { Boards = 2 }, look = "Sawmill",
    worker = { look = "Carpenter" }, inputs = { "Raw", { tags = "Food2", amount = 0.5, storage = 4 } },
    storage = 3, outputs = { "Product" }, workTime = 100 }
)";
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(base, desc));
    BOOST_TEST_REQUIRE(desc.workshops[0].inputs.size() == 2u);
    BOOST_TEST((desc.workshops[0].inputs[0].tags == TagQuery{"Raw"}));
    BOOST_TEST(desc.workshops[0].inputs[0].amount == 100u);
    BOOST_TEST(desc.workshops[0].inputs[0].storage == 0u);
    BOOST_TEST((desc.workshops[0].inputs[1].tags == TagQuery{"Food2"}));
    BOOST_TEST(desc.workshops[0].inputs[1].amount == 50u);
    BOOST_TEST(desc.workshops[0].inputs[1].storage == 4u);
    const civ::Content content(desc);
    const auto& inputs = content.GetBuilding(SHOP)->workDesc.inputs;
    BOOST_TEST_REQUIRE(inputs.size() == 2u);
    BOOST_TEST((inputs[0].wares == std::vector<GoodType>{RAW}));
    BOOST_TEST(inputs[0].amount == 100u);
    BOOST_TEST(inputs[0].storage == 3u); // The storage of the workshop
    BOOST_TEST((inputs[1].wares == std::vector<GoodType>{FISH2, MEAT2}));
    BOOST_TEST(inputs[1].amount == 50u);
    BOOST_TEST(inputs[1].storage == 4u);

    const auto replaced = [&base](const std::string& from, const std::string& to) {
        std::string result = base;
        const auto pos = result.find(from);
        BOOST_TEST_REQUIRE(pos != std::string::npos);
        return result.replace(pos, from.size(), to);
    };
    std::string elevenInputs = "inputs = { ";
    for(unsigned i = 0; i < 11; ++i)
        elevenInputs += "\"Raw\", ";
    elevenInputs += "}";
    for(const std::string& invalid :
        {replaced("outputs = { \"Product\" }", "consume = \"any\", outputs = { \"Product\" }"),
         replaced("amount = 0.5", "amount = 0"), replaced("amount = 0.5", "amount = -1"),
         replaced("amount = 0.5", "amount = 100"), replaced("storage = 4", "storage = 0"),
         replaced("storage = 4", "storage = 100"), replaced("amount = 0.5", "amount = 0.5, typo = 1"),
         replaced("inputs = { \"Raw\", { tags = \"Food2\", amount = 0.5, storage = 4 } }", elevenInputs)})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(invalid, invalidDesc));
    }
    // 10 inputs are fine, overlapping inputs too
    std::string tenInputs = "inputs = { ";
    for(unsigned i = 0; i < 10; ++i)
        tenInputs += "\"Raw\", ";
    tenInputs += "}";
    {
        WorldDescription tenDesc;
        BOOST_TEST_REQUIRE(loadLua(
          replaced("inputs = { \"Raw\", { tags = \"Food2\", amount = 0.5, storage = 4 } }", tenInputs), tenDesc));
        BOOST_TEST(civ::Content(tenDesc).GetBuilding(SHOP)->workDesc.inputs.size() == 10u);
    }
    // More wares per step than the input holds
    {
        WorldDescription invalidDesc;
        BOOST_TEST_REQUIRE(loadLua(replaced("amount = 0.5", "amount = 5"), invalidDesc));
        BOOST_CHECK_THROW(civ::Content{invalidDesc}, GameDataError);
    }
}

BOOST_FIXTURE_TEST_CASE(OneWarePerStepByDefault, InputFixture)
{
    init(makeDesc({InputDesc{{"Raw"}}}));
    BOOST_TEST(produce({{RAW, 5}}) == 5u);
}

BOOST_FIXTURE_TEST_CASE(SeveralWaresPerStep, InputFixture)
{
    // 2 per step: 6 wares are 3 steps
    init(makeDesc({InputDesc{{"Raw"}, 200}}));
    BOOST_TEST(produce({{RAW, 6}}) == 3u);
}

BOOST_FIXTURE_TEST_CASE(PartOfAWarePerStep, InputFixture)
{
    // 0.5 per step: 1 ware in every 2nd step, taken in the first one. 3 wares are 6 steps
    init(makeDesc({InputDesc{{"Raw"}, 50}}));
    BOOST_TEST(produce({{RAW, 3}}) == 6u);
}

BOOST_FIXTURE_TEST_CASE(OneAndAHalfPerStep, InputFixture)
{
    // 1.5 per step: 2, 1, 2, 1 -> 6 wares are 4 steps
    init(makeDesc({InputDesc{{"Raw"}, 150}}));
    BOOST_TEST(produce({{RAW, 6}}) == 4u);
}

BOOST_FIXTURE_TEST_CASE(MixedInputTakesAnyOfItsWares, InputFixture)
{
    // One input for both foods: both are delivered into it and used, one per step
    init(makeDesc({InputDesc{{"Food2"}}}));
    BOOST_TEST(produce({{FISH2, 3}, {MEAT2, 3}}) == 6u);
}

BOOST_FIXTURE_TEST_CASE(EveryInputIsNeeded, InputFixture)
{
    // Raw and a food each step: 2 foods for 5 raw are 2 steps
    init(makeDesc({InputDesc{{"Raw"}}, InputDesc{{"Food2"}}}));
    hq().AddToInventory(GoodCounts::make(FISH2, 2), true);
    const MapPoint bldPos = placeConnectedBuilding();
    RTTR_EXEC_TILL(4000, shop(bldPos).HasWorker());
    RTTR_SKIP_GFS(3000);
    BOOST_TEST(inInventory(PRODUCT) == 0u);
    hq().AddToInventory(GoodCounts::make(RAW, 5), true);
    RTTR_EXEC_TILL(30000, inInventory(PRODUCT) == 2u);
    RTTR_SKIP_GFS(3000);
    BOOST_TEST(inInventory(PRODUCT) == 2u);
    BOOST_TEST(inInventory(RAW) == 3u);
}

BOOST_FIXTURE_TEST_CASE(InputsAreSaved, InputFixture)
{
    // 0.5 per step: saved after the first step (1 ware taken for 2 steps), the second step needs no ware
    init(makeDesc({InputDesc{{"Raw"}, 50}}));
    hq().AddToInventory(GoodCounts::make(RAW, 1), true);
    const MapPoint bldPos = placeConnectedBuilding();
    RTTR_EXEC_TILL(4000, shop(bldPos).HasWorker());
    RTTR_EXEC_TILL(10000, inInventory(PRODUCT) == 1u);
    BOOST_TEST(inInventory(RAW) == 0u);

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    // The second step uses what was taken in advance
    RTTR_EXEC_TILL(10000, inInventory(PRODUCT) == 2u);
    RTTR_SKIP_GFS(3000);
    BOOST_TEST(inInventory(PRODUCT) == 2u);
}

BOOST_AUTO_TEST_SUITE_END()
