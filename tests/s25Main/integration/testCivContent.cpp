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
#include "lua/GameDataLoader.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
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

BOOST_AUTO_TEST_SUITE_END()
