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
        .addFunction("AddWare", &GameDataLoader::AddWare)
        .addFunction("SetWareTags", &GameDataLoader::SetWareTags)
        .addFunction("AddWorkshop", &GameDataLoader::AddWorkshop)
        .addFunction("AddByproduct", &GameDataLoader::AddByproduct)
        .addFunction("SetKnowledgeDistribution", &GameDataLoader::SetKnowledgeDistribution)
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

void GameDataLoader::SetWareTags(const kaguya::LuaTable& data)
{
    for(const std::string& ware : data.keys<std::string>())
    {
        const kaguya::LuaRef value = data[ware];
        if(value.type() != kaguya::LuaRef::TYPE_TABLE)
            throw GameDataError("Tags of ware '" + ware + "' must be a table like { kind = \"Bretter\" }");
        CheckedLuaTable tagData(value.get<kaguya::LuaTable>());
        WareTagsDesc tags;
        tags.kind = tagData.getOrDefault("kind", std::string());
        tags.properties = tagData.getOrDefault("properties", std::vector<std::string>());
        tagData.checkUnused();
        if(!worldDesc_.wareTags.emplace(ware, tags).second)
            throw GameDataError("Tags of ware '" + ware + "' are defined twice");
    }
}

void GameDataLoader::AddWorkshop(const kaguya::LuaTable& data)
{
    worldDesc_.workshops.emplace_back(data);
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

void loadGameData(WorldDescription& worldDesc)

{
    GameDataLoader gdLoader(worldDesc);
    if(!gdLoader.Load())
        throw std::runtime_error("Failed to load game data");
}
