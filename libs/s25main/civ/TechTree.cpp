// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/TechTree.h"
#include "RttrConfig.h"
#include "civ/Identifiers.h"
#include "files.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "lua/GameDataLoader.h"
#include "gameData/BuildingProperties.h"
#include "gameData/GameConsts.h"
#include "gameData/WorldDescription.h"
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

unsigned Technology::GetTotalKnowledgeCost() const
{
    unsigned result = 0;
    for(const int points : knowledgeCost)
        result += static_cast<unsigned>(points);
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

bool Byproduct::AppliesTo(GoodType product) const
{
    // A byproduct is never produced for itself (e.g. when the worker carries a waiting byproduct out)
    if(product == ware)
        return false;
    return perProducts.empty() || helpers::contains(perProducts, product);
}

TechTree::TechTree(const WorldDescription& desc)
{
    content_ = std::make_shared<const Content>(desc);
    std::fill(lockedAtStart_.begin(), lockedAtStart_.end(), false);
    std::fill(buildingKnowledge_.begin(), buildingKnowledge_.end(), KnowledgePoints{});
    for(const auto& entry : desc.buildingKnowledge)
    {
        const auto bld = content_->FindBuilding(entry.first, true);
        if(!bld)
            throw GameDataError("Knowledge points defined for an invalid building '" + entry.first + "'");
        for(const auto type : helpers::enumRange<KnowledgeType>())
            buildingKnowledge_[*bld][type] = entry.second[static_cast<unsigned>(type)];
    }
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
            const auto good = content_->FindGood(entry.first);
            if(!good)
                throw GameDataError("Technology '" + tech.name + "' has an invalid cost good '" + entry.first + "'");
            tech.cost.push_back(TechCost{*good, entry.second});
        }
        for(const auto type : helpers::enumRange<KnowledgeType>())
            tech.knowledgeCost[type] = techDesc.knowledge[static_cast<unsigned>(type)];
        std::sort(tech.cost.begin(), tech.cost.end(),
                  [](const TechCost& lhs, const TechCost& rhs) { return lhs.good < rhs.good; });
        tech.minResearchGFs = static_cast<unsigned>(duration_to_gfs(std::chrono::seconds(techDesc.time)));
        for(const std::string& bldName : techDesc.unlockedBuildings)
        {
            const auto bld = content_->FindBuilding(bldName);
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

    // Byproducts (after the technologies, as they refer to them)
    for(const ByproductDesc& bpDesc : desc.byproducts)
    {
        const std::string context = "Byproduct of '" + bpDesc.building + "'";
        const auto bld = content_->FindBuilding(bpDesc.building);
        if(!bld)
            throw GameDataError(context + ": not a building");
        if(!BuildingProperties::IsUsual(*bld) && !isCustomBuilding(*bld))
            throw GameDataError(context + ": only production buildings can have byproducts");
        Byproduct byproduct;
        const std::vector<GoodType> wares = content_->FindWares(bpDesc.ware);
        if(wares.size() != 1u)
        {
            throw GameDataError(context + ": the ware must match exactly 1 ware, but matches "
                                + std::to_string(wares.size()));
        }
        byproduct.ware = wares.front();
        byproduct.amount = bpDesc.amount;
        if(!bpDesc.per.empty())
        {
            byproduct.perProducts = content_->FindWares(bpDesc.per);
            if(byproduct.perProducts.empty())
                throw GameDataError(context + ": 'per' matches no ware");
        }
        if(!bpDesc.technology.empty())
        {
            byproduct.technology = find(bpDesc.technology);
            if(!byproduct.technology)
                throw GameDataError(context + ": unknown technology '" + bpDesc.technology + "'");
        }
        unsigned totalAmount = byproduct.amount;
        for(const Byproduct& other : byproducts_[*bld])
            totalAmount += other.amount;
        if(totalAmount > 7u)
            throw GameDataError(context + ": at most 7 byproducts per building (a flag holds at most 8 wares)");
        byproducts_[*bld].push_back(std::move(byproduct));
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
