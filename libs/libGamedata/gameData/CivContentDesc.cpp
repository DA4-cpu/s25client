// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CivContentDesc.h"
#include "WorldDescription.h"
#include "lua/CheckedLuaTable.h"
#include <algorithm>
#include <cmath>

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

BuiltinWareDesc parseBuiltinWare(const kaguya::LuaTable& luaTable, const std::string& ware)
{
    const std::string context = "Built-in ware '" + ware + "' (rttr:SetWare)";
    CheckedLuaTable luaData(luaTable);
    BuiltinWareDesc result;
    if(const auto name = luaData.getOptional<std::string>("name"))
    {
        if(!isValidIdentifier(*name))
        {
            throw GameDataError(context + ": name '" + *name
                                + "' may only contain letters A-Z, digits and underscores (no umlauts or spaces)");
        }
        result.name = *name;
    }
    if(const auto kindValue = luaData.getOptional<kaguya::LuaRef>("kind"))
        result.kind = parseTagQuery(*kindValue, context + ", kind");
    result.properties = readTagList(luaData, "properties", context);
    luaData.checkUnused();
    return result;
}

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

std::vector<CostDesc> parseBuildingCost(const kaguya::LuaRef& value, const std::string& context)
{
    if(value.type() != kaguya::LuaRef::TYPE_TABLE)
        throw GameDataError(context
                            + ": expected a table like { Boards = 2 } or { { tags = \"Bretter\", amount = 2 } }");
    const auto table = value.get<kaguya::LuaTable>();
    std::vector<CostDesc> result;
    const auto readAmount = [&context](const kaguya::LuaRef& amountValue) {
        if(amountValue.type() != kaguya::LuaRef::TYPE_NUMBER)
            throw GameDataError(context + ": the amount must be a number");
        const int amount = amountValue.get<int>();
        if(amount <= 0 || amount > static_cast<int>(MAX_TOTAL_COST))
            throw GameDataError(context + ": the amount must be in [1, " + std::to_string(MAX_TOTAL_COST) + "]");
        return static_cast<unsigned>(amount);
    };
    std::vector<std::string> keys;
    unsigned numIndexed = 0;
    for(const kaguya::LuaRef& key : table.keys<kaguya::LuaRef>())
    {
        if(key.type() == kaguya::LuaRef::TYPE_STRING)
            keys.push_back(key.get<std::string>());
        else if(key.type() == kaguya::LuaRef::TYPE_NUMBER)
            ++numIndexed;
        else
            throw GameDataError(context + ": unexpected key");
    }
    if(!keys.empty() && numIndexed > 0)
        throw GameDataError(context + ": use either { Boards = 2 } or { { tags = ..., amount = ... } }, not both");
    // Short form: tag -> amount (a lua table has no order, so alphabetical)
    std::sort(keys.begin(), keys.end());
    for(const std::string& key : keys)
    {
        if(key.empty())
            throw GameDataError(context + ": empty tag");
        result.push_back(CostDesc{{key}, readAmount(table[key])});
    }
    // Long form: list of { tags = ..., amount = ... } in this order
    for(unsigned i = 1; i <= numIndexed; ++i)
    {
        const kaguya::LuaRef entry = table[i];
        if(entry.type() != kaguya::LuaRef::TYPE_TABLE)
            throw GameDataError(context + ": every entry must be a table like { tags = \"Bretter\", amount = 2 }");
        CheckedLuaTable entryData(entry.get<kaguya::LuaTable>());
        const auto tagsValue = entryData.getOptional<kaguya::LuaRef>("tags");
        if(!tagsValue)
            throw GameDataError(context + ": tags of an entry are missing");
        CostDesc desc;
        desc.tags = parseTagQuery(*tagsValue, context);
        const auto amountValue = entryData.getOptional<kaguya::LuaRef>("amount");
        if(!amountValue)
            throw GameDataError(context + ": amount of an entry is missing");
        desc.amount = readAmount(*amountValue);
        entryData.checkUnused();
        result.push_back(std::move(desc));
    }
    if(result.empty())
        throw GameDataError(context + ": no construction material given");
    if(result.size() > MAX_COST_ENTRIES)
        throw GameDataError(context + ": at most " + std::to_string(MAX_COST_ENTRIES) + " materials are possible");
    unsigned total = 0;
    for(const CostDesc& entry : result)
        total += entry.amount;
    if(total > MAX_TOTAL_COST)
        throw GameDataError(context + ": at most " + std::to_string(MAX_TOTAL_COST) + " wares in total are possible");
    return result;
}

