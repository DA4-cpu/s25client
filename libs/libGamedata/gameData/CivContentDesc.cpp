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

BuildingToolsDesc parseBuildingTools(const kaguya::LuaTable& luaTable, const std::string& context)
{
    CheckedLuaTable luaData(luaTable);
    BuildingToolsDesc result;
    result.baseSpeed = luaData.getOrDefault("baseSpeed", 0u);
    if(result.baseSpeed > 1000)
        throw GameDataError(context + ": baseSpeed must be in [0, 1000] (percent)");
    if(const auto toolTable = luaData.getOptional<kaguya::LuaTable>("tools"))
    {
        for(const kaguya::LuaRef& entry : toolTable->values())
            result.tools.push_back(parseTagQuery(entry, context + ", tools"));
    }
    if(result.tools.size() > MAX_BUILDING_TOOLS)
        throw GameDataError(context + ": at most " + std::to_string(MAX_BUILDING_TOOLS) + " tools are possible");
    if(result.tools.empty() && result.baseSpeed == 0)
        throw GameDataError(context + ": without tools baseSpeed must be > 0 (otherwise it never works)");
    luaData.checkUnused();
    return result;
}

ToolMaterialDesc::ToolMaterialDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    name = readIdentifier(luaData, "Tool material");
    title = luaData.getOrDefault("title", name);
    if(luaData.getOptional<kaguya::LuaRef>("priority"))
    {
        throw GameDataError("Tool material '" + name
                            + "': the priority is set in distribution.lua with rttr:SetToolMaterialPriorities");
    }
    luaData.checkUnused();
}

