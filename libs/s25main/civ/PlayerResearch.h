// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "civ/Knowledge.h"
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

    /// Knowledge spent on finished research (tenths of points)
    const KnowledgePoints& GetSpentKnowledge() const { return spentKnowledge_; }
    /// Book knowledge (tenths of points) spent on finished research
    void AddSpentKnowledge(const KnowledgePoints& tenths);

    /// Weights 0..10 per knowledge type of the regulator "Wissen": Which medium a workshop with
    /// outputChoice = "knowledge" makes. Default (from lua) until the player changes it
    using KnowledgeDistribution = helpers::EnumArray<uint8_t, KnowledgeType>;
    KnowledgeDistribution GetKnowledgeDistribution(const TechTree& techTree) const;
    void SetKnowledgeDistribution(const KnowledgeDistribution& distribution);

    void Serialize(SerializedGameData& sgd, const TechTree& techTree) const;
    void Deserialize(SerializedGameData& sgd, const TechTree& techTree);

private:
    std::vector<bool> researched_;
    std::optional<TechIdx> current_;
    std::map<TechIdx, Progress> progress_;
    KnowledgePoints spentKnowledge_{};
    /// Set by the player, otherwise the default of the tech tree is used
    std::optional<KnowledgeDistribution> knowledgeDistribution_;
};

} // namespace civ
