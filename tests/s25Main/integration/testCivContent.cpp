// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AddonHelperFunctions.h"
#include "BuildingRegister.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "Loader.h"
#include "PointOutput.h"
#include "RttrConfig.h"
#include "SerializedGameData.h"
#include "addons/const_addons.h"
#include "ai/AIPlayer.h"
#include "ai/aijh/AIConstruction.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/CustomGraphics.h"
#include "civ/Graphics.h"
#include "civ/ImageFile.h"
#include "civ/Knowledge.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "factories/AIFactory.h"
#include "factories/BuildingFactory.h"
#include "figures/nofBuildingWorker.h"
#include "files.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "lua/GameDataLoader.h"
#include "ogl/glArchivItem_Bitmap_Player.h"
#include "ogl/glArchivItem_Bitmap_Raw.h"
#include "ogl/glSmartBitmap.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "nodeObjs/noDeposit.h"
#include "nodeObjs/noFlag.h"
#include "gameTypes/MapTypes.h"
#include "gameData/BuildingConsts.h"
#include "gameData/GoodConsts.h"
#include "gameData/JobConsts.h"
#include "gameData/SettingTypeConv.h"
#include "gameData/WorldDescription.h"
#include "libsiedler2/Archiv.h"
#include "libsiedler2/ArchivItem_Palette.h"
#include "libsiedler2/PixelBufferPaletted.h"
#include "libsiedler2/libsiedler2.h"
#include "rttr/test/LogAccessor.hpp"
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
    desc.cost = {{{"Boards"}, 2}, {{"Stones"}, 1}};
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

/// Knowledge choice workshop plus a library with the given capacity
WorldDescription makeLibraryDesc(unsigned capacity = 20)
{
    WorldDescription desc = makeKnowledgeChoiceDesc();
    WorkshopDesc library;
    library.name = "Library";
    library.title = "LibraryTitle";
    library.size = "medium";
    library.cost = {{{"Boards"}, 2}};
    library.look = "Bakery";
    library.worker.title = "Librarian";
    library.worker.tool = "none";
    library.worker.look = "Helper";
    library.libraryCapacity = capacity;
    desc.workshops.push_back(library);
    return desc;
}

/// Loam (ware), a deposit with 2 sizes and a pit collecting it in the given radius
WorldDescription makeCollectorDesc(unsigned radius = 6)
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("Loam"));
    MapObjectDesc deposit;
    deposit.name = "Deposit";
    deposit.title = "DepositTitle";
    deposit.sizes = 2;
    deposit.look = "Granite1";
    desc.mapObjects.push_back(deposit);
    auto pit = makeWorkshop("Pit", {}, {"Loam"});
    pit.worker.tool = "Shovel";
    pit.worker.look = "Planer";
    pit.collectObject = "Deposit";
    pit.collectRadius = radius;
    pit.workTime = 1;
    desc.workshops.push_back(pit);
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
    const BuildingCosts& shopCost = content.GetBuildingCost(BuildingType::Custom0);
    BOOST_TEST_REQUIRE(shopCost.size() == 2u);
    BOOST_TEST((shopCost[0].wares == std::vector<GoodType>{GoodType::Boards}));
    BOOST_TEST(shopCost[0].amount == 2u);
    BOOST_TEST((shopCost[1].wares == std::vector<GoodType>{GoodType::Stones}));
    BOOST_TEST(shopCost[1].amount == 1u);
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
        desc.workshops.front().cost = {{{"Unknown"}, 2}};
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

BOOST_AUTO_TEST_CASE(MapObjectsAndCollectors)
{
    const civ::Content content(makeCollectorDesc());
    BOOST_TEST_REQUIRE(content.GetMapObjects().size() == 1u);
    const civ::CustomMapObject& deposit = *content.GetMapObject(0);
    BOOST_TEST(deposit.name == "Deposit");
    BOOST_TEST(deposit.sizes == 2u);
    BOOST_TEST(deposit.lookGranite == 0u);
    BOOST_TEST(deposit.tint == 0xFFFFFFu);
    BOOST_TEST(!content.GetMapObject(1));
    BOOST_TEST((content.FindMapObject("Deposit") == 0u));
    BOOST_TEST(!content.FindMapObject("Other"));
    const civ::CustomBuilding& pit = *content.GetBuilding(BuildingType::Custom0);
    BOOST_TEST(pit.IsCollector());
    BOOST_TEST(pit.collectObject == 0);
    BOOST_TEST(pit.collectRadius == 6u);
    BOOST_TEST((pit.product == GoodType::Custom0));
    BOOST_TEST(pit.workDesc.waresNeeded.empty());
    BOOST_TEST(!content.GetBuilding(BuildingType::Custom0)->IsLibrary());
    // Map objects are part of the signature: appending is fine, renaming not
    const auto signature = content.GetSignature();
    BOOST_TEST((signature == std::vector<std::string>{"W:Loam", "B:Pit", "O:Deposit"}));
    BOOST_TEST(content.IsCompatibleTo({"W:Loam", "B:Pit"}));
    BOOST_TEST(content.IsCompatibleTo(signature));
    BOOST_TEST(!content.IsCompatibleTo({"W:Loam", "B:Pit", "O:Other"}));
    BOOST_TEST(!content.IsCompatibleTo({"W:Loam", "B:Pit", "O:Deposit", "O:Second"}));

    // Invalid: unknown object, too many objects
    {
        WorldDescription desc = makeCollectorDesc();
        desc.workshops.front().collectObject = "Unknown";
        BOOST_CHECK_THROW(civ::Content{desc}, GameDataError);
    }
    {
        WorldDescription desc = makeCollectorDesc();
        desc.mapObjects.push_back(desc.mapObjects.front());
        BOOST_CHECK_THROW(civ::Content{desc}, GameDataError);
    }
}

