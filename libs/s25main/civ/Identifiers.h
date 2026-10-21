// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include <optional>
#include <string>

/// Mapping between enum values and the identifiers used in the Lua game data of the civilization mode
namespace civ {

/// Identifier of a building type as used in Lua (name of the enum value, e.g. "Farm")
const char* getIdentifier(BuildingType bld);
/// Identifier of a good as used in Lua (name of the enum value, e.g. "Boards"). Shields use "Shield"
const char* getIdentifier(GoodType good);

/// Building type for the identifier or nullopt if there is none (or it cannot be used, like the HQ).
/// The HQ can't be unlocked by technologies but has knowledge points -> allowHeadquarters
std::optional<BuildingType> buildingFromIdentifier(const std::string& identifier, bool allowHeadquarters = false);
/// Good type for the identifier or nullopt if there is none or it cannot be stored (e.g. empty water buckets)
std::optional<GoodType> goodFromIdentifier(const std::string& identifier);

} // namespace civ
