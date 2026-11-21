// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "helpers/OptionalEnum.h"
#include "nofWorkman.h"
#include "gameTypes/Resource.h"

class SerializedGameData;
class nobUsual;
namespace civ {
struct CustomBuilding;
}

/// Worker of a workshop defined in lua (civilization mode, see civ/Content.h).
/// Works inside the building: consumes the input wares and produces the ware of the workshop definition.
class nofCustomWorker : public nofWorkman
{
    /// Input ware consumed for the current product (decides the product if properties are passed on)
    helpers::OptionalEnum<GoodType> consumedInput_;
    /// Several products (distribution of all wares): product chosen when starting to work
    helpers::OptionalEnum<GoodType> nextProduct_;

    void DrawWorking(DrawPoint drawPt) override;
    unsigned short GetCarryID() const override;
    helpers::OptionalEnum<GoodType> ProduceWare() override;
    /// Civilization mode, output ratio above 1: each product is chosen on its own, a mine digs one more unit
    helpers::OptionalEnum<GoodType> ProduceExtraWare(GoodType firstWare) override;
    bool StartWorking() override;
    /// A librarian never "works": the library only stores media. A miner needs the ore in range, too
    bool AreWaresAvailable() const override;
    /// Ore of the mine of the worker or Nothing if the workshop is no mine
    ResourceType GetMineResource() const;
    /// Definition of the workshop or nullptr
    const civ::CustomBuilding* GetBuildingDesc() const;
    /// outputChoice = "knowledge": Product weighted by the regulator "Wissen" of the owner
    GoodType ChooseKnowledgeProduct(const civ::CustomBuilding& building) const;

public:
    nofCustomWorker(Job job, MapPoint pos, unsigned char player, nobUsual* workplace);
    nofCustomWorker(SerializedGameData& sgd, unsigned obj_id);

    void Serialize(SerializedGameData& sgd) const override;

    GO_Type GetGOT() const final { return GO_Type::NofCustomWorker; }
};
