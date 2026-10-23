// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AddonHelperFunctions.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "RttrConfig.h"
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
#include "files.h"
#include "helpers/EnumRange.h"
#include "lua/GameDataLoader.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "nodeObjs/noFlag.h"
#include "gameTypes/MapTypes.h"
#include "gameData/BuildingConsts.h"
#include "gameData/GoodConsts.h"
#include "gameData/JobConsts.h"
#include "gameData/SettingTypeConv.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <iterator>
#include <memory>

namespace bnw = boost::nowide;

BOOST_AUTO_TEST_SUITE(CivContentSuite)

namespace {
WareDesc makeWare(const std::string& name, const std::string& kind = "", std::vector<std::string> properties = {},
                  const std::string& look = "Stones")
{
    WareDesc desc;
    desc.name = name;
    desc.title = name + "Title";
    desc.kind = kind;
    desc.properties = std::move(properties);
    desc.look = look;
    desc.transport = look;
    return desc;
}

WorkshopDesc makeWorkshop(const std::string& name, std::vector<TagQuery> inputs, TagQuery output)
{
    WorkshopDesc desc;
    desc.name = name;
    desc.title = name + "Title";
    desc.size = "small";
    desc.cost = {{"Boards", 2}, {"Stones", 1}};
    desc.look = "Sawmill";
    desc.worker.title = name + "Worker";
    desc.worker.tool = "Saw";
    desc.worker.look = "Carpenter";
    desc.inputs = std::move(inputs);
    desc.output = std::move(output);
    desc.workTime = 2;
    return desc;
}

/// Raw -> Product
WorldDescription makeSimpleDesc()
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("Raw", "RawKind"));
    desc.wares.push_back(makeWare("Product", "", {}, "Boards"));
    desc.workshops.push_back(makeWorkshop("Shop", {{"Raw"}}, {"Product"}));
    return desc;
}

/// Two kinds of wood which are sawed to the matching boards
WorldDescription makeInheritDesc()
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("LeafWood", "Holz", {"Laub"}, "Wood"));
    desc.wares.push_back(makeWare("NeedleWood", "Holz", {"Nadel"}, "Wood"));
    desc.wares.push_back(makeWare("LeafBoards", "Bretter", {"Laub"}, "Boards"));
    desc.wares.push_back(makeWare("NeedleBoards", "Bretter", {"Nadel"}, "Boards"));
    auto saw = makeWorkshop("Saw2", {{"Holz"}}, {"Bretter"});
    saw.consumeEach = false;
    saw.inheritProperties = true;
    desc.workshops.push_back(saw);
    return desc;
}

/// Stones (built-in) -> Product
WorldDescription makeBuiltinInputDesc()
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("Product", "", {}, "Boards"));
    desc.workshops.push_back(makeWorkshop("Shop", {{"Stones"}}, {"Product"}));
    return desc;
}

/// Shop without inputs making "Product", with byproduct "Raw" (amount, after technology "tech" if requireTech)
WorldDescription makeByproductDesc(unsigned amount, bool requireTech)
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("Raw", "RawKind"));
    desc.wares.push_back(makeWare("Product", "", {}, "Boards"));
    desc.workshops.push_back(makeWorkshop("Shop", {}, {"Product"}));
    TechnologyDesc tech;
    tech.name = "tech";
    tech.title = "Tech";
    desc.technologies.add(tech);
    ByproductDesc byproduct;
    byproduct.building = "Shop";
    byproduct.ware = {"Raw"};
    byproduct.amount = amount;
    if(requireTech)
        byproduct.technology = "tech";
    desc.byproducts.push_back(byproduct);
    return desc;
}

/// Bones -> carved bone for LWP, IWP or MWP (chosen by knowledge type)
WorldDescription makeKnowledgeChoiceDesc()
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("Bones"));
    const char* types[] = {"LWP", "IWP", "MWP"};
    for(unsigned i = 0; i < 3; ++i)
    {
        WareDesc medium = makeWare(std::string("Carved") + types[i], "Carved", {types[i]});
        medium.knowledge[i] = 1;
        desc.wares.push_back(medium);
    }
    auto carver = makeWorkshop("Carver", {{"Bones"}}, {"Carved"});
    carver.outputChoice = "knowledge";
    desc.workshops.push_back(carver);
    return desc;
}

/// Sets the content as active for the lifetime of this object
struct ActiveContent
{
    std::shared_ptr<const civ::Content> content;
    explicit ActiveContent(const WorldDescription& desc) : content(std::make_shared<const civ::Content>(desc))
    {
        civ::setActiveContent(content);
    }
    ~ActiveContent() { civ::resetActiveContent(content.get()); }
};
} // namespace

