// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/Research.h"
#include "EventManager.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "addons/const_addons.h"
#include "buildings/nobBaseWarehouse.h"
#include "civ/PlayerResearch.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "helpers/format.hpp"
#include "postSystem/PostMsg.h"
#include "world/GameWorld.h"
#include "gameTypes/Inventory.h"
#include "s25util/Log.h"
#include <algorithm>
#include <limits>
#include <memory>

namespace civ {

bool isEnabled(const GameWorldBase& world)
{
    return world.GetGGS().isEnabled(AddonId::CIVILIZATION);
}

std::optional<unsigned> getExclusiveOwner(const GameWorldBase& world, TechIdx tech)
{
    if(!world.GetTechTree().get(tech).exclusive)
        return std::nullopt;
    for(unsigned i = 0; i < world.GetNumPlayers(); ++i)
    {
        if(world.GetPlayer(i).GetResearch().IsResearched(tech))
            return i;
    }
    return std::nullopt;
}

TechState getTechState(const GameWorldBase& world, unsigned playerId, TechIdx tech)
{
    const PlayerResearch& research = world.GetPlayer(playerId).GetResearch();
    if(research.IsResearched(tech))
        return TechState::Researched;
    const Technology& technology = world.GetTechTree().get(tech);
    if(technology.requiredAddon && !world.GetGGS().isEnabled(*technology.requiredAddon))
        return TechState::Unavailable;
    if(getExclusiveOwner(world, tech))
        return TechState::TakenByOther;
    for(const TechIdx req : technology.requirements)
    {
        if(!research.IsResearched(req))
            return TechState::MissingRequirements;
    }
    if(research.GetCurrent() == tech)
        return TechState::InProgress;
    return TechState::Available;
}

bool canResearch(const GameWorldBase& world, unsigned playerId, TechIdx tech)
{
    const TechState state = getTechState(world, playerId, tech);
    return state == TechState::Available || state == TechState::InProgress;
}

void applyBuildingAvailability(GamePlayer& player)
{
    const TechTree& techTree = player.GetGameWorld().GetTechTree();
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(techTree.IsLockedAtStart(bld))
            player.DisableBuilding(bld);
    }
    const PlayerResearch& research = player.GetResearch();
    for(TechIdx tech = 0; tech < techTree.size(); ++tech)
    {
        if(!research.IsResearched(tech))
            continue;
        for(const BuildingType bld : techTree.get(tech).unlockedBuildings)
            player.EnableBuilding(bld);
    }
}

void startGame(GameWorld& world)
{
    if(!isEnabled(world))
        return;
    const Content& content = world.GetTechTree().GetContent();
    for(unsigned i = 0; i < world.GetNumPlayers(); ++i)
    {
        GamePlayer& player = world.GetPlayer(i);
        applyBuildingAvailability(player);
        // Workshops defined in lua consume their input wares
        for(unsigned bldIdx = 0; bldIdx < content.GetNumBuildings(); ++bldIdx)
        {
            const BuildingType bld = getCustomBuilding(bldIdx);
            for(const GoodType input : content.GetBuilding(bld)->workDesc.waresNeeded)
                player.AddWareClient(input, bld);
        }
    }
}

bool setResearch(GameWorld& world, unsigned playerId, std::optional<TechIdx> tech)
{
    if(!isEnabled(world))
        return false;
    PlayerResearch& research = world.GetPlayer(playerId).GetResearch();
    if(!tech)
    {
        research.SetCurrent(std::nullopt);
        return true;
    }
    if(*tech >= world.GetTechTree().size() || !canResearch(world, playerId, *tech))
        return false;
    research.SetCurrent(tech);
    return true;
}

namespace {
    /// Take up to `amount` wares from the warehouses of the player. Returns the number of wares taken
    unsigned takeWaresFromWarehouses(GamePlayer& player, GoodType good, unsigned amount)
    {
        unsigned taken = 0;
        for(nobBaseWarehouse* wh : player.GetBuildingRegister().GetStorehouses())
        {
            if(taken >= amount)
                break;
            taken += wh->TakeWares(good, amount - taken);
        }
        return taken;
    }

