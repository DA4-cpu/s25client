// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "RttrForeachPt.h"
#include "addons/const_addons.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/Identifiers.h"
#include "civ/Looks.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "factories/BuildingFactory.h"
#include "figures/nofFarmhand.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "lua/GameDataLoader.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "nodeObjs/noGrainfield.h"
#include "nodeObjs/noGranite.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/GoodConsts.h"
#include "gameData/JobConsts.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <map>
#include <memory>

// Civilization mode: built-in buildings set in lua (rttr:SetBuilding)
BOOST_AUTO_TEST_SUITE(CivBuiltinBuildingsSuite)

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

bool sameTimes(const civ::WorkTimes& lhs, const civ::WorkTimes& rhs)
{
    return lhs.work == rhs.work && lhs.wait == rhs.wait && lhs.waitAfter == rhs.waitAfter;
}

bool sameMilitary(const civ::MilitaryValues& lhs, const civ::MilitaryValues& rhs)
{
    return lhs.soldiers == rhs.soldiers && lhs.radius == rhs.radius && lhs.vision == rhs.vision
           && lhs.coins == rhs.coins && lhs.armor == rhs.armor && lhs.look == rhs.look;
}
} // namespace

BOOST_AUTO_TEST_CASE(ShippedBuildingsKeepTheOriginalValues)
{
    // Names of the original game (no content of the civilization mode is active here)
    std::map<BuildingType, std::string> originalNames;
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!isCustomBuilding(bld) && BuildingProperties::IsValid(bld))
            originalNames[bld] = BUILDING_NAMES[bld];
    }
    std::map<Job, std::string> originalJobNames;
    for(unsigned i = 0; i < NUM_BUILTIN_JOBS; ++i)
        originalJobNames[Job(i)] = JOB_NAMES[Job(i)];

    const civ::TechTree tree = civ::loadDefaultTechTree();
    const civ::Content& content = tree.GetContent();
    const civ::Content original;
    // Radius of the original game (addons for the reach off)
    const std::map<BuildingType, unsigned> radius = {{BuildingType::Woodcutter, 6u}, {BuildingType::Forester, 6u},
                                                     {BuildingType::Quarry, 8u},     {BuildingType::Fishery, 7u},
                                                     {BuildingType::Hunter, 19u},    {BuildingType::Farm, 2u},
                                                     {BuildingType::Vineyard, 2u},   {BuildingType::Charburner, 3u}};
    for(const auto& entry : originalNames)
    {
        const BuildingType bld = entry.first;
        BOOST_TEST_INFO(civ::getIdentifier(bld));
        // Every built-in building has an entry in workshops.lua with the current values
        BOOST_TEST(content.GetBuiltinTitle(bld).empty()
                   == (bld == BuildingType::Headquarters || bld == BuildingType::HarborBuilding
                       || helpers::contains(std::vector<BuildingType>{BuildingType::DonkeyBreeder,
                                                                      BuildingType::Shipyard, BuildingType::Catapult,
                                                                      BuildingType::LookoutTower, BuildingType::Temple},
                                            bld)));
        if(!content.GetBuiltinTitle(bld).empty())
            BOOST_TEST(content.GetBuiltinTitle(bld) == entry.second);
        BOOST_TEST(content.GetBuiltinDescription(bld).empty());
        BOOST_TEST((content.GetBuiltinSize(bld) == original.GetBuiltinSize(bld)));
        BOOST_TEST((content.GetBuiltinLook(bld) == bld));
        BOOST_TEST(sameTimes(content.GetWorkTimes(bld), original.GetWorkTimes(bld)));
        BOOST_TEST((content.GetMineResource(bld) == original.GetMineResource(bld)));
        BOOST_TEST(content.IsAINearBorder(bld) == original.IsAINearBorder(bld));
        BOOST_TEST(sameMilitary(content.GetMilitaryValues(bld, Nation::Romans),
                                original.GetMilitaryValues(bld, Nation::Romans)));
        const BldWorkDescription& workDesc = content.GetBuiltinWorkDesc(bld);
        const BldWorkDescription& originalWorkDesc = original.GetBuiltinWorkDesc(bld);
        if(BuildingProperties::IsMine(bld))
        {
            // Entscheidung des Nutzers (25.11.): Die Nahrung der Minen ist ein gemischter Platz mit 6 Plätzen, jeder
            // Arbeitsschritt braucht 1 beliebige Nahrung
            BOOST_TEST_REQUIRE(workDesc.inputs.size() == 1u);
            BOOST_TEST(workDesc.inputs[0].storage == 6u);
            BOOST_TEST(workDesc.inputs[0].amount == 100u);
            for(const GoodType food : {GoodType::Fish, GoodType::Meat, GoodType::Bread})
                BOOST_TEST(helpers::contains(workDesc.inputs[0].wares, food));
            BOOST_TEST(workDesc.useOneWareEach);
        } else
        {
            BOOST_TEST(workDesc.numSpacesPerWare == originalWorkDesc.numSpacesPerWare);
            BOOST_TEST_REQUIRE(workDesc.inputs.size() == originalWorkDesc.inputs.size());
            for(unsigned i = 0; i < workDesc.inputs.size(); ++i)
            {
                BOOST_TEST(workDesc.inputs[i].storage == originalWorkDesc.inputs[i].storage);
                BOOST_TEST((workDesc.inputs[i].wares == originalWorkDesc.inputs[i].wares));
                BOOST_TEST(workDesc.inputs[i].amount == originalWorkDesc.inputs[i].amount);
            }
        }
        // Produkte wie im Originalspiel (mit den umbenannten Waren)
        const civ::Products* products = content.GetProducts(bld);
        const civ::Products* originalProducts = original.GetProducts(bld);
        BOOST_TEST_REQUIRE((products == nullptr) == (originalProducts == nullptr));
        if(products)
        {
            BOOST_TEST((products->GetAllProducts() == originalProducts->GetAllProducts()));
            BOOST_TEST((products->productChoice == originalProducts->productChoice));
            BOOST_TEST(products->productSteps.empty());
        }
        // In the civilization mode the collectors and plantations have the original radius, the others none
        const auto itRadius = radius.find(bld);
        if(itRadius == radius.end())
            BOOST_TEST(!content.GetWorkRadius(bld));
        else
            BOOST_TEST(content.GetWorkRadius(bld).value_or(0u) == itRadius->second);
        BOOST_TEST(!original.GetWorkRadius(bld));
    }
    // Wares per step 1, the capacity of the warehouses
    BOOST_TEST(content.GetOutputRatio(BuildingType::Woodcutter) == 100u);
    BOOST_TEST(content.GetWarehouseCapacity(BuildingType::Headquarters) == 700u);
    // The jobs keep their original names and graphics
    for(const auto& entry : originalJobNames)
    {
        BOOST_TEST_INFO(civ::getIdentifier(entry.first));
        const std::string& title = content.GetBuiltinJobTitle(entry.first);
        BOOST_TEST((title.empty() || title == entry.second));
        BOOST_TEST((content.GetBuiltinJobLook(entry.first) == entry.first));
    }
    BOOST_TEST(content.GetBuiltinJobTitle(Job::Miner) == "Miner");
}

