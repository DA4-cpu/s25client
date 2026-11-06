// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "gameTypes/Resource.h"
#include <array>
#include <cstdint>
#include <optional>

/// How the civilization mode stores its data in S2 map files (map objects, ores and the climate of generated maps).
/// Only values which the S2 format does not use are taken.
namespace civ {

// ---------------------------------------------------------------------------------------------------------------------
// Map objects (rttr:AddMapObject): object type 0xD0 + (index % 32), object info = size (1..16) + 32 * (index / 32).
// Maps of the first version (at most 16 objects, info = size) load unchanged.
// ---------------------------------------------------------------------------------------------------------------------

/// Object type in map files for the first map object
constexpr uint8_t FIRST_CUSTOM_MAP_OBJECT_TYPE = 0xD0;
/// Number of object types used for map objects (0xD0..0xEF)
constexpr unsigned NUM_CUSTOM_MAP_OBJECT_TYPES = 32;
/// Maximum number of sizes of a map object (lower 5 bits of the object info)
constexpr unsigned MAX_MAP_OBJECT_SIZES = 16;
/// Maximum number of map objects defined in lua (3 upper bits of the object info)
constexpr unsigned MAX_CUSTOM_MAP_OBJECTS = NUM_CUSTOM_MAP_OBJECT_TYPES * 8;

struct MapObjectCode
{
    uint8_t type, info;
};
/// Code of the map object with the given index and size (1..16)
constexpr MapObjectCode encodeMapObject(unsigned idx, unsigned size)
{
    return MapObjectCode{static_cast<uint8_t>(FIRST_CUSTOM_MAP_OBJECT_TYPE + idx % NUM_CUSTOM_MAP_OBJECT_TYPES),
                         static_cast<uint8_t>(size + 32u * (idx / NUM_CUSTOM_MAP_OBJECT_TYPES))};
}
struct DecodedMapObject
{
    unsigned idx;
    /// 1..16
    unsigned size;
};
/// True if the object type belongs to the map objects of the civilization mode
constexpr bool isCustomMapObjectType(uint8_t type)
{
    return type >= FIRST_CUSTOM_MAP_OBJECT_TYPE && type < FIRST_CUSTOM_MAP_OBJECT_TYPE + NUM_CUSTOM_MAP_OBJECT_TYPES;
}
/// Index and size of a map object or nullopt if the code is no (valid) map object
constexpr std::optional<DecodedMapObject> decodeMapObject(uint8_t type, uint8_t info)
{
    if(!isCustomMapObjectType(type))
        return std::nullopt;
    const unsigned size = info & 0x1Fu;
    if(size == 0 || size > MAX_MAP_OBJECT_SIZES)
        return std::nullopt;
    return DecodedMapObject{(type - FIRST_CUSTOM_MAP_OBJECT_TYPE) + NUM_CUSTOM_MAP_OBJECT_TYPES * (info >> 5u), size};
}

// ---------------------------------------------------------------------------------------------------------------------
// Ores (rttr:AddOre): resource byte = code of the ore + amount (1..7), like coal (0x40 + amount) etc.
// ---------------------------------------------------------------------------------------------------------------------

/// Resource bytes of the ores (each + amount 1..7). Values the S2 format does not use
constexpr std::array<uint8_t, NUM_CUSTOM_ORES> ORE_MAP_CODES = {
  0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8,
  0xF0, 0xF8, 0x60, 0x68, 0x70, 0x78, 0x28, 0x30, 0x38, 0x08, 0x10, 0x18,
};
/// Maximum amount of an ore in map files (like the built-in ores)
constexpr unsigned MAX_ORE_MAP_AMOUNT = 7;

constexpr uint8_t encodeOre(unsigned idx, unsigned amount)
{
    return static_cast<uint8_t>(ORE_MAP_CODES[idx] + amount);
}
struct DecodedOre
{
    unsigned idx;
    /// 1..7
    unsigned amount;
};
/// Ore index and amount of a resource byte or nullopt if it is no ore of the civilization mode
constexpr std::optional<DecodedOre> decodeOre(uint8_t value)
{
    const unsigned amount = value & 0x07u;
    if(amount == 0)
        return std::nullopt;
    const uint8_t base = value & 0xF8u;
    for(unsigned i = 0; i < ORE_MAP_CODES.size(); ++i)
    {
        if(ORE_MAP_CODES[i] == base)
            return DecodedOre{i, amount};
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------------------------------------------------
// Climate of generated maps in the layers S2 does not use (always 0 in layer 7, 7 in layer 9, 0/1 in layer 10):
// layer 7 = CLIMATE_MARKER | temperature, layer 9 = humidity, layer 10 = WaterBits. Without the marker in layer 7 the
// map has no climate.
// ---------------------------------------------------------------------------------------------------------------------

constexpr uint8_t CLIMATE_MARKER = 0x80;
/// Bits of layer 10: The water at the node is fresh water (lake or river) / the node is on the course of a river
enum WaterBits : uint8_t
{
    WATER_FRESH = 1,
    WATER_RIVER = 2
};

} // namespace civ
