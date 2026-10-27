// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MapGenDesc.h"
#include "WorldDescription.h"
#include "helpers/containerUtils.h"
#include "lua/CheckedLuaTable.h"
#include <kaguya/kaguya.hpp>
#include <algorithm>
#include <map>

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

std::string readName(CheckedLuaTable& luaData, const std::string& what)
{
    const auto name = luaData.getOrThrow<std::string>("name");
    if(!isValidIdentifier(name))
    {
        throw GameDataError(what + " '" + name
                            + "': name may only contain letters A-Z, digits and underscores (no umlauts or spaces)");
    }
    return name;
}

MapGenRange readRange(CheckedLuaTable& luaData, const std::string& field, const std::string& context)
{
    MapGenRange result;
    const auto values = luaData.getOptional<std::vector<unsigned>>(field);
    if(!values)
        return result;
    if(values->size() != 2)
        throw GameDataError(context + ": " + field + " must be a range like {20, 80}");
    if((*values)[0] > 100 || (*values)[1] > 100 || (*values)[0] > (*values)[1])
        throw GameDataError(context + ": " + field + " must be a range in [0, 100] with the smaller value first");
    result.minimum = static_cast<uint8_t>((*values)[0]);
    result.maximum = static_cast<uint8_t>((*values)[1]);
    return result;
}

MapGenClimate readClimate(CheckedLuaTable& luaData, const std::string& context)
{
    MapGenClimate result;
    result.temperature = readRange(luaData, "temperature", context);
    result.humidity = readRange(luaData, "humidity", context);
    result.height = readRange(luaData, "height", context);
    return result;
}

/// Read a table like { Oak = 3, Birch = 1 }. Sorted by name, so the result does not depend on the lua table order
MapGenWeights readWeights(CheckedLuaTable& luaData, const std::string& field, const std::vector<std::string>& valid,
                          const std::string& context)
{
    MapGenWeights result;
    const auto values = luaData.getOrDefault(field, std::map<std::string, unsigned>());
    for(const auto& entry : values)
    {
        if(!helpers::contains(valid, entry.first))
        {
            std::string validNames;
            for(const std::string& name : valid)
                validNames += (validNames.empty() ? "" : ", ") + name;
            throw GameDataError(context + ": unknown entry '" + entry.first + "' in " + field
                                + ". Possible: " + validNames);
        }
        if(entry.second > 0)
            result.emplace_back(entry.first, entry.second);
    }
    return result;
}

std::string sliderList()
{
    std::string result;
    for(const std::string& name : getMapGenSliderNames())
        result += (result.empty() ? "\"" : ", \"") + name + "\"";
    return result;
}

/// Iterate over the entries of a lua list ({ {...}, {...} }) in order
template<class T_Func>
void forEachListEntry(const kaguya::LuaTable& list, const std::string& context, T_Func&& func)
{
    const size_t numEntries = list.size();
    for(size_t i = 1; i <= numEntries; ++i)
    {
        const kaguya::LuaRef entry = list[i];
        if(entry.type() != kaguya::LuaRef::TYPE_TABLE)
            throw GameDataError(context + ": entry " + std::to_string(i) + " must be a table { ... }");
        func(entry.get<kaguya::LuaTable>());
    }
}
} // namespace

const std::vector<std::string>& getMapGenTerrainNames()
{
    static const std::vector<std::string> names{"land",        "mountain",   "freshwater",  "saltwater",
                                                "river",       "shore_fresh", "shore_salt", "shore_river",
                                                "near_fresh",  "near_salt",  "near_river"};
    return names;
}

const std::vector<std::string>& getMapGenObjectNames()
{
    static const std::vector<std::string> names{"trees", "stones", "animals", "decoration", "fish",
                                                "groundwater", "coal", "iron", "gold", "granite"};
    return names;
}

const std::vector<std::string>& getMapGenSliderNames()
{
    static const std::vector<std::string> names{"forest", "stonePiles", "coal", "iron", "gold", "granite"};
    return names;
}