BOOST_AUTO_TEST_CASE(MapObjectsAndCollectorsFromLua)
{
    const std::string base = R"(
rttr:AddWare{ name = "Loam", title = "Lehm", look = "Stones", transport = "Stones" }
rttr:AddMapObject{ name = "LoamDeposit", title = "Lehmvorkommen", sizes = 4, look = "Granite2", tint = 0xC08050 }
rttr:AddWorkshop{ name = "LoamPit", title = "Lehmgrube", size = "small", cost = { Boards = 2 }, look = "Quarry",
    worker = { title = "Lehmstecher", tool = "Shovel", look = "Planer" },
    collect = { object = "LoamDeposit", radius = 8 }, output = "Loam", workTime = 7, waitTime = 40 }
)";
    const auto load = [](const std::string& content, WorldDescription& desc) {
        rttr::test::TmpFolder tmp;
        {
            bnw::ofstream file(tmp / "default.lua");
            file << content;
        }
        GameDataLoader loader(desc, tmp);
        rttr::test::LogAccessor logAcc;
        return loader.Load();
    };
    {
        WorldDescription desc;
        BOOST_TEST_REQUIRE(load(base, desc));
        BOOST_TEST_REQUIRE(desc.mapObjects.size() == 1u);
        BOOST_TEST(desc.mapObjects[0].tint == 0xC08050u);
        BOOST_TEST(desc.workshops[0].collectObject == "LoamDeposit");
        BOOST_TEST(desc.workshops[0].collectRadius == 8u);
        const civ::Content content(desc);
        BOOST_TEST(content.GetMapObject(0)->sizes == 4u);
        BOOST_TEST(content.GetMapObject(0)->lookGranite == 1u);
        BOOST_TEST(content.GetBuilding(BuildingType::Custom0)->IsCollector());
    }
    const auto replaced = [&base](const std::string& from, const std::string& to) {
        std::string result = base;
        const auto pos = result.find(from);
        BOOST_TEST_REQUIRE(pos != std::string::npos);
        return result.replace(pos, from.size(), to);
    };
    for(const std::string& invalid :
        {replaced("sizes = 4", "sizes = 0"), replaced("sizes = 4", "sizes = 17"),
         replaced("look = \"Granite2\"", "look = \"Tree\""), replaced("tint = 0xC08050", "tint = 0x1000000"),
         replaced("radius = 8", "radius = 0"), replaced("collect = {", "inputs = { \"Loam\" }, collect = {")})
    {
        WorldDescription desc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!load(invalid, desc));
    }
}

BOOST_AUTO_TEST_CASE(WorkshopNearBorderFromLua)
{
    const std::string base = R"(
rttr:AddWare{ name = "Loam", title = "Lehm", look = "Stones", transport = "Stones" }
rttr:AddMapObject{ name = "LoamDeposit", title = "Lehmvorkommen", sizes = 4, look = "Granite2" }
rttr:AddWorkshop{ name = "LoamPit", title = "Lehmgrube", size = "small", cost = { Boards = 2 }, look = "Quarry",
    worker = { title = "Lehmstecher", tool = "Shovel", look = "Planer" },
    collect = { object = "LoamDeposit", radius = 8 }, output = "Loam", workTime = 7 AI_NEAR_BORDER }
)";
    const auto load = [&base](const std::string& aiNearBorder, WorldDescription& desc) {
        std::string content = base;
        const std::string marker = " AI_NEAR_BORDER";
        content.replace(content.find(marker), marker.size(), aiNearBorder);
        rttr::test::TmpFolder tmp;
        {
            bnw::ofstream file(tmp / "default.lua");
            file << content;
        }
        GameDataLoader loader(desc, tmp);
        rttr::test::LogAccessor logAcc;
        return loader.Load();
    };
    // Default: The AI keeps the space at the border free for military buildings
    {
        WorldDescription desc;
        BOOST_TEST_REQUIRE(load("", desc));
        BOOST_TEST(!desc.workshops[0].aiNearBorder);
        const ActiveContent active(desc);
        BOOST_TEST(!civ::getActiveContent().GetBuilding(BuildingType::Custom0)->aiNearBorder);
        BOOST_TEST(!AIJH::AIConstruction::IsAllowedNearBorder(BuildingType::Custom0));
        BOOST_TEST(AIJH::AIConstruction::GetBorderReserve(BuildingType::Custom0) == 4u);
    }
    {
        WorldDescription desc;
        BOOST_TEST_REQUIRE(load(", aiNearBorder = true", desc));
        BOOST_TEST(desc.workshops[0].aiNearBorder);
        const ActiveContent active(desc);
        BOOST_TEST(civ::getActiveContent().GetBuilding(BuildingType::Custom0)->aiNearBorder);
        BOOST_TEST(AIJH::AIConstruction::IsAllowedNearBorder(BuildingType::Custom0));
        BOOST_TEST(AIJH::AIConstruction::GetBorderReserve(BuildingType::Custom0) == 0u);
    }
    // Built-in buildings are not affected by the content
    {
        WorldDescription desc;
        BOOST_TEST_REQUIRE(load(", aiNearBorder = true", desc));
        const ActiveContent active(desc);
        BOOST_TEST(!AIJH::AIConstruction::IsAllowedNearBorder(BuildingType::Sawmill));
        BOOST_TEST(AIJH::AIConstruction::IsAllowedNearBorder(BuildingType::Quarry));
    }
}

BOOST_AUTO_TEST_CASE(GlobalTablesUseActiveContent)
{
    ActiveContent active(makeSimpleDesc());
    BOOST_TEST(WARE_NAMES[GoodType::Custom0] == "RawTitle");
    BOOST_TEST(WARE_NAMES[getCustomGood(2)].empty());
    BOOST_TEST(std::string(BUILDING_NAMES[BuildingType::Custom0]) == "ShopTitle");
    BOOST_TEST(BUILDING_COSTS[BuildingType::Custom0].front().amount == 2u);
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
    tech.cost = {{{"Raw"}, 2}};
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
        // outputChoice: "knowledge", "tools" or a regulator
        WorldDescription desc;
        std::string script = wares;
        script.replace(script.find("outputChoice = \"knowledge\""), 26, "outputChoice = \"random\"");
        BOOST_TEST_REQUIRE(load(script, desc));
        BOOST_CHECK_THROW(civ::TechTree{desc}, GameDataError);
        script.replace(script.find("outputChoice = \"random\""), 23, "outputChoice = \"no regulator\"");
        BOOST_TEST(!load(script, desc));
    }
}

