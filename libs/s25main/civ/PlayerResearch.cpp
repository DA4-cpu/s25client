// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/PlayerResearch.h"
#include "SerializedGameData.h"
#include "helpers/serializeEnums.h"
#include "s25util/Log.h"
#include <algorithm>

namespace civ {

unsigned PlayerResearch::GetNumResearched() const
{
    return static_cast<unsigned>(std::count(researched_.begin(), researched_.end(), true));
}

const PlayerResearch::Progress* PlayerResearch::GetProgress(TechIdx tech) const
{
    const auto it = progress_.find(tech);
    return it == progress_.end() ? nullptr : &it->second;
}

PlayerResearch::Progress& PlayerResearch::GetOrCreateProgress(const TechTree& techTree, TechIdx tech)
{
    Progress& progress = progress_[tech];
    progress.paid.resize(techTree.get(tech).cost.size(), 0u);
    return progress;
}

std::optional<PlayerResearch::Progress> PlayerResearch::RemoveProgress(TechIdx tech)
{
    const auto it = progress_.find(tech);
    if(it == progress_.end())
        return std::nullopt;
    Progress result = std::move(it->second);
    progress_.erase(it);
    return result;
}

void PlayerResearch::SetResearched(TechIdx tech)
{
    if(researched_.size() <= tech)
        researched_.resize(tech + 1, false);
    researched_[tech] = true;
    progress_.erase(tech);
    if(current_ == tech)
        current_.reset();
}

// Technologies are stored by name and costs by good type so that changes of the (Lua) tech tree
// don't break existing savegames. Unknown technologies are dropped when loading.
void PlayerResearch::Serialize(SerializedGameData& sgd, const TechTree& techTree) const
{
    std::vector<TechIdx> researchedTechs;
    for(TechIdx i = 0; i < researched_.size(); ++i)
    {
        if(researched_[i])
            researchedTechs.push_back(i);
    }
    sgd.PushUnsignedInt(static_cast<unsigned>(researchedTechs.size()));
    for(const TechIdx tech : researchedTechs)
        sgd.PushString(techTree.get(tech).name);

    sgd.PushString(current_ ? techTree.get(*current_).name : std::string());

    sgd.PushUnsignedInt(static_cast<unsigned>(progress_.size()));
    for(const auto& entry : progress_)
    {
        const Technology& tech = techTree.get(entry.first);
        sgd.PushString(tech.name);
        sgd.PushUnsignedInt(entry.second.researchedGFs);
        sgd.PushUnsignedInt(static_cast<unsigned>(tech.cost.size()));
        for(unsigned i = 0; i < tech.cost.size(); ++i)
        {
            helpers::pushEnum<uint8_t>(sgd, tech.cost[i].good);
            sgd.PushUnsignedInt(entry.second.paid.at(i));
        }
    }
}

void PlayerResearch::Deserialize(SerializedGameData& sgd, const TechTree& techTree)
{
    researched_.clear();
    current_.reset();
    progress_.clear();

    const unsigned numResearched = sgd.PopUnsignedInt();
    for(unsigned i = 0; i < numResearched; ++i)
    {
        const std::string name = sgd.PopString();
        if(const auto tech = techTree.find(name))
            SetResearched(*tech);
        else
            LOG.write("Savegame: Ignoring unknown researched technology '%1%'\n") % name;
    }

    const std::string currentName = sgd.PopString();
    if(!currentName.empty())
        current_ = techTree.find(currentName);

    const unsigned numProgress = sgd.PopUnsignedInt();
    for(unsigned i = 0; i < numProgress; ++i)
    {
        const std::string name = sgd.PopString();
        const unsigned researchedGFs = sgd.PopUnsignedInt();
        const unsigned numCosts = sgd.PopUnsignedInt();
        const auto tech = techTree.find(name);
        Progress* progress = nullptr;
        if(tech && !IsResearched(*tech))
        {
            progress = &GetOrCreateProgress(techTree, *tech);
            progress->researchedGFs = researchedGFs;
        }
        for(unsigned j = 0; j < numCosts; ++j)
        {
            const auto good = helpers::popEnum<GoodType>(sgd);
            const unsigned paid = sgd.PopUnsignedInt();
            if(!progress)
                continue;
            // Cost entries may have changed -> match by good and never store more than needed
            if(const auto costIdx = techTree.get(*tech).GetCostIdx(good))
                progress->paid[*costIdx] = std::min(paid, techTree.get(*tech).cost[*costIdx].amount);
        }
    }
    if(current_ && IsResearched(*current_))
        current_.reset();
}

} // namespace civ