BOOST_AUTO_TEST_CASE(ValuesFromLua)
{
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(R"(
rttr:SetBuilding{ name = "Woodcutter", title = "Holzfaeller", description = "Faellt Baeume", size = "medium",
    look = "Sawmill", worker = { title = "Faeller", look = "Carpenter" }, collect = { radius = 9 },
    workTime = 100, waitTime = 200, aiNearBorder = false }
rttr:SetBuilding{ name = "Sawmill", storage = 4, waitAfterTime = 0 }
rttr:SetBuilding{ name = "Farm", plant = { radius = 3 } }
rttr:SetBuilding{ name = "CoalMine", worker = { title = "Bergmann" }, mine = { resource = "Iron" } }
rttr:SetBuilding{ name = "IronMine", worker = { title = "Bergmann", look = "Miner" } }
rttr:SetBuilding{ name = "Barracks", look = "Fortress", military = { soldiers = 4, radius = 9, coins = 0 } }
rttr:SetBuilding{ name = "Mill", aiNearBorder = true }
rttr:SetBuilding{ name = "Quarry", aiNearBorder = false }
)",
                               desc));
    const ActiveContent active(desc);
    const civ::Content& content = *active.content;
    BOOST_TEST(content.GetBuiltinTitle(BuildingType::Woodcutter) == "Holzfaeller");
    BOOST_TEST(std::string(BUILDING_NAMES[BuildingType::Woodcutter]) == "Holzfaeller");
    BOOST_TEST(std::string(BUILDING_HELP_STRINGS[BuildingType::Woodcutter]) == "Faellt Baeume");
    BOOST_TEST((BUILDING_SIZE[BuildingType::Woodcutter] == BuildingQuality::House));
    BOOST_TEST((civ::getLook(BuildingType::Woodcutter) == BuildingType::Sawmill));
    // The job: same in all buildings
    BOOST_TEST(JOB_NAMES[Job::Woodcutter] == "Faeller");
    BOOST_TEST((civ::getLook(Job::Woodcutter) == Job::Carpenter));
    BOOST_TEST(JOB_NAMES[Job::Miner] == "Bergmann");
    BOOST_TEST((civ::getLook(Job::Miner) == Job::Miner));
    // Times in frames, not set: the original ones
    BOOST_TEST(sameTimes(content.GetWorkTimes(BuildingType::Woodcutter), civ::WorkTimes{100, 200, 5}));
    BOOST_TEST(sameTimes(content.GetWorkTimes(BuildingType::Sawmill), civ::WorkTimes{479, 600, 0}));
    // Radius of collectors and plantations
    BOOST_TEST(nofFarmhand::GetWorkRadius(BuildingType::Woodcutter) == 9u);
    BOOST_TEST(nofFarmhand::GetWorkRadius(BuildingType::Farm) == 3u);
    BOOST_TEST(nofFarmhand::GetWorkRadius(BuildingType::Quarry) == 8u);
    // Storage of every input
    const BldWorkDescription& sawmill = BLD_WORK_DESC[BuildingType::Sawmill];
    BOOST_TEST(sawmill.numSpacesPerWare == 4u);
    BOOST_TEST_REQUIRE(sawmill.inputs.size() == 1u);
    BOOST_TEST(sawmill.inputs[0].storage == 4u);
    // Ore of the mine
    BOOST_TEST((content.GetMineResource(BuildingType::CoalMine) == ResourceType::Iron));
    BOOST_TEST((content.GetMineResource(BuildingType::IronMine) == ResourceType::Iron));
    // Military values and the look (flags) of another military building
    const civ::MilitaryValues barracks = content.GetMilitaryValues(BuildingType::Barracks, Nation::Romans);
    BOOST_TEST(barracks.soldiers == 4u);
    BOOST_TEST(barracks.radius == 9u);
    BOOST_TEST(barracks.vision == 14u);
    BOOST_TEST(barracks.coins == 0u);
    BOOST_TEST(barracks.armor == 0u);
    BOOST_TEST(barracks.look == 3u);
    // Near the border
    BOOST_TEST(!content.IsAINearBorder(BuildingType::Woodcutter));
    BOOST_TEST(content.IsAINearBorder(BuildingType::Mill));
    BOOST_TEST(!content.IsAINearBorder(BuildingType::Quarry));
    BOOST_TEST(content.IsAINearBorder(BuildingType::Fishery));
    BOOST_TEST(content.IsAINearBorder(BuildingType::Guardhouse));
}

