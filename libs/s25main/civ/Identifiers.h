// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include "gameTypes/JobTypes.h"
#include <optional>
#include <string>

namespace civ {

/// Identifier as used in lua. Custom slots return the name from the active content (or "Custom<n>" if unused)
std::string getIdentifier(BuildingType bld);
std::string getIdentifier(GoodType good);
std::string getIdentifier(Job job);

/// Built-in building type for the identifier or nullopt if there is none (or it cannot be used, like the HQ).
/// The HQ can't be unlocked by technologies but has knowledge points -> allowHeadquarters
std::optional<BuildingType> buildingFromIdentifier(const std::string& identifier, bool allowHeadquarters = false);
/// Built-in ware for the identifier (nation specific shields are all "Shield", empty water is not usable)
std::optional<GoodType> goodFromIdentifier(const std::string& identifier);
/// Built-in job for the identifier
std::optional<Job> jobFromIdentifier(const std::string& identifier);

} // namespace civ
