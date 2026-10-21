// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/Knowledge.h"
#include "GamePlayer.h"
#include "civ/PlayerResearch.h"
#include "civ/TechTree.h"
#include "helpers/EnumRange.h"
#include "world/GameWorld.h"
#include "gameTypes/BuildingCount.h"
#include <cstdlib>

namespace civ {

namespace {
    const helpers::EnumArray<const char*, KnowledgeType> shortNames = {{"LWP", "IWP", "MWP"}};
} // namespace

const char* getShortName(KnowledgeType type)
{
    return shortNames[type];
}

std::optional<KnowledgeType> knowledgeTypeFromShortName(const std::string& name)
{
    for(const auto type : helpers::enumRange<KnowledgeType>())
    {
        if(name == shortNames[type])
            return type;
    }
    return std::nullopt;
}

KnowledgePoints toTenths(const KnowledgePoints& points)
{
    KnowledgePoints result;
    for(const auto type : helpers::enumRange<KnowledgeType>())
        result[type] = points[type] * KNOWLEDGE_SCALE;
    return result;
}

std::string formatKnowledge(int tenths)
{
    const int absTenths = std::abs(tenths);
    std::string result = (tenths < 0) ? "-" : "";
    result += std::to_string(absTenths / KNOWLEDGE_SCALE);
    if(absTenths % KNOWLEDGE_SCALE != 0)
        result += "," + std::to_string(absTenths % KNOWLEDGE_SCALE);
    return result;
}

KnowledgePoints calcTotalKnowledge(const GamePlayer& player)
{
    KnowledgePoints result{};
    const TechTree& techTree = player.GetGameWorld().GetTechTree();
    const BuildingCount buildingNums = player.GetBuildingRegister().GetBuildingNums();
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        const unsigned numBuildings = buildingNums.buildings[bld];
        if(numBuildings == 0)
            continue;
        const KnowledgePoints& bldKnowledge = techTree.GetBuildingKnowledge(bld);
        for(const auto type : helpers::enumRange<KnowledgeType>())
            result[type] += static_cast<int>(numBuildings) * bldKnowledge[type] * KNOWLEDGE_SCALE;
    }
    return result;
}

KnowledgePoints calcAvailableKnowledge(const GamePlayer& player)
{
    KnowledgePoints result = calcTotalKnowledge(player);
    const KnowledgePoints& spent = player.GetResearch().GetSpentKnowledge();
    for(const auto type : helpers::enumRange<KnowledgeType>())
        result[type] -= spent[type];
    return result;
}

} // namespace civ