BOOST_AUTO_TEST_CASE(InputsAndOutputsFromLua)
{
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua(R"(
rttr:AddWare{ name = "Raw", kind = "Material", properties = { "Soft" }, look = "Wood", transport = "Wood" }
rttr:AddWare{ name = "HardRaw", kind = "Material", properties = { "Hard" }, look = "Wood", transport = "Wood" }
rttr:AddWare{ name = "SoftPlank", kind = "Plank", properties = { "Soft" }, look = "Boards", transport = "Boards" }
rttr:AddWare{ name = "HardPlank", kind = "Plank", properties = { "Hard" }, look = "Boards", transport = "Boards" }
rttr:AddWare{ name = "CopperAxe", kind = "Axe", look = "Axe", transport = "Axe" }
rttr:SetWare{ Axe = { name = "IronAxe", kind = { "Axe" } } }
rttr:SetWare{ Shield = { name = "IronShield", kind = { "Shield" } } }
rttr:SetBuilding{ name = "Sawmill", inputs = { "Material" }, outputs = { "Plank" }, inheritProperties = true }
rttr:SetBuilding{ name = "Metalworks", inputs = { "Iron", { tags = "Coal", amount = 2, storage = 4 } },
    outputs = { "IronAxe", { "CopperAxe", steps = 2 } } }
rttr:SetBuilding{ name = "Armory", outputs = { "IronShield" } }
rttr:SetBuilding{ name = "Well", inputs = { "Raw" }, outputs = { "Fish" } }
rttr:SetBuilding{ name = "Bakery", inputs = { } }
rttr:SetBuilding{ name = "CoalMine", inputs = { { tags = "Fish", storage = 3 } }, outputs = { "Gold" } }
rttr:SetBuilding{ name = "Farm", inputs = { "Water" }, outputs = { "Raw" } }
rttr:SetBuilding{ name = "Woodcutter", outputs = { "HardRaw" } }
)",
                               desc));
    const ActiveContent active(desc);
    const civ::Content& content = *active.content;
    const GoodType raw = GoodType::Custom0;
    const GoodType hardRaw = getCustomGood(1);
    const GoodType softPlank = getCustomGood(2);
    const GoodType hardPlank = getCustomGood(3);
    const GoodType copperAxe = getCustomGood(4);
    // Sägewerk: Eingang nach Tag, Produkt nach den Eigenschaften der verbrauchten Ware
    const BldWorkDescription& sawmill = BLD_WORK_DESC[BuildingType::Sawmill];
    BOOST_TEST_REQUIRE(sawmill.inputs.size() == 1u);
    BOOST_TEST((sawmill.inputs[0].wares == std::vector<GoodType>{raw, hardRaw}));
    BOOST_TEST(sawmill.inputs[0].storage == 6u);
    const civ::Products& sawmillProducts = *content.GetProducts(BuildingType::Sawmill);
    BOOST_TEST((sawmillProducts.GetProduct(raw) == softPlank));
    BOOST_TEST((sawmillProducts.GetProduct(hardRaw) == hardPlank));
    BOOST_TEST((boost::variant2::get<GoodType>(sawmill.producedWare) == softPlank));
    // Schlosserei: nur die beiden Äxte (der Tag "Axe" träfe beide, der Name nur die eingebaute), mit steps
    const BldWorkDescription& metalworks = BLD_WORK_DESC[BuildingType::Metalworks];
    BOOST_TEST_REQUIRE(metalworks.inputs.size() == 2u);
    BOOST_TEST(metalworks.inputs[1].amount == 200u);
    BOOST_TEST(metalworks.inputs[1].storage == 4u);
    const civ::Products& metalworksProducts = *content.GetProducts(BuildingType::Metalworks);
    BOOST_TEST((metalworksProducts.GetAllProducts() == std::vector<GoodType>{GoodType::Axe, copperAxe}));
    BOOST_TEST((metalworksProducts.productChoice == civ::ProductChoice::Distribution));
    BOOST_TEST(metalworksProducts.GetProductSteps(copperAxe) == 200u);
    BOOST_TEST(metalworksProducts.GetProductSteps(GoodType::Axe) == 100u);
    // Waffenschmiede nur noch Schilde: ein Produkt, keine Wahl
    const civ::Products& armoryProducts = *content.GetProducts(BuildingType::Armory);
    BOOST_TEST((armoryProducts.GetAllProducts() == std::vector<GoodType>{GoodType::ShieldRomans}));
    BOOST_TEST((armoryProducts.productChoice == civ::ProductChoice::None));
    // Alle Schilde heißen wie das umbenannte Schild der Römer
    BOOST_TEST(WARE_NAMES[GoodType::ShieldVikings] == "IronShield");
    // Brunnen mit Eingang, Bäckerei ohne Eingänge
    BOOST_TEST(BLD_WORK_DESC[BuildingType::Well].inputs.size() == 1u);
    BOOST_TEST((content.GetProducts(BuildingType::Well)->product == GoodType::Fish));
    BOOST_TEST(BLD_WORK_DESC[BuildingType::Bakery].inputs.empty());
    BOOST_TEST((content.GetProducts(BuildingType::Bakery)->product == GoodType::Bread));
    // Mine: der Eingang aus Lua, jeder Schritt braucht ihn (nicht mehr "eine beliebige Nahrung" aus 3 Plätzen)
    const BldWorkDescription& coalMine = BLD_WORK_DESC[BuildingType::CoalMine];
    BOOST_TEST_REQUIRE(coalMine.inputs.size() == 1u);
    BOOST_TEST((coalMine.inputs[0].wares == std::vector<GoodType>{GoodType::Fish}));
    BOOST_TEST(coalMine.inputs[0].storage == 3u);
    BOOST_TEST(coalMine.useOneWareEach);
    BOOST_TEST((content.GetProducts(BuildingType::CoalMine)->product == GoodType::Gold));
    BOOST_TEST((content.GetMineResource(BuildingType::CoalMine) == ResourceType::Coal));
    // Bauernhof mit Eingang, Holzfäller mit anderem Produkt
    BOOST_TEST(BLD_WORK_DESC[BuildingType::Farm].inputs.size() == 1u);
    BOOST_TEST((content.GetProducts(BuildingType::Farm)->product == raw));
    BOOST_TEST((content.GetProducts(BuildingType::Woodcutter)->product == hardRaw));
    // Nicht gesetzt: die Produkte des Originalspiels; Tempel, Eselzucht und Militär ohne Produkte aus dem Inhalt
    BOOST_TEST((content.GetProducts(BuildingType::Mill)->product == GoodType::Flour));
    BOOST_TEST(!content.GetProducts(BuildingType::Forester)->HasProduct());
    BOOST_TEST(!content.GetProducts(BuildingType::Temple));
    BOOST_TEST(!content.GetProducts(BuildingType::DonkeyBreeder));
    BOOST_TEST(!content.GetProducts(BuildingType::Barracks));
}