const std::vector<std::string>& getMapGenTreeNames()
{
    static const std::vector<std::string> names{"Pine",      "Birch",   "Oak",    "Palm1", "Palm2",
                                                "Pineapple", "Cypress", "Cherry", "Fir"};
    return names;
}

const std::vector<std::string>& getMapGenAnimalNames()
{
    static const std::vector<std::string> names{"Rabbit", "Fox", "Stag", "Deer", "Duck", "Sheep"};
    return names;
}

MapZoneDesc::MapZoneDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    name = readName(luaData, "Map zone");
    const std::string context = "Map zone '" + name + "'";
    title = luaData.getOrDefault("title", name);
    const unsigned colorValue = luaData.getOrDefault("color", 0u);
    if(colorValue > 0xFFFFFF)
        throw GameDataError(context + ": color must be 0xRRGGBB");
    color = colorValue;
    if(const auto packageList = luaData.getOptional<kaguya::LuaTable>("packages"))
    {
        forEachListEntry(*packageList, context + ", packages", [&](const kaguya::LuaTable& entry) {
            CheckedLuaTable packageData(entry);
            MapZonePackageDesc package;
            package.name = packageData.getOrDefault("name", std::string());
            const std::string packageContext =
              context + ", package " + (package.name.empty() ? std::to_string(packages.size() + 1) : package.name);
            package.climate = readClimate(packageData, packageContext);
            package.trees = readWeights(packageData, "trees", getMapGenTreeNames(), packageContext);
            package.animals = readWeights(packageData, "animals", getMapGenAnimalNames(), packageContext);
            packageData.checkUnused();
            packages.push_back(package);
        });
    }
    luaData.checkUnused();
}

MapResourceDesc::MapResourceDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    name = readName(luaData, "Map resource");
    const std::string context = "Map resource '" + name + "'";
    luaData.getOrThrow(object, "object");
    if(!helpers::contains(getMapGenObjectNames(), object))
    {
        std::string validNames;
        for(const std::string& curName : getMapGenObjectNames())
            validNames += (validNames.empty() ? "" : ", ") + curName;
        throw GameDataError(context + ": unknown object '" + object + "'. Possible: " + validNames);
    }

    const auto amountValue = luaData.getOptional<kaguya::LuaRef>("amount");
    if(!amountValue)
        throw GameDataError(context + ": amount is missing");
    if(amountValue->type() == kaguya::LuaRef::TYPE_STRING)
    {
        amountSlider = amountValue->get<std::string>();
        if(!helpers::contains(getMapGenSliderNames(), amountSlider))
            throw GameDataError(context + ": amount must be a number (percent) or the name of a slider: " + sliderList());
    } else if(amountValue->type() == kaguya::LuaRef::TYPE_NUMBER)
    {
        amount = amountValue->get<double>();
        if(amount < 0 || amount > 100)
            throw GameDataError(context + ": amount must be in [0, 100] (percent)");
    } else
        throw GameDataError(context + ": amount must be a number (percent) or the name of a slider: " + sliderList());

    climate = readClimate(luaData, context);
    zones = luaData.getOrDefault("zones", std::vector<std::string>());

    const auto terrainNames = luaData.getOrThrow<std::vector<std::string>>("terrain");
    if(terrainNames.empty())
        throw GameDataError(context + ": terrain must contain at least one entry");
    const auto& validTerrains = getMapGenTerrainNames();
    for(const std::string& terrainName : terrainNames)
    {
        const auto it = std::find(validTerrains.begin(), validTerrains.end(), terrainName);
        if(it == validTerrains.end())
        {
            std::string validNames;
            for(const std::string& curName : validTerrains)
                validNames += (validNames.empty() ? "" : ", ") + curName;
            throw GameDataError(context + ": unknown terrain '" + terrainName + "'. Possible: " + validNames);
        }
        terrain |= static_cast<MapGenTerrainFlags>(1u << (it - validTerrains.begin()));
    }

    cluster = luaData.getOrDefault("cluster", 0u);
    if(cluster > 100)
        throw GameDataError(context + ": cluster must be in [0, 100] (nodes)");
    density = luaData.getOrDefault("density", 100u);
    if(density == 0 || density > 100)
        throw GameDataError(context + ": density must be in [1, 100] (percent)");

    if(const auto weightList = luaData.getOptional<kaguya::LuaTable>("weights"))
    {
        forEachListEntry(*weightList, context + ", weights", [&](const kaguya::LuaTable& entry) {
            CheckedLuaTable weightData(entry);
            MapResourceWeight weight;
            weight.climate = readClimate(weightData, context + ", weights");
            weightData.getOrThrow(weight.factor, "factor");
            if(weight.factor < 0 || weight.factor > 100)
                throw GameDataError(context + ", weights: factor must be in [0, 100]");
            weightData.checkUnused();
            weights.push_back(weight);
        });
    }

    if(const auto hqValues = luaData.getOptional<std::vector<unsigned>>("hqDistance"))
    {
        if(hqValues->size() != 2 || (*hqValues)[0] > (*hqValues)[1])
            throw GameDataError(context + ": hqDistance must be a range like {0, 15} (nodes, smaller value first)");
        hqDistance = std::make_pair((*hqValues)[0], (*hqValues)[1]);
    }
    luaData.checkUnused();
}

