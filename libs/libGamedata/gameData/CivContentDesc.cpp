// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CivContentDesc.h"
#include "WorldDescription.h"
#include "lua/CheckedLuaTable.h"

namespace {
bool isValidIdentifier(const std::string& name)
{
    if(name.empty())
        return false;
    for(const char c : name)
    {
        const bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if(!valid)
            return false;
    }
    return true;
}

std::string readIdentifier(CheckedLuaTable& luaData, const char* what)
{
    const auto name = luaData.getOrThrow<std::string>("name");
    if(!isValidIdentifier(name))
    {
        throw GameDataError(std::string(what) + " '" + name
                            + "': name may only contain letters A-Z, digits and underscores (no umlauts or spaces)");
    }
    return name;
}

std::vector<std::string> readTagList(CheckedLuaTable& luaData, const char* field, const std::string& context)
{
    const auto tags = luaData.getOrDefault(field, std::vector<std::string>());
    for(const std::string& tag : tags)
    {
        if(tag.empty())
            throw GameDataError(context + ": empty tag in '" + field + "'");
    }
    return tags;
}
} // namespace

TagQuery parseTagQuery(const kaguya::LuaRef& value, const std::string& context)
{
    TagQuery result;
    if(value.type() == kaguya::LuaRef::TYPE_STRING)
        result.push_back(value.get<std::string>());
    else if(value.type() == kaguya::LuaRef::TYPE_TABLE)
    {
        const auto table = value.get<kaguya::LuaTable>();
        for(const kaguya::LuaRef& entry : table.values())
        {
            if(entry.type() != kaguya::LuaRef::TYPE_STRING)
                throw GameDataError(context + ": tag lists may only contain strings");
            result.push_back(entry.get<std::string>());
        }
    } else
        throw GameDataError(context + ": expected a tag (string) or a list of tags");
    if(result.empty())
        throw GameDataError(context + ": empty list of tags");
    for(const std::string& tag : result)
    {
        if(tag.empty())
            throw GameDataError(context + ": empty tag");
    }
    return result;
}

WareDesc::WareDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    name = readIdentifier(luaData, "Ware");
    const std::string context = "Ware '" + name + "'";
    title = luaData.getOrDefault("title", name);
    kind = luaData.getOrDefault("kind", std::string());
    properties = readTagList(luaData, "properties", context);
    luaData.getOrThrow(look, "look");
    luaData.getOrThrow(transport, "transport");
    if(const auto knowledgeTable = luaData.getOptional<kaguya::LuaTable>("knowledge"))
        knowledge = parseKnowledge(*knowledgeTable, false, context);
    luaData.checkUnused();
}

WorkshopDesc::WorkshopDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    name = readIdentifier(luaData, "Workshop");
    const std::string context = "Workshop '" + name + "'";
    title = luaData.getOrDefault("title", name);
    description = luaData.getOrDefault("description", std::string());
    luaData.getOrThrow(size, "size");
    if(size != "small" && size != "medium" && size != "large")
        throw GameDataError(context + ": size must be \"small\", \"medium\" or \"large\"");
    cost = luaData.getOrDefault("cost", std::map<std::string, unsigned>());
    luaData.getOrThrow(look, "look");

    {
        auto workerTable = luaData.getOrThrow<kaguya::LuaTable>("worker");
        CheckedLuaTable workerData(workerTable);
        worker.title = workerData.getOrDefault("title", title);
        workerData.getOrThrow(worker.tool, "tool");
        workerData.getOrThrow(worker.look, "look");
        workerData.checkUnused();
    }

    if(const auto inputTable = luaData.getOptional<kaguya::LuaTable>("inputs"))
    {
        for(const kaguya::LuaRef& entry : inputTable->values())
            inputs.push_back(parseTagQuery(entry, context + ", inputs"));
    }
    if(inputs.size() > 3)
        throw GameDataError(context + ": at most 3 inputs are possible");
    const auto consume = luaData.getOrDefault("consume", std::string("each"));
    if(consume != "each" && consume != "any")
        throw GameDataError(context + ": consume must be \"each\" or \"any\"");
    consumeEach = consume == "each";

    const auto outputValue = luaData.getOptional<kaguya::LuaRef>("output");
    if(!outputValue)
        throw GameDataError(context + ": output is missing");
    output = parseTagQuery(*outputValue, context + ", output");
    inheritProperties = luaData.getOrDefault("inheritProperties", false);
    storage = luaData.getOrDefault("storage", 6u);
    if(storage == 0 || storage > 99)
        throw GameDataError(context + ": storage must be in [1, 99]");
    luaData.getOrThrow(workTime, "workTime");
    if(workTime == 0)
        throw GameDataError(context + ": workTime must be > 0");
    waitTime = luaData.getOrDefault("waitTime", 0u);
    luaData.checkUnused();
}
