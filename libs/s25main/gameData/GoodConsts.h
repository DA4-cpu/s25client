// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "gameTypes/GoodTypes.h"
#include <string>

/// Names of the wares. Custom wares (civilization mode): Title from lua, empty if the slot is unused
struct WareNamesTable
{
    const std::string& operator[](GoodType good) const;
};
extern const WareNamesTable WARE_NAMES;