ToolDesc::ToolDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    luaData.getOrThrow(ware, "ware");
    const std::string context = "Tool '" + ware + "'";
    luaData.getOrThrow(material, "material");
    luaData.getOrThrow(durability, "durability");
    luaData.getOrThrow(speed, "speed");
    if(durability == 0 || durability > 60000)
        throw GameDataError(context + ": durability must be in [1, 60000] (work steps)");
    if(speed > 1000)
        throw GameDataError(context + ": speed must be in [0, 1000] (percent)");
    luaData.checkUnused();
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
    if(size != "small" && size != "medium" && size != "large" && size != "mine")
        throw GameDataError(context + ": size must be \"small\", \"medium\", \"large\" or \"mine\"");
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

    if(auto libraryTable = luaData.getOptional<kaguya::LuaTable>("library"))
    {
        CheckedLuaTable libraryData(*libraryTable);
        libraryData.getOrThrow(libraryCapacity, "capacity");
        libraryData.checkUnused();
        if(libraryCapacity == 0 || libraryCapacity > 255)
            throw GameDataError(context + ": library capacity must be in [1, 255]");
    }
    const bool isLibrary = libraryCapacity > 0;
    if(auto collectTable = luaData.getOptional<kaguya::LuaTable>("collect"))
    {
        CheckedLuaTable collectData(*collectTable);
        collectData.getOrThrow(collectObject, "object");
        collectData.getOrThrow(collectRadius, "radius");
        collectData.checkUnused();
        if(collectRadius == 0 || collectRadius > 20)
            throw GameDataError(context + ": collect radius must be in [1, 20] (nodes)");
        if(!inputs.empty() || isLibrary)
            throw GameDataError(context + ": a workshop that collects map objects has no inputs and is no library");
    }
    if(auto plantTable = luaData.getOptional<kaguya::LuaTable>("plant"))
    {
        CheckedLuaTable plantData(*plantTable);
        plantData.getOrThrow(plantObject, "object");
        plantData.getOrThrow(plantRadius, "radius");
        plantData.checkUnused();
        if(plantRadius == 0 || plantRadius > 20)
            throw GameDataError(context + ": plant radius must be in [1, 20] (nodes)");
        if(isLibrary || !collectObject.empty())
            throw GameDataError(context
                                + ": a plantation is no library and does not collect (it harvests its own "
                                  "objects if it has an output)");
    }
    if(auto mineTable = luaData.getOptional<kaguya::LuaTable>("mine"))
    {
        CheckedLuaTable mineData(*mineTable);
        mineData.getOrThrow(mineResource, "resource");
        mineData.checkUnused();
        if(isLibrary || !collectObject.empty() || !plantObject.empty())
            throw GameDataError(context + ": a mine is no library, collector or plantation");
    }
    if((size == "mine") != !mineResource.empty())
        throw GameDataError(context + ": size = \"mine\" and mine = { resource = ... } belong together");
    if(const auto knowledgeTable = luaData.getOptional<kaguya::LuaTable>("knowledge"))
        knowledge = parseKnowledge(*knowledgeTable, true, context + ", knowledge");
    if(isLibrary && !inputs.empty())
    {
        throw GameDataError(context
                            + ": a library has no inputs, it takes all wares with knowledge points (knowledge media)");
    }
    const auto outputValue = luaData.getOptional<kaguya::LuaRef>("output");
    const auto outputsTable = luaData.getOptional<kaguya::LuaTable>("outputs");
    if(outputValue && outputsTable)
        throw GameDataError(context + ": use either output or outputs");
    if(isLibrary)
    {
        if(outputValue || outputsTable)
            throw GameDataError(context + ": a library produces nothing (no output)");
    } else if(outputValue)
        output = parseTagQuery(*outputValue, context + ", output");
    else if(outputsTable)
    {
        for(const kaguya::LuaRef& entry : outputsTable->values())
            outputs.push_back(parseTagQuery(entry, context + ", outputs"));
        if(outputs.empty())
            throw GameDataError(context + ": outputs is empty");
        if(outputs.size() > MAX_WORKSHOP_OUTPUTS)
        {
            throw GameDataError(context + ": at most " + std::to_string(MAX_WORKSHOP_OUTPUTS)
                                + " entries in outputs are possible");
        }
        if(!collectObject.empty() || !plantObject.empty() || !mineResource.empty())
            throw GameDataError(context + ": collectors, plantations and mines have a single output, not outputs");
    } else if(plantObject.empty())
        throw GameDataError(context + ": output is missing (only a plantation may have none: then it only plants)");
    inheritProperties = luaData.getOrDefault("inheritProperties", false);
    outputChoice = luaData.getOrDefault("outputChoice", std::string());
    if(!outputChoice.empty() && !isValidIdentifier(outputChoice))
    {
        throw GameDataError(context
                            + ": outputChoice must be \"knowledge\", \"tools\" or the name of a regulator "
                              "(rttr:AddRegulator)");
    }
    if(!outputChoice.empty() && inheritProperties)
        throw GameDataError(context + ": outputChoice and inheritProperties can not be combined");
    if(!outputs.empty() && inheritProperties)
        throw GameDataError(context + ": outputs and inheritProperties can not be combined (use output)");
    if(outputs.size() > 1u && outputChoice.empty())
    {
        throw GameDataError(context
                            + ": with several outputs an outputChoice is needed (\"knowledge\", \"tools\" or the "
                              "name of a regulator)");
    }
    storage = luaData.getOrDefault("storage", 6u);
    if(storage == 0 || storage > 99)
        throw GameDataError(context + ": storage must be in [1, 99]");
    if(isLibrary)
    {
        if(luaData.getOptional<unsigned>("workTime"))
            throw GameDataError(context + ": a library has no workTime");
    } else
    {
        luaData.getOrThrow(workTime, "workTime");
        if(workTime == 0)
            throw GameDataError(context + ": workTime must be > 0");
    }
    waitTime = luaData.getOrDefault("waitTime", 0u);
    aiNearBorder = luaData.getOrDefault("aiNearBorder", false);
    if(const auto toolsTable = luaData.getOptional<kaguya::LuaTable>("tools"))
        tools = parseBuildingTools(*toolsTable, context);
    luaData.checkUnused();
}

