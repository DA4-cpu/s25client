// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BuildingRegister.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "PointOutput.h"
#include "RttrForeachPt.h"
#include "SerializedGameData.h"
#include "addons/const_addons.h"
#include "ai/AIPlayer.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "buildings/nobUsual.h"
#include "civ/Climate.h"
#include "civ/Content.h"
#include "civ/MapCodes.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "factories/AIFactory.h"
#include "factories/BuildingFactory.h"
#include "figures/nofBuildingWorker.h"
#include "figures/nofCustomPlanter.h"
#include "helpers/containerUtils.h"
#include "lua/GameDataLoader.h"
#include "mapGenerator/RandomMap.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "worldFixtures/terrainHelpers.h"
#include "world/MapLoader.h"
#include "nodeObjs/noDeposit.h"
#include "gameData/BuildingProperties.h"
#include "gameData/MapGenDesc.h"
#include "gameData/WorldDescription.h"
#include "libsiedler2/Archiv.h"
#include "libsiedler2/ArchivItem_Map.h"
#include "libsiedler2/ArchivItem_Map_Header.h"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>
#include <set>

namespace bnw = boost::nowide;

BOOST_AUTO_TEST_SUITE(CivPlantsAndOresSuite)

namespace {
WareDesc makeWare(const std::string& name, const std::string& look = "Stones")
{
    WareDesc desc;
    desc.name = name;
    desc.title = name + "Title";
    desc.look = look;
    desc.transport = look;
    return desc;
}

/// Ore "Copper" with a mine digging it (ware 0 = CopperOre) and a plantation (ware 1 = PapyrusPlant) planting the
/// map object "Papyrus" (grows within 1 second to size 2) with water and harvesting it
WorldDescription makeDesc(unsigned plantRadius = 3)
{
    WorldDescription desc;
    desc.wares.push_back(makeWare("CopperOre", "IronOre"));
    desc.wares.push_back(makeWare("PapyrusPlant", "Grain"));
    OreDesc copper;
    copper.name = "Copper";
    copper.title = "Kupfer";
    copper.look = "Iron";
    copper.tint = 0xC07040;
    desc.ores.push_back(copper);
    MapObjectDesc papyrus;
    papyrus.name = "Papyrus";
    papyrus.title = "Papyrusvorkommen";
    papyrus.sizes = 3;
    papyrus.look = "Granite1";
    papyrus.growTime = 1;
    papyrus.plantSize = 2;
    desc.mapObjects.push_back(papyrus);

    WorkshopDesc mine;
    mine.name = "CopperMine";
    mine.title = "Kupfermine";
    mine.size = "mine";
    mine.cost = {{"Boards", 4}};
    mine.look = "IronMine";
    mine.worker.title = "Bergmann";
    mine.worker.tool = "PickAxe";
    mine.worker.look = "Miner";
    mine.inputs = {{"Fish"}, {"Meat"}, {"Bread"}};
    mine.consumeEach = false;
    mine.storage = 2;
    mine.mineResource = "Copper";
    mine.output = {"CopperOre"};
    mine.workTime = 1;
    mine.waitTime = 1;
    desc.workshops.push_back(mine);

    WorkshopDesc plantation;
    plantation.name = "Plantation";
    plantation.title = "Papyrusplantage";
    plantation.size = "small";
    plantation.cost = {{"Boards", 2}};
    plantation.look = "Farm";
    plantation.worker.title = "Papyrusbauer";
    plantation.worker.tool = "Scythe";
    plantation.worker.look = "Farmer";
    plantation.inputs = {{"Water"}};
    plantation.plantObject = "Papyrus";
    plantation.plantRadius = plantRadius;
    plantation.output = {"PapyrusPlant"};
    plantation.workTime = 1;
    plantation.waitTime = 1;
    desc.workshops.push_back(plantation);
    return desc;
}

constexpr BuildingType MINE = BuildingType::Custom0;
const BuildingType PLANTATION = getCustomBuilding(1);
constexpr GoodType COPPER_ORE = GoodType::Custom0;
const GoodType PAPYRUS_PLANT = getCustomGood(1);
constexpr ResourceType COPPER = ResourceType::Custom0;

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
} // namespace

