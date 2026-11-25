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
    // Das Produkt wählt nofWorkman::StartWorking (ChooseStepProduct)
    return nofWorkman::StartWorking();
}

helpers::OptionalEnum<GoodType> nofCustomWorker::ProduceWare()
{
    // Das für diesen Arbeitsschritt gewählte Produkt (nichts bei einer Bibliothek)
    return TakeStepProduct(std::nullopt);
}
