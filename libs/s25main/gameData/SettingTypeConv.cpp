// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "SettingTypeConv.h"
#include "RTTR_Assert.h"
#include "civ/Content.h"
#include "helpers/EnumRange.h"
#include <stdexcept>

/// Max value for each setting.
/// Note: We skip the first 2 steppings for the occupation (last 4 values) as they had no effect in S2
const MilitarySettings SUPPRESS_UNUSED MILITARY_SETTINGS_SCALE = {{10, 5, 5, 5, 8, 8, 8, 8}};
const TransportPriorities BUILTIN_STD_TRANSPORT_PRIO = {{2,  13, 13, 13, 13, 13, 13, 13, 13, 13, 11, 11, 13, 13,
                                                         13, 14, 1,  3,  12, 12, 12, 1,  10, 8,  9,  1,  1,  12,
                                                         0,  4,  5,  6,  12, 12, 1,  12, 12, 7,  7,  1}};

const StdTransportPrioTable STD_TRANSPORT_PRIO{};

uint8_t StdTransportPrioTable::operator[](GoodType good) const
{
    return BUILTIN_STD_TRANSPORT_PRIO[civ::getTransportLike(good)];
}

unsigned GetTransportPrioFromOrdering(const TransportOrders& ordering, GoodType good)
{
    // The priority is the index into the ordering -> 0 = highest, first entry
    for(unsigned prio = 0; prio < ordering.size(); ++prio)
    {
        // The entry values are the standard priorities which makes it simple to use groups
        // of goods, that always have the same priority
        if(ordering[prio] == STD_TRANSPORT_PRIO[good])
            return prio;
    }
    RTTR_Assert(false);
    throw std::logic_error("Invalid ordering or ware");
}

TransportOrders GetOrderingFromTransportPrio(const TransportPriorities& priorities)
{
    TransportOrders result;
    // Map prio of each ware to STD prio (custom wares have no own priority)
    for(const auto ware : helpers::EnumRange<GoodType>{})
    {
        if(isCustomGood(ware))
            continue;
        RTTR_Assert(priorities[ware] < result.size());
        result[priorities[ware]] = STD_TRANSPORT_PRIO[ware];
    }
    return result;
}