BOOST_AUTO_TEST_CASE(ResourcesHaveSlotsForOres)
{
    BOOST_TEST(NUM_CUSTOM_ORES == civ::ORE_MAP_CODES.size());
    for(const auto type : helpers::enumRange<ResourceType>())
    {
        const Resource res(type, 5);
        const Resource copy(res.getValue());
        BOOST_TEST((copy.getType() == type));
        BOOST_TEST(copy.getAmount() == (type == ResourceType::Nothing ? 0u : 5u));
    }
    BOOST_TEST(isCustomOre(COPPER));
    BOOST_TEST(!isCustomOre(ResourceType::Fish));
    BOOST_TEST((getCustomOre(3) == ResourceType(static_cast<unsigned>(ResourceType::Custom0) + 3)));
    BOOST_TEST(getCustomOreIdx(getCustomOre(5)) == 5u);
    // Values of old savegames: type in the upper 4 bits
    BOOST_TEST((Resource::fromLegacyValue(0x35) == Resource(ResourceType::Coal, 5)));
    BOOST_TEST((Resource::fromLegacyValue(0x64) == Resource(ResourceType::Fish, 4)));
    BOOST_TEST((Resource::fromLegacyValue(0xF4) == Resource()));
    // Invalid types
    BOOST_TEST((Resource(static_cast<uint16_t>(0xFF05)) == Resource()));
}

BOOST_AUTO_TEST_CASE(MapCodesOfObjectsAndOres)
{
    // Old maps: type 0xD0 + index, info = size
    const auto old = civ::decodeMapObject(0xD3, 5);
    BOOST_TEST_REQUIRE(old.has_value());
    BOOST_TEST(old->idx == 3u);
    BOOST_TEST(old->size == 5u);
    BOOST_TEST(!civ::decodeMapObject(0xD3, 0));
    BOOST_TEST(!civ::decodeMapObject(0xCC, 5)); // Granite
    BOOST_TEST(!civ::decodeMapObject(0xF0, 5));
    for(unsigned idx : {0u, 15u, 16u, 31u, 32u, 100u, 255u})
    {
        for(unsigned size : {1u, 7u, 16u})
        {
            const auto code = civ::encodeMapObject(idx, size);
            BOOST_TEST(civ::isCustomMapObjectType(code.type));
            const auto decoded = civ::decodeMapObject(code.type, code.info);
            BOOST_TEST_REQUIRE(decoded.has_value());
            BOOST_TEST(decoded->idx == idx);
            BOOST_TEST(decoded->size == size);
            if(idx < 16)
            {
                // Same as before
                BOOST_TEST(code.type == 0xD0 + idx);
                BOOST_TEST(code.info == size);
            }
        }
    }
    BOOST_TEST(civ::MAX_CUSTOM_MAP_OBJECTS == 256u);

    // Ores: all codes are unused by the S2 format
    std::set<uint8_t> usedValues;
    for(unsigned idx = 0; idx < civ::ORE_MAP_CODES.size(); ++idx)
    {
        for(unsigned amount = 1; amount <= civ::MAX_ORE_MAP_AMOUNT; ++amount)
        {
            const uint8_t value = civ::encodeOre(idx, amount);
            BOOST_TEST(usedValues.insert(value).second);
            const auto decoded = civ::decodeOre(value);
            BOOST_TEST_REQUIRE(decoded.has_value());
            BOOST_TEST(decoded->idx == idx);
            BOOST_TEST(decoded->amount == amount);
        }
    }
    for(const unsigned builtin : {0x00u, 0x20u, 0x21u, 0x41u, 0x47u, 0x49u, 0x51u, 0x5Fu, 0x81u, 0x87u, 0x8Fu})
        BOOST_TEST(!civ::decodeOre(static_cast<uint8_t>(builtin)));
}

