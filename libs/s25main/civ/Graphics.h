// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DrawPoint.h"

namespace civ {

/// Multiply two colors (0xAARRGGBB) channel by channel
unsigned multiplyColors(unsigned color1, unsigned color2);

/// Size of the granite pile (0..5) used as placeholder for the given size (0..sizes-1) of a map object
unsigned getPlaceholderGraniteState(unsigned state, unsigned sizes);

/// Draw a map object defined in lua (noDeposit) of the given size (0 = smallest). Uses the own graphics of the object
/// if there are any, otherwise the tinted placeholder (granite pile). colorMod is multiplied (e.g. fog of war)
void drawMapObject(unsigned objIdx, unsigned state, DrawPoint drawPt, unsigned colorMod = 0xFFFFFFFF);

} // namespace civ
