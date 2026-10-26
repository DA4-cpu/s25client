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
    void SetBuildingKnowledge(const kaguya::LuaTable& data);
    void AddWare(const kaguya::LuaTable& data);
    void SetWareTags(const kaguya::LuaTable& data);
    void AddWorkshop(const kaguya::LuaTable& data);
    void AddByproduct(const kaguya::LuaTable& data);
    void SetKnowledgeDistribution(const kaguya::LuaTable& data);
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
