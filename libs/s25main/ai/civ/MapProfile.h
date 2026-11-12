// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "ai/civ/EconomyGraph.h"
#include "helpers/EnumArray.h"
#include "gameTypes/MapCoordinates.h"
#include "gameTypes/Resource.h"
#include <cstdint>
#include <vector>

class GameWorldBase;

namespace civai {

/// What there is in a part of the map
struct ResourceProfile
{
    /// Grown and growing trees
    unsigned trees = 0;
    /// Stones in stone piles (a pile of size n has n+1 stones)
    unsigned stones = 0;
    /// Wild animals
    unsigned animals = 0;
    /// Free buildable nodes (fields)
    unsigned land = 0;
    /// Amount of each resource in the ground (ores, water, fish)
    helpers::EnumArray<unsigned, ResourceType> resources{};
    /// Pieces of each map object defined in lua (collectors), per map object
    std::vector<unsigned> mapObjects;
    /// Nodes where the map object grows naturally (plantations), per map object
    std::vector<unsigned> plantable;

    /// Amount of the source (0: not there)
    unsigned GetAmount(const Source& source) const;
    void Add(const ResourceProfile& other);
};

/// The map in regions (squares of REGION_SIZE nodes) with the resources of each. The AI knows the whole map.
/// Built once, then a few regions per update are scanned again (round robin), so the costs per GF stay small
class MapProfile
{
public:
    static constexpr unsigned REGION_SIZE = 8;

    MapProfile(const GameWorldBase& world, unsigned playerId);

    /// Scan the next `numRegions` regions again
    void Update(unsigned numRegions);

    unsigned GetNumRegions() const { return static_cast<unsigned>(regions_.size()); }
    const ResourceProfile& GetRegion(unsigned idx) const { return regions_[idx]; }
    unsigned GetRegionIdx(MapPoint pt) const;
    /// Sum of all regions
    ResourceProfile GetTotal() const;
    /// Resources on the nodes owned by the player (counted while scanning, so as current as the regions)
    ResourceProfile GetTotalOwned() const;
    const ResourceProfile& GetRegionOwned(unsigned idx) const { return ownedRegions_[idx]; }

private:
    void ScanRegion(unsigned idx);

    const GameWorldBase& world_;
    unsigned numRegionsX_, numRegionsY_;
    const unsigned playerId_;
    std::vector<ResourceProfile> regions_;
    /// Per region: resources on the nodes of the player
    std::vector<ResourceProfile> ownedRegions_;
    unsigned nextRegion_ = 0;
    /// What does not change during the game, per node (computed once, so a scan only looks at objects and resources):
    /// bit 0: buildable terrain, bit 1 + i: the i-th plantable map object grows naturally here
    std::vector<uint32_t> staticInfo_;
    /// Map objects which can be planted (index into the map objects), at most 31
    std::vector<unsigned> plantableObjects_;
};

} // namespace civai
