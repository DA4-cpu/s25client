// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DescIdx.h"
#include <array>
#include <map>
#include <string>
#include <vector>

struct WorldDescription;
class CheckedLuaTable;
struct TechnologyDesc;
namespace kaguya {
class LuaTable;
}

/// Knowledge points of the civilization mode per kind in the order agriculture (LWP), industry (IWP), military (MWP)
using KnowledgeValues = std::array<int, 3>;
/// Names of the knowledge kinds in lua, in the order of KnowledgeValues
extern const std::array<const char*, 3> KNOWLEDGE_LUA_NAMES;
/// Read knowledge points from a lua table like { LWP = 2, IWP = 1 } (missing kinds are 0).
/// Throws GameDataError for unknown kinds or (if not allowed) negative values. `context` is used in error messages.
KnowledgeValues parseKnowledge(const kaguya::LuaTable& table, bool allowNegative, const std::string& context);

/// The tech tree may become large (technologies + cultures + wonders) so allow more than 254 entries
template<>
struct DescIdxTraits<TechnologyDesc>
{
    using value_type = uint16_t;
};

/// Raw description of a technology of the tech tree used by the civilization mode.
/// Buildings and goods are stored by their identifier (enum name) and resolved by the game (civ::TechTree)
struct TechnologyDesc
{
    /// Unique identifier, used for prerequisites and savegames
    std::string name;
    /// Displayed name and description
    std::string title, description;
    /// Era/level of the technology (0 = neolithic, 6 = contemporary)
    unsigned era = 0;
    /// Technologies which have to be researched first. Must be defined before this one
    std::vector<std::string> requirements;
    /// Costs: Good identifier -> amount
    std::map<std::string, unsigned> cost;
    /// Knowledge points required (and spent on completion), never negative
    KnowledgeValues knowledge{};
    /// Minimum research duration in seconds (counted from the start of the research)
    unsigned time = 0;
    /// Buildings (identifiers) enabled by this technology
    std::vector<std::string> unlockedBuildings;
    /// Only one player can get this technology (cultures, wonders)
    bool exclusive = false;
    /// Name of an addon (AddonId, e.g. "WINE") which must be enabled for this technology to be available
    std::string requiredAddon;
    /// Higher priority means the AI will research it earlier
    int aiPriority = 0;

    TechnologyDesc() = default;
    TechnologyDesc(CheckedLuaTable luaData, const WorldDescription& worldDesc);
};
