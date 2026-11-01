// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/Graphics.h"
#include "Loader.h"
#include "civ/Content.h"
#include "civ/CustomGraphics.h"
#include "ogl/glSmartBitmap.h"
#include "gameTypes/FlagType.h"
#include <algorithm>

namespace civ {

unsigned multiplyColors(unsigned color1, unsigned color2)
{
    unsigned result = 0;
    for(unsigned shift = 0; shift < 32; shift += 8)
    {
        const unsigned c1 = (color1 >> shift) & 0xFFu;
        const unsigned c2 = (color2 >> shift) & 0xFFu;
        result |= ((c1 * c2 + 127u) / 255u) << shift;
    }
    return result;
}

unsigned getPlaceholderGraniteState(unsigned state, unsigned sizes)
{
    const unsigned numGraniteSizes = 6;
    unsigned graniteState = state;
    if(sizes > numGraniteSizes)
        graniteState = state * numGraniteSizes / sizes;
    return std::min(graniteState, numGraniteSizes - 1);
}

void drawMapObject(unsigned objIdx, unsigned state, DrawPoint drawPt, unsigned colorMod)
{
    // Own graphics (assets/civ/<identifier>/Groesse<state>.png)
    if(glSmartBitmap* sprite = LOADER.GetCivGraphics().GetMapObject(objIdx, state))
    {
        sprite->draw(drawPt, colorMod);
        return;
    }
    const CustomMapObject* obj = getActiveContent().GetMapObject(objIdx);
    if(!obj)
        return;
    // Placeholder: granite pile of a similar size, tinted
    const auto type = obj->lookGranite == 1 ? GraniteType::Two : GraniteType::One;
    LOADER.granite_cache[type][getPlaceholderGraniteState(state, obj->sizes)].draw(
      drawPt, multiplyColors(0xFF000000u | obj->tint, colorMod));
}

} // namespace civ
