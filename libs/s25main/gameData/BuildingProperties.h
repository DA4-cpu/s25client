// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "gameTypes/BuildingType.h"
#include <vector>

/// Static class to query properties of buildings (building types)
class BuildingProperties
{
public:
    /// Static class only!
    BuildingProperties() = delete;

    /// All military buildings of the running game (built-in and defined in lua), from the fewest soldiers to the most
    /// (civ::Content::GetMilitaryBuildings)
    static const std::vector<BuildingType>& GetMilitaryBuildings();

    /// True iff the building type is used (not nothing)
    static bool IsValid(BuildingType bld);
    /// True iff this is a regular military building (also one defined in lua with military = { ... })
    static bool IsMilitary(BuildingType bld);
    /// True iff this is a mine
    static bool IsMine(BuildingType bld);
    /// True iff wares can be stored in this building
    static bool IsWareHouse(BuildingType bld);
    /// True iff this is a "usual" building, i.e. production.
    static bool IsUsual(BuildingType bld);
};