BOOST_AUTO_TEST_CASE(EmptyContent)
{
    const civ::Content content;
    BOOST_TEST(content.GetNumWares() == 0u);
    BOOST_TEST(content.GetNumBuildings() == 0u);
    BOOST_TEST(!content.GetWare(GoodType::Custom0));
    BOOST_TEST(!content.IsUsed(GoodType::Custom0));
    BOOST_TEST(!content.IsUsed(BuildingType::Custom0));
    BOOST_TEST(!content.IsUsed(Job::Custom0));
    BOOST_TEST(content.IsUsed(GoodType::Boards));
    BOOST_TEST(content.IsUsed(BuildingType::Sawmill));
    // Built-in wares have their identifier as tag
    BOOST_TEST((content.FindWares({"Boards"}) == std::vector<GoodType>{GoodType::Boards}));
    BOOST_TEST((content.FindWares({"Shield"}) == std::vector<GoodType>{GoodType::ShieldRomans}));
    BOOST_TEST(content.FindWares({"WaterEmpty"}).empty());
    // Unused slots are hidden from the global tables
    BOOST_TEST(WARE_NAMES[GoodType::Custom0].empty());
    BOOST_TEST(std::string(BUILDING_NAMES[BuildingType::Custom0]).empty());
    BOOST_TEST((BUILDING_SIZE[BuildingType::Custom0] == BuildingQuality::Nothing));
    BOOST_TEST(!JOB_CONSTS[Job::Custom0].tool);
}

BOOST_AUTO_TEST_CASE(WaresGetSlotsAndTags)
{
    WorldDescription desc = makeSimpleDesc();
    desc.wareTags["Boards"] = WareTagsDesc{"Bretter", {"Nadel"}};
    const civ::Content content(desc);
    BOOST_TEST_REQUIRE(content.GetNumWares() == 2u);
    const civ::CustomWare* raw = content.GetWare(GoodType::Custom0);
    BOOST_TEST_REQUIRE(raw);
    BOOST_TEST(raw->name == "Raw");
    BOOST_TEST(raw->title == "RawTitle");
    BOOST_TEST((raw->look == GoodType::Stones));
    BOOST_TEST((raw->transportLike == GoodType::Stones));
    BOOST_TEST((content.FindGood("Product") == getCustomGood(1)));
    BOOST_TEST((content.FindGood("Boards") == GoodType::Boards));
    BOOST_TEST(!content.FindGood("Nothing"));
    BOOST_TEST(content.IsUsed(getCustomGood(1)));
    BOOST_TEST(!content.IsUsed(getCustomGood(2)));

    BOOST_TEST((content.GetTags(GoodType::Custom0) == std::vector<std::string>{"Raw", "RawKind"}));
    BOOST_TEST((content.FindWares({"RawKind"}) == std::vector<GoodType>{GoodType::Custom0}));
    // Tags of built-in wares
    BOOST_TEST((content.FindWares({"Bretter"}) == std::vector<GoodType>{GoodType::Boards}));
    BOOST_TEST((content.FindWares({"Bretter", "Nadel"}) == std::vector<GoodType>{GoodType::Boards}));
    BOOST_TEST(content.FindWares({"Bretter", "Laub"}).empty());
    BOOST_TEST((content.GetProperties(GoodType::Boards) == std::vector<std::string>{"Nadel"}));

    // Workshop
    BOOST_TEST_REQUIRE(content.GetNumBuildings() == 1u);
    const civ::CustomBuilding* shop = content.GetBuilding(BuildingType::Custom0);
    BOOST_TEST_REQUIRE(shop);
    BOOST_TEST(shop->name == "Shop");
    BOOST_TEST((shop->size == BuildingQuality::Hut));
    BOOST_TEST(shop->cost.boards == 2u);
    BOOST_TEST(shop->cost.stones == 1u);
    BOOST_TEST((shop->look == BuildingType::Sawmill));
    BOOST_TEST((shop->workDesc.job == Job::Custom0));
    BOOST_TEST_REQUIRE(shop->workDesc.waresNeeded.size() == 1u);
    BOOST_TEST((shop->workDesc.waresNeeded[0] == GoodType::Custom0));
    BOOST_TEST((shop->GetProduct(std::nullopt) == getCustomGood(1)));
    BOOST_TEST((shop->GetProduct(GoodType::Custom0) == getCustomGood(1)));
    BOOST_TEST(shop->workGFs > 0u);
    const civ::CustomWorker* worker = content.GetWorker(Job::Custom0);
    BOOST_TEST_REQUIRE(worker);
    BOOST_TEST((worker->tool == GoodType::Saw));
    BOOST_TEST((worker->look == Job::Carpenter));
    BOOST_TEST((content.FindBuilding("Shop") == BuildingType::Custom0));
    BOOST_TEST((content.FindBuilding("Sawmill") == BuildingType::Sawmill));
    BOOST_TEST((content.GetSignature() == std::vector<std::string>{"W:Raw", "W:Product", "B:Shop"}));
    BOOST_TEST(content.IsCompatibleTo(content.GetSignature()));
    BOOST_TEST(content.IsCompatibleTo({"W:Raw", "B:Shop"}));
    BOOST_TEST(content.IsCompatibleTo({}));
    BOOST_TEST(!content.IsCompatibleTo({"W:Product"}));
    BOOST_TEST(!content.IsCompatibleTo({"W:Raw", "W:Product", "W:Third"}));
    BOOST_TEST(!content.IsCompatibleTo({"B:Other"}));
}

