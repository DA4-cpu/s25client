// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "AddonBool.h"
#include "mygettext/mygettext.h"

/**
 *  Civilization mode: Buildings have to be unlocked via a tech tree, research costs wares
 */
class AddonCivilization : public AddonBool
{
public:
    AddonCivilization()
        : AddonBool(AddonId::CIVILIZATION, AddonGroup::GamePlay | AddonGroup::Economy,
                    _("Zivilisationsmodus (Technologiebaum)"),
                    _("Nur die Grundwirtschaft ist zu Beginn verfügbar. Alle weiteren Gebäude werden über den "
                      "Technologiebaum erforscht (Hauptauswahl -> Forschung). Forschung verbraucht Waren aus den "
                      "Lagerhäusern. Exklusive Technologien (z. B. Kulturen) kann nur der Spieler bekommen, der sie "
                      "zuerst erforscht."))
    {}
};
