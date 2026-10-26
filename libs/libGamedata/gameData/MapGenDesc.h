// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kaguya {
class LuaTable;
} // namespace kaguya

/// Data of the random map generator ("Welt"): climate, zones with plants and animals, resource rules.
/// Read from lua (data/RTTR/gamedata/world/mapgen.lua), used by rttr::mapGenerator.

/// Inclusive range [minimum, maximum] of a value in percent (0..100)
struct MapGenRange
{
    uint8_t minimum = 0;
    uint8_t maximum = 100;
    bool contains(unsigned value) const { return value >= minimum && value <= maximum; }
};

/// Climate condition. All values are percentages:
/// temperature 0 = coldest, 100 = hottest; humidity 0 = driest, 100 = wettest;
/// height 0 = sea level, 100 = highest point of the map
struct MapGenClimate
{
    MapGenRange temperature, humidity, height;
    bool matches(unsigned temperatureValue, unsigned humidityValue, unsigned heightValue) const
    {
        return temperature.contains(temperatureValue) && humidity.contains(humidityValue)
               && height.contains(heightValue);
    }
};

/// Weighted list of names (tree species or animal species)
using MapGenWeights = std::vector<std::pair<std::string, unsigned>>;

/// Plants and animals of a zone for one climate
struct MapZonePackageDesc
{
    std::string name;
    MapGenClimate climate;
    MapGenWeights trees;
    MapGenWeights animals;
};

/// Geographic zone ("Nordamerika", "Afrika" ...) as read from lua (rttr:AddMapZone)
struct MapZoneDesc
{
    /// Identifier
    std::string name;
    /// Displayed name
    std::string title;
    /// Color in the preview image (0xRRGGBB)
    uint32_t color = 0;
    /// Packages in order of definition. The first package whose climate matches is used
    std::vector<MapZonePackageDesc> packages;

    MapZoneDesc() = default;
    explicit MapZoneDesc(const kaguya::LuaTable& luaData);
};

/// Terrain types a resource rule can be restricted to (combined as bit flags)
enum class MapGenTerrain : uint16_t
{
    Land = 1 << 0,
    Mountain = 1 << 1,
    Freshwater = 1 << 2,
    Saltwater = 1 << 3,
    River = 1 << 4,
    ShoreFresh = 1 << 5,
    ShoreSalt = 1 << 6,
    ShoreRiver = 1 << 7,
    NearFresh = 1 << 8,
    NearSalt = 1 << 9,
    NearRiver = 1 << 10,
};
using MapGenTerrainFlags = uint16_t;
constexpr MapGenTerrainFlags toFlag(MapGenTerrain terrain)
{
    return static_cast<MapGenTerrainFlags>(terrain);
}

/// Lua names of the terrain types in the order of the flags
const std::vector<std::string>& getMapGenTerrainNames();
/// Objects a resource rule can place
const std::vector<std::string>& getMapGenObjectNames();
/// Sliders of the map generator window a resource rule can take its amount from
const std::vector<std::string>& getMapGenSliderNames();
/// Names of the tree species usable in zone packages
const std::vector<std::string>& getMapGenTreeNames();
/// Names of the animal species usable in zone packages
const std::vector<std::string>& getMapGenAnimalNames();

/// Factor for the amount of a resource in a part of the climate
struct MapResourceWeight
{
    MapGenClimate climate;
    double factor = 1.;
};

/// Rule for distributing a resource on generated maps as read from lua (rttr:AddMapResource)
struct MapResourceDesc
{
    std::string name;
    /// What is placed, see getMapGenObjectNames()
    std::string object;
    /// Percentage of the matching nodes that get the object
    double amount = 0.;
    /// Instead of amount: take the amount from a slider of the map generator window (see getMapGenSliderNames())
    std::string amountSlider;
    /// Hard climate limits
    MapGenClimate climate;
    /// Zones (identifiers) the rule applies to. Empty: all zones
    std::vector<std::string> zones;
    /// Allowed terrain (bit flags of MapGenTerrain)
    MapGenTerrainFlags terrain = 0;
    /// Approximate size of a cluster in nodes. 0: scattered randomly
    unsigned cluster = 0;
    /// Percentage of the nodes inside a cluster that get the object
    unsigned density = 100;
    /// Factors for parts of the climate, the first matching one is used (otherwise 1)
    std::vector<MapResourceWeight> weights;
    /// Allowed distance to the nearest headquarters in nodes [minimum, maximum]
    std::optional<std::pair<unsigned, unsigned>> hqDistance;

    MapResourceDesc() = default;
    explicit MapResourceDesc(const kaguya::LuaTable& luaData);
    /// Factor for the given climate according to weights
    double getWeight(unsigned temperature, unsigned humidity, unsigned height) const;
};

/// General climate settings as read from lua (rttr:SetMapClimate)
struct MapClimateDesc
{
    /// Temperature points subtracted at the highest point of the map (linear with height)
    unsigned heightCooling = 0;
    /// Maximum distance in nodes for the terrain types "near_fresh", "near_salt" and "near_river"
    unsigned nearWaterDistance = 4;
    /// Bodies of water up to this size (percent of the map) are fresh water, larger ones salt water
    double freshwaterMaxSize = 2.;
    /// Save preview images (zones, climate) next to the generated map
    bool previewImages = true;

    MapClimateDesc() = default;
    explicit MapClimateDesc(const kaguya::LuaTable& luaData);
};

struct MapGenDesc
{
    MapClimateDesc climate;
    bool climateSet = false;
    std::vector<MapZoneDesc> zones;
    std::vector<MapResourceDesc> resources;

    /// Index of the zone with the given identifier or -1
    int getZoneIndex(const std::string& name) const;
};