BOOST_AUTO_TEST_CASE(ProductsInheritProperties)
{
    const civ::Content content(makeInheritDesc());
    const auto leafWood = getCustomGood(0), needleWood = getCustomGood(1);
    const auto leafBoards = getCustomGood(2), needleBoards = getCustomGood(3);
    const civ::CustomBuilding* saw = content.GetBuilding(BuildingType::Custom0);
    BOOST_TEST_REQUIRE(saw);
    BOOST_TEST(!saw->workDesc.useOneWareEach);
    BOOST_TEST_REQUIRE(saw->workDesc.waresNeeded.size() == 2u);
    BOOST_TEST((saw->GetProduct(leafWood) == leafBoards));
    BOOST_TEST((saw->GetProduct(needleWood) == needleBoards));
    // Exact name, kind and property queries
    BOOST_TEST((content.FindWares({"Bretter"}) == std::vector<GoodType>{leafBoards, needleBoards}));
    BOOST_TEST((content.FindWares({"Bretter", "Nadel"}) == std::vector<GoodType>{needleBoards}));
    BOOST_TEST((content.FindWares({"NeedleBoards"}) == std::vector<GoodType>{needleBoards}));
}

BOOST_AUTO_TEST_CASE(InvalidDefinitions)
{
    const auto expectError = [](const WorldDescription& desc) { BOOST_CHECK_THROW(civ::Content{desc}, GameDataError); };
    {
        WorldDescription desc = makeSimpleDesc();
        desc.wares.push_back(makeWare("Raw")); // Duplicate
        expectError(desc);
    }
    {
        WorldDescription desc;
        desc.wares.push_back(makeWare("Boards")); // Built-in name
        expectError(desc);
    }
    {
        WorldDescription desc;
        desc.wares.push_back(makeWare("New", "", {}, "NotAWare"));
        expectError(desc);
    }
    {
        WorldDescription desc = makeSimpleDesc();
        desc.wares.back().look = "Raw"; // Look must be built-in
        expectError(desc);
    }
    {
        WorldDescription desc;
        desc.wareTags["Raw"] = WareTagsDesc{"Kind", {}}; // Only built-in wares
        expectError(desc);
    }
    {
        WorldDescription desc = makeSimpleDesc();
        desc.workshops.front().inputs = {{"Unknown"}};
        expectError(desc);
    }
    {
        // consume each: Input must match exactly one ware
        WorldDescription desc = makeInheritDesc();
        desc.workshops.front().consumeEach = true;
        expectError(desc);
    }
    {
        // More than 3 different input wares
        WorldDescription desc = makeInheritDesc();
        desc.wares.push_back(makeWare("Wood3", "Holz", {"X"}));
        desc.wares.push_back(makeWare("Wood4", "Holz", {"Y"}));
        expectError(desc);
    }
    {
        // Ambiguous output without passing on properties
        WorldDescription desc = makeInheritDesc();
        desc.workshops.front().inheritProperties = false;
        expectError(desc);
    }
    {
        // No product with matching properties
        WorldDescription desc = makeInheritDesc();
        desc.wares.push_back(makeWare("OtherWood", "Holz", {"Other"}));
        expectError(desc);
    }
    {
        WorldDescription desc = makeSimpleDesc();
        desc.workshops.front().output = {"Unknown"};
        expectError(desc);
    }
    {
        WorldDescription desc = makeSimpleDesc();
        desc.workshops.front().cost = {{"Wood", 2}};
        expectError(desc);
    }
    {
        WorldDescription desc = makeSimpleDesc();
        desc.workshops.front().worker.tool = "Wood"; // Not a tool
        expectError(desc);
    }
    {
        WorldDescription desc = makeSimpleDesc();
        desc.workshops.front().worker.look = "NotAJob";
        expectError(desc);
    }
    {
        WorldDescription desc = makeSimpleDesc();
        desc.workshops.front().look = "Headquarters";
        expectError(desc);
    }
    {
        WorldDescription desc = makeSimpleDesc();
        desc.workshops.push_back(desc.workshops.front()); // Duplicate
        expectError(desc);
    }
    {
        WorldDescription desc = makeSimpleDesc();
        desc.workshops.front().name = "Sawmill"; // Built-in name
        expectError(desc);
    }
}