unsigned parseOutputRatio(const double value, const std::string& context)
{
    if(!(value >= 0 && value <= MAX_OUTPUT_RATIO / 100.))
        throw GameDataError(context + ": outputRatio must be in [0, " + std::to_string(MAX_OUTPUT_RATIO / 100) + "]");
    return static_cast<unsigned>(std::lround(value * 100));
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

namespace {
/// A ratio of a tool: a number (for everything, returned) or a table tag -> number (byTag). In hundredths, at most
/// maxValue in both directions
int parseToolRatio(const kaguya::LuaRef& value, std::vector<std::pair<std::string, int>>& byTag,
                   const unsigned maxValue, const std::string& context)
{
    const auto toHundredths = [&](const kaguya::LuaRef& number) {
        if(number.type() != kaguya::LuaRef::TYPE_NUMBER)
            throw GameDataError(context + ": expected a number or a table like { Coal = -0.25 }");
        const double ratio = number.get<double>();
        if(!(std::abs(ratio) <= maxValue))
            throw GameDataError(context + ": must be in [-" + std::to_string(maxValue) + ", " + std::to_string(maxValue)
                                + "]");
        return static_cast<int>(std::lround(ratio * 100));
    };
    if(value.type() != kaguya::LuaRef::TYPE_TABLE)
        return toHundredths(value);
    const auto table = value.get<kaguya::LuaTable>();
    for(const kaguya::LuaRef& key : table.keys())
    {
        if(key.type() != kaguya::LuaRef::TYPE_STRING || key.get<std::string>().empty())
            throw GameDataError(context + ": every key must be a tag, e.g. { Coal = -0.25 }");
        const std::string tag = key.get<std::string>();
        byTag.emplace_back(tag, toHundredths(table[tag]));
    }
    // A lua table has no order
    std::sort(byTag.begin(), byTag.end());
    if(byTag.empty())
        throw GameDataError(context + ": the table is empty");
    return 0;
}

/// An entry of outputs: a tag query ("CopperSpear", { "Saw", "Copper" }) or { "CopperSword", steps = 1.5 }. Returns
/// the work steps of the product in hundredths
unsigned parseOutput(const kaguya::LuaRef& value, TagQuery& tags, const std::string& context)
{
    if(value.type() == kaguya::LuaRef::TYPE_TABLE)
    {
        const auto table = value.get<kaguya::LuaTable>();
        const kaguya::LuaRef steps = table["steps"];
        if(!steps.isNilref())
        {
            if(steps.type() != kaguya::LuaRef::TYPE_NUMBER || !(steps.get<double>() > 0) || steps.get<double>() > 99)
                throw GameDataError(context + ": steps must be above 0 and at most 99 (work steps of the product)");
            const auto result = static_cast<unsigned>(std::lround(steps.get<double>() * 100));
            if(result == 0)
                throw GameDataError(context + ": steps must be at least 0.01");
            for(const kaguya::LuaRef& key : table.keys())
            {
                if(key.type() == kaguya::LuaRef::TYPE_STRING && key.get<std::string>() != "steps")
                    throw GameDataError(context + ": unknown field '" + key.get<std::string>() + "'");
            }
            for(unsigned i = 1; !table[i].isNilref(); ++i)
            {
                if(table[i].type() != kaguya::LuaRef::TYPE_STRING || table[i].get<std::string>().empty())
                    throw GameDataError(context + ": tag lists may only contain strings");
                tags.push_back(table[i].get<std::string>());
            }
            if(tags.empty())
                throw GameDataError(context + ": the product is missing, e.g. { \"CopperSword\", steps = 1.5 }");
            return result;
        }
    }
    tags = parseTagQuery(value, context);
    return 100;
}
} // namespace

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
    if(const auto ratio = luaData.getOptional<kaguya::LuaRef>("outputRatio"))
        outputRatio = parseToolRatio(*ratio, outputRatioByTag, MAX_OUTPUT_RATIO / 100, context + ", outputRatio");
    if(const auto ratio = luaData.getOptional<kaguya::LuaRef>("inputRatio"))
        inputRatio = parseToolRatio(*ratio, inputRatioByTag, 99, context + ", inputRatio");
    luaData.checkUnused();
}

