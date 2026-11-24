// Copyright (C) 2005 - 2025 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GameDataLoader.h"
#include "CheckedLuaTable.h"
#include "RttrConfig.h"
#include "files.h"
#include "helpers/containerUtils.h"
#include "helpers/format.hpp"
#include "gameData/EdgeDesc.h"
#include "gameData/LandscapeDesc.h"
#include "gameData/TechnologyDesc.h"
#include "gameData/TerrainDesc.h"
#include "gameData/WorldDescription.h"
#include "s25util/Log.h"
#include <kaguya/kaguya.hpp>
#include <boost/filesystem.hpp>
#include <algorithm>
#include <stdexcept>

namespace bfs = boost::filesystem;

GameDataLoader::GameDataLoader(WorldDescription& worldDesc, const boost::filesystem::path& basePath)
    : worldDesc_(worldDesc), basePath_(basePath.lexically_normal().make_preferred()), curIncludeDepth_(0)
{
    Register(lua);

    lua["rttr"] = this;
    lua["include"] = kaguya::function([this](const std::string& file) { Include(file); });
}

GameDataLoader::GameDataLoader(WorldDescription& worldDesc)
    : GameDataLoader(worldDesc, RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "world")
{}

GameDataLoader::~GameDataLoader() = default;

bool GameDataLoader::Load()
{
    curFile_ = basePath_ / "default.lua";
    curIncludeDepth_ = 0;
    return loadScript(curFile_) && CheckMapGen();
}

void GameDataLoader::Register(kaguya::State& state)
{
    state["RTTRGameData"].setClass(
      kaguya::UserdataMetatable<GameDataLoader, LuaInterfaceBase>()
        .addFunction("AddLandscape", &GameDataLoader::AddLandscape)
        .addFunction("AddTerrainEdge", &GameDataLoader::AddTerrainEdge)
        .addFunction("AddTerrain", &GameDataLoader::AddTerrain)
        .addFunction("AddTechnology", &GameDataLoader::AddTechnology)
        .addFunction("SetBuildingKnowledge", &GameDataLoader::SetBuildingKnowledge)
        .addFunction("SetAIExpandingBuildings", &GameDataLoader::SetAIExpandingBuildings)
        .addFunction("SetAIBasicEconomy", &GameDataLoader::SetAIBasicEconomy)
        .addFunction("SetAIStartStockNoSurplus", &GameDataLoader::SetAIStartStockNoSurplus)
        .addFunction("SetAIBuildingLimit", &GameDataLoader::SetAIBuildingLimit)
        .addFunction("SetAIStorageFull", &GameDataLoader::SetAIStorageFull)
        .addFunction("SetWarehouseCapacity", &GameDataLoader::SetWarehouseCapacity)
        .addFunction("SetFlagWorkLimit", &GameDataLoader::SetFlagWorkLimit)
        .addFunction("AddWare", &GameDataLoader::AddWare)
        .addFunction("SetWare", &GameDataLoader::SetWare)
        .addFunction("AddWorkshop", &GameDataLoader::AddWorkshop)
        .addFunction("AddMapObject", &GameDataLoader::AddMapObject)
        .addFunction("AddOre", &GameDataLoader::AddOre)
        .addFunction("AddByproduct", &GameDataLoader::AddByproduct)
        .addFunction("SetKnowledgeDistribution", &GameDataLoader::SetKnowledgeDistribution)
        .addFunction("AddToolMaterial", &GameDataLoader::AddToolMaterial)
        .addFunction("AddTool", &GameDataLoader::AddTool)
        .addFunction("SetBuildingTools", &GameDataLoader::SetBuildingTools)
        .addFunction("SetToolReorder", &GameDataLoader::SetToolReorder)
        .addFunction("SetBuildingOutputRatio", &GameDataLoader::SetBuildingOutputRatio)
        .addFunction("SetBuildingCost", &GameDataLoader::SetBuildingCost)
        .addFunction("SetToolMaterialPriorities", &GameDataLoader::SetToolMaterialPriorities)
        .addFunction("SetMapClimate", &GameDataLoader::SetMapClimate)
        .addFunction("AddMapZone", &GameDataLoader::AddMapZone)
        .addFunction("AddMapResource", &GameDataLoader::AddMapResource));
}

class LuaIncludeError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