BOOST_AUTO_TEST_CASE(DocumentedExamplesAreValid)
{
    // The examples in the comments of wares.lua and workshops.lua must work when uncommented
    const auto extractExample = [](const boost::filesystem::path& file) {
        bnw::ifstream in(file);
        const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const auto start = content.find("\nrttr:", content.find("Beispiel"));
        const auto end = content.rfind("]]");
        BOOST_TEST_REQUIRE(start != std::string::npos);
        BOOST_TEST_REQUIRE(end != std::string::npos);
        return content.substr(start, end - start);
    };
    const auto civFolder = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "civ";
    rttr::test::TmpFolder tmp;
    {
        bnw::ofstream file(tmp / "default.lua");
        file << extractExample(civFolder / "wares.lua") << "\n" << extractExample(civFolder / "workshops.lua");
    }
    WorldDescription desc;
    GameDataLoader loader(desc, tmp);
    BOOST_TEST_REQUIRE(loader.Load());
    BOOST_TEST(desc.wares.size() == 2u);
    BOOST_TEST(desc.workshops.size() == 1u);
    const civ::Content content(desc);
    BOOST_TEST(content.GetNumBuildings() == 1u);
}

BOOST_AUTO_TEST_CASE(GlobalTablesUseActiveContent)
{
    ActiveContent active(makeSimpleDesc());
    BOOST_TEST(WARE_NAMES[GoodType::Custom0] == "RawTitle");
    BOOST_TEST(WARE_NAMES[getCustomGood(2)].empty());
    BOOST_TEST(std::string(BUILDING_NAMES[BuildingType::Custom0]) == "ShopTitle");
    BOOST_TEST(BUILDING_COSTS[BuildingType::Custom0].boards == 2u);
    BOOST_TEST((BUILDING_SIZE[BuildingType::Custom0] == BuildingQuality::Hut));
    BOOST_TEST((BLD_WORK_DESC[BuildingType::Custom0].job == Job::Custom0));
    BOOST_TEST(JOB_NAMES[Job::Custom0] == "ShopWorker");
    BOOST_TEST((JOB_CONSTS[Job::Custom0].tool == GoodType::Saw));
    BOOST_TEST(JOB_CONSTS[Job::Custom0].work_length > 0u);
    BOOST_TEST(&JOB_SPRITE_CONSTS[Job::Custom0] == &JOB_SPRITE_CONSTS[Job::Carpenter]);
    BOOST_TEST(STD_TRANSPORT_PRIO[GoodType::Custom0] == STD_TRANSPORT_PRIO[GoodType::Stones]);
    BOOST_TEST(STD_TRANSPORT_PRIO[getCustomGood(1)] == STD_TRANSPORT_PRIO[GoodType::Boards]);
    BOOST_TEST((civ::getLook(getCustomGood(1)) == GoodType::Boards));
    BOOST_TEST((civ::getLook(BuildingType::Custom0) == BuildingType::Sawmill));
    BOOST_TEST((civ::getLook(Job::Custom0) == Job::Carpenter));
    BOOST_TEST((civ::getLook(GoodType::Wood) == GoodType::Wood));

    GlobalGameSettings ggs;
    BOOST_TEST(!makeIsUnusedWare(ggs)(GoodType::Custom0));
    BOOST_TEST(makeIsUnusedWare(ggs)(getCustomGood(2)));
    BOOST_TEST(!makeIsUnusedBuilding(ggs)(BuildingType::Custom0));
    BOOST_TEST(makeIsUnusedBuilding(ggs)(getCustomBuilding(1)));
    BOOST_TEST(!makeIsUnusedJob(ggs)(Job::Custom0));
    BOOST_TEST(makeIsUnusedJob(ggs)(getCustomJob(1)));
}

