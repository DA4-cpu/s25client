// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "gameTypes/MapCoordinates.h"
#include "gameData/DescIdx.h"
#include "gameData/LandscapeDesc.h"
#include <string>

namespace rttr::mapGenerator {

enum class MapStyle
{
    Water,
    Land,
    Mixed
};
constexpr auto maxEnumValue(MapStyle)
{
    return MapStyle::Mixed;
}

enum class MountainDistance : uint8_t
{
    Close = 5,
    Normal = 15,
    Far = 25,
    VeryFar = 30
};

enum class IslandAmount : uint8_t
{
    Few = 0,
    Normal = 10,
    Many = 30
};

/// Overall temperature bias for the map generator's climate system (see rttr::mapGenerator::Climate.h).
/// "Temperate" reproduces the original, unbiased behaviour (full [0, 100] value range); the other
/// presets progressively shift and narrow that range towards the cold or hot end.
enum class ClimateTemperature : uint8_t
{
    Cold,
    Cool,
    Temperate,
    Warm,
    Hot
};
constexpr auto maxEnumValue(ClimateTemperature)
{
    return ClimateTemperature::Hot;
}

/// Overall humidity bias for the map generator's climate system (see rttr::mapGenerator::Climate.h).
/// "Temperate" (=moderate) reproduces the original, unbiased behaviour (full [0, 100] value range);
/// the other presets shift that range towards dry or humid.
enum class ClimateHumidity : uint8_t
{
    Humid,
    Temperate,
    Dry
};
constexpr auto maxEnumValue(ClimateHumidity)
{
    return ClimateHumidity::Dry;
}

struct MapSettings
{
    void MakeValid();

    std::string name, author;
    unsigned numPlayers = 2;
    MapExtent size = MapExtent::all(128);
    unsigned short ratioGold = 9;
    unsigned short ratioIron = 36;
    unsigned short ratioCoal = 40;
    unsigned short ratioGranite = 15;
    unsigned short rivers = 15;
    unsigned short trees = 40;
    unsigned short stonePiles = 5;
    IslandAmount islands = IslandAmount::Few;
    MountainDistance mountainDistance = MountainDistance::Normal;
    // Index 3 is the "world" landscape (data/RTTR/gamedata/world/world.lua), which is registered
    // after the original greenland(0)/wasteland(1)/winterworld(2) and combines all of their
    // terrains under one landscape for the climate system. This mirrors how the previous default
    // of 0 only "worked" by relying on greenland being registered first - iwMapGenerator's
    // constructor re-resolves this by name at runtime as well, so this default only matters if a
    // map is generated without ever opening the map generator settings dialog.
    DescIdx<LandscapeDesc> type = DescIdx<LandscapeDesc>(3);
    MapStyle style = MapStyle::Mixed;
    ClimateTemperature climateTemperature = ClimateTemperature::Temperate;
    ClimateHumidity climateHumidity = ClimateHumidity::Temperate;
    /// Approximate width/height (in nodes) of a single climate zone (see GenerateClimateMap()).
    /// Smaller values produce many small zones, larger values produce few large, near-uniform zones.
    /// Unlike the two presets above this is exposed as a fine-grained slider in the UI rather than a
    /// small fixed set of options.
    unsigned short climateZoneSize = 40;
};

} // namespace rttr::mapGenerator
