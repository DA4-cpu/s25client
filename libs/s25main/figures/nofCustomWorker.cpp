// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "nofCustomWorker.h"
#include "GamePlayer.h"
#include "SerializedGameData.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/TechTree.h"
#include "random/Random.h"
#include "world/GameWorld.h"
#include <algorithm>
#include <vector>

nofCustomWorker::nofCustomWorker(const Job job, const MapPoint pos, const unsigned char player, nobUsual* workplace)
    : nofWorkman(job, pos, player, workplace)
{
    RTTR_Assert(isCustomJob(job));
}

nofCustomWorker::nofCustomWorker(SerializedGameData& sgd, const unsigned obj_id)
    : nofWorkman(sgd, obj_id), consumedInput_(sgd.PopOptionalEnum<GoodType>())
{}

void nofCustomWorker::Serialize(SerializedGameData& sgd) const
{
    nofWorkman::Serialize(sgd);
    sgd.PushOptionalEnum<uint8_t>(consumedInput_);
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

bool nofCustomWorker::AreWaresAvailable() const
{
    if(workplace->IsLibrary())
        return false;
    return nofWorkman::AreWaresAvailable();
}

bool nofCustomWorker::StartWorking()
{
    if(!nofWorkman::StartWorking())
        return false;
    consumedInput_ = workplace->GetLastConsumedWare();
    return true;
}

helpers::OptionalEnum<GoodType> nofCustomWorker::ProduceWare()
{
    const civ::CustomBuilding* building = civ::getActiveContent().GetBuilding(workplace->GetBuildingType());
    if(!building)
        return std::nullopt;
    const auto consumedInput = consumedInput_;
    consumedInput_.reset();
    if(!building->productChoices.empty())
    {
        // outputChoice = "knowledge": Weighted by the regulator "Wissen" of the owner (synchronized game RNG, so all
        // clients choose the same). If all weights of the possible products are 0, all are equally likely
        const auto distribution = world->GetPlayer(player).GetResearch().GetKnowledgeDistribution(world->GetTechTree());
        std::vector<unsigned> weights;
        unsigned totalWeight = 0;
        for(const civ::KnowledgeType type : building->productChoiceTypes)
        {
            weights.push_back(distribution[type]);
            totalWeight += distribution[type];
        }
        if(totalWeight == 0)
        {
            std::fill(weights.begin(), weights.end(), 1u);
            totalWeight = static_cast<unsigned>(weights.size());
        }
        unsigned pick = RANDOM_RAND(totalWeight);
        for(unsigned i = 0; i < weights.size(); ++i)
        {
            if(pick < weights[i])
                return building->productChoices[i];
            pick -= weights[i];
        }
        return building->productChoices.back();
    }
    return building->GetProduct(consumedInput ? std::optional<GoodType>(*consumedInput) : std::nullopt);
}