BOOST_AUTO_TEST_CASE(OresMinesAndPlantationsFromLua)
{
    const std::string base = R"(
rttr:AddWare{ name = "CopperOre", title = "Kupfererz", look = "IronOre", transport = "IronOre" }
rttr:AddWare{ name = "PapyrusPlant", title = "Papyruspflanze", look = "Grain", transport = "Grain" }
rttr:AddMapObject{ name = "PapyrusDeposit", title = "Papyrus", sizes = 8, look = "Granite1", growTime = 240,
    plantSize = 4 }
rttr:AddOre{ name = "Copper", title = "Kupfer", look = "Iron", tint = 0xC07040 }
rttr:AddWorkshop{ name = "CopperMine", title = "Kupfermine", size = "mine", cost = { Boards = 4 }, look = "IronMine",
    worker = { title = "Bergmann", tool = "PickAxe", look = "Miner" }, inputs = { "Fish", "Meat", "Bread" },
    consume = "any", mine = { resource = "Copper" }, output = "CopperOre", workTime = 28, waitTime = 29,
    knowledge = { IWP = 2 } }
rttr:AddWorkshop{ name = "Plantation", title = "Papyrusplantage", size = "small", cost = { Boards = 2 },
    look = "Farm", worker = { title = "Bauer", tool = "Scythe", look = "Farmer" }, inputs = { "Water" },
    plant = { object = "PapyrusDeposit", radius = 4 }, output = "PapyrusPlant", workTime = 6 }
)";
    {
        WorldDescription desc;
        BOOST_TEST_REQUIRE(loadLua(base, desc));
        BOOST_TEST_REQUIRE(desc.ores.size() == 1u);
        BOOST_TEST(desc.ores[0].name == "Copper");
        BOOST_TEST(desc.ores[0].tint == 0xC07040u);
        BOOST_TEST(desc.mapObjects[0].growTime == 240u);
        BOOST_TEST(desc.mapObjects[0].plantSize == 4u);
        BOOST_TEST(desc.workshops[0].mineResource == "Copper");
        BOOST_TEST(desc.workshops[1].plantObject == "PapyrusDeposit");
        BOOST_TEST(desc.workshops[1].plantRadius == 4u);
        // Knowledge of the workshop like rttr:SetBuildingKnowledge
        BOOST_TEST_REQUIRE(desc.buildingKnowledge.count("CopperMine") == 1u);
        BOOST_TEST(desc.buildingKnowledge.at("CopperMine")[1] == 2);
        const civ::TechTree techTree(desc);
        const civ::Content& content = techTree.GetContent();
        BOOST_TEST(techTree.GetBuildingKnowledge(MINE)[civ::KnowledgeType::Industry] == 2);
        BOOST_TEST_REQUIRE(content.GetOres().size() == 1u);
        BOOST_TEST((content.FindOre("Copper") == COPPER));
        BOOST_TEST(!content.FindOre("Tin"));
        BOOST_TEST(content.GetOre(COPPER)->title == "Kupfer");
        BOOST_TEST((content.GetOre(COPPER)->look == ResourceType::Iron));
        BOOST_TEST(!content.GetOre(getCustomOre(1)));
        BOOST_TEST(!content.GetOre(ResourceType::Iron));
        const civ::CustomBuilding& mine = *content.GetBuilding(MINE);
        BOOST_TEST(mine.IsMine());
        BOOST_TEST((mine.mineResource == COPPER));
        BOOST_TEST((mine.size == BuildingQuality::Mine));
        BOOST_TEST(!mine.workDesc.useOneWareEach);
        const civ::CustomBuilding& plantation = *content.GetBuilding(PLANTATION);
        BOOST_TEST(plantation.IsPlanter());
        BOOST_TEST(plantation.HarvestsPlants());
        BOOST_TEST(plantation.plantObject == 0);
        BOOST_TEST(plantation.plantRadius == 4u);
        BOOST_TEST(content.GetMapObject(0)->CanBePlanted());
        BOOST_TEST(content.GetMapObject(0)->plantSize == 4u);
        BOOST_TEST(content.GetMapObject(0)->growGFs == 240u * 20u);
        // Ores are part of the signature
        const auto signature = content.GetSignature();
        BOOST_TEST(signature.back() == "R:Copper");
        BOOST_TEST(content.IsCompatibleTo(signature));
        BOOST_TEST(!content.IsCompatibleTo({"R:Tin"}));
        BOOST_TEST(!content.IsCompatibleTo({"R:Copper", "R:Tin"}));
    }
    // Plantation without output: only plants
    {
        std::string onlyPlant = base;
        const auto pos = onlyPlant.find(R"(output = "PapyrusPlant", )");
        onlyPlant.erase(pos, std::string(R"(output = "PapyrusPlant", )").size());
        WorldDescription desc;
        BOOST_TEST_REQUIRE(loadLua(onlyPlant, desc));
        const civ::Content content(desc);
        BOOST_TEST(content.GetBuilding(PLANTATION)->IsPlanter());
        BOOST_TEST(!content.GetBuilding(PLANTATION)->HarvestsPlants());
        BOOST_TEST(
          boost::variant2::holds_alternative<std::nullopt_t>(content.GetBuilding(PLANTATION)->workDesc.producedWare));
    }
    const auto replaced = [&base](const std::string& from, const std::string& to) {
        std::string result = base;
        const auto pos = result.find(from);
        BOOST_TEST_REQUIRE(pos != std::string::npos);
        return result.replace(pos, from.size(), to);
    };
    for(const std::string& invalid :
        {replaced("look = \"Iron\"", "look = \"Wood\""), replaced("size = \"mine\"", "size = \"small\""),
         replaced("mine = { resource = \"Copper\" }, ", ""), replaced("growTime = 240", "growTime = 4000"),
         replaced("plantSize = 4", "plantSize = 9"), replaced("growTime = 240,", ""),
         replaced("radius = 4", "radius = 0"),
         replaced("plant = {", "collect = { object = \"PapyrusDeposit\", radius = 2 }, plant = {"),
         base + "\nrttr:SetBuildingKnowledge{ CopperMine = { IWP = 1 } }"})
    {
        WorldDescription desc;
        BOOST_TEST_INFO(invalid);
        BOOST_TEST(!loadLua(invalid, desc));
    }
    // Invalid content
    for(const std::string& invalid :
        {replaced("resource = \"Copper\"", "resource = \"Tin\""), replaced("growTime = 240,\n    plantSize = 4 ", ""),
         base + "\nrttr:AddOre{ name = \"Copper\", look = \"Gold\" }",
         base + "\nrttr:AddOre{ name = \"PapyrusDeposit\", look = \"Gold\" }",
         base + "\nrttr:AddOre{ name = \"coal\", look = \"Gold\" }"})
    {
        WorldDescription desc;
        BOOST_TEST_INFO(invalid);
        // Either the description or the content is invalid
        if(loadLua(invalid, desc))
            BOOST_CHECK_THROW(civ::Content{desc}, GameDataError);
    }
}

