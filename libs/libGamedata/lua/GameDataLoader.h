// Copyright (C) 2005 - 2025 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "LuaInterfaceBase.h"
#include <boost/filesystem/path.hpp>

namespace kaguya {
class State;
class LuaTable;
} // namespace kaguya
struct WorldDescription;

class GameDataLoader : public LuaInterfaceBase
{
public:
    GameDataLoader(WorldDescription& worldDesc, const boost::filesystem::path& basePath);
    GameDataLoader(WorldDescription& worldDesc);
    ~GameDataLoader() override;

    bool Load();

    static void Register(kaguya::State& state);

private:
    void Include(const std::string& filepath);
    void AddLandscape(const kaguya::LuaTable& data);
    void AddTerrainEdge(const kaguya::LuaTable& data);
    void AddTerrain(const kaguya::LuaTable& data);
    void AddTechnology(const kaguya::LuaTable& data);
    void SetAIExpandingBuildings(const kaguya::LuaTable& data);
    void SetAIBasicEconomy(const kaguya::LuaTable& data);
    void SetAIStartStockNoSurplus(const kaguya::LuaTable& data);
    void SetAIBuildingLimit(const kaguya::LuaTable& data);
    void SetAIStorageFull(unsigned percent);
    void SetFlagWorkLimit(unsigned numWares);
    void AddWare(const kaguya::LuaTable& data);
    void SetWare(const kaguya::LuaTable& data);
    void AddWorkshop(const kaguya::LuaTable& data);
    void SetBuilding(const kaguya::LuaTable& data);
    /// Functions replaced by rttr:SetBuilding: they stop the loading with a hint
    [[noreturn]] void SetBuildingCost(const kaguya::LuaTable& data);
    [[noreturn]] void SetBuildingKnowledge(const kaguya::LuaTable& data);
    [[noreturn]] void SetBuildingTools(const kaguya::LuaTable& data);
    [[noreturn]] void SetBuildingOutputRatio(const kaguya::LuaTable& data);
    [[noreturn]] void SetWarehouseCapacity(const kaguya::LuaTable& data);
    void AddMapObject(const kaguya::LuaTable& data);
    void AddOre(const kaguya::LuaTable& data);
    void AddByproduct(const kaguya::LuaTable& data);
    void SetKnowledgeDistribution(const kaguya::LuaTable& data);
    void AddToolMaterial(const kaguya::LuaTable& data);
    void AddTool(const kaguya::LuaTable& data);
    void SetToolReorder(unsigned percent);
    void SetToolMaterialPriorities(const kaguya::LuaTable& data);
    void SetMapClimate(const kaguya::LuaTable& data);
    void AddMapZone(const kaguya::LuaTable& data);
    void AddMapResource(const kaguya::LuaTable& data);
    /// Check references between the loaded definitions. Logs and returns false on errors
    bool CheckMapGen();

    WorldDescription& worldDesc_;
    boost::filesystem::path basePath_, curFile_;
    int curIncludeDepth_;
};

void loadGameData(WorldDescription& worldDesc);