BOOST_AUTO_TEST_CASE(KnowledgeMediaAndLibrary)
{
    WorldDescription desc = makeLibraryDesc(20);
    desc.knowledgeDistribution = KnowledgeValues{10, 3, 0};
    const civ::Content content(desc);
    const std::vector<GoodType> media{getCustomGood(1), getCustomGood(2), getCustomGood(3)};
    BOOST_TEST((content.GetKnowledgeMedia() == media));
    BOOST_TEST(content.GetKnowledgeOf(getCustomGood(2))[civ::KnowledgeType::Industry] == 1);
    BOOST_TEST(content.GetKnowledgeOf(GoodType::Custom0)[civ::KnowledgeType::Industry] == 0);
    BOOST_TEST(content.GetKnowledgeOf(GoodType::Boards)[civ::KnowledgeType::Agriculture] == 0);
    BOOST_TEST(content.GetDefaultKnowledgeDistribution()[civ::KnowledgeType::Agriculture] == 10u);
    BOOST_TEST(content.GetDefaultKnowledgeDistribution()[civ::KnowledgeType::Industry] == 3u);
    BOOST_TEST(content.GetDefaultKnowledgeDistribution()[civ::KnowledgeType::Military] == 0u);
    // Product choices know their knowledge type
    const civ::CustomBuilding& carver = *content.GetBuilding(BuildingType::Custom0);
    BOOST_TEST((carver.productChoiceTypes
                == std::vector<civ::KnowledgeType>{civ::KnowledgeType::Agriculture, civ::KnowledgeType::Industry,
                                                   civ::KnowledgeType::Military}));
    // The library takes nothing as input and produces nothing
    const civ::CustomBuilding& library = *content.GetBuilding(getCustomBuilding(1));
    BOOST_TEST(library.IsLibrary());
    BOOST_TEST(library.libraryCapacity == 20u);
    BOOST_TEST(library.workDesc.waresNeeded.empty());
    BOOST_TEST(!carver.IsLibrary());
    // Without lua setting: all equal
    BOOST_TEST(civ::Content(makeLibraryDesc()).GetDefaultKnowledgeDistribution()[civ::KnowledgeType::Military] == 5u);
    {
        // A library needs media
        WorldDescription noMedia = makeSimpleDesc();
        noMedia.workshops.push_back(makeLibraryDesc().workshops.back());
        BOOST_CHECK_THROW(civ::Content{noMedia}, GameDataError);
    }
    {
        // Products chosen by knowledge must have exactly one knowledge type
        WorldDescription twoTypes = makeKnowledgeChoiceDesc();
        twoTypes.wares[1].knowledge = {1, 1, 0};
        BOOST_CHECK_THROW(civ::Content{twoTypes}, GameDataError);
    }
}

BOOST_AUTO_TEST_CASE(LibraryAndRegulatorFromLua)
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
rttr:AddWare{ name = "CarvedL", kind = "Carved", properties = { "LWP" }, look = "Stones", transport = "Stones",
              knowledge = { LWP = 1 } }
)";
    const std::string library = R"(
rttr:AddWorkshop{ name = "Library", size = "medium", cost = { Boards = 2 }, look = "Bakery",
                  worker = { tool = "none", look = "Helper" }, library = { capacity = 20 } }
)";
    {
        WorldDescription desc;
        BOOST_TEST_REQUIRE(load(wares + library + "rttr:SetKnowledgeDistribution{ LWP = 10, MWP = 2 }", desc));
        BOOST_TEST_REQUIRE(desc.workshops.size() == 1u);
        BOOST_TEST(desc.workshops[0].libraryCapacity == 20u);
        BOOST_TEST_REQUIRE(desc.knowledgeDistribution.has_value());
        BOOST_TEST(((*desc.knowledgeDistribution) == KnowledgeValues{10, 0, 2}));
        BOOST_TEST(civ::Content(desc).GetBuilding(BuildingType::Custom0)->IsLibrary());
    }
    const auto replaced = [&library](const std::string& search, const std::string& replacement) {
        std::string result = library;
        result.replace(result.find(search), search.size(), replacement);
        return result;
    };
    for(const std::string invalid :
        {wares + library + "rttr:SetKnowledgeDistribution{ LWP = 11 }",
         wares + library + "rttr:SetKnowledgeDistribution{ LWP = 1 }\nrttr:SetKnowledgeDistribution{ LWP = 1 }",
         wares + library + "rttr:SetKnowledgeDistribution{ XWP = 1 }",
         wares + replaced("capacity = 20", "capacity = 0"),
         wares + replaced("capacity = 20", "capacity = 20, typo = 1"),
         wares + replaced("library =", "inputs = { \"CarvedL\" }, library ="),
         wares + replaced("library =", "output = \"CarvedL\", library ="),
         wares + replaced("library =", "workTime = 10, library =")})
    {
        WorldDescription desc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!load(invalid, desc));
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
struct CollectorWorldFixture : public ContentWorldFixture
{
    // Small radius as the test world is small (and wraps around)
    CollectorWorldFixture() : ContentWorldFixture(makeCollectorDesc(2)) {}
};
} // namespace

BOOST_FIXTURE_TEST_CASE(CollectorCollectsDeposits, CollectorWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Shovel, 1), true);
    const MapPoint bldPos = world.MakeMapPoint(hqPos + Position(4, 2));
    // 2 deposits with 2 wares each in range of the building (radius 2), one too far away
    const MapPoint deposit1 = world.MakeMapPoint(bldPos + Position(2, 0));
    const MapPoint deposit2 = world.MakeMapPoint(bldPos + Position(0, 2));
    const MapPoint farDeposit = world.MakeMapPoint(bldPos + Position(5, 0));
    BOOST_TEST_REQUIRE(world.CalcDistance(bldPos, deposit1) == 2u);
    BOOST_TEST_REQUIRE(world.CalcDistance(bldPos, deposit2) == 2u);
    BOOST_TEST_REQUIRE(world.CalcDistance(bldPos, farDeposit) > 2u);
    for(const MapPoint pt : {deposit1, deposit2, farDeposit})
    {
        world.SetNO(pt, new noDeposit(0, 1));
        world.RecalcBQAroundPointBig(pt);
    }
    BOOST_TEST((world.GetNO(deposit1)->GetType() == NodalObjectType::Deposit));
    placeConnectedBuilding(BuildingType::Custom0);
    const auto& bld = *world.GetSpecObj<nobUsual>(bldPos);
    RTTR_EXEC_TILL(4000, bld.HasWorker());
    BOOST_TEST((bld.GetWorker()->GetGOT() == GO_Type::NofCustomCollector));
    // The worker goes out, digs and brings the loam home: each deposit gets smaller and disappears after 2 loam
    RTTR_EXEC_TILL(20000, countInHQ(GoodType::Custom0) >= 4u);
    BOOST_TEST(!world.GetSpecObj<noDeposit>(deposit1));
    BOOST_TEST(!world.GetSpecObj<noDeposit>(deposit2));
    BOOST_TEST((world.GetNO(deposit1)->GetType() == NodalObjectType::Nothing));
    // Out of range: still there and nothing more to collect
    BOOST_TEST_REQUIRE(world.GetSpecObj<noDeposit>(farDeposit));
    BOOST_TEST(world.GetSpecObj<noDeposit>(farDeposit)->GetSize() == 1u);
    RTTR_SKIP_GFS(2000);
    BOOST_TEST(countInHQ(GoodType::Custom0) == 4u);
    BOOST_TEST(bld.GetProductivity() == 0u);
}