BOOST_AUTO_TEST_CASE(TechTreeUsesCustomNames)
{
    WorldDescription desc = makeSimpleDesc();
    TechnologyDesc tech;
    tech.name = "crafting";
    tech.title = "Crafting";
    tech.cost = {{"Raw", 2}};
    tech.unlockedBuildings = {"Shop"};
    desc.technologies.add(tech);
    desc.buildingKnowledge["Shop"] = KnowledgeValues{0, 2, 0};
    const civ::TechTree tree(desc);
    BOOST_TEST_REQUIRE(tree.size() == 1u);
    BOOST_TEST_REQUIRE(tree.get(0).cost.size() == 1u);
    BOOST_TEST((tree.get(0).cost[0].good == GoodType::Custom0));
    BOOST_TEST((tree.get(0).unlockedBuildings == std::vector<BuildingType>{BuildingType::Custom0}));
    BOOST_TEST(tree.IsLockedAtStart(BuildingType::Custom0));
    BOOST_TEST(tree.GetBuildingKnowledge(BuildingType::Custom0)[civ::KnowledgeType::Industry] == 2);
    BOOST_TEST(tree.GetContent().GetNumBuildings() == 1u);
}

BOOST_AUTO_TEST_CASE(ByproductsAreResolved)
{
    WorldDescription desc = makeByproductDesc(2, true);
    ByproductDesc hunter;
    hunter.building = "Hunter";
    hunter.ware = {"RawKind"};
    hunter.per = {"Meat"};
    desc.byproducts.push_back(hunter);
    const civ::TechTree tree(desc);
    const auto& shopBps = tree.GetByproducts(BuildingType::Custom0);
    BOOST_TEST_REQUIRE(shopBps.size() == 1u);
    BOOST_TEST((shopBps[0].ware == GoodType::Custom0));
    BOOST_TEST(shopBps[0].amount == 2u);
    BOOST_TEST_REQUIRE(shopBps[0].technology.has_value());
    BOOST_TEST(*shopBps[0].technology == 0u);
    BOOST_TEST(shopBps[0].AppliesTo(getCustomGood(1)));
    // Never a byproduct of itself
    BOOST_TEST(!shopBps[0].AppliesTo(GoodType::Custom0));

    const auto& hunterBps = tree.GetByproducts(BuildingType::Hunter);
    BOOST_TEST_REQUIRE(hunterBps.size() == 1u);
    BOOST_TEST(!hunterBps[0].technology.has_value());
    BOOST_TEST(hunterBps[0].AppliesTo(GoodType::Meat));
    BOOST_TEST(!hunterBps[0].AppliesTo(GoodType::Ham));
    BOOST_TEST(tree.GetByproducts(BuildingType::Slaughterhouse).empty());
}

BOOST_AUTO_TEST_CASE(InvalidByproducts)
{
    const auto expectError = [](const WorldDescription& desc) {
        BOOST_CHECK_THROW(civ::TechTree{desc}, GameDataError);
    };
    {
        WorldDescription desc = makeByproductDesc(1, true);
        desc.byproducts.front().building = "NoBuilding";
        expectError(desc);
    }
    {
        // Warehouses have no worker who carries out products
        WorldDescription desc = makeByproductDesc(1, true);
        desc.byproducts.front().building = "Storehouse";
        expectError(desc);
    }
    {
        WorldDescription desc = makeByproductDesc(1, true);
        desc.byproducts.front().ware = {"Unknown"};
        expectError(desc);
    }
    {
        // Must match exactly one ware
        WorldDescription desc = makeInheritDesc();
        TechnologyDesc tech;
        tech.name = "tech";
        tech.title = "Tech";
        desc.technologies.add(tech);
        ByproductDesc byproduct;
        byproduct.building = "Hunter";
        byproduct.ware = {"Holz"};
        desc.byproducts.push_back(byproduct);
        expectError(desc);
    }
    {
        WorldDescription desc = makeByproductDesc(1, true);
        desc.byproducts.front().technology = "unknownTech";
        expectError(desc);
    }
    {
        WorldDescription desc = makeByproductDesc(1, true);
        desc.byproducts.front().per = {"Unknown"};
        expectError(desc);
    }
    {
        // At most 7 per building in total (flag holds 8 wares)
        WorldDescription desc = makeByproductDesc(4, true);
        desc.byproducts.push_back(desc.byproducts.front());
        expectError(desc);
    }
}

BOOST_AUTO_TEST_CASE(OutputChoiceByKnowledge)
{
    const civ::Content content(makeKnowledgeChoiceDesc());
    const civ::CustomBuilding* carver = content.GetBuilding(BuildingType::Custom0);
    BOOST_TEST_REQUIRE(carver);
    BOOST_TEST((carver->productChoices == std::vector<GoodType>{getCustomGood(1), getCustomGood(2), getCustomGood(3)}));
    BOOST_TEST((carver->product == getCustomGood(1)));
    {
        // Every product needs knowledge points
        WorldDescription desc = makeKnowledgeChoiceDesc();
        desc.wares[2].knowledge = {};
        BOOST_CHECK_THROW(civ::Content{desc}, GameDataError);
    }
    {
        // Without outputChoice the output must be unique
        WorldDescription desc = makeKnowledgeChoiceDesc();
        desc.workshops.front().outputChoice.clear();
        BOOST_CHECK_THROW(civ::Content{desc}, GameDataError);
    }
}