    /// Return the paid costs to the first warehouse of the player (if any)
    void refund(GamePlayer& player, const Technology& tech, const PlayerResearch::Progress& progress)
    {
        nobBaseWarehouse* wh = player.GetFirstWH();
        if(!wh)
            return;
        GoodCounts goods;
        for(unsigned i = 0; i < tech.cost.size(); ++i)
            goods[tech.cost[i].good] += progress.paid.at(i);
        wh->AddToInventory(goods, true);
    }

    void sendMessage(GameWorld& world, unsigned playerId, const std::string& text)
    {
        world.GetPlayer(playerId).SendPostMessage(
          std::make_unique<PostMsg>(world.GetEvMgr().GetCurrentGF(), text, PostCategory::Economy));
    }
} // namespace

void completeResearch(GameWorld& world, unsigned playerId, TechIdx tech)
{
    const TechTree& techTree = world.GetTechTree();
    const Technology& technology = techTree.get(tech);
    GamePlayer& player = world.GetPlayer(playerId);
    PlayerResearch& research = player.GetResearch();
    if(research.IsResearched(tech))
        return;
    research.AddSpentKnowledge(toTenths(technology.knowledgeCost));
    research.SetResearched(tech);
    for(const BuildingType bld : technology.unlockedBuildings)
        player.EnableBuilding(bld);
    sendMessage(world, playerId, helpers::format(_("Forschung abgeschlossen: %1%"), _(technology.title)));

    if(!technology.exclusive)
        return;
    // Nobody else can get this technology anymore
    for(unsigned i = 0; i < world.GetNumPlayers(); ++i)
    {
        if(i == playerId)
            continue;
        GamePlayer& otherPlayer = world.GetPlayer(i);
        PlayerResearch& otherResearch = otherPlayer.GetResearch();
        const bool wasResearching = otherResearch.GetCurrent() == tech;
        if(wasResearching)
            otherResearch.SetCurrent(std::nullopt);
        if(const auto progress = otherResearch.RemoveProgress(tech))
            refund(otherPlayer, technology, *progress);
        if(wasResearching)
        {
            sendMessage(world, i,
                        helpers::format(_("%1% hat \"%2%\" zuerst erforscht. Bereits bezahlte Waren wurden erstattet."),
                                        player.name, _(technology.title)));
        }
    }
}

void update(GameWorld& world, unsigned numGFs)
{
    if(!isEnabled(world))
        return;
    const TechTree& techTree = world.GetTechTree();
    for(unsigned playerId = 0; playerId < world.GetNumPlayers(); ++playerId)
    {
        GamePlayer& player = world.GetPlayer(playerId);
        if(!player.isUsed() || player.IsDefeated())
            continue;
        PlayerResearch& research = player.GetResearch();
        const std::optional<TechIdx> current = research.GetCurrent();
        if(!current)
            continue;
        if(!canResearch(world, playerId, *current))
        {
            // Should not happen (exclusive techs are handled on completion) but be safe
            research.SetCurrent(std::nullopt);
            continue;
        }
        const Technology& tech = techTree.get(*current);
        PlayerResearch::Progress& progress = research.GetOrCreateProgress(techTree, *current);
        progress.researchedGFs += numGFs;
        bool allPaid = true;
        for(unsigned i = 0; i < tech.cost.size(); ++i)
        {
            unsigned& paid = progress.paid[i];
            if(paid < tech.cost[i].amount)
                paid += takeWaresFromWarehouses(player, tech.cost[i].good, tech.cost[i].amount - paid);
            if(paid < tech.cost[i].amount)
                allPaid = false;
        }
        // Knowledge points are only checked (and spent) when everything else is done
        if(allPaid && progress.researchedGFs >= tech.minResearchGFs && hasEnoughKnowledge(world, playerId, *current))
            completeResearch(world, playerId, *current);
    }
}