BOOST_FIXTURE_TEST_CASE(SerializeDepositsAndCollectors, CollectorWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Shovel, 1), true);
    const MapPoint bldPos = world.MakeMapPoint(hqPos + Position(4, 2));
    const MapPoint depositPos = world.MakeMapPoint(bldPos + Position(2, 0));
    world.SetNO(depositPos, new noDeposit(0, 1));
    world.RecalcBQAroundPointBig(depositPos);
    placeConnectedBuilding(BuildingType::Custom0);
    RTTR_EXEC_TILL(4000, world.GetSpecObj<nobUsual>(bldPos)->HasWorker());

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    const auto* deposit = world.GetSpecObj<noDeposit>(depositPos);
    BOOST_TEST_REQUIRE(deposit);
    BOOST_TEST(deposit->GetObjectIdx() == 0u);
    BOOST_TEST(deposit->GetSize() == 1u);
    RTTR_EXEC_TILL(20000, countInHQ(GoodType::Custom0) >= 2u);
    BOOST_TEST(!world.GetSpecObj<noDeposit>(depositPos));
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

namespace {
struct LibraryWorldFixture : public ContentWorldFixture
{
    LibraryWorldFixture() : ContentWorldFixture(makeLibraryDesc(20)) {}
    const GoodType mediumL = getCustomGood(1), mediumI = getCustomGood(2), mediumM = getCustomGood(3);
    const BuildingType carver = BuildingType::Custom0, library = getCustomBuilding(1);
    civ::KnowledgePoints total() { return civ::calcTotalKnowledge(world.GetPlayer(0)); }
    civ::PlayerResearch& research() { return world.GetPlayer(0).GetResearch(); }
    unsigned inInventory(GoodType good) { return world.GetPlayer(0).GetInventory()[good]; }
};
} // namespace

BOOST_FIXTURE_TEST_CASE(RegulatorCommandAndDefaults, LibraryWorldFixture)
{
    // Default from the tech tree (lua), here all equal
    auto distribution = research().GetKnowledgeDistribution(world.GetTechTree());
    BOOST_TEST(distribution[civ::KnowledgeType::Agriculture] == 5u);
    this->SetKnowledgeDistribution({10, 2, 0});
    distribution = research().GetKnowledgeDistribution(world.GetTechTree());
    BOOST_TEST(distribution[civ::KnowledgeType::Agriculture] == 10u);
    BOOST_TEST(distribution[civ::KnowledgeType::Industry] == 2u);
    BOOST_TEST(distribution[civ::KnowledgeType::Military] == 0u);
    // Values above 10 are limited
    this->SetKnowledgeDistribution({200, 2, 0});
    BOOST_TEST(research().GetKnowledgeDistribution(world.GetTechTree())[civ::KnowledgeType::Agriculture] == 10u);
}

BOOST_FIXTURE_TEST_CASE(RegulatorIgnoredWithoutCivMode, LibraryWorldFixture)
{
    ggs.setSelection(AddonId::CIVILIZATION, 0);
    this->SetKnowledgeDistribution({10, 0, 0});
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    BOOST_TEST(research().GetKnowledgeDistribution(world.GetTechTree())[civ::KnowledgeType::Military] == 5u);
}

BOOST_FIXTURE_TEST_CASE(RegulatorChoosesMedium, LibraryWorldFixture)
{
    // Only LWP media
    this->SetKnowledgeDistribution({10, 0, 0});
    constexpr unsigned numBones = 6;
    hq().AddToInventory(GoodCounts::make(GoodType::Custom0, numBones), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    placeConnectedBuilding(carver);
    RTTR_EXEC_TILL(20000, countInHQ(mediumL) == numBones);
    BOOST_TEST(inInventory(mediumI) == 0u);
    BOOST_TEST(inInventory(mediumM) == 0u);
}

BOOST_FIXTURE_TEST_CASE(MediaCountHalfInWarehouses, LibraryWorldFixture)
{
    const civ::KnowledgePoints before = total();
    hq().AddToInventory(GoodCounts::make(mediumL, 3), true);
    hq().AddToInventory(GoodCounts::make(mediumM, 2), true);
    const civ::KnowledgePoints after = total();
    // Tenths: 3 x 1 WP x 0.5 = 1.5 WP, 2 x 1 WP x 0.5 = 1 WP
    BOOST_TEST(after[civ::KnowledgeType::Agriculture] - before[civ::KnowledgeType::Agriculture] == 15);
    BOOST_TEST(after[civ::KnowledgeType::Industry] - before[civ::KnowledgeType::Industry] == 0);
    BOOST_TEST(after[civ::KnowledgeType::Military] - before[civ::KnowledgeType::Military] == 10);
    // Other wares do not count
    hq().AddToInventory(GoodCounts::make(GoodType::Custom0, 5), true);
    BOOST_TEST(total()[civ::KnowledgeType::Agriculture] == after[civ::KnowledgeType::Agriculture]);
}

BOOST_FIXTURE_TEST_CASE(LibraryOrdersAndStoresMedia, LibraryWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(mediumL, 15), true);
    hq().AddToInventory(GoodCounts::make(mediumI, 10), true);
    const civ::KnowledgePoints before = total();
    const MapPoint bldPos = placeConnectedBuilding(library);
    const auto& lib = *world.GetSpecObj<nobUsual>(bldPos);
    BOOST_TEST_REQUIRE(lib.IsLibrary());
    BOOST_TEST(lib.GetLibraryCapacity() == 20u);
    // Librarian (helper, no tool) arrives, the library fetches media by carriers up to its capacity
    RTTR_EXEC_TILL(20000, lib.GetNumLibraryMedia() == 20u);
    for(unsigned i = 0; i < 2000; ++i)
        em.ExecuteNextGF();
    BOOST_TEST(lib.GetNumLibraryMedia() == 20u);
    BOOST_TEST(lib.GetNumOrderedLibraryMedia() == 0u);
    BOOST_TEST(countInHQ(mediumL) + countInHQ(mediumI) == 5u);
    // Both kinds were taken
    BOOST_TEST(lib.GetLibraryMedia().count(mediumL) == 1u);
    BOOST_TEST(lib.GetLibraryMedia().count(mediumI) == 1u);
    // Media still belong to the player
    BOOST_TEST(inInventory(mediumL) == 15u);
    BOOST_TEST(inInventory(mediumI) == 10u);
    // In the library they count full instead of half: +0.5 WP per moved medium
    const civ::KnowledgePoints after = total();
    const unsigned movedL = lib.GetLibraryMedia().at(mediumL), movedI = lib.GetLibraryMedia().at(mediumI);
    BOOST_TEST(after[civ::KnowledgeType::Agriculture] - before[civ::KnowledgeType::Agriculture]
               == static_cast<int>(movedL) * 5);
    BOOST_TEST(after[civ::KnowledgeType::Industry] - before[civ::KnowledgeType::Industry]
               == static_cast<int>(movedI) * 5);

    // Destroyed library: media are lost, so are their points
    this->DestroyBuilding(bldPos);
    BOOST_TEST(inInventory(mediumL) + inInventory(mediumI) == 5u);
    const civ::KnowledgePoints destroyed = total();
    BOOST_TEST(destroyed[civ::KnowledgeType::Agriculture]
               == after[civ::KnowledgeType::Agriculture] - static_cast<int>(movedL) * 10);
}