void GameDataLoader::Include(const std::string& filepath)
{
    try
    {
        constexpr int maxIncludeDepth = 10;
        // Protect against cycles and stack overflows
        if(curIncludeDepth_ >= maxIncludeDepth)
            throw LuaIncludeError(helpers::format("Maximum include depth of %1% is reached!", maxIncludeDepth));
        const auto isAllowedChar = [](const char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '/'
                   || c == '.';
        };
        if(helpers::contains_if(filepath, [isAllowedChar](const char c) { return !isAllowedChar(c); }))
            throw LuaIncludeError("It contains disallowed chars. Allowed: alpha-numeric, underscore, slash and dot.");
        if(bfs::path(filepath).is_absolute())
            throw LuaIncludeError("Path to file must be relative to current file");
        bfs::path absFilePath = bfs::absolute(filepath, curFile_.parent_path());
        if(!bfs::is_regular_file(absFilePath))
            throw LuaIncludeError("File not found!");

        // Normalize for below check against basePath
        absFilePath = absFilePath.lexically_normal().make_preferred();
        if(absFilePath.extension() != ".lua")
            throw LuaIncludeError("File must have .lua as the extension!");
        if(absFilePath.string().find(basePath_.string()) != 0)
            throw LuaIncludeError("File is outside the lua data directory!");
        const auto oldCurFile = curFile_;
        curFile_ = absFilePath;
        ++curIncludeDepth_;
        const bool fileLoaded = loadScript(absFilePath);
        curFile_ = oldCurFile;
        RTTR_Assert(curIncludeDepth_ > 0);
        --curIncludeDepth_;
        if(!fileLoaded)
            throw std::runtime_error(helpers::format("Include file '%1%' cannot be included", filepath));
    } catch(const LuaIncludeError& e)
    {
        throw std::runtime_error(helpers::format("Include file '%1%' cannot be included: %2%", filepath, e.what()));
    }
}

void GameDataLoader::AddLandscape(const kaguya::LuaTable& data)
{
    worldDesc_.landscapes.add(LandscapeDesc(data, worldDesc_));
}

void GameDataLoader::AddTerrainEdge(const kaguya::LuaTable& data)
{
    worldDesc_.edges.add(EdgeDesc(data, worldDesc_));
}

void GameDataLoader::AddTerrain(const kaguya::LuaTable& data)
{
    worldDesc_.terrain.add(TerrainDesc(data, worldDesc_));
}

void GameDataLoader::AddTechnology(const kaguya::LuaTable& data)
{
    worldDesc_.technologies.add(TechnologyDesc(data, worldDesc_));
}

void GameDataLoader::AddWare(const kaguya::LuaTable& data)
{
    worldDesc_.wares.emplace_back(data);
}

void GameDataLoader::SetWare(const kaguya::LuaTable& data)
{
    if(data.keys<std::string>().size() != data.keys<kaguya::LuaRef>().size())
        throw GameDataError("rttr:SetWare expects a table like { Wood = { name = \"PineLog\", kind = { \"Wood\" } } }");
    for(const std::string& ware : data.keys<std::string>())
    {
        const kaguya::LuaRef value = data[ware];
        if(value.type() != kaguya::LuaRef::TYPE_TABLE)
        {
            throw GameDataError("Built-in ware '" + ware
                                + "' (rttr:SetWare) must be a table like { name = \"PineLog\", kind = { \"Wood\" } }");
        }
        if(!worldDesc_.builtinWares.emplace(ware, parseBuiltinWare(value.get<kaguya::LuaTable>(), ware)).second)
            throw GameDataError("Built-in ware '" + ware + "' (rttr:SetWare) is defined twice");
    }
}

void GameDataLoader::AddWorkshop(const kaguya::LuaTable& data)
{
    WorkshopDesc workshop(data);
    // Knowledge points can also be given here instead of rttr:SetBuildingKnowledge
    if(workshop.knowledge && !worldDesc_.buildingKnowledge.emplace(workshop.name, *workshop.knowledge).second)
        throw GameDataError("Knowledge of building '" + workshop.name + "' is defined twice");
    worldDesc_.workshops.push_back(std::move(workshop));
}

void GameDataLoader::AddMapObject(const kaguya::LuaTable& data)
{
    worldDesc_.mapObjects.emplace_back(data);
}

void GameDataLoader::AddOre(const kaguya::LuaTable& data)
{
    worldDesc_.ores.emplace_back(data);
}

void GameDataLoader::AddByproduct(const kaguya::LuaTable& data)
{
    worldDesc_.byproducts.emplace_back(data);
}

void GameDataLoader::SetKnowledgeDistribution(const kaguya::LuaTable& data)
{
    const std::string context = "Knowledge distribution (regulator \"Wissen\")";
    if(worldDesc_.knowledgeDistribution)
        throw GameDataError(context + " is defined twice");
    const KnowledgeValues values = parseKnowledge(data, false, context);
    for(const int value : values)
    {
        if(value > 10)
            throw GameDataError(context + ": values must be in [0, 10]");
    }
    worldDesc_.knowledgeDistribution = values;
}

