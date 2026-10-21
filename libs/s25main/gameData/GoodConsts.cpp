// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GoodConsts.h"
#include "civ/Content.h"
#include "mygettext/mygettext.h"
#include <array>

namespace {
const std::array<std::string, NUM_BUILTIN_GOODS> builtinWareNames = {{
  /*  0 */ gettext_noop("Beer"),         // Bier
  /*  1 */ gettext_noop("Tongs"),        // Zange
  /*  2 */ gettext_noop("Hammer"),       // Hammer
  /*  3 */ gettext_noop("Axe"),          // Axt
  /*  4 */ gettext_noop("Saw"),          // Säge
  /*  5 */ gettext_noop("Pick-axe"),     // Spitzhacke
  /*  6 */ gettext_noop("Shovel"),       // Schaufel
  /*  7 */ gettext_noop("Crucible"),     // Schmelztiegel
  /*  8 */ gettext_noop("Rod and line"), // Angel
  /*  9 */ gettext_noop("Scythe"),       // Sense
  /* 10 */ gettext_noop("Water"),        // Wasser
  /* 11 */ gettext_noop("Water"),        // Wasser
  /* 12 */ gettext_noop("Cleaver"),      // Beil
  /* 13 */ gettext_noop("Rolling pin"),  // Nudelholz
  /* 14 */ gettext_noop("Bow"),          // Bogen
  /* 15 */ gettext_noop("Boat"),         // Boot
  /* 16 */ gettext_noop("Sword"),        // Schwert
  /* 17 */ gettext_noop("Iron"),         // Eisen
  /* 18 */ gettext_noop("Flour"),        // Mehl
  /* 19 */ gettext_noop("Fish"),         // Fisch
  /* 20 */ gettext_noop("Bread"),        // Brot
  /* 21 */ gettext_noop("Shield"),       // Schild
  /* 22 */ gettext_noop("Wood"),         // Holz
  /* 23 */ gettext_noop("Boards"),       // Bretter
  /* 24 */ gettext_noop("Stones"),       // Steine
  /* 25 */ "",                           // Schild
  /* 26 */ "",                           // Schild
  /* 27 */ gettext_noop("Grain"),        // Getreide
  /* 28 */ gettext_noop("Coins"),        // Mnzen
  /* 29 */ gettext_noop("Gold"),         // Gold
  /* 30 */ gettext_noop("Iron ore"),     // Eisenerz
  /* 31 */ gettext_noop("Coal"),         // Kohle
  /* 32 */ gettext_noop("Meat"),         // Fleisch
  /* 33 */ gettext_noop("Ham"),          // Schinken ( Schwein )
  /* 34 */ "",                           // Schild
  /* 35 */ gettext_noop("Grapes"),
  /* 36 */ gettext_noop("Wine"),
  /* 37 */ gettext_noop("Skins"),
  /* 38 */ gettext_noop("Leather"),
  /* 39 */ gettext_noop("Armor"),
}};
} // namespace

const WareNamesTable WARE_NAMES{};

const std::string& WareNamesTable::operator[](GoodType good) const
{
    static const std::string empty;
    if(isCustomGood(good))
    {
        const civ::CustomWare* ware = civ::getActiveContent().GetWare(good);
        return ware ? ware->title : empty;
    }
    if(good == GoodType::Nothing)
        return empty;
    return builtinWareNames[static_cast<unsigned>(good)];
}