BOOST_AUTO_TEST_CASE(ByproductsAndOutputChoiceFromLua)
{
    rttr::test::TmpFolder tmp;
    const auto load = [&tmp](const std::string& script, WorldDescription& desc) {
        {
            bnw::ofstream file(tmp / "default.lua");
            file << script;
        }
        GameDataLoader loader(desc, tmp);
        return loader.Load();
    };
    const std::string wares = R"(
rttr:AddWare{ name = "Bones", look = "Stones", transport = "Stones" }
rttr:AddWare{ name = "CarvedL", kind = "Carved", properties = { "LWP" }, look = "Stones", transport = "Stones",
              knowledge = { LWP = 1 } }
rttr:AddWare{ name = "CarvedI", kind = "Carved", properties = { "IWP" }, look = "Stones", transport = "Stones",
              knowledge = { IWP = 1 } }
rttr:AddWorkshop{ name = "Carver", size = "small", cost = { Boards = 2 }, look = "Hunter",
                  worker = { tool = "Cleaver", look = "Butcher" }, inputs = { "Bones" }, output = "Carved",
                  outputChoice = "knowledge", workTime = 30 }
rttr:AddTechnology{ name = "bones", title = "Bones", knowledge = { LWP = 1 }, unlocks = { buildings = { "Carver" } } }
)";
    {
        WorldDescription desc;
        BOOST_TEST_REQUIRE(load(wares + R"(
rttr:AddByproduct{ building = "Hunter", ware = "Bones", amount = 2, per = "Meat", technology = "bones" }
rttr:AddByproduct{ building = "Slaughterhouse", ware = "Bones" }
)",
                                desc));
        BOOST_TEST_REQUIRE(desc.byproducts.size() == 2u);
        BOOST_TEST(desc.byproducts[0].amount == 2u);
        BOOST_TEST((desc.byproducts[0].per == TagQuery{"Meat"}));
        BOOST_TEST(desc.byproducts[0].technology == "bones");
        BOOST_TEST(desc.byproducts[1].amount == 1u);
        BOOST_TEST(desc.byproducts[1].technology.empty());
        BOOST_TEST(desc.workshops.front().outputChoice == "knowledge");
        const civ::TechTree tree(desc);
        BOOST_TEST(tree.GetByproducts(BuildingType::Hunter).size() == 1u);
        BOOST_TEST(tree.GetContent().GetBuilding(BuildingType::Custom0)->productChoices.size() == 2u);
    }
    for(const std::string invalid : {R"(rttr:AddByproduct{ building = "Hunter", ware = "Bones", amount = 0 })",
                                     R"(rttr:AddByproduct{ building = "Hunter", ware = "Bones", amount = 8 })",
                                     R"(rttr:AddByproduct{ building = "Hunter" })",
                                     R"(rttr:AddByproduct{ building = "Hunter", ware = "Bones", typo = 1 })"})
    {
        WorldDescription desc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!load(wares + invalid, desc));
    }
    {
        // outputChoice only "knowledge"
        WorldDescription desc;
        std::string script = wares;
        script.replace(script.find("outputChoice = \"knowledge\""), 26, "outputChoice = \"random\"");
        BOOST_TEST(!load(script, desc));
    }
}

namespace {
struct ContentWorldFixture : public WorldWithGCExecution1P
{
    ContentWorldFixture(WorldDescription desc = makeSimpleDesc())
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
    }

    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    unsigned countInHQ(GoodType good) { return hq().GetNumRealWares(good); }

    /// Place a finished custom building connected to the HQ
    MapPoint placeConnectedBuilding(BuildingType bld)
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + Position(4, 2));
        BuildingFactory::CreateBuilding(world, bld, pos, 0, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<nobUsual>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        return pos;
    }
};

struct InheritWorldFixture : public ContentWorldFixture
{
    InheritWorldFixture() : ContentWorldFixture(makeInheritDesc()) {}
};
} // namespace