void GameDataLoader::AddToolMaterial(const kaguya::LuaTable& data)
{
    ToolMaterialDesc material(data);
    for(const ToolMaterialDesc& other : worldDesc_.toolMaterials)
    {
        if(other.name == material.name)
            throw GameDataError("Tool material '" + material.name + "' is defined twice");
    }
    if(worldDesc_.toolMaterials.size() >= MAX_TOOL_MATERIALS)
        throw GameDataError("At most " + std::to_string(MAX_TOOL_MATERIALS) + " tool materials are possible");
    worldDesc_.toolMaterials.push_back(std::move(material));
}

void GameDataLoader::AddTool(const kaguya::LuaTable& data)
{
    ToolDesc tool(data);
    for(const ToolDesc& other : worldDesc_.tools)
    {
        if(other.ware == tool.ware)
            throw GameDataError("Tool '" + tool.ware + "' is defined twice");
    }
    worldDesc_.tools.push_back(std::move(tool));
}

void GameDataLoader::SetBuildingTools(const kaguya::LuaTable& data)
{
    for(const std::string& building : data.keys<std::string>())
    {
        const kaguya::LuaRef value = data[building];
        const std::string context = "Tools of building '" + building + "'";
        if(value.type() != kaguya::LuaRef::TYPE_TABLE)
            throw GameDataError(context + " must be a table like { baseSpeed = 0, tools = { \"Axe\" } }");
        if(!worldDesc_.buildingTools.emplace(building, parseBuildingTools(value.get<kaguya::LuaTable>(), context))
              .second)
            throw GameDataError(context + " are defined twice");
    }
}

void GameDataLoader::SetToolReorder(unsigned percent)
{
    if(percent > 100)
        throw GameDataError("rttr:SetToolReorder: the value must be in [0, 100] (percent of the durability)");
    worldDesc_.toolReorderPercent = percent;
}

void GameDataLoader::SetBuildingCost(const kaguya::LuaTable& data)
{
    for(const std::string& building : data.keys<std::string>())
    {
        const std::string context = "Construction costs of building '" + building + "'";
        if(!worldDesc_.buildingCosts.emplace(building, parseBuildingCost(data[building], context)).second)
            throw GameDataError(context + " are defined twice");
    }
}

void GameDataLoader::SetBuildingOutputRatio(const kaguya::LuaTable& data)
{
    for(const std::string& building : data.keys<std::string>())
    {
        const kaguya::LuaRef value = data[building];
        const std::string context = "Output ratio of building '" + building + "'";
        if(value.type() != kaguya::LuaRef::TYPE_NUMBER)
            throw GameDataError(context + " must be a number like 1.5");
        if(!worldDesc_.buildingOutputRatios.emplace(building, parseOutputRatio(value.get<double>(), context)).second)
            throw GameDataError(context + " is defined twice");
    }
}

void GameDataLoader::SetToolMaterialPriorities(const kaguya::LuaTable& data)
{
    const std::string context = "Tool material priorities (regulator \"Werkzeuge\")";
    if(!worldDesc_.toolMaterialPriorities.empty())
        throw GameDataError(context + " are defined twice");
    for(const std::string& material : data.keys<std::string>())
    {
        const kaguya::LuaRef value = data[material];
        if(value.type() != kaguya::LuaRef::TYPE_NUMBER)
            throw GameDataError(context + ": the value of '" + material + "' must be a number in [0, 10]");
        const int priority = value.get<int>();
        if(priority < 0 || priority > 10)
            throw GameDataError(context + ": the value of '" + material + "' must be in [0, 10]");
        worldDesc_.toolMaterialPriorities[material] = static_cast<unsigned>(priority);
    }
    if(worldDesc_.toolMaterialPriorities.empty())
        throw GameDataError(context + ": no material given, e.g. { Iron = 8 }");
}

void GameDataLoader::SetMapClimate(const kaguya::LuaTable& data)
{
    if(worldDesc_.mapGen.climateSet)
        throw GameDataError("Map climate (rttr:SetMapClimate) is defined twice");
    worldDesc_.mapGen.climate = MapClimateDesc(data);
    worldDesc_.mapGen.climateSet = true;
}

void GameDataLoader::AddMapZone(const kaguya::LuaTable& data)
{
    MapZoneDesc zone(data);
    if(worldDesc_.mapGen.getZoneIndex(zone.name) >= 0)
        throw GameDataError("Map zone '" + zone.name + "' is defined twice");
    if(worldDesc_.mapGen.zones.size() >= 32)
        throw GameDataError("At most 32 map zones are possible");
    worldDesc_.mapGen.zones.push_back(zone);
}

