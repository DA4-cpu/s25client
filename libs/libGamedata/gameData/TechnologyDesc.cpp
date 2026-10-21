// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TechnologyDesc.h"
#include "WorldDescription.h"
#include "helpers/containerUtils.h"
#include "lua/CheckedLuaTable.h"
#include <set>

const std::array<const char*, 3> KNOWLEDGE_LUA_NAMES = {"LWP", "IWP", "MWP"};

KnowledgeValues parseKnowledge(const kaguya::LuaTable& table, bool allowNegative, const std::string& context)
{
    for(const std::string& key : table.keys<std::string>())
    {
        if(!helpers::contains(KNOWLEDGE_LUA_NAMES, key))
            throw GameDataError(context + ": unknown kind of knowledge '" + key + "' (allowed: LWP, IWP, MWP)");
    }
    KnowledgeValues result{};
    for(unsigned i = 0; i < result.size(); ++i)
    {
        const kaguya::LuaRef value = table[KNOWLEDGE_LUA_NAMES[i]];
        if(value.isNilref())
            continue;
        if(!value.isType<int>())
            throw GameDataError(context + ": " + KNOWLEDGE_LUA_NAMES[i] + " must be a whole number");
        result[i] = value.get<int>();
        if(!allowNegative && result[i] < 0)
            throw GameDataError(context + ": " + KNOWLEDGE_LUA_NAMES[i] + " must not be negative");
    }
    return result;
}

TechnologyDesc::TechnologyDesc(CheckedLuaTable luaData, const WorldDescription& worldDesc)
{
    luaData.getOrThrow(name, "name");
    if(name.empty())
        throw GameDataError("Technology without name");
    title = luaData.getOrDefault("title", name);
    description = luaData.getOrDefault("description", std::string());
    era = luaData.getOrDefault("era", 0u);
    if(era > 6)
        throw GameDataError("Technology '" + name + "': era must be in [0, 6]");

    requirements = luaData.getOrDefault("requires", std::vector<std::string>());
    std::set<std::string> seenRequirements;
    for(const std::string& req : requirements)
    {
        // Requiring earlier definition guarantees that there are no cycles
        if(!worldDesc.technologies.getIndex(req))
            throw GameDataError("Technology '" + name + "' requires unknown technology '" + req
                                + "'. Prerequisites must be defined before the technology using them.");
        if(!seenRequirements.insert(req).second)
            throw GameDataError("Technology '" + name + "' requires '" + req + "' twice");
    }

    cost = luaData.getOrDefault("cost", std::map<std::string, unsigned>());
    for(const auto& entry : cost)
    {
        if(entry.second == 0)
            throw GameDataError("Technology '" + name + "': cost of '" + entry.first + "' must be > 0");
    }
    if(const auto knowledgeTable = luaData.getOptional<kaguya::LuaTable>("knowledge"))
        knowledge = parseKnowledge(*knowledgeTable, false, "Technology '" + name + "'");
    time = luaData.getOrDefault("time", 0u);

    if(auto unlocks = luaData.getOptional<CheckedLuaTable>("unlocks"))
    {
        unlockedBuildings = unlocks->getOrDefault("buildings", std::vector<std::string>());
        unlocks->checkUnused();
    }
    exclusive = luaData.getOrDefault("exclusive", false);
    requiredAddon = luaData.getOrDefault("addon", std::string());
    aiPriority = luaData.getOrDefault("aiPriority", 0);
    luaData.checkUnused();
}
