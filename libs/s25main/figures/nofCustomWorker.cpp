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

nofCustomWorker::nofCustomWorker(SerializedGameData& sgd, const unsigned obj_id) : nofWorkman(sgd, obj_id) {}

void nofCustomWorker::Serialize(SerializedGameData& sgd) const
{
    nofWorkman::Serialize(sgd);
}

const civ::CustomBuilding* nofCustomWorker::GetBuildingDesc() const
{
    return workplace ? civ::getActiveContent().GetBuilding(workplace->GetBuildingType()) : nullptr;
}

GoodType nofCustomWorker::ChooseKnowledgeProduct(const civ::CustomBuilding& building) const
{
    // Weighted by the regulator "Wissen" of the owner (synchronized game RNG, so all clients choose the same).
    // If all weights of the possible products are 0, all are equally likely. Products yielding no ware with the tools
    // of the work step are not possible (unless none yields one)
    std::vector<bool> possible;
    for(const GoodType good : building.productChoices)
        possible.push_back(workplace->GetStepOutputRatio(good) > 0);
    if(std::none_of(possible.begin(), possible.end(), [](const bool value) { return value; }))
        possible.assign(possible.size(), true);
    std::vector<unsigned> weights;
    const civ::PlayerResearch& research = world->GetPlayer(player).GetResearch();
    const auto distribution = research.GetKnowledgeDistribution(world->GetTechTree());
    for(unsigned i = 0; i < building.productChoiceTypes.size(); ++i)
        weights.push_back(possible[i] ? distribution[building.productChoiceTypes[i]] : 0u);
    unsigned totalWeight = 0;
    for(const unsigned weight : weights)
        totalWeight += weight;
    if(totalWeight == 0)
    {
        for(unsigned i = 0; i < weights.size(); ++i)
            weights[i] = possible[i] ? 1u : 0u;
        totalWeight = static_cast<unsigned>(std::count(possible.begin(), possible.end(), true));
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

GoodType nofCustomWorker::ChooseProduct(const civ::CustomBuilding& building)
{
    switch(building.productChoice)
    {
        case civ::ProductChoice::Distribution:
        {
            std::vector<GoodType> candidates;
            for(const GoodType good : building.productChoices)
            {
                if(workplace->GetStepOutputRatio(good) > 0)
                    candidates.push_back(good);
            }
            if(candidates.empty())
                candidates = building.productChoices;
            return civ::chooseProduct(*world, player, candidates, GetObjId(),
                                      [this](const GoodType good) { return workplace->GetNumMadeInStep(good); });
        }
        case civ::ProductChoice::Knowledge: return ChooseKnowledgeProduct(building);
        case civ::ProductChoice::None: break;
    }
    const auto consumed = workplace->GetLastConsumedWare();
    return building.GetProduct(consumed ? std::optional<GoodType>(*consumed) : std::nullopt);
}

bool nofCustomWorker::StartWorking()
{
    const civ::CustomBuilding* desc = GetBuildingDesc();
    const bool hasProduct = desc && desc->HasProduct();
    // Several products: chosen anew for every work step, the saved steps of the products do not matter (Nutzer,
    // 2026-11-25). Orders are processed now
    workplace->SetChosenProduct(std::nullopt);
    if(hasProduct && desc->productChoice != civ::ProductChoice::None)
        workplace->SetChosenProduct(ChooseProduct(*desc));
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
    // One product: with inheritProperties it is decided by the ware consumed from the first input
    if(hasProduct && desc->productChoice == civ::ProductChoice::None)
    {
        const GoodType product = ChooseProduct(*desc);
        workplace->SetChosenProduct(product);
        // Several products by the inputs: their orders are processed now (one product: when the step starts)
        if(desc->GetAllProducts().size() > 1u)
            civ::processOrders(*world, player, product, workplace->GetNumMadeInStep(product));
    }
    return true;
}

helpers::OptionalEnum<GoodType> nofCustomWorker::ProduceWare()
{
    const civ::CustomBuilding* building = GetBuildingDesc();
    if(!building || !building->HasProduct())
        return std::nullopt;
    const auto chosen = workplace->GetChosenProduct();
    workplace->SetChosenProduct(std::nullopt);
    if(chosen)
        return chosen;
    return ChooseProduct(*building);
}
