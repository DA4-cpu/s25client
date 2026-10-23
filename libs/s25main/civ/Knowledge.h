// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "helpers/EnumArray.h"
#include <cstdint>
#include <optional>
#include <string>

class GamePlayer;

namespace civ {

/// Kinds of knowledge points (WP) of the civilization mode
enum class KnowledgeType : uint8_t
{
    Agriculture, ///< LWP
    Industry,    ///< IWP
    Military     ///< MWP
};
constexpr auto maxEnumValue(KnowledgeType)
{
    return KnowledgeType::Military;
}

/// Knowledge points per kind. The tech tree stores whole points, calculations use tenths (KNOWLEDGE_SCALE)
using KnowledgePoints = helpers::EnumArray<int, KnowledgeType>;

/// Calculations are done in tenths of points so media stored in warehouses can count half
constexpr int KNOWLEDGE_SCALE = 10;
/// Tenths per knowledge point of a medium stored in a warehouse (storehouse, HQ, harbor): counts half
constexpr int MEDIA_IN_WAREHOUSE_TENTHS = 5;
/// Tenths per knowledge point of a medium stored in a library: counts full
constexpr int MEDIA_IN_LIBRARY_TENTHS = 10;

/// Short name as used in lua and the UI: "LWP", "IWP", "MWP"
const char* getShortName(KnowledgeType type);
std::optional<KnowledgeType> knowledgeTypeFromShortName(const std::string& name);

/// Convert whole points to tenths
KnowledgePoints toTenths(const KnowledgePoints& points);
/// Format tenths with at most one decimal digit: 75 -> "7,5", 70 -> "7", -50 -> "-5"
std::string formatKnowledge(int tenths);

/// Total knowledge of the player in tenths: Points of all finished buildings plus the stored knowledge media
/// (half in warehouses, full in libraries).
/// Only meaningful in the civilization mode (uses the tech tree of the world)
KnowledgePoints calcTotalKnowledge(const GamePlayer& player);
/// Total knowledge minus the knowledge spent on finished research (tenths). May be negative
KnowledgePoints calcAvailableKnowledge(const GamePlayer& player);

} // namespace civ
