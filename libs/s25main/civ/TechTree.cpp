// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/TechTree.h"
#include "civ/Identifiers.h"
#include "RttrConfig.h"
#include "files.h"
#include "helpers/containerUtils.h"
#include "gameData/GameConsts.h"
#include "gameData/WorldDescription.h"
#include "lua/GameDataLoader.h"
#include <algorithm>
#include <chrono>

namespace civ {

unsigned Technology::GetTotalCost() const
{
    unsigned result = 0;
    for(const TechCost& c : cost)
        result += c.amount;
    return result;
}

std::optional<unsigned> Technology::GetCostIdx(GoodType good) const
{
    for(unsigned i = 0; i < cost.size(); ++i)
    {
        if(cost[i].good == good)
            return i;
    }
    return std::nullopt;
}

TechTree::TechTree(const WorldDescription& desc)
{
    std::fill(lockedAtStart_.begin(), lockedAtStart_.end(), false);
    techs_.reserve(desc.technologies.size());
    for(const TechnologyDesc& techDesc : desc.technologies)
    {
        Technology tech;
        tech.name = techDesc.name;
        tech.title = techDesc.title;
        tech.description = techDesc.description;
        tech.era = techDesc.era;
        for(const std::string& req : techDesc.requirements)
        {
            const auto reqIdx = find(req);
            // Checked by the loader already
            if(!reqIdx)
                throw GameDataError("Technology '" + tech.name + "' requires unknown technology '" + req + "'");
            tech.requirements.push_back(*reqIdx);
        }
        for(const auto& entry : techDesc.cost)
        {
            const auto good = goodFromIdentifier(entry.first);
            if(!good)
                throw GameDataError("Technology '" + tech.name + "' has an invalid cost good '" + entry.first + "'");
            tech.cost.push_back(TechCost{*good, entry.second});
        }
        std::sort(tech.cost.begin(), tech.cost.end(),
                  [](const TechCost& lhs, const TechCost& rhs) { return lhs.good < rhs.good; });
        tech.minResearchGFs = static_cast<unsigned>(duration_to_gfs(std::chrono::seconds(techDesc.time)));
        for(const std::string& bldName : techDesc.unlockedBuildings)
        {
            const auto bld = buildingFromIdentifier(bldName);
            if(!bld)
                throw GameDataError("Technology '" + tech.name + "' unlocks an invalid building '" + bldName + "'");
            if(!helpers::contains(tech.unlockedBuildings, *bld))
                tech.unlockedBuildings.push_back(*bld);
            lockedAtStart_[*bld] = true;
        }
        tech.exclusive = techDesc.exclusive;
        if(!techDesc.requiredAddon.empty())
        {
            for(const AddonId id : rttrEnum::values<AddonId>)
            {
                if(rttrEnum::toString(id) == techDesc.requiredAddon)
                    tech.requiredAddon = id;
            }
            if(!tech.requiredAddon)
                throw GameDataError("Technology '" + tech.name + "' requires an invalid addon '"
                                    + techDesc.requiredAddon + "'");
        }
        tech.aiPriority = techDesc.aiPriority;
        techs_.push_back(std::move(tech));
    }
}

std::optional<TechIdx> TechTree::find(const std::string& name) const
{
    for(TechIdx i = 0; i < techs_.size(); ++i)
    {
        if(techs_[i].name == name)
            return i;
    }
    return std::nullopt;
}

std::vector<TechIdx> TechTree::GetTechsUnlocking(BuildingType bld) const
{
    std::vector<TechIdx> result;
    for(TechIdx i = 0; i < techs_.size(); ++i)
    {
        if(helpers::contains(techs_[i].unlockedBuildings, bld))
            result.push_back(i);
    }
    return result;
}

TechTree loadTechTree(const boost::filesystem::path& folder)
{
    WorldDescription desc;
    GameDataLoader loader(desc, folder);
    if(!loader.Load())
        throw GameDataLoadError("Could not load the tech tree from " + folder.string());
    return TechTree(desc);
}

TechTree loadDefaultTechTree()
{
    return loadTechTree(RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "civ");
}

} // namespace civ
