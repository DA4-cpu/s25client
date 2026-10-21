// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include "gameTypes/JobTypes.h"

namespace civ {

/// Built-in type whose graphics are used for the given type (identity for built-in types).
/// Wares, buildings and jobs defined in lua (civilization mode) use placeholder graphics of a built-in type.
GoodType getLook(GoodType good);
BuildingType getLook(BuildingType bld);
Job getLook(Job job);

} // namespace civ