WareDesc::WareDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    name = readIdentifier(luaData, "Ware");
    const std::string context = "Ware '" + name + "'";
    title = luaData.getOrDefault("title", name);
    if(const auto kindValue = luaData.getOptional<kaguya::LuaRef>("kind"))
        kind = parseTagQuery(*kindValue, context + ", kind");
    properties = readTagList(luaData, "properties", context);
    luaData.getOrThrow(look, "look");
    luaData.getOrThrow(transport, "transport");
    if(const auto knowledgeTable = luaData.getOptional<kaguya::LuaTable>("knowledge"))
        knowledge = parseKnowledge(*knowledgeTable, false, context);
    luaData.checkUnused();
}

namespace {
/// An entry of inputs: a tag query ("Bones", { "Bretter", "Nadelholz" }) or { tags = ..., amount = 0.5, storage = 6 }
InputDesc parseInput(const kaguya::LuaRef& value, const std::string& context)
{
    InputDesc result;
    if(value.type() != kaguya::LuaRef::TYPE_TABLE)
    {
        result.tags = parseTagQuery(value, context);
        return result;
    }
    const auto table = value.get<kaguya::LuaTable>();
    const kaguya::LuaRef tagsRef = table["tags"];
    if(tagsRef.isNilref())
    {
        result.tags = parseTagQuery(value, context);
        return result;
    }
    CheckedLuaTable data(table);
    result.tags = parseTagQuery(*data.getOptional<kaguya::LuaRef>("tags"), context);
    if(const auto amount = data.getOptional<double>("amount"))
    {
        if(!(*amount > 0) || *amount > 99)
            throw GameDataError(context + ": amount must be above 0 and at most 99 (wares per work step)");
        result.amount = static_cast<unsigned>(std::lround(*amount * 100));
        if(result.amount == 0)
            throw GameDataError(context + ": amount must be at least 0.01");
    }
    if(const auto storage = data.getOptional<unsigned>("storage"))
    {
        if(*storage == 0 || *storage > 99)
            throw GameDataError(context + ": storage of an input must be in [1, 99]");
        result.storage = *storage;
    }
    data.checkUnused();
    return result;
}
} // namespace

MilitaryDesc parseMilitary(const kaguya::LuaTable& luaTable, const std::string& context)
{
    CheckedLuaTable militaryData(luaTable);
    const auto readValue = [&](const char* key, const unsigned minValue) -> std::optional<unsigned> {
        const auto value = militaryData.getOptional<double>(key);
        if(!value)
            return std::nullopt;
        if(*value != std::floor(*value) || *value < minValue || *value > MAX_MILITARY_VALUE)
        {
            throw GameDataError(context + ": military " + key + " must be a whole number in ["
                                + std::to_string(minValue) + ", " + std::to_string(MAX_MILITARY_VALUE) + "]");
        }
        return static_cast<unsigned>(*value);
    };
    const auto readRequired = [&](const char* key, const unsigned minValue) {
        const auto value = readValue(key, minValue);
        if(!value)
        {
            throw GameDataError(context + ": military needs " + key
                                + " (military = { soldiers = 2, radius = 10, coins = 1 })");
        }
        return *value;
    };
    MilitaryDesc mil;
    mil.soldiers = readRequired("soldiers", 1);
    mil.radius = readRequired("radius", 1);
    mil.vision = readValue("vision", 1);
    mil.coins = readRequired("coins", 0);
    mil.armor = readValue("armor", 0);
    militaryData.checkUnused();
    return mil;
}

