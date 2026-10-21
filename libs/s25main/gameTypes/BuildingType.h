// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>

enum class BuildingType : uint8_t
{
    Headquarters,   // 0
    Barracks,       // 1
    Guardhouse,     // 2
    Skinner,        // 3
    Watchtower,     // 4
    Vineyard,       // 5
    Winery,         // 6
    Temple,         // 7
    Tannery,        // 8
    Fortress,       // 9
    GraniteMine,    // 10
    CoalMine,       // 11
    IronMine,       // 12
    GoldMine,       // 13
    LookoutTower,   // 14
    LeatherWorks,   // 15
    Catapult,       // 16
    Woodcutter,     // 17
    Fishery,        // 18
    Quarry,         // 19
    Forester,       // 20
    Slaughterhouse, // 21
    Hunter,         // 22
    Brewery,        // 23
    Armory,         // 24
    Metalworks,     // 25
    Ironsmelter,    // 26
    Charburner,     // 27
    PigFarm,        // 28
    Storehouse,     // 29
    Nothing9,       // 30
    Mill,           // 31
    Bakery,         // 32
    Sawmill,        // 33
    Mint,           // 34
    Well,           // 35
    Shipyard,       // 36
    Farm,           // 37
    DonkeyBreeder,  // 38
    HarborBuilding, // 39
    // Buildings defined in lua by the civilization mode (see civ/Content.h). Unused unless defined.
    Custom0,                   // 40
    CustomLast = Custom0 + 31, // 71
};

constexpr auto maxEnumValue(BuildingType)
{
    return BuildingType::CustomLast;
}

/// Number of building slots which can be defined in lua (civilization mode)
constexpr unsigned NUM_CUSTOM_BUILDINGS =
  static_cast<unsigned>(BuildingType::CustomLast) - static_cast<unsigned>(BuildingType::Custom0) + 1u;
/// Number of built-in building types (before the custom slots). Used e.g. for loading older savegames
constexpr unsigned NUM_BUILTIN_BUILDINGS = static_cast<unsigned>(BuildingType::Custom0);
constexpr bool isCustomBuilding(BuildingType bld)
{
    return bld >= BuildingType::Custom0 && bld <= BuildingType::CustomLast;
}
/// Index of a custom building slot (0..NUM_CUSTOM_BUILDINGS-1)
constexpr unsigned getCustomBuildingIdx(BuildingType bld)
{
    return static_cast<unsigned>(bld) - static_cast<unsigned>(BuildingType::Custom0);
}
constexpr BuildingType getCustomBuilding(unsigned idx)
{
    return BuildingType(static_cast<unsigned>(BuildingType::Custom0) + idx);
}

/// Number of NOTHING entries (currently unused buildings)
const unsigned NUM_UNUSED_BLD_TYPES = 1;