namespace {
struct PlantWorldFixture : public WorldWithGCExecution1P
{
    PlantWorldFixture(WorldDescription desc = makeDesc())
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
    }

    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    unsigned countInHQ(GoodType good) { return hq().GetNumRealWares(good); }

    /// Place a finished custom building connected to the HQ
    MapPoint placeConnectedBuilding(BuildingType bld, Position offset = Position(4, 2))
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + offset);
        BuildingFactory::CreateBuilding(world, bld, pos, 0, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<nobUsual>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        return pos;
    }

    std::vector<MapPoint> findDeposits()
    {
        std::vector<MapPoint> result;
        RTTR_FOREACH_PT(MapPoint, world.GetSize())
        {
            if(world.GetSpecObj<noDeposit>(pt))
                result.push_back(pt);
        }
        return result;
    }

    /// Make the nodes around center water (a small lake)
    void createLake(MapPoint center)
    {
        const DescIdx<TerrainDesc> water = GetWaterTerrain(world.GetDescription());
        for(const MapPoint pt : world.GetPointsInRadiusWithCenter(center, 1))
        {
            MapNode& node = world.GetNodeWriteable(pt);
            node.t1 = node.t2 = water;
        }
        world.SetClimate(nullptr); // Computed again from the terrain
    }
};
} // namespace

BOOST_FIXTURE_TEST_CASE(MineDigsOre, PlantWorldFixture)
{
    BOOST_TEST(BuildingProperties::IsMine(MINE));
    BOOST_TEST(!BuildingProperties::IsMine(PLANTATION));
    hq().AddToInventory(GoodCounts::make(GoodType::PickAxe, 1), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Fish, 10), true);
    const MapPoint bldPos = world.MakeMapPoint(hqPos + Position(4, 2));
    // 3 copper in range of the miner (radius 2), iron does not count, more copper too far away
    const MapPoint copperPt = world.MakeMapPoint(bldPos + Position(1, 0));
    const MapPoint ironPt = world.MakeMapPoint(bldPos + Position(0, 1));
    const MapPoint farPt = world.MakeMapPoint(bldPos + Position(4, 0));
    world.SetResource(copperPt, Resource(COPPER, 3));
    world.SetResource(ironPt, Resource(ResourceType::Iron, 5));
    world.SetResource(farPt, Resource(COPPER, 5));
    placeConnectedBuilding(MINE);
    const auto& bld = *world.GetSpecObj<nobUsual>(bldPos);
    RTTR_EXEC_TILL(4000, bld.HasWorker());
    BOOST_TEST((bld.GetWorker()->GetGOT() == GO_Type::NofCustomWorker));
    RTTR_EXEC_TILL(20000, countInHQ(COPPER_ORE) >= 3u);
    BOOST_TEST(world.GetNode(copperPt).resources.getAmount() == 0u);
    BOOST_TEST(world.GetNode(ironPt).resources.getAmount() == 5u);
    BOOST_TEST(world.GetNode(farPt).resources.getAmount() == 5u);
    // Exhausted
    RTTR_SKIP_GFS(2000);
    BOOST_TEST(countInHQ(COPPER_ORE) == 3u);
    BOOST_TEST(bld.GetProductivity() == 0u);
}

