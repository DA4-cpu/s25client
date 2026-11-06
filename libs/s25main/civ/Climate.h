// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "world/NodeMapBase.h"
#include "gameTypes/MapCoordinates.h"
#include <cstdint>

class World;
class SerializedGameData;
struct MapResourceDesc;
namespace libsiedler2 {
class ArchivItem_Map;
}

namespace civ {

/// Climate and terrain types of every node (civilization mode), e.g. to decide where a plantation can plant something:
/// only where it grows naturally according to the rules of the map generator (world/mapgen.lua).
/// Temperature and humidity exist only on generated maps (stored in the map file by the generator, see MapCodes.h).
/// The terrain types ("land", "shore_fresh", ...) are computed like in the generator for every map; on other maps
/// small bodies of water are lakes (fresh water) and there are no rivers.
class ClimateMap
{
public:
    /// Compute for the world. map: The map file or nullptr (then there is no climate)
    void Init(const World& world, const libsiedler2::ArchivItem_Map* map);

    /// True if temperature, humidity and height are known (generated map)
    bool HasClimate() const { return hasClimate_; }
    /// Climate values 0..100 (only if HasClimate())
    unsigned GetTemperature(MapPoint pt) const { return temperature_[pt]; }
    unsigned GetHumidity(MapPoint pt) const { return humidity_[pt]; }
    unsigned GetHeight(MapPoint pt) const { return height_[pt]; }
    /// Terrain types as bit flags of MapGenTerrain
    uint16_t GetTerrain(MapPoint pt) const { return terrain_[pt]; }

    /// True if the rule of the map generator (rttr:AddMapResource) would allow its object at the node: Terrain type,
    /// climate limits and a weight > 0. Zones, amounts and the distance to the HQ do not count.
    /// Without climate only the terrain type counts.
    bool AllowsNaturally(MapPoint pt, const MapResourceDesc& rule) const;

    void Serialize(SerializedGameData& sgd) const;
    void Deserialize(SerializedGameData& sgd, const World& world);

private:
    /// Fresh water and rivers from the climate (generated maps) or lakes by their size (other maps), then the terrain
    /// types
    void ComputeTerrain(const World& world);

    bool hasClimate_ = false;
    NodeMapBase<uint8_t> temperature_, humidity_, height_;
    /// WaterBits per node (see MapCodes.h)
    NodeMapBase<uint8_t> water_;
    NodeMapBase<uint16_t> terrain_;
};

} // namespace civ
