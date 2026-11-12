// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "nofFarmhand.h"
class SerializedGameData;
class nobUsual;
namespace civ {
struct CustomBuilding;
}

/// Worker of a workshop defined in lua that collects map objects (rttr:AddWorkshop with collect, e.g. the loam pit):
/// Walks to a map object (noDeposit) of the right kind in range like the stonemason, works there, makes the object
/// smaller (it disappears after the last ware) and carries the product home
class nofCustomCollector : public nofFarmhand
{
    const civ::CustomBuilding& GetBuildingDesc() const;

    void DrawWorking(DrawPoint drawPt) override;
    unsigned short GetCarryID() const override;
    void WorkStarted() override {}
    void WorkFinished() override;
    /// Civilization mode, output ratio above 1: one more piece of the object per ware
    helpers::OptionalEnum<GoodType> ProduceExtraWare(GoodType firstWare) override;
    /// Take a piece of the object at the current position (it disappears after the last one). False if there is none
    bool CollectPiece();
    PointQuality GetPointQuality(MapPoint pt, bool isBeforeWork) const override;

public:
    nofCustomCollector(Job job, MapPoint pos, unsigned char player, nobUsual* workplace);
    nofCustomCollector(SerializedGameData& sgd, unsigned obj_id);

    GO_Type GetGOT() const final { return GO_Type::NofCustomCollector; }

    /// Range in nodes of the collector of the given job (custom job of a collecting workshop), 0 if none
    static unsigned GetCollectRadius(Job job);
};
