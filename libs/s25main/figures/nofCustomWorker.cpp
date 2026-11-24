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
#include "civ/WareDistribution.h"
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
}

nofCustomWorker::nofCustomWorker(SerializedGameData& sgd, const unsigned obj_id)
    : nofWorkman(sgd, obj_id), consumedInput_(sgd.PopOptionalEnum<GoodType>())
{
    if(sgd.GetGameDataVersion() >= 23)
        nextProduct_ = sgd.PopOptionalEnum<GoodType>();
}

void nofCustomWorker::Serialize(SerializedGameData& sgd) const
{
    nofWorkman::Serialize(sgd);
    sgd.PushOptionalEnum<uint16_t>(consumedInput_);
    sgd.PushOptionalEnum<uint16_t>(nextProduct_);
}

const civ::CustomBuilding* nofCustomWorker::GetBuildingDesc() const
{
    return workplace ? civ::getActiveContent().GetBuilding(workplace->GetBuildingType()) : nullptr;
}

GoodType nofCustomWorker::ChooseKnowledgeProduct(const civ::CustomBuilding& building) const
{
    // Weighted by the regulator "Wissen" of the owner (synchronized game RNG, so all clients choose the same).
    // If all weights of the possible products are 0, all are equally likely
    std::vector<unsigned> weights;
    const civ::PlayerResearch& research = world->GetPlayer(player).GetResearch();
    const auto distribution = research.GetKnowledgeDistribution(world->GetTechTree());
    for(const civ::KnowledgeType type : building.productChoiceTypes)
        weights.push_back(distribution[type]);
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
    return nofWorkman::AreWaresAvailable();
}

bool nofCustomWorker::StartWorking()
{
    // Several products: chosen by the distribution of all wares when starting (orders are processed now)
    const civ::CustomBuilding* desc = GetBuildingDesc();
    if(desc && desc->productChoice == civ::ProductChoice::Distribution)
        nextProduct_ = civ::chooseProduct(*world, player, desc->productChoices, GetObjId());
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
        case civ::ProductChoice::Distribution:
            return civ::chooseProduct(*world, player, building->productChoices, GetObjId());
        case civ::ProductChoice::Knowledge: return ChooseKnowledgeProduct(*building);
        case civ::ProductChoice::None: break;
    }
    // Mine: the ore lost one unit for the whole work step (StartWorking), not one per ware
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
        case civ::ProductChoice::Distribution:
            // Chosen when starting to work
            if(nextProduct && helpers::contains(building->productChoices, *nextProduct))
                return nextProduct;
            return building->productChoices.front();
        case civ::ProductChoice::Knowledge: return ChooseKnowledgeProduct(*building);
    }
    return building->GetProduct(consumedInput ? std::optional<GoodType>(*consumedInput) : std::nullopt);
}