BOOST_FIXTURE_TEST_CASE(LibraryStopsWhenProductionDisabled, LibraryWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(mediumL, 10), true);
    const MapPoint bldPos = placeConnectedBuilding(library);
    const auto& lib = *world.GetSpecObj<nobUsual>(bldPos);
    RTTR_EXEC_TILL(20000, lib.GetNumLibraryMedia() >= 2u);
    this->SetProductionEnabled(bldPos, false);
    RTTR_EXEC_TILL(4000, lib.GetNumOrderedLibraryMedia() == 0u);
    const unsigned numStored = lib.GetNumLibraryMedia();
    for(unsigned i = 0; i < 4000; ++i)
        em.ExecuteNextGF();
    BOOST_TEST(lib.GetNumLibraryMedia() == numStored);
    BOOST_TEST(numStored < 10u);
}

BOOST_FIXTURE_TEST_CASE(ProducedMediaGoToLibrary, LibraryWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Custom0, 4), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Saw, 1), true);
    const MapPoint libPos = world.MakeMapPoint(hqPos + Position(-4, 2));
    BuildingFactory::CreateBuilding(world, library, libPos, 0, Nation::Romans);
    this->BuildRoadForBlds(libPos, hqPos);
    const auto& lib = *world.GetSpecObj<nobUsual>(libPos);
    RTTR_EXEC_TILL(4000, lib.HasWorker() && lib.GetWorker()->GetState() != nofBuildingWorker::State::FigureWork);
    placeConnectedBuilding(carver);
    // Media made by the carver are delivered to the library directly
    RTTR_EXEC_TILL(30000, lib.GetNumLibraryMedia() == 4u);
    BOOST_TEST(countInHQ(mediumL) + countInHQ(mediumI) + countInHQ(mediumM) == 0u);
}

BOOST_FIXTURE_TEST_CASE(SerializeLibraryAndRegulator, LibraryWorldFixture)
{
    this->SetKnowledgeDistribution({7, 1, 3});
    hq().AddToInventory(GoodCounts::make(mediumL, 8), true);
    const MapPoint bldPos = placeConnectedBuilding(library);
    RTTR_EXEC_TILL(20000, world.GetSpecObj<nobUsual>(bldPos)->GetNumLibraryMedia() >= 3u);
    // Save while media are still on the way
    BOOST_TEST_REQUIRE(world.GetSpecObj<nobUsual>(bldPos)->GetNumOrderedLibraryMedia() > 0u);
    const unsigned numStored = world.GetSpecObj<nobUsual>(bldPos)->GetNumLibraryMedia();
    const civ::KnowledgePoints knowledgeBefore = total();

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    const auto* lib = world.GetSpecObj<nobUsual>(bldPos);
    BOOST_TEST_REQUIRE(lib);
    BOOST_TEST(lib->GetNumLibraryMedia() == numStored);
    const civ::KnowledgePoints knowledgeAfter = total();
    for(const auto type : helpers::enumRange<civ::KnowledgeType>())
        BOOST_TEST(knowledgeAfter[type] == knowledgeBefore[type]);
    const auto distribution = research().GetKnowledgeDistribution(world.GetTechTree());
    BOOST_TEST(distribution[civ::KnowledgeType::Agriculture] == 7u);
    BOOST_TEST(distribution[civ::KnowledgeType::Industry] == 1u);
    BOOST_TEST(distribution[civ::KnowledgeType::Military] == 3u);
    // Media on the way arrive after loading
    RTTR_EXEC_TILL(20000, lib->GetNumLibraryMedia() == 8u);
}

namespace {
using BiggerCivWorldFixture = WorldWithGCExecution<1, 24, 22>;
} // namespace

BOOST_FIXTURE_TEST_CASE(AIBuildsWorkshopsAndLibraries, BiggerCivWorldFixture)
{
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    WorldDescription desc = makeLibraryDesc(20);
    // The research needs knowledge only media can give: the planner of the new AI wants a carver (bones in stock), the
    // old rule for libraries one library for the media waiting in the warehouse
    TechnologyDesc tech;
    tech.name = "tech";
    tech.title = "Tech";
    tech.knowledge = {10, 0, 0};
    desc.technologies.add(tech);
    world.SetTechTree(std::make_unique<civ::TechTree>(desc));
    civ::startGame(world);
    addStartResources();
    auto& hqBld = *world.GetPlayer(curPlayer).GetHQ();
    // Bones and some media in stock -> the AI should build a carver and a library
    hqBld.AddToInventory(GoodCounts::make(GoodType::Custom0, 15), true);
    hqBld.AddToInventory(GoodCounts::make(getCustomGood(1), 5), true);
    world.InitAfterLoad();
    const BuildingRegister& buildings = world.GetPlayer(curPlayer).GetBuildingRegister();
    const auto hasBld = [&buildings](BuildingType bld) {
        return !buildings.GetBuildings(bld).empty()
               || helpers::contains_if(buildings.GetBuildingSites(),
                                       [bld](const noBuildingSite* site) { return site->GetBuildingType() == bld; });
    };
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    for(unsigned gf = 0; gf < 12000 && !(hasBld(BuildingType::Custom0) && hasBld(getCustomBuilding(1)));)
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
    BOOST_TEST(hasBld(BuildingType::Custom0));
    BOOST_TEST(hasBld(getCustomBuilding(1)));
}

