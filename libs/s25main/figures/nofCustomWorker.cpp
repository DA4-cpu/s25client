// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "nofCustomWorker.h"
#include "EventManager.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "SerializedGameData.h"
#include "addons/const_addons.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/TechTree.h"
#include "helpers/containerUtils.h"
#include "notifications/NotificationManager.h"
#include "notifications/ToolNote.h"
#include "postSystem/PostMsg.h"
#include "random/Random.h"
#include "world/GameWorld.h"
#include "gameData/ToolConsts.h"
#include <algorithm>
#include <vector>

nofCustomWorker::nofCustomWorker(const Job job, const MapPoint pos, const unsigned char player, nobUsual* workplace)
    : nofWorkman(job, pos, player, workplace)
{
    RTTR_Assert(isCustomJob(job));
    SubscribeToolOrders();
}

nofCustomWorker::nofCustomWorker(SerializedGameData& sgd, const unsigned obj_id)
    : nofWorkman(sgd, obj_id), consumedInput_(sgd.PopOptionalEnum<GoodType>())
{
    if(sgd.GetGameDataVersion() >= 23)
        nextProduct_ = sgd.PopOptionalEnum<GoodType>();
    SubscribeToolOrders();
}

void nofCustomWorker::Serialize(SerializedGameData& sgd) const
{
    nofWorkman::Serialize(sgd);
    sgd.PushOptionalEnum<uint8_t>(consumedInput_);
    sgd.PushOptionalEnum<uint8_t>(nextProduct_);
}

void nofCustomWorker::SubscribeToolOrders()
{
    // Like the metalworks: If it waits because no tool shall be made, an order or a changed regulator may change that.
    // The workplace may not be loaded yet (savegame), so whether it makes tools is checked when it happens
    toolOrderSub_ = world->GetNotifications().subscribe<ToolNote>([this](const ToolNote& note) {
        if((note.type == ToolNote::OrderPlaced || note.type == ToolNote::SettingsChanged) && note.player == player
           && state == State::WaitingForWaresOrProductionStopped && workplace && ChoosesTools())
            TryToWork();
    });
}

const civ::CustomBuilding* nofCustomWorker::GetBuildingDesc() const
{
    return workplace ? civ::getActiveContent().GetBuilding(workplace->GetBuildingType()) : nullptr;
}

bool nofCustomWorker::ChoosesTools() const
{
    const civ::CustomBuilding* building = GetBuildingDesc();
    return building && building->productChoice == civ::ProductChoice::Tools;
}

bool nofCustomWorker::HasToolOrder() const
{
    const GamePlayer& owner = world->GetPlayer(player);
    const civ::CustomBuilding& building = *GetBuildingDesc();
    return helpers::contains_if(building.productChoiceTools,
                                [&owner](const Tool tool) { return owner.GetToolsOrdered(tool) > 0u; });
}

bool nofCustomWorker::HasToolPriority() const
{
    const GamePlayer& owner = world->GetPlayer(player);
    const civ::CustomBuilding& building = *GetBuildingDesc();
    return helpers::contains_if(building.productChoiceTools,
                                [&owner](const Tool tool) { return owner.GetToolPriority(tool) > 0u; });
}

helpers::OptionalEnum<GoodType> nofCustomWorker::ChooseTool()
{
    GamePlayer& owner = world->GetPlayer(player);
    const civ::CustomBuilding& building = *GetBuildingDesc();
    const auto productOf = [&building](const Tool tool) {
        return building.productChoices[helpers::indexOf(building.productChoiceTools, tool)];
    };

    // Orders first (shared with the metalworks: whoever starts the tool first fulfills the order)
    std::vector<Tool> randomArray;
    for(const Tool tool : building.productChoiceTools)
    {
        if(owner.GetToolsOrdered(tool) > 0u)
            randomArray.insert(randomArray.end(), std::max(owner.GetToolPriority(tool), 1u), tool);
    }
    if(!randomArray.empty())
    {
        const Tool tool = RANDOM_ELEMENT(randomArray);
        owner.ToolOrderProcessed(tool);
        bool anyOrderLeft = false;
        for(const auto otherTool : helpers::enumRange<Tool>())
            anyOrderLeft |= owner.GetToolsOrdered(otherTool) > 0u;
        if(!anyOrderLeft)
        {
            SendPostMessage(player, std::make_unique<PostMsg>(GetEvMgr().GetCurrentGF(),
                                                              _("Completed the ordered amount of tools."),
                                                              PostCategory::Economy));
        }
        return productOf(tool);
    }

    // Then weighted by the tool regulator
    for(const Tool tool : building.productChoiceTools)
        randomArray.insert(randomArray.end(), owner.GetToolPriority(tool), tool);
    if(randomArray.empty())
    {
        // All 0: Nothing if the addon says so, otherwise any (like the metalworks)
        if(world->GetGGS().getSelection(AddonId::METALWORKSBEHAVIORONZERO) == 1)
            return std::nullopt;
        return RANDOM_ELEMENT(building.productChoices);
    }
    return productOf(RANDOM_ELEMENT(randomArray));
}

