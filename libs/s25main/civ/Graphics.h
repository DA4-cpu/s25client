// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DrawPoint.h"
#include "gameTypes/Resource.h"

namespace civ {

/// Multiply two colors (0xAARRGGBB) channel by channel
unsigned multiplyColors(unsigned color1, unsigned color2);

/// Size of the granite pile (0..5) used as placeholder for the given size (0..sizes-1) of a map object
unsigned getPlaceholderGraniteState(unsigned state, unsigned sizes);

/// Draw a map object defined in lua (noDeposit) of the given size (0 = smallest). Uses the own graphics of the object
/// if there are any, otherwise the tinted placeholder (granite pile). colorMod is multiplied (e.g. fog of war)
void drawMapObject(unsigned objIdx, unsigned state, DrawPoint drawPt, unsigned colorMod = 0xFFFFFFFF);

/// Image of the sign of the built-in ore in the map graphics for the amount index (0 = little .. 2 = much)
unsigned getOreSignImage(ResourceType look, unsigned amountIdx);

/// Draw the sign of the geologist for an ore defined in lua: Own image or the sign of the look ore, tinted.
/// amountIdx: 0 = little, 1 = medium, 2 = much. colorMod: e.g. the alpha of a disappearing sign
void drawOreSign(unsigned oreIdx, unsigned amountIdx, DrawPoint drawPt, unsigned colorMod = 0xFFFFFFFF);

} // namespace civ
