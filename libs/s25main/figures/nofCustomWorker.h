// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "helpers/OptionalEnum.h"
#include "nofWorkman.h"
#include "notifications/Subscription.h"
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
    /// outputChoice = "tools": Tool chosen when starting to work (from the tool orders and the tool regulator)
    helpers::OptionalEnum<GoodType> nextProduct_;
    /// outputChoice = "tools": Start working when tools are ordered or the tool regulator is changed
    Subscription toolOrderSub_;

    void DrawWorking(DrawPoint drawPt) override;
    unsigned short GetCarryID() const override;
    helpers::OptionalEnum<GoodType> ProduceWare() override;
    bool StartWorking() override;
    /// A librarian never "works": the library only stores media. A miner needs the ore in range, too
    bool AreWaresAvailable() const override;
    /// Ore of the mine of the worker or Nothing if the workshop is no mine
    ResourceType GetMineResource() const;
    /// Definition of the workshop or nullptr
    const civ::CustomBuilding* GetBuildingDesc() const;
    /// True if the workshop chooses its product like the metalworks (outputChoice = "tools")
    bool ChoosesTools() const;
    /// outputChoice = "tools": Any of the tools of the workshop ordered or with a priority > 0?
    bool HasToolOrder() const;
    bool HasToolPriority() const;
    /// outputChoice = "tools": Next tool according to the orders (processes the order), then the priorities.
    /// Nothing if no tool shall be made
    helpers::OptionalEnum<GoodType> ChooseTool();
    /// outputChoice = knowledge or a regulator: Product weighted by the regulator of the owner
    GoodType ChooseWeightedProduct(const civ::CustomBuilding& building) const;
    void SubscribeToolOrders();

public:
    nofCustomWorker(Job job, MapPoint pos, unsigned char player, nobUsual* workplace);
    nofCustomWorker(SerializedGameData& sgd, unsigned obj_id);

    void Serialize(SerializedGameData& sgd) const override;

    GO_Type GetGOT() const final { return GO_Type::NofCustomWorker; }
};
