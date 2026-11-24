// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "BuildingQuality.h"
#include "GoodTypes.h"
#include "JobTypes.h"
#include "Point.h"
#include "helpers/OptionalEnum.h"
#include "variant.h"
#include <cassert>
#include <string>
#include <vector>

/// Original construction costs (boards and stones)
struct BuildingCost
{
    uint8_t boards;
    uint8_t stones;
};

/// One kind of construction material of a building: `amount` wares, each of them any of `wares` (civilization mode:
/// all wares matching the tags). The builder uses the materials in the order of the list
struct CostEntry
{
    /// Tag query (civilization mode) or the identifier of the ware
    std::vector<std::string> tags;
    /// Wares that can be used, never empty
    std::vector<GoodType> wares;
    uint8_t amount = 0;
};
/// Construction costs of a building
using BuildingCosts = std::vector<CostEntry>;

class WaresNeeded
{
    GoodType elems_[3];
    unsigned size_;

public:
    constexpr WaresNeeded() : elems_{}, size_(0) {}
    constexpr explicit WaresNeeded(GoodType good1) : elems_{good1}, size_(1) {}
    constexpr WaresNeeded(GoodType good1, GoodType good2) : elems_{good1, good2}, size_(2) {}
    constexpr WaresNeeded(GoodType good1, GoodType good2, GoodType good3) : elems_{good1, good2, good3}, size_(3) {}
    constexpr unsigned size() const { return size_; }
    constexpr bool empty() const { return size_ == 0u; }
    constexpr GoodType operator[](unsigned i) const
    {
        assert(i < size_);
        return elems_[i];
    }
    constexpr const GoodType* begin() const { return elems_; }
    constexpr const GoodType* end() const { return elems_ + size_; }
};

/// One input of a building: a place for wares. Built-in buildings have one per ware they need, workshops of the
/// civilization mode one per entry of their inputs (a tag query, any matching ware can be delivered, also mixed)
struct InputSlot
{
    /// Wares that can be delivered to it, never empty
    std::vector<GoodType> wares;
    /// Wares used per work step in hundredths: 100 = 1 per step, 50 = 1 every 2nd step, 300 = 3 per step
    uint16_t amount = 100;
    /// How many wares it holds (in the building and on their way)
    uint8_t storage = 6;
};

/// Describes the work the building does
struct BldWorkDescription
{
    /// Worker belonging to the building, if any
    helpers::OptionalEnum<Job> job = std::nullopt;
    /// Ware produced, if any
    boost_variant2<GoodType, Job, std::nullopt_t> producedWare = std::nullopt;
    // Required for use in aggregate initialization
    // NOLINTBEGIN(readability-redundant-member-init)
    /// Built-in buildings: wares the building needs, if any (`inputs` is made from them)
    WaresNeeded waresNeeded = {};
    // NOLINTEND(readability-redundant-member-init)
    /// Built-in buildings: how many wares of each type can be stored
    uint8_t numSpacesPerWare = 6;
    /// True: a work step needs and uses the wares of every input (InputSlot::amount)
    /// False (built-in mines): a work step needs a ware in any input and uses one of the input with the most wares
    bool useOneWareEach = true;
    /// Inputs of the building (BLD_WORK_DESC fills them for built-in buildings from waresNeeded)
    std::vector<InputSlot> inputs = {};
};

/// Smoke definition for buildings
struct SmokeConst
{
    constexpr SmokeConst() = default;
    constexpr SmokeConst(uint8_t type, const Point<int8_t>& offset) : type(type), offset(offset) {}
    /// Smoke type (1-4), 0 = no smoke
    uint8_t type = 0;
    /// Position of the smoke relative to the buildings origin
    Point<int8_t> offset = {0, 0};
};