BOOST_AUTO_TEST_CASE(InvalidBuildings)
{
    for(const std::string& invalid :
        {// Fields the building does not have
         std::string(R"(rttr:SetBuilding{ name = "Hunter", workTime = 100 })"),
         std::string(R"(rttr:SetBuilding{ name = "Woodcutter", waitAfterTime = 10 })"),
         std::string(R"(rttr:SetBuilding{ name = "Woodcutter", storage = 3 })"),
         std::string(R"(rttr:SetBuilding{ name = "Well", worker = { title = "Wasserholer" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Mill", collect = { radius = 3 } })"),
         std::string(R"(rttr:SetBuilding{ name = "Woodcutter", plant = { radius = 3 } })"),
         std::string(R"(rttr:SetBuilding{ name = "Mill", mine = { resource = "Coal" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Headquarters", cost = { Boards = 1 } })"),
         std::string(R"(rttr:SetBuilding{ name = "HarborBuilding", cost = { Boards = 1 } })"),
         std::string(R"(rttr:SetBuilding{ name = "Barracks", worker = { title = "Soldat" } })"),
         std::string(R"(rttr:SetBuilding{ name = "DonkeyBreeder", workTime = 100 })"),
         std::string(R"(rttr:SetBuilding{ name = "Storehouse", military = { soldiers = 1, radius = 5, coins = 0 } })"),
         std::string(R"(rttr:SetBuilding{ name = "Woodcutter", warehouse = { capacity = 5 } })"),
         // Eingänge und Produkte
         std::string(R"(rttr:SetBuilding{ name = "Woodcutter", inputs = { "Water" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Forester", outputs = { "Wood" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Temple", outputs = { "Gold" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Temple", inputs = { "Wine" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Barracks", inputs = { "Coins" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Quarry", outputs = { "Stones", "Wood" } })"),
         std::string(R"(rttr:SetBuilding{ name = "IronMine", outputs = { "IronOre", "Coal" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Farm", outputs = { "Grain", "Wood" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Mill", outputs = { "Flour" }, outputChoice = "tools" })"),
         std::string(
           R"(rttr:SetBuilding{ name = "Mill", outputs = { "Flour", "Bread" }, outputChoice = "knowledge" })"),
         std::string(R"(rttr:SetBuilding{ name = "Mill", outputs = { "Flour", "Flour" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Mill", outputs = { "Nothing2" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Mill", inputs = { "Nothing2" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Mill", inputs = { { tags = "Grain", amount = 7 } } })"),
         std::string(R"(rttr:SetBuilding{ name = "Mill", outputs = { "Flour", "Bread" }, inheritProperties = true })"),
         // Size and look
         std::string(R"(rttr:SetBuilding{ name = "Mill", size = "mine" })"),
         std::string(R"(rttr:SetBuilding{ name = "CoalMine", size = "small" })"),
         std::string(R"(rttr:SetBuilding{ name = "Barracks", look = "Woodcutter" })"),
         std::string(R"(rttr:SetBuilding{ name = "Woodcutter", look = "Palisade" })"),
         // One job, different titles
         std::string(R"(rttr:SetBuilding{ name = "CoalMine", worker = { title = "Bergmann" } }
rttr:SetBuilding{ name = "IronMine", worker = { title = "Kumpel" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Woodcutter", worker = { look = "Cook" } })"),
         // Unknown ore, unknown building
         std::string(R"(rttr:SetBuilding{ name = "CoalMine", mine = { resource = "Tin" } })"),
         std::string(R"(rttr:SetBuilding{ name = "Shop", workTime = 100 })"),
         std::string(R"(rttr:SetBuilding{ name = "Nothing9", workTime = 100 })")})
    {
        WorldDescription desc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST_REQUIRE(loadLua(invalid, desc));
        BOOST_CHECK_THROW(civ::Content{desc}, GameDataError);
    }
}

namespace {
struct BuiltinWorldFixture : public WorldWithGCExecution1P
{
    explicit BuiltinWorldFixture(const std::string& lua)
    {
        WorldDescription desc;
        BOOST_TEST_REQUIRE(loadLua(lua, desc));
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
    }
    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    unsigned countInHQ(GoodType good) { return hq().GetNumRealWares(good); }
    MapPoint placeConnectedBuilding(BuildingType bld, Position offset = Position(4, 2))
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + offset);
        BuildingFactory::CreateBuilding(world, bld, pos, 0, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<nobUsual>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        return pos;
    }
};

struct MineFixture : public BuiltinWorldFixture
{
    // A coal mine digging iron, fast
    MineFixture()
        : BuiltinWorldFixture(
          R"(rttr:SetBuilding{ name = "CoalMine", mine = { resource = "Iron" }, workTime = 20, waitTime = 20 })")
    {}
};
} // namespace

BOOST_FIXTURE_TEST_CASE(MineDigsTheOreOfLua, MineFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::PickAxe, 1), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Fish, 10), true);
    const MapPoint bldPos = world.MakeMapPoint(hqPos + Position(4, 2));
    const MapPoint ironPt = world.MakeMapPoint(bldPos + Position(1, 0));
    const MapPoint coalPt = world.MakeMapPoint(bldPos + Position(0, 1));
    world.SetResource(ironPt, Resource(ResourceType::Iron, 3));
    world.SetResource(coalPt, Resource(ResourceType::Coal, 5));
    const unsigned coalBefore = countInHQ(GoodType::Coal);
    placeConnectedBuilding(BuildingType::CoalMine);
    const auto& bld = *world.GetSpecObj<nobUsual>(bldPos);
    RTTR_EXEC_TILL(4000, bld.HasWorker());
    // The product stays coal, the ore is iron
    RTTR_EXEC_TILL(4000, countInHQ(GoodType::Coal) >= coalBefore + 3u);
    BOOST_TEST(world.GetNode(ironPt).resources.getAmount() == 0u);
    BOOST_TEST(world.GetNode(coalPt).resources.getAmount() == 5u);
    RTTR_SKIP_GFS(1000);
    BOOST_TEST(countInHQ(GoodType::Coal) == coalBefore + 3u);
}