namespace {
/// Read a number of frames (workTime, waitTime, waitAfterTime): a whole number in [minValue, MAX_WORK_FRAMES]
std::optional<unsigned> readFrames(CheckedLuaTable& luaData, const char* key, const unsigned minValue,
                                   const std::string& context)
{
    const auto value = luaData.getOptional<double>(key);
    if(!value)
        return std::nullopt;
    if(*value != std::floor(*value) || *value < minValue || *value > MAX_WORK_FRAMES)
    {
        throw GameDataError(context + ": " + key + " must be a whole number of frames in [" + std::to_string(minValue)
                            + ", " + std::to_string(MAX_WORK_FRAMES) + "] (1 frame = 50 ms at normal speed)");
    }
    return static_cast<unsigned>(*value);
}

/// Read { radius = 8 } of collect or plant of a built-in building
unsigned readBuiltinRadius(const kaguya::LuaTable& luaTable, const std::string& context)
{
    CheckedLuaTable data(luaTable);
    const auto radius = data.getOptional<double>("radius");
    if(!radius || *radius != std::floor(*radius) || *radius < 1 || *radius > MAX_WORK_RADIUS)
        throw GameDataError(context + ": radius must be a whole number in [1, " + std::to_string(MAX_WORK_RADIUS)
                            + "] (nodes)");
    data.checkUnused();
    return static_cast<unsigned>(*radius);
}
} // namespace

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
    if(const auto costValue = luaData.getOptional<kaguya::LuaRef>("cost"))
        cost = parseBuildingCost(*costValue, context + ", cost");
    luaData.getOrThrow(look, "look");

    if(auto militaryTable = luaData.getOptional<kaguya::LuaTable>("military"))
    {
        // A military building: soldiers hold the land around it, no worker, no production
        military = parseMilitary(*militaryTable, context);
        if(size == "mine")
            throw GameDataError(context + ": a military building can not stand on a mine place (size = \"mine\")");
        for(const char* key : {"worker", "inputs", "consume", "library", "collect", "plant", "mine", "warehouse",
                               "output", "outputs", "inheritProperties", "outputChoice", "storage", "workTime",
                               "waitTime", "waitAfterTime", "tools", "outputRatio", "aiNearBorder"})
        {
            if(luaData.getOptional<kaguya::LuaRef>(key))
            {
                throw GameDataError(context + ": a military building has no '" + key
                                    + "' (it only holds soldiers; the AI may always build it near a border)");
            }
        }
        if(const auto knowledgeTable = luaData.getOptional<kaguya::LuaTable>("knowledge"))
            knowledge = parseKnowledge(*knowledgeTable, true, context + ", knowledge");
        luaData.checkUnused();
        return;
    }

    if(auto warehouseTable = luaData.getOptional<kaguya::LuaTable>("warehouse"))
    {
        // A warehouse for some wares: no worker, no production
        CheckedLuaTable warehouseData(*warehouseTable);
        warehouseData.getOrThrow(warehouseCapacity, "capacity");
        if(warehouseCapacity == 0 || warehouseCapacity > 100000)
            throw GameDataError(context + ": warehouse capacity must be in [1, 100000]");
        auto waresTable = warehouseData.getOrThrow<kaguya::LuaTable>("wares");
        for(const kaguya::LuaRef& entry : waresTable.values())
            warehouseWares.push_back(parseTagQuery(entry, context + ", warehouse wares"));
        if(warehouseWares.empty())
            throw GameDataError(context + ": warehouse wares is empty (the wares it stores, e.g. { \"Wood\" })");
        warehouseData.checkUnused();
        if(size == "mine")
            throw GameDataError(context + ": a warehouse is no mine");
        for(const char* key :
            {"worker", "inputs", "consume", "library", "collect", "plant", "mine", "outputs", "inheritProperties",
             "outputChoice", "storage", "workTime", "waitTime", "waitAfterTime", "tools", "outputRatio"})
        {
            if(luaData.getOptional<kaguya::LuaRef>(key))
                throw GameDataError(context + ": a warehouse has no '" + key + "' (it only stores wares)");
        }
        if(const auto knowledgeTable = luaData.getOptional<kaguya::LuaTable>("knowledge"))
            knowledge = parseKnowledge(*knowledgeTable, true, context + ", knowledge");
        aiNearBorder = luaData.getOrDefault("aiNearBorder", false);
        luaData.checkUnused();
        return;
    }

    {
        auto workerTable = luaData.getOrThrow<kaguya::LuaTable>("worker");
        CheckedLuaTable workerData(workerTable);
        worker.title = workerData.getOrDefault("title", title);
        workerData.getOrThrow(worker.look, "look");
        workerData.checkUnused();
    }

    if(const auto inputTable = luaData.getOptional<kaguya::LuaTable>("inputs"))
    {
        for(const kaguya::LuaRef& entry : inputTable->values())
            inputs.push_back(parseInput(entry, context + ", inputs"));
    }
    if(inputs.size() > MAX_WORKSHOP_INPUTS)
    {
        throw GameDataError(context + ": at most " + std::to_string(MAX_WORKSHOP_INPUTS) + " inputs are possible");
    }
    if(luaData.getOptional<kaguya::LuaRef>("consume"))
    {
        throw GameDataError(
          context
          + ": consume does not exist anymore. An input taking any of several wares is one entry with "
            "their common tag, e.g. inputs = { { tags = \"Food\" } }");
    }

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
                                  "objects if it has outputs)");
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
    if(luaData.getOptional<kaguya::LuaRef>("output"))
    {
        throw GameDataError(
          context
          + ": output does not exist anymore. The products are in outputs, e.g. outputs = { \"Clay\" } "
            "or outputs = { { \"Saw\", \"Copper\" } } for a ware with both tags");
    }
    const auto outputsTable = luaData.getOptional<kaguya::LuaTable>("outputs");
    if(isLibrary)
    {
        if(outputsTable)
            throw GameDataError(context + ": a library produces nothing (no outputs)");
    } else if(outputsTable)
    {
        for(const kaguya::LuaRef& entry : outputsTable->values())
        {
            TagQuery tags;
            outputSteps.push_back(parseOutput(entry, tags, context + ", outputs"));
            outputs.push_back(std::move(tags));
        }
        if(outputs.empty())
            throw GameDataError(context + ": outputs is empty");
        if(outputs.size() > MAX_WORKSHOP_OUTPUTS)
        {
            throw GameDataError(context + ": at most " + std::to_string(MAX_WORKSHOP_OUTPUTS)
                                + " entries in outputs are possible");
        }
    } else if(plantObject.empty())
        throw GameDataError(context + ": outputs is missing (only a plantation may have none: then it only plants)");
    inheritProperties = luaData.getOrDefault("inheritProperties", false);
    outputChoice = luaData.getOrDefault("outputChoice", std::string());
    if(!outputChoice.empty() && outputChoice != "knowledge")
    {
        throw GameDataError(context
                            + ": outputChoice can only be \"knowledge\". A workshop with several products is steered "
                              "by the distribution of all wares (Gesamtverteilung)");
    }
    if(!outputChoice.empty() && inheritProperties)
        throw GameDataError(context + ": outputChoice and inheritProperties can not be combined");
    storage = luaData.getOrDefault("storage", 6u);
    if(storage == 0 || storage > 99)
        throw GameDataError(context + ": storage must be in [1, 99]");
    if(isLibrary)
    {
        for(const char* key : {"workTime", "waitTime", "waitAfterTime"})
        {
            if(luaData.getOptional<kaguya::LuaRef>(key))
                throw GameDataError(context + ": a library has no " + key);
        }
    } else
    {
        const auto frames = readFrames(luaData, "workTime", 1, context);
        if(!frames)
            throw GameDataError(context + ": workTime is missing (frames of work per work step)");
        workTime = *frames;
        waitTime = readFrames(luaData, "waitTime", 0, context).value_or(0u);
        waitAfterTime = readFrames(luaData, "waitAfterTime", 0, context).value_or(DEFAULT_WAIT_AFTER_TIME);
        // Collectors and plantations go out again after waitTime, they do not wait after a work step
        if((!collectObject.empty() || !plantObject.empty()) && luaData.getOptional<kaguya::LuaRef>("waitAfterTime"))
            throw GameDataError(context + ": a collector or plantation has no waitAfterTime (only waitTime)");
    }
    aiNearBorder = luaData.getOrDefault("aiNearBorder", false);
    if(const auto toolsTable = luaData.getOptional<kaguya::LuaTable>("tools"))
        tools = parseBuildingTools(*toolsTable, context);
    if(const auto ratio = luaData.getOptional<double>("outputRatio"))
    {
        if(isLibrary || outputs.empty())
            throw GameDataError(context + ": outputRatio needs a product (outputs)");
        outputRatio = parseOutputRatio(*ratio, context);
    }
    luaData.checkUnused();
}