void GameDataLoader::AddMapResource(const kaguya::LuaTable& data)
{
    MapResourceDesc resource(data);
    for(const MapResourceDesc& other : worldDesc_.mapGen.resources)
    {
        if(other.name == resource.name)
            throw GameDataError("Map resource '" + resource.name + "' is defined twice");
    }
    worldDesc_.mapGen.resources.push_back(resource);
}

bool GameDataLoader::CheckMapGen()
{
    for(const MapResourceDesc& resource : worldDesc_.mapGen.resources)
    {
        for(const std::string& zone : resource.zones)
        {
            if(worldDesc_.mapGen.getZoneIndex(zone) < 0)
            {
                log("Invalid game data: Map resource '" + resource.name + "': unknown zone '" + zone
                    + "' (see rttr:AddMapZone)");
                return false;
            }
        }
    }
    return true;
}

void GameDataLoader::SetBuildingKnowledge(const kaguya::LuaTable& data)
{
    for(const std::string& building : data.keys<std::string>())
    {
        const kaguya::LuaRef value = data[building];
        if(value.type() != kaguya::LuaRef::TYPE_TABLE)
            throw GameDataError("Knowledge of building '" + building + "' must be a table like { LWP = 1 }");
        const KnowledgeValues knowledge =
          parseKnowledge(value.get<kaguya::LuaTable>(), true, "Knowledge of building '" + building + "'");
        if(!worldDesc_.buildingKnowledge.emplace(building, knowledge).second)
            throw GameDataError("Knowledge of building '" + building + "' is defined twice");
    }
}

namespace {
/// A lua list of names like { "Woodcutter", "Quarry" } (no other keys, no name twice, not empty). `what`: "building"
/// or "ware" for the messages
std::vector<std::string> readNameList(const kaguya::LuaTable& data, const std::string& context, const std::string& what,
                                      const std::string& example)
{
    const size_t numEntries = data.size();
    if(data.keys<kaguya::LuaRef>().size() != numEntries)
        throw GameDataError(context + ": expected a list of " + what + " names like " + example);
    std::vector<std::string> result;
    for(size_t i = 1; i <= numEntries; ++i)
    {
        const kaguya::LuaRef value = data[i];
        if(value.type() != kaguya::LuaRef::TYPE_STRING)
            throw GameDataError(context + ": entry " + std::to_string(i) + " is no " + what + " name");
        const std::string name = value.get<std::string>();
        if(std::find(result.begin(), result.end(), name) != result.end())
            throw GameDataError(context + ": '" + name + "' is listed twice");
        result.push_back(name);
    }
    if(result.empty())
        throw GameDataError(context + ": the list is empty");
    return result;
}
} // namespace

void GameDataLoader::SetAIExpandingBuildings(const kaguya::LuaTable& data)
{
    const std::string context = "Expanding buildings of the AI (rttr:SetAIExpandingBuildings)";
    if(!worldDesc_.aiExpandingBuildings.empty())
        throw GameDataError(context + " are defined twice");
    worldDesc_.aiExpandingBuildings = readNameList(data, context, "building", "{ \"Woodcutter\", \"Quarry\" }");
}

void GameDataLoader::SetAIBasicEconomy(const kaguya::LuaTable& data)
{
    const std::string context = "Basic economy of the AI (rttr:SetAIBasicEconomy)";
    if(!worldDesc_.aiBasicEconomy.empty())
        throw GameDataError(context + " is defined twice");
    const std::string example = "{ { \"Woodcutter\", \"Quarry\" }, { \"IronMine\" } }";
    const size_t numLevels = data.size();
    if(numLevels == 0 || data.keys<kaguya::LuaRef>().size() != numLevels)
        throw GameDataError(context + ": expected a list of levels like " + example);
    std::vector<std::string> all;
    for(size_t i = 1; i <= numLevels; ++i)
    {
        const kaguya::LuaRef value = data[i];
        if(value.type() != kaguya::LuaRef::TYPE_TABLE)
            throw GameDataError(context + ": level " + std::to_string(i) + " is no list of building names (" + example
                                + ")");
        const std::string levelContext = context + ", level " + std::to_string(i);
        std::vector<std::string> level =
          readNameList(value.get<kaguya::LuaTable>(), levelContext, "building", "{ \"Woodcutter\", \"Quarry\" }");
        for(const std::string& name : level)
        {
            if(std::find(all.begin(), all.end(), name) != all.end())
                throw GameDataError(context + ": '" + name + "' is in two levels");
            all.push_back(name);
        }
        worldDesc_.aiBasicEconomy.push_back(std::move(level));
    }
}