BOOST_FIXTURE_TEST_CASE(WorkshopProducesWares, ContentWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Custom0, 3), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    BOOST_TEST(world.GetPlayer(0).GetInventory()[GoodType::Custom0] == 3u);
    const MapPoint bldPos = placeConnectedBuilding(BuildingType::Custom0);
    const auto& bld = *world.GetSpecObj<nobUsual>(bldPos);
    // Worker is recruited (helper + saw), wares are delivered and processed
    RTTR_EXEC_TILL(4000, bld.HasWorker());
    BOOST_TEST((bld.GetWorker()->GetJobType() == Job::Custom0));
    const GoodType product = getCustomGood(1);
    RTTR_EXEC_TILL(8000, countInHQ(product) >= 3u);
    BOOST_TEST(countInHQ(GoodType::Custom0) == 0u);
    BOOST_TEST(world.GetPlayer(0).GetInventory()[GoodType::Custom0] == 0u);
    BOOST_TEST(world.GetPlayer(0).GetInventory()[product] >= 3u);
}

struct BuiltinInputWorldFixture : public ContentWorldFixture
{
    BuiltinInputWorldFixture() : ContentWorldFixture(makeBuiltinInputDesc()) {}
};

BOOST_FIXTURE_TEST_CASE(WorkshopUsesBuiltinInput, BuiltinInputWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Stones, 5), true);
    const unsigned stonesBefore = world.GetPlayer(0).GetInventory()[GoodType::Stones];
    placeConnectedBuilding(BuildingType::Custom0);
    const GoodType product = getCustomGood(0);
    // Stones are delivered by carriers like for any other consumer
    RTTR_EXEC_TILL(10000, countInHQ(product) >= 2u);
    BOOST_TEST(world.GetPlayer(0).GetInventory()[GoodType::Stones] < stonesBefore);
}

BOOST_FIXTURE_TEST_CASE(WorkshopPassesOnProperties, InheritWorldFixture)
{
    const auto needleWood = getCustomGood(1);
    const auto leafBoards = getCustomGood(2), needleBoards = getCustomGood(3);
    hq().AddToInventory(GoodCounts::make(needleWood, 2), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    placeConnectedBuilding(BuildingType::Custom0);
    RTTR_EXEC_TILL(10000, countInHQ(needleBoards) >= 2u);
    BOOST_TEST(countInHQ(leafBoards) == 0u);
}

BOOST_FIXTURE_TEST_CASE(SerializeCustomContent, ContentWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Custom0, 2), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    const MapPoint bldPos = placeConnectedBuilding(BuildingType::Custom0);
    RTTR_EXEC_TILL(4000, world.GetSpecObj<nobUsual>(bldPos)->HasWorker());

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    const auto* bld = world.GetSpecObj<nobUsual>(bldPos);
    BOOST_TEST_REQUIRE(bld);
    BOOST_TEST((bld->GetBuildingType() == BuildingType::Custom0));
    // Production continues after loading
    RTTR_EXEC_TILL(8000, countInHQ(getCustomGood(1)) >= 2u);

    // Same data again
    SerializedGameData sgd2;
    sgd2.MakeSnapshot(*game);
    SerializedGameData sgd3;
    sgd3.MakeSnapshot(*game);
    BOOST_CHECK_EQUAL_COLLECTIONS(sgd2.GetData(), sgd2.GetData() + sgd2.GetLength(), sgd3.GetData(),
                                  sgd3.GetData() + sgd3.GetLength());

    // Loading with reordered definitions fails, appended definitions are fine
    {
        WorldDescription desc = makeSimpleDesc();
        desc.wares.push_back(makeWare("Appended"));
        desc.workshops.push_back(makeWorkshop("Shop2", {{"Appended"}}, {"Raw"}));
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        em.Clear();
        world.Unload();
        sgd2.ReadSnapshot(*game, lgs);
    }
    {
        WorldDescription desc = makeSimpleDesc();
        std::swap(desc.wares[0], desc.wares[1]);
        desc.workshops.front().inputs = {{"Raw"}};
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        em.Clear();
        world.Unload();
        BOOST_CHECK_THROW(sgd3.ReadSnapshot(*game, lgs), SerializedGameData::Error);
    }
}

namespace {
struct ByproductWorldFixture : public ContentWorldFixture
{
    ByproductWorldFixture(unsigned amount = 1) : ContentWorldFixture(makeByproductDesc(amount, true)) {}
    const GoodType raw = GoodType::Custom0, product = getCustomGood(1);
    unsigned inInventory(GoodType good) { return world.GetPlayer(0).GetInventory()[good]; }
};
struct ByproductWorldFixture2 : public ByproductWorldFixture
{
    ByproductWorldFixture2() : ByproductWorldFixture(2) {}
};
} // namespace