BOOST_FIXTURE_TEST_CASE(PlantationPlantsWaterAndHarvests, PlantWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Scythe, 1), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Water, 4), true);
    const MapPoint bldPos = placeConnectedBuilding(PLANTATION);
    const auto& bld = *world.GetSpecObj<nobUsual>(bldPos);
    RTTR_EXEC_TILL(4000, bld.HasWorker());
    BOOST_TEST((bld.GetWorker()->GetGOT() == GO_Type::NofCustomPlanter));
    // Something is planted (no rule for it: on land) and grows
    RTTR_EXEC_TILL(10000, !findDeposits().empty());
    const MapPoint planted = findDeposits().front();
    BOOST_TEST(world.CalcDistance(bldPos, planted) <= 3u);
    const auto* deposit = world.GetSpecObj<noDeposit>(planted);
    BOOST_TEST(deposit->IsGrowing());
    BOOST_TEST(deposit->GetSize() == 0u);
    // Grows within 1 second (20 GF) to its plant size 2
    RTTR_EXEC_TILL(21, !world.GetSpecObj<noDeposit>(planted)->IsGrowing());
    BOOST_TEST(world.GetSpecObj<noDeposit>(planted)->GetSize() == 1u);
    // Every planting needs water: At most 4 objects are planted, each harvested twice
    RTTR_EXEC_TILL(30000, countInHQ(PAPYRUS_PLANT) >= 8u);
    BOOST_TEST(world.GetPlayer(0).GetInventory().goods[GoodType::Water] == 0u);
    RTTR_SKIP_GFS(3000);
    BOOST_TEST(countInHQ(PAPYRUS_PLANT) == 8u);
    BOOST_TEST(findDeposits().empty());
}

BOOST_FIXTURE_TEST_CASE(CollectorWaitsTillPlantsAreGrown, PlantWorldFixture)
{
    // A planted object can not be collected (or harvested) while it grows
    const MapPoint pt = world.MakeMapPoint(hqPos + Position(4, 4));
    world.SetNO(pt, noDeposit::CreatePlanted(0));
    const auto* deposit = world.GetSpecObj<noDeposit>(pt);
    BOOST_TEST(deposit->IsGrowing());
    RTTR_SKIP_GFS(10);
    BOOST_TEST(deposit->IsGrowing());
    BOOST_TEST(deposit->GetSize() == 0u);
    RTTR_SKIP_GFS(11);
    BOOST_TEST(!deposit->IsGrowing());
    BOOST_TEST(deposit->GetSize() == 1u);
    // Destroying a growing object removes its event
    const MapPoint pt2 = world.MakeMapPoint(hqPos + Position(-4, 4));
    world.SetNO(pt2, noDeposit::CreatePlanted(0));
    world.DestroyNO(pt2);
    RTTR_SKIP_GFS(30);
    BOOST_TEST(!world.GetSpecObj<noDeposit>(pt2));
}

BOOST_FIXTURE_TEST_CASE(PlantationOnlyWhereItGrowsNaturally, PlantWorldFixture)
{
    // Rule of the map generator: Papyrus at the shore of fresh water, only in a zone and only where it is hot.
    // It is part of the description of the world (world/mapgen.lua) like in the game, the content of the civilization
    // mode is loaded from its own folder without it
    MapResourceDesc rule;
    rule.name = "Papyrus";
    rule.object = "Papyrus";
    rule.terrain = toFlag(MapGenTerrain::ShoreFresh);
    rule.zones = {"Afrika"};
    rule.climate.temperature = MapGenRange{50, 100};
    world.GetDescriptionWriteable().mapGen.resources.push_back(rule);
    const civ::CustomBuilding& plantation = *world.GetTechTree().GetContent().GetBuilding(PLANTATION);

    // Only land: Nowhere
    RTTR_FOREACH_PT(MapPoint, world.GetSize())
        BOOST_TEST_REQUIRE(!nofCustomPlanter::GrowsNaturallyAt(world, pt, 0));

    // A lake: At its shore, but not at the HQ (not free) and not directly at the water (not land)
    const MapPoint lakeCenter = world.MakeMapPoint(hqPos + Position(4, 4));
    createLake(lakeCenter);
    const civ::ClimateMap& climate = world.GetClimate();
    BOOST_TEST(!climate.HasClimate());
    BOOST_TEST((climate.GetTerrain(lakeCenter) & toFlag(MapGenTerrain::Freshwater)) != 0u);
    unsigned numShore = 0;
    RTTR_FOREACH_PT(MapPoint, world.GetSize())
    {
        const bool atShore = (climate.GetTerrain(pt) & toFlag(MapGenTerrain::ShoreFresh)) != 0u;
        // Zone and temperature do not count (no climate on this map)
        BOOST_TEST_REQUIRE(nofCustomPlanter::GrowsNaturallyAt(world, pt, 0) == atShore);
        if(atShore)
            ++numShore;
    }
    BOOST_TEST(numShore > 6u);
    BOOST_TEST(!nofCustomPlanter::CanPlantAt(world, hqPos, plantation));

    // The plantation plants only at the shore
    hq().AddToInventory(GoodCounts::make(GoodType::Scythe, 1), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Water, 2), true);
    placeConnectedBuilding(PLANTATION, Position(4, 1));
    RTTR_EXEC_TILL(20000, !findDeposits().empty());
    for(const MapPoint pt : findDeposits())
        BOOST_TEST((climate.GetTerrain(pt) & toFlag(MapGenTerrain::ShoreFresh)) != 0u);
}

