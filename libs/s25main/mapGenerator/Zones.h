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
 * Fill map.terrainFlags (bit flags of MapGenTerrain) from the textures and map.riverCourse:
 * Bodies of water up to freshwaterMaxSize percent of the map are fresh water, larger ones salt water. River courses
 * are always fresh water. "Shore" nodes touch the water, "near" nodes are at most nearWaterDistance nodes away.
 */
void ComputeTerrainFlags(Map& map, unsigned nearWaterDistance, double freshwaterMaxSize);

} // namespace rttr::mapGenerator