BOOST_FIXTURE_TEST_CASE(AIBuildsCollectorNextToDeposits, BiggerCivWorldFixture)
{
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    // The research costs loam: the planner of the new AI wants a pit
    WorldDescription desc = makeCollectorDesc(4);
    TechnologyDesc tech;
    tech.name = "pottery";
    tech.title = "Pottery";
    tech.cost = {{"Loam", 5}};
    desc.technologies.add(tech);
    world.SetTechTree(std::make_unique<civ::TechTree>(desc));
    civ::startGame(world);
    addStartResources();
    auto& hqBld = *world.GetPlayer(curPlayer).GetHQ();
    hqBld.AddToInventory(GoodCounts::make(GoodType::Shovel, 2), true);
    // Some deposits south east of the HQ
    std::vector<MapPoint> deposits;
    for(const Position offset : {Position(5, 4), Position(6, 5), Position(4, 6)})
    {
        const MapPoint pt = world.MakeMapPoint(hqPos + offset);
        world.SetNO(pt, new noDeposit(0, 1));
        world.RecalcBQAroundPointBig(pt);
        deposits.push_back(pt);
    }
    world.InitAfterLoad();
    const BuildingRegister& buildings = world.GetPlayer(curPlayer).GetBuildingRegister();
    const auto findCollector = [&buildings]() {
        for(const noBuildingSite* site : buildings.GetBuildingSites())
        {
            if(site->GetBuildingType() == BuildingType::Custom0)
                return site->GetPos();
        }
        for(const nobUsual* bld : buildings.GetBuildings(BuildingType::Custom0))
            return bld->GetPos();
        return MapPoint::Invalid();
    };
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    for(unsigned gf = 0; gf < 12000 && !findCollector().isValid();)
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
    const MapPoint collectorPos = findCollector();
    BOOST_TEST_REQUIRE(collectorPos.isValid());
    // In range of the deposits
    BOOST_TEST(
      helpers::contains_if(deposits, [&](const MapPoint& pt) { return world.CalcDistance(pt, collectorPos) <= 4u; }));
}

BOOST_FIXTURE_TEST_CASE(AISetsRegulatorForResearch, BiggerCivWorldFixture)
{
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    WorldDescription desc = makeLibraryDesc(20);
    desc.knowledgeDistribution = KnowledgeValues{4, 4, 4};
    // Needs 10 LWP and 5 IWP, the HQ gives no points here
    TechnologyDesc tech;
    tech.name = "tech";
    tech.title = "Tech";
    tech.knowledge = {10, 5, 0};
    desc.technologies.add(tech);
    world.SetTechTree(std::make_unique<civ::TechTree>(desc));
    civ::startGame(world);
    addStartResources();
    world.InitAfterLoad();
    civ::PlayerResearch& research = world.GetPlayer(curPlayer).GetResearch();
    const auto setting = [&research, this]() {
        const auto distribution = research.GetKnowledgeDistribution(world.GetTechTree());
        return std::array<unsigned, 3>{distribution[civ::KnowledgeType::Agriculture],
                                       distribution[civ::KnowledgeType::Industry],
                                       distribution[civ::KnowledgeType::Military]};
    };
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    const auto runAI = [&ai, this](unsigned numGFs) {
        for(unsigned gf = 0; gf < numGFs;)
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
    };
    // The AI researches the technology and sets the regulator after the missing points: LWP 10, IWP 5, MWP 0
    runAI(600);
    BOOST_TEST_REQUIRE(research.GetCurrent().has_value());
    BOOST_TEST((setting() == std::array<unsigned, 3>{10, 5, 0}));
    // Nothing to research anymore -> default from lua
    research.SetResearched(0);
    runAI(600);
    BOOST_TEST(!research.GetCurrent().has_value());
    BOOST_TEST((setting() == std::array<unsigned, 3>{4, 4, 4}));
}

namespace {
namespace bfs = boost::filesystem;

libsiedler2::PixelBufferBGRA makeImage(uint16_t width, uint16_t height, libsiedler2::ColorBGRA clr)
{
    return libsiedler2::PixelBufferBGRA(width, height, clr);
}
} // namespace

BOOST_AUTO_TEST_CASE(ImageFileNames)
{
    auto info = civ::parseImageFileName("Gebaeude.png");
    BOOST_TEST_REQUIRE(info.has_value());
    BOOST_TEST(info->name == "gebaeude");
    BOOST_TEST(!info->nx);
    BOOST_TEST(!info->ny);
    info = civ::parseImageFileName("dir/Gebaeude_Schatten.nx12.ny-3.BMP");
    BOOST_TEST_REQUIRE(info.has_value());
    BOOST_TEST(info->name == "gebaeude_schatten");
    BOOST_TEST((info->nx == 12));
    BOOST_TEST((info->ny == -3));
    // Types of RttR archive folders are ignored
    info = civ::parseImageFileName("Gebaeude.rle.nx40.ny61.bmp");
    BOOST_TEST_REQUIRE(info.has_value());
    BOOST_TEST(info->name == "gebaeude");
    BOOST_TEST((info->ny == 61));
    // Not an image or unknown parts
    BOOST_TEST(!civ::parseImageFileName("Gebaeude.txt"));
    BOOST_TEST(!civ::parseImageFileName("Gebaeude.alt.png"));
    BOOST_TEST(!civ::parseImageFileName("Gebaeude.nxA.png"));
    BOOST_TEST(!civ::parseImageFileName(".png"));
    BOOST_TEST(!civ::parseImageFileName("LIESMICH"));
}

