// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "nofCustomWorker.h"
#include "SerializedGameData.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"

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
    const GoodType product =
      building->GetProduct(consumedInput_ ? std::optional<GoodType>(*consumedInput_) : std::nullopt);
    consumedInput_.reset();
    return product;
}