double MapResourceDesc::getWeight(unsigned temperature, unsigned humidity, unsigned height) const
{
    for(const MapResourceWeight& weight : weights)
    {
        if(weight.climate.matches(temperature, humidity, height))
            return weight.factor;
    }
    return 1.;
}

MapClimateDesc::MapClimateDesc(const kaguya::LuaTable& luaTable)
{
    CheckedLuaTable luaData(luaTable);
    const std::string context = "Map climate (rttr:SetMapClimate)";
    heightCooling = luaData.getOrDefault("heightCooling", heightCooling);
    if(heightCooling > 100)
        throw GameDataError(context + ": heightCooling must be in [0, 100]");
    nearWaterDistance = luaData.getOrDefault("nearWaterDistance", nearWaterDistance);
    if(nearWaterDistance == 0 || nearWaterDistance > 30)
        throw GameDataError(context + ": nearWaterDistance must be in [1, 30] (nodes)");
    lakeMaxSize = luaData.getOrDefault("lakeMaxSize", lakeMaxSize);
    if(lakeMaxSize > 1000000)
        throw GameDataError(context + ": lakeMaxSize must be in [0, 1000000] (nodes)");
    waterHumidity = luaData.getOrDefault("waterHumidity", waterHumidity);
    if(waterHumidity > 100)
        throw GameDataError(context + ": waterHumidity must be in [0, 100]");
    previewImages = luaData.getOrDefault("previewImages", previewImages);
    temperatureSpread = luaData.getOrDefault("temperatureSpread", temperatureSpread);
    humiditySpread = luaData.getOrDefault("humiditySpread", humiditySpread);
    if(temperatureSpread < 0 || temperatureSpread > 100 || humiditySpread < 0 || humiditySpread > 100)
        throw GameDataError(context + ": temperatureSpread and humiditySpread must be in [0, 100]");
    riverMinHeight = luaData.getOrDefault("riverMinHeight", riverMinHeight);
    if(riverMinHeight > 100)
        throw GameDataError(context + ": riverMinHeight must be in [0, 100] (percent)");
    riverMinLength = luaData.getOrDefault("riverMinLength", riverMinLength);
    if(riverMinLength > 500)
        throw GameDataError(context + ": riverMinLength must be in [0, 500] (steps)");
    mountainBuildSpacing = luaData.getOrDefault("mountainBuildSpacing", mountainBuildSpacing);
    if(mountainBuildSpacing == 1 || mountainBuildSpacing > 50)
        throw GameDataError(context + ": mountainBuildSpacing must be 0 (none) or in [2, 50] (nodes)");
    luaData.checkUnused();
}

int MapGenDesc::getZoneIndex(const std::string& name) const
{
    for(unsigned i = 0; i < zones.size(); i++)
    {
        if(zones[i].name == name)
            return static_cast<int>(i);
    }
    return -1;
}