BOOST_AUTO_TEST_CASE(PngRoundTripAndPlayerColors)
{
    rttr::test::TmpFolder tmp;
    libsiedler2::PixelBufferBGRA img = makeImage(5, 3, libsiedler2::ColorBGRA());
    img.set(0, 0, libsiedler2::ColorBGRA(10, 20, 30, 255)); // B, G, R, A
    img.set(4, 2, libsiedler2::ColorBGRA(1, 2, 3, 128));
    const std::array<libsiedler2::ColorRGB, 4> playerColors = {
      libsiedler2::ColorRGB(0, 0, 200), libsiedler2::ColorRGB(0, 0, 180), libsiedler2::ColorRGB(0, 0, 160),
      libsiedler2::ColorRGB(0, 0, 140)};
    img.set(2, 1, libsiedler2::ColorBGRA(playerColors[2], 255));
    civ::writePngFile(tmp / "Test.png", img);
    const libsiedler2::PixelBufferBGRA loaded = civ::loadImageFile(tmp / "Test.png");
    BOOST_TEST_REQUIRE(loaded.getWidth() == 5u);
    BOOST_TEST_REQUIRE(loaded.getHeight() == 3u);
    for(uint16_t y = 0; y < 3; ++y)
    {
        for(uint16_t x = 0; x < 5; ++x)
            BOOST_TEST(loaded.get(x, y).asValue() == img.get(x, y).asValue());
    }
    BOOST_CHECK_THROW(civ::loadImageFile(tmp / "Missing.png"), std::runtime_error);
    {
        boost::nowide::ofstream broken(tmp / "Broken.png");
        broken << "no png";
    }
    BOOST_CHECK_THROW(civ::loadImageFile(tmp / "Broken.png"), std::runtime_error);

    // 8 bit BMP with palette (e.g. from s2_sprite_pipeline.py): Index 254 is transparent
    {
        libsiedler2::ArchivItem_Palette palette;
        for(unsigned i = 0; i < 256; ++i)
            palette.set(i, libsiedler2::ColorRGB(i, 255 - i, 7));
        palette.set(254, libsiedler2::TRANSPARENT_COLOR); // Like in the S2 palette
        libsiedler2::PixelBufferPaletted indices(3, 2, 254);
        indices.set(1, 0, 17);
        indices.set(2, 1, 253);
        auto palBmp = std::make_unique<glArchivItem_Bitmap_Raw>();
        BOOST_TEST_REQUIRE(palBmp->create(indices, &palette) == 0);
        libsiedler2::Archiv archive;
        archive.push(std::move(palBmp));
        BOOST_TEST_REQUIRE(libsiedler2::Write(tmp / "Gebaeude.rle.nx1.ny1.bmp", archive, &palette) == 0);
        const libsiedler2::PixelBufferBGRA bmpImg = civ::loadImageFile(tmp / "Gebaeude.rle.nx1.ny1.bmp");
        BOOST_TEST_REQUIRE(bmpImg.getWidth() == 3u);
        BOOST_TEST(bmpImg.get(0, 0).getAlpha() == 0u);
        BOOST_TEST(bmpImg.get(1, 0).getAlpha() == 255u);
        BOOST_TEST(bmpImg.get(1, 0).getRed() == 17u);
        BOOST_TEST(bmpImg.get(1, 0).getGreen() == 238u);
        BOOST_TEST(bmpImg.get(2, 1).getAlpha() == 255u); // Shadow index is visible
    }

    const auto plBmp = civ::createPlayerBitmap(loaded, Position(2, 3), playerColors);
    BOOST_TEST(plBmp->getNx() == 2);
    BOOST_TEST(plBmp->getNy() == 3);
    BOOST_TEST(plBmp->isPlayerColor(2, 1));
    BOOST_TEST(plBmp->getPlayerColorIdx(2, 1) == 2u);
    BOOST_TEST(!plBmp->isPlayerColor(0, 0));
    BOOST_TEST(!plBmp->isPlayerColor(1, 1));
    const auto bmp = civ::createBitmap(loaded, Position(-1, 7));
    BOOST_TEST(bmp->getWidth() == 5u);
    BOOST_TEST(bmp->getNx() == -1);
    BOOST_TEST(bmp->getNy() == 7);
}

BOOST_AUTO_TEST_CASE(AnchorOfOwnImages)
{
    civ::ImageFileInfo info;
    // Without placeholder: Center of the bottom line
    BOOST_TEST(civ::calcAnchor(info, Extent(10, 20), std::nullopt) == Position(5, 19));
    // Same relative position as the placeholder
    const civ::AnchorRef ref{Position(10, 30), Extent(20, 40)};
    BOOST_TEST(civ::calcAnchor(info, Extent(20, 40), ref) == Position(10, 30));
    BOOST_TEST(civ::calcAnchor(info, Extent(40, 20), ref) == Position(20, 15));
    // From the file name
    info.nx = 3;
    BOOST_TEST(civ::calcAnchor(info, Extent(40, 20), ref) == Position(3, 15));
    info.ny = -2;
    BOOST_TEST(civ::calcAnchor(info, Extent(40, 20), ref) == Position(3, -2));
    // Anchor of a placeholder drawn with an offset and of combined bitmaps
    glArchivItem_Bitmap_Raw body, overlay;
    body.init(10, 20, libsiedler2::TextureFormat::BGRA);
    body.setNx(5);
    body.setNy(18);
    overlay.init(6, 30, libsiedler2::TextureFormat::BGRA);
    overlay.setNx(2);
    overlay.setNy(25);
    auto bodyRef = civ::getAnchorRef(&body, DrawPoint(-9, 0));
    BOOST_TEST_REQUIRE(bodyRef.has_value());
    BOOST_TEST(bodyRef->origin == Position(14, 18));
    auto combined = civ::getAnchorRef({&body, nullptr, &overlay});
    BOOST_TEST_REQUIRE(combined.has_value());
    BOOST_TEST(combined->origin == Position(5, 25));
    // Right: max(10-5, 6-2) = 5, bottom: max(20-18, 30-25) = 5
    BOOST_TEST(combined->size == Extent(10, 30));
    BOOST_TEST(!civ::getAnchorRef(nullptr));
}