namespace {
const std::string otherProductsLua = R"(
rttr:AddWare{ name = "Raw", look = "Wood", transport = "Wood" }
rttr:SetBuilding{ name = "Quarry", outputs = { "Raw" }, waitTime = 20 }
rttr:SetBuilding{ name = "Mill", inputs = { "Raw" }, outputs = { "Fish" }, workTime = 20, waitTime = 20 }
rttr:SetBuilding{ name = "Farm", inputs = { "Raw" }, waitTime = 20 }
)";
const GoodType RAW = GoodType::Custom0;

struct OtherProductsFixture : public BuiltinWorldFixture
{
    OtherProductsFixture() : BuiltinWorldFixture(otherProductsLua) {}
    unsigned numGrainfields()
    {
        unsigned result = 0;
        RTTR_FOREACH_PT(MapPoint, world.GetSize())
        {
            if(world.GetNO(pt)->GetType() == NodalObjectType::Grainfield)
                ++result;
        }
        return result;
    }
};
} // namespace

BOOST_FIXTURE_TEST_CASE(CollectorBringsTheProductOfLua, OtherProductsFixture)
{
    // Der Steinmetz nimmt je Gang ein Stück vom Steinhaufen und bringt die Ware aus outputs statt Steinen
    hq().AddToInventory(GoodCounts::make(GoodType::PickAxe, 1), true);
    const MapPoint pilePos = world.MakeMapPoint(hqPos + Position(8, 0));
    world.DestroyNO(pilePos, false);
    world.SetNO(pilePos, new noGranite(GraniteType::One, 1));
    const unsigned numStones = countInHQ(GoodType::Stones);
    placeConnectedBuilding(BuildingType::Quarry, Position(6, 2));
    RTTR_EXEC_TILL(30000, world.GetNO(pilePos)->GetType() != NodalObjectType::Granite);
    RTTR_EXEC_TILL(3000, countInHQ(RAW) == 2u);
    BOOST_TEST(countInHQ(GoodType::Stones) == numStones);
}

