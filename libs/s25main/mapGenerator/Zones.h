// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/Map.h"
#include "mapGenerator/RandomUtility.h"
#include "world/NodeMapBase.h"

namespace rttr::mapGenerator {

/**
 * Splits the map into numZones large connected zones (geographic zones like "Afrika"). Every node belongs to exactly
 * one zone and every zone index in [0, numZones) occurs exactly once as one connected area (on the wrapped map).
 * The zones are grown from seed points which are spread far apart, with wavy borders.
 *
 * @returns the zone index per node
 */
NodeMapBase<uint8_t> GenerateZones(RandomUtility& rnd, const MapExtent& size, unsigned numZones);

/// Fill map.heightPercent from map.z: 0 = sea level (or below), 100 = highest point
void ComputeHeightPercent(Map& map);

/// Lower map.temperature with the height: by `cooling` points at the highest point (map.heightPercent must be set)
void ApplyHeightCooling(Map& map, unsigned cooling);

/**
 * Raise map.climate (humidity) at lakes (map.lakes) and rivers (map.riverCourse): By `humidity` points at the water,
 * `falloff` points less every `step` nodes away from it (falloff 0 = the same everywhere), nothing from
 * maxDistance + 1 nodes on.
 */
void ApplyWaterHumidity(Map& map, unsigned maxDistance, unsigned humidity, unsigned falloff, unsigned step);

/// What ComputeTerrainFlags needs to know about a node (bit flags)
enum TerrainInfo : uint8_t
{
    /// All triangles around the node are water
    TERRAIN_INFO_ALL_WATER = 1,
    /// At least one triangle is water
    TERRAIN_INFO_ANY_WATER = 2,
    /// At least one triangle is mountain, snow or lava
    TERRAIN_INFO_ANY_MOUNTAIN = 4,
    /// All triangles are land
    TERRAIN_INFO_ALL_LAND = 8,
    /// The water of the node (if it is water) is fresh water (lake or river)
    TERRAIN_INFO_FRESH = 16,
    /// The node is on the course of a river
    TERRAIN_INFO_RIVER = 32,
};

/**
 * Terrain types per node (bit flags of MapGenTerrain) from the TerrainInfo flags of the nodes:
 * Lakes and rivers are fresh water, the sea is salt water. "Shore" nodes touch the water, "near" nodes are at most
 * nearWaterDistance nodes away. Also used in the game (civ::ClimateMap).
 */
NodeMapBase<uint16_t> ComputeTerrainFlags(const NodeMapBase<uint8_t>& info, unsigned nearWaterDistance);

/// TerrainInfo flags per node of the map (textures, map.nodeKind, map.lakes and map.riverCourse)
NodeMapBase<uint8_t> GetTerrainInfo(const Map& map);

/// Fill map.terrainFlags from the textures, map.lakes and map.riverCourse (see above)
void ComputeTerrainFlags(Map& map, unsigned nearWaterDistance);

} // namespace rttr::mapGenerator