GoodType nofCustomWorker::ChooseWeightedProduct(const civ::CustomBuilding& building) const
{
    // Weighted by the regulator of the owner (synchronized game RNG, so all clients choose the same).
    // If all weights of the possible products are 0, all are equally likely
    std::vector<unsigned> weights;
    const civ::PlayerResearch& research = world->GetPlayer(player).GetResearch();
    if(building.productChoice == civ::ProductChoice::Knowledge)
    {
        const auto distribution = research.GetKnowledgeDistribution(world->GetTechTree());
        for(const civ::KnowledgeType type : building.productChoiceTypes)
            weights.push_back(distribution[type]);
    } else
    {
        const auto values = research.GetRegulatorValues(world->GetTechTree().GetContent(), building.regulator);
        for(const unsigned entry : building.productChoiceEntries)
            weights.push_back(entry < values.size() ? values[entry] : 0u);
    }
    unsigned totalWeight = 0;
    for(const unsigned weight : weights)
        totalWeight += weight;
    if(totalWeight == 0)
    {
        std::fill(weights.begin(), weights.end(), 1u);
        totalWeight = static_cast<unsigned>(weights.size());
    }
    unsigned pick = RANDOM_RAND(totalWeight);
    for(unsigned i = 0; i < weights.size(); ++i)
    {
        if(pick < weights[i])
            return building.productChoices[i];
        pick -= weights[i];
    }
    return building.productChoices.back();
}

void nofCustomWorker::DrawWorking(DrawPoint /*drawPt*/)
{
    // Works inside the building (no animation yet)
}

unsigned short nofCustomWorker::GetCarryID() const
{
    // Carry the product like a carrier (graphics of the "look" ware)
    return CARRY_ID_CARRIER_OFFSET + static_cast<unsigned short>(ware ? *ware : GoodType::Stones);
}

ResourceType nofCustomWorker::GetMineResource() const
{
    const civ::CustomBuilding* building = civ::getActiveContent().GetBuilding(workplace->GetBuildingType());
    return building ? building->mineResource : ResourceType::Nothing;
}

bool nofCustomWorker::AreWaresAvailable() const
{
    if(workplace->IsLibrary())
        return false;
    // Mine: Also the ore must be there (like nofMiner)
    const ResourceType ore = GetMineResource();
    if(ore != ResourceType::Nothing && !FindPointWithResource(ore).isValid())
        return false;
    if(!nofWorkman::AreWaresAvailable())
        return false;
    // Tools like the metalworks: Nothing to do if no tool is ordered and all priorities are 0 (if the addon says so)
    if(ChoosesTools() && world->GetGGS().getSelection(AddonId::METALWORKSBEHAVIORONZERO) == 1)
        return HasToolOrder() || HasToolPriority();
    return true;
}

bool nofCustomWorker::StartWorking()
{
    if(ChoosesTools())
    {
        nextProduct_ = ChooseTool();
        if(!nextProduct_)
            return false;
    }
    const ResourceType ore = GetMineResource();
    if(ore != ResourceType::Nothing)
    {
        // Mine: Dig one unit of the ore like nofMiner (unless mines are inexhaustible)
        const MapPoint resPt = FindPointWithResource(ore);
        if(!resPt.isValid())
            return false;
        if(!world->GetGGS().isEnabled(AddonId::INEXHAUSTIBLE_MINES))
            world->ReduceResource(resPt);
    }
    if(!nofWorkman::StartWorking())
        return false;
    consumedInput_ = workplace->GetLastConsumedWare();
    return true;
}

helpers::OptionalEnum<GoodType> nofCustomWorker::ProduceExtraWare(const GoodType firstWare)
{
    const civ::CustomBuilding* building = GetBuildingDesc();
    if(!building)
        return std::nullopt;
    switch(building->productChoice)
    {
        case civ::ProductChoice::Tools: return ChooseTool();
        case civ::ProductChoice::Knowledge:
        case civ::ProductChoice::Regulator: return ChooseWeightedProduct(*building);
        case civ::ProductChoice::None: break;
    }
    const ResourceType ore = GetMineResource();
    if(ore != ResourceType::Nothing)
    {
        // Mine: one more unit of the ore per ware
        const MapPoint resPt = FindPointWithResource(ore);
        if(!resPt.isValid())
            return std::nullopt;
        if(!world->GetGGS().isEnabled(AddonId::INEXHAUSTIBLE_MINES))
            world->ReduceResource(resPt);
    }
    return firstWare;
}

helpers::OptionalEnum<GoodType> nofCustomWorker::ProduceWare()
{
    const civ::CustomBuilding* building = GetBuildingDesc();
    if(!building)
        return std::nullopt;
    const auto consumedInput = consumedInput_;
    consumedInput_.reset();
    const auto nextProduct = nextProduct_;
    nextProduct_.reset();
    switch(building->productChoice)
    {
        case civ::ProductChoice::None: break;
        case civ::ProductChoice::Tools:
            // Chosen when starting to work (savegames from before that: the first tool)
            if(nextProduct && helpers::contains(building->productChoices, *nextProduct))
                return nextProduct;
            return building->productChoices.front();
        case civ::ProductChoice::Knowledge:
        case civ::ProductChoice::Regulator: return ChooseWeightedProduct(*building);
    }
    return building->GetProduct(consumedInput ? std::optional<GoodType>(*consumedInput) : std::nullopt);
}