BOOST_FIXTURE_TEST_CASE(WorkerUsesTheInputsOfLua, OtherProductsFixture)
{
    // Die Mühle braucht je Schritt 1 Raw statt Getreide und bringt Fisch statt Mehl
    hq().AddToInventory(GoodCounts::make(RAW, 2), true);
    const unsigned numFish = countInHQ(GoodType::Fish);
    placeConnectedBuilding(BuildingType::Mill);
    RTTR_EXEC_TILL(8000, countInHQ(GoodType::Fish) == numFish + 2u);
    RTTR_SKIP_GFS(2000);
    BOOST_TEST(countInHQ(GoodType::Fish) == numFish + 2u);
    BOOST_TEST(world.GetPlayer(0).GetInventory().goods[RAW] == 0u);
}

BOOST_FIXTURE_TEST_CASE(FarmerPlantsOnlyWithItsInputs, OtherProductsFixture)
{
    // Jedes Säen verbraucht 1 Raw: mit 2 Raw entstehen höchstens 2 Felder
    hq().AddToInventory(GoodCounts::make(GoodType::Scythe, 1), true);
    hq().AddToInventory(GoodCounts::make(RAW, 2), true);
    const unsigned numFieldsBefore = numGrainfields();
    placeConnectedBuilding(BuildingType::Farm, Position(6, 2));
    RTTR_EXEC_TILL(20000, world.GetPlayer(0).GetInventory().goods[RAW] == 0u);
    RTTR_SKIP_GFS(1000);
    BOOST_TEST(numGrainfields() == numFieldsBefore + 2u);
    RTTR_SKIP_GFS(5000);
    BOOST_TEST(numGrainfields() <= numFieldsBefore + 2u);
}