void GameDataLoader::SetAIBuildingLimit(const kaguya::LuaTable& data)
{
    const std::string context = "Building limit of the AI (rttr:SetAIBuildingLimit)";
    if(worldDesc_.aiBuildingLimitSet)
        throw GameDataError(context + " is defined twice");
    const std::string example = "{ base = 3, perArea = 1, areaSize = 331 }";
    std::vector<std::string> keys = data.keys<std::string>();
    if(keys.size() != data.keys<kaguya::LuaRef>().size())
        throw GameDataError(context + ": expected a table like " + example);
    for(const std::string& key : keys)
    {
        if(key != "base" && key != "perArea" && key != "areaSize")
            throw GameDataError(context + ": unknown entry '" + key + "', expected a table like " + example);
    }
    const auto getValue = [&](const std::string& key, const unsigned minValue, const unsigned maxValue) {
        const kaguya::LuaRef value = data[key];
        if(value.type() != kaguya::LuaRef::TYPE_NUMBER)
            throw GameDataError(context + ": '" + key + "' is missing or no number (" + example + ")");
        const double number = value.get<double>();
        if(number < minValue || number > maxValue || number != static_cast<double>(static_cast<unsigned>(number)))
        {
            throw GameDataError(context + ": '" + key + "' must be a whole number in [" + std::to_string(minValue)
                                + ", " + std::to_string(maxValue) + "]");
        }
        return static_cast<unsigned>(number);
    };
    worldDesc_.aiBuildingLimitBase = getValue("base", 0, 1000);
    worldDesc_.aiBuildingLimitPerArea = getValue("perArea", 0, 1000);
    worldDesc_.aiBuildingLimitAreaSize = getValue("areaSize", 1, 1000000);
    worldDesc_.aiBuildingLimitSet = true;
}

void GameDataLoader::SetAIStorageFull(const unsigned percent)
{
    if(percent < 1 || percent > 100)
        throw GameDataError("rttr:SetAIStorageFull: the value must be in [1, 100] (percent of the capacity)");
    worldDesc_.aiStorageFullPercent = percent;
}

void GameDataLoader::SetWarehouseCapacity(const kaguya::LuaTable& data)
{
    const std::string context = "Capacity of the warehouses (rttr:SetWarehouseCapacity)";
    const std::string example = "{ Headquarters = 500, Storehouse = 200 }";
    const std::vector<std::string> keys = data.keys<std::string>();
    if(keys.empty() || keys.size() != data.keys<kaguya::LuaRef>().size())
        throw GameDataError(context + ": expected a table like " + example);
    for(const std::string& building : keys)
    {
        const kaguya::LuaRef value = data[building];
        if(value.type() != kaguya::LuaRef::TYPE_NUMBER)
            throw GameDataError(context + ": the capacity of '" + building + "' is no number (" + example + ")");
        const double number = value.get<double>();
        if(number < 1 || number > 100000 || number != static_cast<double>(static_cast<unsigned>(number)))
            throw GameDataError(context + ": the capacity of '" + building + "' must be a whole number in [1, 100000]");
        if(!worldDesc_.warehouseCapacity.emplace(building, static_cast<unsigned>(number)).second)
            throw GameDataError(context + ": '" + building + "' is defined twice");
    }
}

void GameDataLoader::SetFlagWorkLimit(const unsigned numWares)
{
    if(numWares < 1 || numWares > 8)
        throw GameDataError("rttr:SetFlagWorkLimit: the value must be in [1, 8] (a flag holds 8 wares)");
    worldDesc_.flagWorkLimit = numWares;
}

void GameDataLoader::SetAIStartStockNoSurplus(const kaguya::LuaTable& data)
{
    const std::string context = "Start stock of the AI that is no surplus (rttr:SetAIStartStockNoSurplus)";
    if(!worldDesc_.aiStartStockNoSurplus.empty())
        throw GameDataError(context + " is defined twice");
    // Every entry is a tag query: "BasicBuildingMaterial" or { "Bretter", "Nadelholz" }
    for(const kaguya::LuaRef& entry : data.values())
        worldDesc_.aiStartStockNoSurplus.push_back(parseTagQuery(entry, context));
    if(worldDesc_.aiStartStockNoSurplus.empty())
        throw GameDataError(context + ": the list is empty");
}

void loadGameData(WorldDescription& worldDesc)

{
    GameDataLoader gdLoader(worldDesc);
    if(!gdLoader.Load())
        throw std::runtime_error("Failed to load game data");
}
