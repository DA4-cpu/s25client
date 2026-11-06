// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "helpers/MaxEnumValue.h"
#include <cstdint>

enum class ResourceType : uint8_t
{
    Nothing,
    Iron,
    Gold,
    Coal,
    Granite,
    Water,
    Fish,
    /// Ores defined in lua (civilization mode, rttr:AddOre), in order of definition
    Custom0,
    CustomLast = Custom0 + 23
};
constexpr auto maxEnumValue(ResourceType)
{
    return ResourceType::CustomLast;
}

/// Number of ores that can be defined in lua (civilization mode)
constexpr unsigned NUM_CUSTOM_ORES =
  static_cast<unsigned>(ResourceType::CustomLast) + 1u - static_cast<unsigned>(ResourceType::Custom0);
constexpr bool isCustomOre(ResourceType type)
{
    return type >= ResourceType::Custom0;
}
constexpr ResourceType getCustomOre(unsigned idx)
{
    return ResourceType(static_cast<unsigned>(ResourceType::Custom0) + idx);
}
constexpr unsigned getCustomOreIdx(ResourceType type)
{
    return static_cast<unsigned>(type) - static_cast<unsigned>(ResourceType::Custom0);
}

/// Holds a resource and its value.
/// Maximum amount is 15!
/// It makes sure that Type == Nothing => Amount == 0, but not vice versa!
/// Also: A value of 0 means Nothing|0
class Resource
{
    /// Type in the upper byte, amount in the lower 4 bits
    uint16_t value_;

public:
    constexpr Resource(ResourceType type, uint8_t amount);
    constexpr explicit Resource(uint16_t value = 0);
    /// Value of savegames before the ores of the civilization mode (type in the upper 4 bits, amount in the lower 4)
    static constexpr Resource fromLegacyValue(uint8_t value);
    constexpr explicit operator uint16_t() const { return value_; }
    constexpr uint16_t getValue() const { return value_; }
    constexpr ResourceType getType() const { return ResourceType(value_ >> 8); }
    constexpr uint8_t getAmount() const { return value_ & 0x0F; }
    constexpr void setType(ResourceType newType);
    constexpr void setAmount(uint8_t newAmount);
    /// True if we have a non-zero amount of the given resource. Always false for Nothing
    constexpr bool has(ResourceType type) const { return getAmount() > 0u && getType() == type; }
    constexpr bool operator==(Resource rhs) const noexcept { return value_ == rhs.value_; }
    constexpr bool operator!=(Resource rhs) const noexcept { return !(*this == rhs); }
};

constexpr Resource::Resource(ResourceType type, uint8_t amount)
    : value_(
      (type == ResourceType::Nothing) ? 0 : static_cast<uint16_t>((static_cast<unsigned>(type) << 8) | (amount & 0x0F)))
{}
constexpr Resource::Resource(uint16_t value) : value_(value & 0xFF0F)
{
    const unsigned type = value >> 8u;
    if(type > static_cast<unsigned>(helpers::MaxEnumValue_v<ResourceType>)
       || type == static_cast<unsigned>(ResourceType::Nothing))
        value_ = 0;
}
constexpr Resource Resource::fromLegacyValue(uint8_t value)
{
    const unsigned type = value >> 4u;
    if(type > static_cast<unsigned>(ResourceType::Fish))
        return Resource();
    return Resource(ResourceType(type), value & 0x0F);
}
constexpr void Resource::setType(ResourceType newType)
{
    if(newType == ResourceType::Nothing)
        value_ = 0;
    else
        value_ = static_cast<uint16_t>((static_cast<unsigned>(newType) << 8) | getAmount());
}
constexpr void Resource::setAmount(uint8_t newAmount)
{
    if(getType() != ResourceType::Nothing)
        value_ = static_cast<uint16_t>((value_ & 0xFF00) | (newAmount & 0x0F));
}