KnowledgePoints getMissingKnowledge(const GameWorldBase& world, unsigned playerId, TechIdx tech)
{
    const KnowledgePoints cost = toTenths(world.GetTechTree().get(tech).knowledgeCost);
    KnowledgePoints missing{};
    if(std::all_of(cost.begin(), cost.end(), [](int points) { return points <= 0; }))
        return missing;
    const KnowledgePoints available = calcAvailableKnowledge(world.GetPlayer(playerId));
    for(const auto type : helpers::enumRange<KnowledgeType>())
    {
        if(cost[type] > 0 && available[type] < cost[type])
            missing[type] = cost[type] - available[type];
    }
    return missing;
}

bool hasEnoughKnowledge(const GameWorldBase& world, unsigned playerId, TechIdx tech)
{
    const KnowledgePoints missing = getMissingKnowledge(world, playerId, tech);
    return std::all_of(missing.begin(), missing.end(), [](int points) { return points == 0; });
}

bool isAffordable(const GameWorldBase& world, unsigned playerId, TechIdx tech)
{
    if(!hasEnoughKnowledge(world, playerId, tech))
        return false;
    const Technology& technology = world.GetTechTree().get(tech);
    const GamePlayer& player = world.GetPlayer(playerId);
    const PlayerResearch::Progress* progress = player.GetResearch().GetProgress(tech);
    for(unsigned i = 0; i < technology.cost.size(); ++i)
    {
        const unsigned paid = progress ? progress->paid.at(i) : 0u;
        if(paid >= technology.cost[i].amount)
            continue;
        unsigned stock = 0;
        for(const nobBaseWarehouse* wh : player.GetBuildingRegister().GetStorehouses())
            stock += wh->GetNumRealWares(technology.cost[i].good);
        if(stock < technology.cost[i].amount - paid)
            return false;
    }
    return true;
}

unsigned getAmountPaid(const GameWorldBase& world, unsigned playerId, TechIdx tech)
{
    const PlayerResearch::Progress* progress = world.GetPlayer(playerId).GetResearch().GetProgress(tech);
    unsigned result = 0;
    if(progress)
    {
        for(const unsigned paid : progress->paid)
            result += paid;
    }
    return result;
}

std::optional<TechIdx> chooseResearchForAI(const GameWorldBase& world, unsigned playerId,
                                           std::optional<TechIdx> exclude)
{
    const TechTree& techTree = world.GetTechTree();
    std::optional<TechIdx> best;
    bool bestAffordable = false;
    for(TechIdx tech = 0; tech < techTree.size(); ++tech)
    {
        if(tech == exclude)
            continue;
        const TechState state = getTechState(world, playerId, tech);
        if(state != TechState::Available && state != TechState::InProgress)
            continue;
        const bool affordable = isAffordable(world, playerId, tech);
        if(!best)
        {
            best = tech;
            bestAffordable = affordable;
            continue;
        }
        const Technology& cur = techTree.get(tech);
        const Technology& bestTech = techTree.get(*best);
        bool isBetter;
        if(affordable != bestAffordable)
            isBetter = affordable;
        else if(cur.aiPriority != bestTech.aiPriority)
            isBetter = cur.aiPriority > bestTech.aiPriority;
        else if(cur.GetTotalCost() + cur.GetTotalKnowledgeCost()
                != bestTech.GetTotalCost() + bestTech.GetTotalKnowledgeCost())
            isBetter = cur.GetTotalCost() + cur.GetTotalKnowledgeCost()
                       < bestTech.GetTotalCost() + bestTech.GetTotalKnowledgeCost();
        else
            isBetter = cur.era < bestTech.era;
        if(isBetter)
        {
            best = tech;
            bestAffordable = affordable;
        }
    }
    return best;
}

} // namespace civ