BOOST_FIXTURE_TEST_CASE(ByproductOnlyAfterTechnology, ByproductWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    placeConnectedBuilding(BuildingType::Custom0);
    RTTR_EXEC_TILL(8000, countInHQ(product) >= 2u);
    // Technology not researched yet -> no byproduct
    BOOST_TEST(inInventory(raw) == 0u);

    civ::completeResearch(world, 0, 0);
    const unsigned productsBefore = inInventory(product);
    RTTR_EXEC_TILL(8000, countInHQ(product) >= productsBefore + 3u);
    // One byproduct per product put at the flag after the research (the product carried at that moment may count)
    const unsigned newProducts = inInventory(product) - productsBefore;
    BOOST_TEST(inInventory(raw) >= newProducts);
    BOOST_TEST(inInventory(raw) <= newProducts + 1u);
    RTTR_EXEC_TILL(2000, countInHQ(raw) >= 2u);
}

BOOST_FIXTURE_TEST_CASE(BuiltinBuildingGetsByproduct, ContentWorldFixture)
{
    // Slaughterhouse (built-in building): 1 byproduct per meat
    {
        WorldDescription desc = makeByproductDesc(1, false);
        desc.byproducts.front().building = "Slaughterhouse";
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
    }
    hq().AddToInventory(GoodCounts::make(GoodType::Ham, 3), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Cleaver, 1), true);
    placeConnectedBuilding(BuildingType::Slaughterhouse);
    RTTR_EXEC_TILL(10000, countInHQ(GoodType::Meat) >= 3u);
    BOOST_TEST(world.GetPlayer(0).GetInventory()[GoodType::Custom0] == 3u);
    RTTR_EXEC_TILL(2000, countInHQ(GoodType::Custom0) == 3u);
}

BOOST_FIXTURE_TEST_CASE(ByproductsWaitForSpaceAtFlag, ByproductWorldFixture2)
{
    civ::completeResearch(world, 0, 0);
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    const MapPoint bldPos = placeConnectedBuilding(BuildingType::Custom0);
    const auto& bld = *world.GetSpecObj<nobUsual>(bldPos);
    RTTR_EXEC_TILL(4000, bld.HasWorker() && bld.GetWorker()->GetState() != nofBuildingWorker::State::FigureWork);
    // Disconnect the building: Everything the worker brings out stays at the flag
    const MapPoint flagPos = world.GetNeighbour(bldPos, Direction::SouthEast);
    for(const auto dir : helpers::EnumRange<Direction>{})
    {
        if(dir != Direction::NorthWest && world.GetPointRoad(flagPos, dir) == PointRoad::Normal)
        {
            this->DestroyRoad(flagPos, dir);
            break;
        }
    }
    const noFlag& flag = *world.GetSpecObj<noFlag>(flagPos);
    BOOST_TEST_REQUIRE(flag.GetNumWares() == 0u);
    // Product + 2 byproducts each time: 3, 6, then only 2 places are left -> wait instead of leaving a part behind
    RTTR_EXEC_TILL(8000, flag.GetNumWares() == 6u);
    for(unsigned i = 0; i < 3000; ++i)
        em.ExecuteNextGF();
    BOOST_TEST(flag.GetNumWares() == 6u);
    BOOST_TEST((bld.GetWorker()->GetState() == nofBuildingWorker::State::WaitForWareSpace));
    BOOST_TEST(inInventory(raw) == 2u * inInventory(product));

    // Reconnect: the wares are carried away and the worker brings out the waiting ones
    this->BuildRoadForBlds(bldPos, hqPos);
    RTTR_EXEC_TILL(10000, countInHQ(product) >= 4u);
    BOOST_TEST(inInventory(raw) == 2u * inInventory(product));
}

BOOST_FIXTURE_TEST_CASE(WorkshopChoosesProductByKnowledge, ContentWorldFixture)
{
    world.SetTechTree(std::make_unique<civ::TechTree>(makeKnowledgeChoiceDesc()));
    civ::startGame(world);
    constexpr unsigned numBones = 12;
    hq().AddToInventory(GoodCounts::make(GoodType::Custom0, numBones), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    placeConnectedBuilding(BuildingType::Custom0);
    const auto numCarved = [this]() {
        return countInHQ(getCustomGood(1)) + countInHQ(getCustomGood(2)) + countInHQ(getCustomGood(3));
    };
    RTTR_EXEC_TILL(30000, numCarved() == numBones);
    // Until the regulator exists, all types are made (12 tries: chance that one is missing is < 2.5%, but the game
    // RNG is seeded the same in every test run)
    for(unsigned i = 1; i <= 3; ++i)
        BOOST_TEST(countInHQ(getCustomGood(i)) > 0u);
}

BOOST_AUTO_TEST_SUITE_END()