BuiltinBuildingDesc::BuiltinBuildingDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    luaData.getOrThrow(name, "name");
    const std::string context = "Building '" + name + "' (rttr:SetBuilding)";
    for(const std::string& key : luaTable.keys<std::string>())
    {
        if(key != "name")
            fields.push_back(key);
    }
    title = luaData.getOptional<std::string>("title");
    description = luaData.getOptional<std::string>("description");
    size = luaData.getOptional<std::string>("size");
    if(size && *size != "small" && *size != "medium" && *size != "large" && *size != "mine")
        throw GameDataError(context + ": size must be \"small\", \"medium\", \"large\" or \"mine\"");
    if(const auto costValue = luaData.getOptional<kaguya::LuaRef>("cost"))
        cost = parseBuildingCost(*costValue, context + ", cost");
    look = luaData.getOptional<std::string>("look");
    if(auto workerTable = luaData.getOptional<kaguya::LuaTable>("worker"))
    {
        CheckedLuaTable workerData(*workerTable);
        workerTitle = workerData.getOptional<std::string>("title");
        workerLook = workerData.getOptional<std::string>("look");
        workerData.checkUnused();
        if(!workerTitle && !workerLook)
            throw GameDataError(context
                                + ": worker needs title or look (worker = { title = \"...\", look = \"Carpenter\" })");
    }
    if(const auto knowledgeTable = luaData.getOptional<kaguya::LuaTable>("knowledge"))
        knowledge = parseKnowledge(*knowledgeTable, true, context + ", knowledge");
    if(const auto toolsTable = luaData.getOptional<kaguya::LuaTable>("tools"))
        tools = parseBuildingTools(*toolsTable, context);
    if(const auto ratio = luaData.getOptional<double>("outputRatio"))
        outputRatio = parseOutputRatio(*ratio, context);
    if(const auto storageValue = luaData.getOptional<double>("storage"))
    {
        if(*storageValue != std::floor(*storageValue) || *storageValue < 1 || *storageValue > 99)
            throw GameDataError(context + ": storage must be a whole number in [1, 99]");
        storage = static_cast<unsigned>(*storageValue);
    }
    workTime = readFrames(luaData, "workTime", 1, context);
    waitTime = readFrames(luaData, "waitTime", 1, context);
    waitAfterTime = readFrames(luaData, "waitAfterTime", 0, context);
    aiNearBorder = luaData.getOptional<bool>("aiNearBorder");
    if(const auto collectTable = luaData.getOptional<kaguya::LuaTable>("collect"))
        collectRadius = readBuiltinRadius(*collectTable, context + ", collect");
    if(const auto plantTable = luaData.getOptional<kaguya::LuaTable>("plant"))
        plantRadius = readBuiltinRadius(*plantTable, context + ", plant");
    if(const auto mineTable = luaData.getOptional<kaguya::LuaTable>("mine"))
    {
        CheckedLuaTable mineData(*mineTable);
        mineResource = mineData.getOrThrow<std::string>("resource");
        mineData.checkUnused();
    }
    if(const auto militaryTable = luaData.getOptional<kaguya::LuaTable>("military"))
        military = parseMilitary(*militaryTable, context);
    if(const auto warehouseTable = luaData.getOptional<kaguya::LuaTable>("warehouse"))
    {
        CheckedLuaTable warehouseData(*warehouseTable);
        const auto capacity = warehouseData.getOptional<double>("capacity");
        if(!capacity || *capacity != std::floor(*capacity) || *capacity < 1 || *capacity > 100000)
            throw GameDataError(context + ": warehouse capacity must be a whole number in [1, 100000]");
        warehouseCapacity = static_cast<unsigned>(*capacity);
        warehouseData.checkUnused();
    }
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