MapObjectDesc::MapObjectDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    name = readIdentifier(luaData, "Map object");
    const std::string context = "Map object '" + name + "'";
    title = luaData.getOrDefault("title", name);
    sizes = luaData.getOrDefault("sizes", sizes);
    if(sizes == 0 || sizes > 16)
        throw GameDataError(context + ": sizes must be in [1, 16]");
    luaData.getOrThrow(look, "look");
    if(look != "Granite1" && look != "Granite2")
        throw GameDataError(context + ": look must be \"Granite1\" or \"Granite2\"");
    tint = luaData.getOrDefault("tint", tint);
    if(tint > 0xFFFFFF)
        throw GameDataError(context + ": tint must be a color like 0xB07040");
    growTime = luaData.getOrDefault("growTime", growTime);
    if(growTime > 3600)
        throw GameDataError(context + ": growTime must be at most 3600 (seconds)");
    plantSize = luaData.getOrDefault("plantSize", plantSize);
    if(plantSize > sizes)
        throw GameDataError(context + ": plantSize must be in [1, sizes]");
    if(plantSize > 0 && growTime == 0)
        throw GameDataError(context + ": plantSize needs growTime (only planted objects grow)");
    luaData.checkUnused();
}

OreDesc::OreDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    name = readIdentifier(luaData, "Ore");
    const std::string context = "Ore '" + name + "'";
    title = luaData.getOrDefault("title", name);
    luaData.getOrThrow(look, "look");
    if(look != "Iron" && look != "Gold" && look != "Coal" && look != "Granite")
        throw GameDataError(context + ": look must be \"Iron\", \"Gold\", \"Coal\" or \"Granite\"");
    tint = luaData.getOrDefault("tint", tint);
    if(tint > 0xFFFFFF)
        throw GameDataError(context + ": tint must be a color like 0xB07040");
    luaData.checkUnused();
}

RegulatorDesc::RegulatorDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    name = readIdentifier(luaData, "Regulator");
    const std::string context = "Regulator '" + name + "'";
    if(name == "knowledge" || name == "tools")
        throw GameDataError(context + ": the names \"knowledge\" and \"tools\" are reserved");
    title = luaData.getOrDefault("title", name);
    const auto entryTable = luaData.getOrThrow<kaguya::LuaTable>("entries");
    for(const kaguya::LuaRef& entryValue : entryTable.values())
    {
        if(entryValue.type() != kaguya::LuaRef::TYPE_TABLE)
            throw GameDataError(context + ": every entry must be a table like { tags = \"Jug\", value = 5 }");
        CheckedLuaTable entryData(entryValue.get<kaguya::LuaTable>());
        Entry entry;
        const auto tagsValue = entryData.getOptional<kaguya::LuaRef>("tags");
        if(!tagsValue)
            throw GameDataError(context + ": tags of an entry are missing");
        entry.tags = parseTagQuery(*tagsValue, context + ", entries");
        entry.title = entryData.getOrDefault("title", entry.tags.front());
        entry.value = entryData.getOrDefault("value", 5u);
        if(entry.value > 10)
            throw GameDataError(context + ", entry '" + entry.title + "': value must be in [0, 10]");
        entryData.checkUnused();
        entries.push_back(std::move(entry));
    }
    if(entries.empty())
        throw GameDataError(context + ": entries are missing");
    if(entries.size() > MAX_REGULATOR_ENTRIES)
    {
        throw GameDataError(context + ": at most " + std::to_string(MAX_REGULATOR_ENTRIES) + " entries are possible");
    }
    luaData.checkUnused();
}

ByproductDesc::ByproductDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    luaData.getOrThrow(building, "building");
    const std::string context = "Byproduct of '" + building + "'";
    const auto wareValue = luaData.getOptional<kaguya::LuaRef>("ware");
    if(!wareValue)
        throw GameDataError(context + ": ware is missing");
    ware = parseTagQuery(*wareValue, context + ", ware");
    amount = luaData.getOrDefault("amount", 1u);
    if(amount == 0 || amount > 7)
        throw GameDataError(context + ": amount must be in [1, 7] (a flag holds at most 8 wares)");
    if(const auto perValue = luaData.getOptional<kaguya::LuaRef>("per"))
        per = parseTagQuery(*perValue, context + ", per");
    technology = luaData.getOrDefault("technology", std::string());
    luaData.checkUnused();
}