BOOST_FIXTURE_TEST_CASE(PlantationPlantsNextToFlags, PlantWorldFixture)
{
    // Like next to stones flags are allowed next to planted objects, buildings and other planted objects are not
    const civ::CustomBuilding& plantation = *world.GetTechTree().GetContent().GetBuilding(PLANTATION);
    const MapPoint pt = world.MakeMapPoint(hqPos + Position(-4, 4));
    BOOST_TEST_REQUIRE(nofCustomPlanter::CanPlantAt(world, pt, plantation));
    BOOST_TEST_REQUIRE(this->SetFlag(world.GetNeighbour(pt, Direction::East)));
    BOOST_TEST(nofCustomPlanter::CanPlantAt(world, pt, plantation));
    // Not on the road of a flag
    BOOST_TEST_REQUIRE(
      this->BuildRoad(world.GetNeighbour(pt, Direction::East), false, std::vector<Direction>(2, Direction::West)));
    BOOST_TEST(!nofCustomPlanter::CanPlantAt(world, pt, plantation));
    // Next to it: allowed, but not next to another planted object
    const MapPoint nextPt = world.GetNeighbour(pt, Direction::NorthEast);
    BOOST_TEST_REQUIRE(nofCustomPlanter::CanPlantAt(world, nextPt, plantation));
    world.SetNO(world.GetNeighbour(nextPt, Direction::NorthEast), noDeposit::CreatePlanted(0));
    BOOST_TEST(!nofCustomPlanter::CanPlantAt(world, nextPt, plantation));
    // Not next to buildings
    const MapPoint nextToHQ = world.GetNeighbour(hqPos, Direction::East);
    BOOST_TEST_REQUIRE((world.GetNO(nextToHQ)->GetType() == NodalObjectType::Nothing));
    BOOST_TEST(!nofCustomPlanter::CanPlantAt(world, nextToHQ, plantation));
}

BOOST_FIXTURE_TEST_CASE(ClimateFromGeneratedMap, PlantWorldFixture)
{
    const MapExtent size = world.GetSize();
    const MapPoint lakeCenter = world.MakeMapPoint(hqPos + Position(4, 4));
    createLake(lakeCenter);
    // Map file like the generator writes it: temperature 30 in the left half, 80 in the right half
    libsiedler2::ArchivItem_Map map;
    {
        auto header = std::make_unique<libsiedler2::ArchivItem_Map_Header>();
        header->setWidth(size.x);
        header->setHeight(size.y);
        map.init(std::move(header));
    }
    using libsiedler2::MapLayer;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        map.setMapDataAt(MapLayer::Unknown7, pt.x, pt.y,
                         static_cast<uint8_t>(civ::CLIMATE_MARKER | (pt.x < size.x / 2 ? 30 : 80)));
        map.setMapDataAt(MapLayer::Unknown9, pt.x, pt.y, 60);
        const bool isLake = world.CalcDistance(pt, lakeCenter) <= 1u;
        map.setMapDataAt(MapLayer::Unknown10, pt.x, pt.y, isLake ? civ::WATER_FRESH : 0);
    }
    auto climate = std::make_unique<civ::ClimateMap>();
    climate->Init(world, &map);
    BOOST_TEST_REQUIRE(climate->HasClimate());
    BOOST_TEST(climate->GetTemperature(MapPoint(0, 0)) == 30u);
    BOOST_TEST(climate->GetTemperature(MapPoint(size.x - 1, 0)) == 80u);
    BOOST_TEST(climate->GetHumidity(MapPoint(0, 0)) == 60u);

    MapResourceDesc rule;
    rule.terrain = toFlag(MapGenTerrain::Land) | toFlag(MapGenTerrain::ShoreFresh);
    rule.climate.temperature = MapGenRange{50, 100};
    RTTR_FOREACH_PT(MapPoint, size)
    {
        const bool terrainOk = (climate->GetTerrain(pt) & rule.terrain) != 0u;
        BOOST_TEST_REQUIRE(climate->AllowsNaturally(pt, rule) == (terrainOk && pt.x >= size.x / 2));
    }
    // A weight of 0 means it does not grow there
    rule.weights.push_back(MapResourceWeight{MapGenClimate{MapGenRange{70, 100}, MapGenRange{}, MapGenRange{}}, 0.});
    BOOST_TEST(!climate->AllowsNaturally(MapPoint(size.x - 1, 0), rule));

    // Without the marker there is no climate
    map.setMapDataAt(MapLayer::Unknown7, 0, 0, 30);
    civ::ClimateMap noClimate;
    noClimate.Init(world, &map);
    BOOST_TEST(!noClimate.HasClimate());
    BOOST_TEST(noClimate.AllowsNaturally(MapPoint(0, 0), rule)
               == ((noClimate.GetTerrain(MapPoint(0, 0)) & rule.terrain) != 0u));
}

