// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "helpers/OptionalEnum.h"
#include "nofWorkman.h"

class SerializedGameData;
class nobUsual;

/// Worker of a workshop defined in lua (civilization mode, see civ/Content.h).
/// Works inside the building: consumes the input wares and produces the ware of the workshop definition.
class nofCustomWorker : public nofWorkman
{
    /// Input ware consumed for the current product (decides the product if properties are passed on)
    helpers::OptionalEnum<GoodType> consumedInput_;

    void DrawWorking(DrawPoint drawPt) override;
    unsigned short GetCarryID() const override;
    helpers::OptionalEnum<GoodType> ProduceWare() override;
    bool StartWorking() override;

public:
    nofCustomWorker(Job job, MapPoint pos, unsigned char player, nobUsual* workplace);
    nofCustomWorker(SerializedGameData& sgd, unsigned obj_id);

    void Serialize(SerializedGameData& sgd) const override;

    GO_Type GetGOT() const final { return GO_Type::NofCustomWorker; }
};
