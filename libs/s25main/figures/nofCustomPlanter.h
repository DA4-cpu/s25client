// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "nofFarmhand.h"
class GameWorldBase;
class SerializedGameData;
class nobUsual;
namespace civ {
struct CustomBuilding;
}

/// Worker of a plantation defined in lua (rttr:AddWorkshop with plant, like the farmer, forester or charburner):
/// Plants map objects (noDeposit) of his workshop where they grow naturally (rules of the map generator without the
/// zones). Each planting consumes the inputs of the workshop (e.g. water). If the workshop has an output, he also
/// harvests grown objects of this kind in range (one ware per trip, like the collector) and prefers that to planting.
class nofCustomPlanter : public nofFarmhand
{
    /// Current trip: Harvest (true) or plant (false)
    bool harvest_;

    const civ::CustomBuilding& GetBuildingDesc() const;

    void DrawWorking(DrawPoint drawPt) override;
    unsigned short GetCarryID() const override;
    void WalkingStarted() override;
    void WorkStarted() override {}
    void WorkFinished() override;
    PointQuality GetPointQuality(MapPoint pt, bool isBeforeWork) const override;
    /// Harvesting needs no wares, they are checked for planting in GetPointQuality
    bool AreWaresAvailable() const override;
    /// True if the inputs for planting are there (or none are needed)
    bool HasWaresToPlant() const;

public:
    nofCustomPlanter(Job job, MapPoint pos, unsigned char player, nobUsual* workplace);
    nofCustomPlanter(SerializedGameData& sgd, unsigned obj_id);

    void Serialize(SerializedGameData& sgd) const override;

    GO_Type GetGOT() const final { return GO_Type::NofCustomPlanter; }

    /// Range in nodes of the planter of the given job (custom job of a plantation), 0 if none
    static unsigned GetPlantRadius(Job job);
    /// True if the map object (index) grows naturally at the point: According to the rules of the map generator for
    /// this object (world/mapgen.lua) without the zones. Objects without a rule grow on land
    static bool GrowsNaturallyAt(const GameWorldBase& world, MapPoint pt, unsigned objIdx);
    /// True if the plantation can plant its map object at the point: The point is free (or has only decoration), no
    /// road, no buildings, flags or other fields/objects next to it and the object grows naturally there
    static bool CanPlantAt(const GameWorldBase& world, MapPoint pt, const civ::CustomBuilding& building);
};