BOOST_AUTO_TEST_CASE(OwnGraphicsReplacePlaceholders)
{
    LOADER.LoadDummyGUIFiles();
    LOADER.LoadDummyMapFiles();
    const ActiveContent active(makeCollectorDesc());
    const civ::Content& content = *active.content;
    rttr::test::TmpFolder images, templates;
    const libsiedler2::ColorBGRA brown(40, 80, 160, 255);
    bfs::create_directories(images / "Loam");
    bfs::create_directories(images / "Deposit");
    bfs::create_directories(images / "Pit");
    civ::writePngFile(images / "Loam" / "Symbol.nx3.ny4.png", makeImage(8, 6, brown));
    civ::writePngFile(images / "Loam" / "fahne.png", makeImage(4, 4, brown)); // Case does not matter
    civ::writePngFile(images / "Deposit" / "Groesse1.png", makeImage(12, 7, brown));
    civ::writePngFile(images / "Deposit" / "Groesse1_Schatten.nx0.ny0.png", makeImage(14, 5, brown));
    civ::writePngFile(images / "Deposit" / "Groesse0_Winter.png", makeImage(9, 9, brown));
    civ::writePngFile(images / "Pit" / "Arbeiter_Symbol.png", makeImage(5, 5, brown));
    civ::writePngFile(images / "Pit" / "Arbeiter_Arbeit_0.png", makeImage(6, 6, brown));
    civ::writePngFile(images / "Pit" / "Arbeiter_Arbeit_1.png", makeImage(6, 6, brown));
    civ::writePngFile(images / "Pit" / "Arbeiter_Arbeit_3.png", makeImage(6, 6, brown)); // Gap: not used

    civ::CustomGraphics& gfx = LOADER.GetCivGraphics();
    civ::CustomGraphics::Settings settings;
    settings.folder = images.get();
    settings.templateFolder = templates.get();
    settings.landscape = "Gruenland";
    gfx.fillCaches(LOADER, nullptr, content, settings);

    const auto* icon = dynamic_cast<glArchivItem_Bitmap*>(LOADER.GetWareTex(GoodType::Custom0));
    BOOST_TEST_REQUIRE(icon);
    BOOST_TEST(icon == gfx.GetWareIcon(GoodType::Custom0));
    BOOST_TEST(icon->getWidth() == 8u);
    BOOST_TEST(icon->getNx() == 3);
    BOOST_TEST(icon->getNy() == 4);
    BOOST_TEST(gfx.GetWareFlag(GoodType::Custom0));
    BOOST_TEST(!gfx.GetWareDonkey(GoodType::Custom0));
    BOOST_TEST(LOADER.GetJobTex(Job::Custom0) == gfx.GetJobIcon(Job::Custom0));
    BOOST_TEST(gfx.GetJobIcon(Job::Custom0));
    BOOST_TEST(gfx.GetWorkFrames(Job::Custom0).size() == 2u);
    BOOST_TEST(!gfx.HasWalking(Job::Custom0));
    // Map object: Only the size with an own image, the winter variant is not used on greenland
    BOOST_TEST(!gfx.GetMapObject(0, 0));
    glSmartBitmap* deposit = gfx.GetMapObject(0, 1);
    BOOST_TEST_REQUIRE(deposit);
    BOOST_TEST(deposit->GetSize().x == 14u);
    BOOST_TEST(!gfx.GetMapObject(0, 2));
    BOOST_TEST(!gfx.GetMapObject(1, 0));
    BOOST_TEST(gfx.GetNumLoaded() == 7u);
    // Templates of the placeholders which exist in the dummy files (granite piles)
    BOOST_TEST(bfs::exists(templates / "Deposit" / "Groesse0.nx0.ny0.png"));
    BOOST_TEST(bfs::exists(templates / "Deposit" / "Groesse1.nx0.ny0.png"));

    // Winter: The landscape variant is preferred
    settings.landscape = "Winter";
    gfx.fillCaches(LOADER, nullptr, content, settings);
    BOOST_TEST_REQUIRE(gfx.GetMapObject(0, 0));
    BOOST_TEST(gfx.GetMapObject(0, 0)->GetSize().x == 9u);
    BOOST_TEST(gfx.GetMapObject(0, 1));
    BOOST_TEST(bfs::exists(templates / "Deposit" / "Groesse0_Winter.nx0.ny0.png"));

    // Without content everything is removed again
    gfx.fillCaches(LOADER, nullptr, civ::Content(), settings);
    BOOST_TEST(!gfx.GetWareIcon(GoodType::Custom0));
    BOOST_TEST(!gfx.GetMapObject(0, 1));
    BOOST_TEST(gfx.GetWorkFrames(Job::Custom0).empty());
    BOOST_TEST(gfx.GetNumLoaded() == 0u);
}

BOOST_AUTO_TEST_CASE(OreSignsUseTheSignsOfTheLookOre)
{
    LOADER.LoadDummyGUIFiles();
    LOADER.LoadDummyMapFiles();
    // As in the original map graphics the signs of the geologist are player bitmaps (no glArchivItem_Bitmap)
    const unsigned ironSign = civ::getOreSignImage(ResourceType::Iron, 0);
    BOOST_TEST_REQUIRE(LOADER.GetMapTexture(ironSign));
    BOOST_TEST_REQUIRE(!dynamic_cast<glArchivItem_Bitmap*>(LOADER.GetMapTexture(ironSign)));

    WorldDescription desc;
    OreDesc copper;
    copper.name = "Copper";
    copper.title = "Kupfer";
    copper.look = "Iron";
    copper.tint = 0xC07040;
    desc.ores.push_back(copper);
    const ActiveContent active(desc);
    rttr::test::TmpFolder images, templates;
    bfs::create_directories(images / "Copper");
    civ::writePngFile(images / "Copper" / "Schild2.png", makeImage(4, 6, libsiedler2::ColorBGRA(40, 80, 160, 255)));

    civ::CustomGraphics& gfx = LOADER.GetCivGraphics();
    civ::CustomGraphics::Settings settings;
    settings.folder = images.get();
    settings.templateFolder = templates.get();
    settings.landscape = "Gruenland";
    gfx.fillCaches(LOADER, nullptr, *active.content, settings);
    // Own image for much ore, otherwise the tinted sign of iron is drawn
    BOOST_TEST(!gfx.GetOreSign(0, 0));
    BOOST_TEST(!gfx.GetOreSign(0, 1));
    BOOST_TEST_REQUIRE(gfx.GetOreSign(0, 2));
    BOOST_TEST(gfx.GetOreSign(0, 2)->getWidth() == 4u);
    // Templates from the player bitmaps
    for(const std::string name : {"Schild0", "Schild1", "Schild2"})
        BOOST_TEST(bfs::exists(templates / "Copper" / (name + ".nx0.ny0.png")));

    gfx.fillCaches(LOADER, nullptr, civ::Content(), settings);
}

BOOST_AUTO_TEST_SUITE_END()