namespace {
struct FoodMineFixture : public BuiltinWorldFixture
{
    // Eine Kohlemine mit der Nahrung als gemischtem Platz (wie in workshops.lua)
    FoodMineFixture()
        : BuiltinWorldFixture(R"(rttr:SetWare{ Bread = { kind = { "Food" } } }
rttr:SetWare{ Fish = { kind = { "Food" } } }
rttr:SetBuilding{ name = "CoalMine", inputs = { { tags = "Food", storage = 6 } }, workTime = 20, waitTime = 20 })")
    {}
};
} // namespace

BOOST_FIXTURE_TEST_CASE(MineTakesAnyFoodOfItsInput, FoodMineFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::PickAxe, 1), true);
    // Nur Brot und Fisch, kein Fleisch
    GamePlayer& owner = world.GetPlayer(0);
    hq().AddToInventory(GoodCounts::make(GoodType::Bread, 2), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Fish, 1), true);
    const unsigned numBread = owner.GetInventory().goods[GoodType::Bread];
    const unsigned numFish = owner.GetInventory().goods[GoodType::Fish];
    // 3 Einheiten Kohle: genau 3 Arbeitsschritte
    const MapPoint bldPos = world.MakeMapPoint(hqPos + Position(4, 2));
    world.SetResource(world.MakeMapPoint(bldPos + Position(1, 0)), Resource(ResourceType::Coal, 3));
    const unsigned coalBefore = countInHQ(GoodType::Coal);
    placeConnectedBuilding(BuildingType::CoalMine);
    RTTR_EXEC_TILL(8000, countInHQ(GoodType::Coal) == coalBefore + 3u);
    RTTR_SKIP_GFS(2000);
    BOOST_TEST(countInHQ(GoodType::Coal) == coalBefore + 3u);
    // Jeder Arbeitsschritt hat genau eine Nahrung verbraucht, egal welche
    const unsigned usedFood =
      (numBread - owner.GetInventory().goods[GoodType::Bread]) + (numFish - owner.GetInventory().goods[GoodType::Fish]);
    BOOST_TEST(usedFood == 3u);
}

BOOST_AUTO_TEST_SUITE_END()
