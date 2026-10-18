// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "addons/const_addons.h"
#include "helpers/EnumArray.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include <boost/filesystem/path.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct WorldDescription;

namespace civ {

/// Index of a technology in the TechTree
using TechIdx = unsigned;

struct TechCost
{
    GoodType good;
    unsigned amount;
};

/// A technology with all identifiers resolved to game types
struct Technology
{
    std::string name, title, description;
    unsigned era = 0;
    std::vector<TechIdx> requirements;
    /// Costs, sorted by good type
    std::vector<TechCost> cost;
    /// Minimum research time in game frames
    unsigned minResearchGFs = 0;
    std::vector<BuildingType> unlockedBuildings;
    bool exclusive = false;
    /// Addon which must be enabled for this technology to be available at all
    std::optional<AddonId> requiredAddon;
    int aiPriority = 0;

    unsigned GetTotalCost() const;
    /// Index of the cost entry for the good or nullopt if the technology doesn't cost this good
    std::optional<unsigned> GetCostIdx(GoodType good) const;
};

/// The tech tree of the civilization mode. Created from the WorldDescription (Lua game data)
class TechTree
{
public:
    /// Resolve the raw descriptions. Throws GameDataError for unknown buildings or goods
    explicit TechTree(const WorldDescription& desc);

    unsigned size() const { return static_cast<unsigned>(techs_.size()); }
    bool empty() const { return techs_.empty(); }
    const Technology& get(TechIdx idx) const { return techs_.at(idx); }
    const std::vector<Technology>& GetTechs() const { return techs_; }
    std::optional<TechIdx> find(const std::string& name) const;

    /// True if the building is unlocked by any technology and hence not available at the start
    bool IsLockedAtStart(BuildingType bld) const { return lockedAtStart_[bld]; }
    /// Technologies which unlock the given building
    std::vector<TechIdx> GetTechsUnlocking(BuildingType bld) const;

private:
    std::vector<Technology> techs_;
    helpers::EnumArray<bool, BuildingType> lockedAtStart_;
};

/// Load the tech tree from the Lua file `default.lua` in the given folder. Throws on error
TechTree loadTechTree(const boost::filesystem::path& folder);
/// Load the tech tree from the game data folder (RTTR/gamedata/civ). Throws on error
TechTree loadDefaultTechTree();

} // namespace civ
