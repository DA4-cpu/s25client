// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "civ/TechTree.h"
#include <map>
#include <optional>
#include <vector>

class SerializedGameData;

namespace civ {

/// Research state of a single player in the civilization mode
class PlayerResearch
{
public:
    /// Progress of a technology which was (partially) paid but not yet completed
    struct Progress
    {
        /// Amount paid for each cost entry of the technology (same order as Technology::cost)
        std::vector<unsigned> paid;
        /// Game frames spent researching this technology
        unsigned researchedGFs = 0;
    };

    bool IsResearched(TechIdx tech) const { return tech < researched_.size() && researched_[tech]; }
    unsigned GetNumResearched() const;
    const std::optional<TechIdx>& GetCurrent() const { return current_; }
    /// Progress of the technology or nullptr if nothing was paid yet
    const Progress* GetProgress(TechIdx tech) const;

    void SetCurrent(std::optional<TechIdx> tech) { current_ = tech; }
    /// Get progress of the technology, creating it if required
    Progress& GetOrCreateProgress(const TechTree& techTree, TechIdx tech);
    /// Remove the progress of the technology and return what was paid
    std::optional<Progress> RemoveProgress(TechIdx tech);
    /// Mark the technology as researched. Removes its progress and resets current research if it was this tech
    void SetResearched(TechIdx tech);

    void Serialize(SerializedGameData& sgd, const TechTree& techTree) const;
    void Deserialize(SerializedGameData& sgd, const TechTree& techTree);

private:
    std::vector<bool> researched_;
    std::optional<TechIdx> current_;
    std::map<TechIdx, Progress> progress_;
};

} // namespace civ