BOOST_FIXTURE_TEST_CASE(SerializePlantsOresAndClimate, PlantWorldFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Scythe, 1), true);
    const MapPoint orePt = world.MakeMapPoint(hqPos + Position(-3, 2));
    world.SetResource(orePt, Resource(COPPER, 6));
    const MapPoint plantPt = world.MakeMapPoint(hqPos + Position(4, 4));
    world.SetNO(plantPt, noDeposit::CreatePlanted(0));
    // Climate from a map
    {
        libsiedler2::ArchivItem_Map map;
        auto header = std::make_unique<libsiedler2::ArchivItem_Map_Header>();
        header->setWidth(world.GetWidth());
        header->setHeight(world.GetHeight());
        map.init(std::move(header));
        RTTR_FOREACH_PT(MapPoint, world.GetSize())
        {
            map.setMapDataAt(libsiedler2::MapLayer::Unknown7, pt.x, pt.y, civ::CLIMATE_MARKER | 70);
            map.setMapDataAt(libsiedler2::MapLayer::Unknown9, pt.x, pt.y, 20);
        }
        auto climate = std::make_unique<civ::ClimateMap>();
        climate->Init(world, &map);
        world.SetClimate(std::move(climate));
    }
    const MapPoint bldPos = placeConnectedBuilding(PLANTATION);
    RTTR_EXEC_TILL(4000, world.GetSpecObj<nobUsual>(bldPos)->HasWorker());
    world.DestroyNO(plantPt);
    world.SetNO(plantPt, noDeposit::CreatePlanted(0));
    RTTR_SKIP_GFS(5);

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    BOOST_TEST((world.GetNode(orePt).resources == Resource(COPPER, 6)));
    BOOST_TEST_REQUIRE(world.HasClimate());
    BOOST_TEST(world.GetClimate().HasClimate());
    BOOST_TEST(world.GetClimate().GetTemperature(hqPos) == 70u);
    BOOST_TEST(world.GetClimate().GetHumidity(hqPos) == 20u);
    const auto* deposit = world.GetSpecObj<noDeposit>(plantPt);
    BOOST_TEST_REQUIRE(deposit);
    BOOST_TEST(deposit->IsGrowing());
    RTTR_EXEC_TILL(20, !world.GetSpecObj<noDeposit>(plantPt)->IsGrowing());
    BOOST_TEST((world.GetSpecObj<nobUsual>(bldPos)->GetWorker()->GetGOT() == GO_Type::NofCustomPlanter));
    // The planter harvests it
    RTTR_EXEC_TILL(10000, countInHQ(PAPYRUS_PLANT) >= 2u);
}

namespace {
struct NoWorldCreator
{
    explicit NoWorldCreator(const MapExtent&) {}
    bool operator()(GameWorldBase&) { return true; }
};
using UninitializedWorldFixture = WorldFixture<NoWorldCreator, 2>;

/// Map file and the data of the generator for a map generated with copper (ore) and papyrus (map object) like
/// makeDesc()
struct GeneratedMap
{
    libsiedler2::Archiv archive;
    MapExtent size;
    NodeMapBase<uint8_t> resources, objectTypes, objectInfos, temperature, humidity, height;
    NodeMapBase<uint16_t> terrainFlags;

    const libsiedler2::ArchivItem_Map& getMapFile() const
    {
        return *static_cast<const libsiedler2::ArchivItem_Map*>(archive[0]);
    }
};

GeneratedMap generateMapWithOresAndObjects()
{
    using namespace rttr::mapGenerator;
    WorldDescription worldDesc;
    loadGameData(worldDesc);
    MapResourceDesc copper;
    copper.name = copper.object = "Copper";
    copper.amount = 30;
    copper.terrain = toFlag(MapGenTerrain::Mountain);
    copper.cluster = 4;
    worldDesc.mapGen.resources.push_back(copper);
    MapResourceDesc papyrus;
    papyrus.name = papyrus.object = "Papyrus";
    papyrus.amount = 10;
    papyrus.terrain = toFlag(MapGenTerrain::Land);
    worldDesc.mapGen.resources.push_back(papyrus);
    MapSettings settings;
    settings.size = MapExtent(64, 64);
    settings.numPlayers = 2;
    settings.mountains = 30;
    RandomUtility rnd(7);
    // The map refers to worldDesc, so take everything needed while it exists
    const Map map = GenerateRandomMap(rnd, worldDesc, settings, {{"Papyrus", 3, false}, {"Copper", 0, true}});
    return GeneratedMap{map.CreateArchiv(), map.size,    map.resources,     map.objectTypes, map.objectInfos,
                        map.temperature,    map.climate, map.heightPercent, map.terrainFlags};
}
} // namespace

BOOST_FIXTURE_TEST_CASE(GeneratedMapKeepsOresObjectsAndClimate, UninitializedWorldFixture)
{
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    world.SetTechTree(std::make_unique<civ::TechTree>(makeDesc()));
    const GeneratedMap map = generateMapWithOresAndObjects();

    MapLoader loader(world);
    BOOST_TEST_REQUIRE(loader.Load(map.getMapFile(), Exploration::Disabled));
    BOOST_TEST_REQUIRE(world.GetSize() == map.size);
    unsigned numCopper = 0, numPapyrus = 0;
    const civ::ClimateMap& climate = world.GetClimate();
    BOOST_TEST_REQUIRE(climate.HasClimate());
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        BOOST_TEST_INFO("pt " << pt);
        if(const auto ore = civ::decodeOre(map.resources[pt]))
        {
            BOOST_TEST_REQUIRE((world.GetNode(pt).resources == Resource(COPPER, static_cast<uint8_t>(ore->amount))));
            ++numCopper;
        }
        if(const auto obj = civ::decodeMapObject(map.objectTypes[pt], map.objectInfos[pt]))
        {
            const auto* deposit = world.GetSpecObj<noDeposit>(pt);
            BOOST_TEST_REQUIRE(deposit);
            BOOST_TEST_REQUIRE(deposit->GetSize() + 1u == obj->size);
            BOOST_TEST_REQUIRE(!deposit->IsGrowing());
            ++numPapyrus;
        }
        // The game knows the climate and terrain types like the generator
        BOOST_TEST_REQUIRE(climate.GetTemperature(pt) == map.temperature[pt]);
        BOOST_TEST_REQUIRE(climate.GetHumidity(pt) == map.humidity[pt]);
        BOOST_TEST_REQUIRE(climate.GetHeight(pt) == map.height[pt]);
        BOOST_TEST_REQUIRE(climate.GetTerrain(pt) == map.terrainFlags[pt]);
    }
    BOOST_TEST(numCopper > 10u);
    BOOST_TEST(numPapyrus > 10u);
}

BOOST_FIXTURE_TEST_CASE(GeneratedMapWithoutCivilizationMode, UninitializedWorldFixture)
{
    // Without the civilization mode there are no ores and map objects
    const GeneratedMap map = generateMapWithOresAndObjects();
    MapLoader loader(world);
    BOOST_TEST_REQUIRE(loader.Load(map.getMapFile(), Exploration::Disabled));
    BOOST_TEST_REQUIRE(helpers::contains_if(map.resources, [](uint8_t value) { return civ::decodeOre(value); }));
    RTTR_FOREACH_PT(MapPoint, map.size)
    {
        BOOST_TEST_REQUIRE(!isCustomOre(world.GetNode(pt).resources.getType()));
        BOOST_TEST_REQUIRE(!world.GetSpecObj<noDeposit>(pt));
    }
}

namespace {
using BiggerPlantWorldFixture = WorldWithGCExecution<1, 24, 22>;
} // namespace

BOOST_FIXTURE_TEST_CASE(AIBuildsPlantation, BiggerPlantWorldFixture)
{
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    world.SetTechTree(std::make_unique<civ::TechTree>(makeDesc()));
    civ::startGame(world);
    addStartResources();
    auto& hqBld = *world.GetPlayer(curPlayer).GetHQ();
    hqBld.AddToInventory(GoodCounts::make(GoodType::Scythe, 2), true);
    hqBld.AddToInventory(GoodCounts::make(GoodType::Water, 5), true);
    world.InitAfterLoad();
    const BuildingRegister& buildings = world.GetPlayer(curPlayer).GetBuildingRegister();
    const auto findPlantation = [&buildings]() {
        for(const noBuildingSite* site : buildings.GetBuildingSites())
        {
            if(site->GetBuildingType() == PLANTATION)
                return site->GetPos();
        }
        for(const nobUsual* bld : buildings.GetBuildings(PLANTATION))
            return bld->GetPos();
        return MapPoint::Invalid();
    };
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    for(unsigned gf = 0; gf < 12000 && !findPlantation().isValid();)
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
    BOOST_TEST(findPlantation().isValid());
}

BOOST_AUTO_TEST_SUITE_END()
