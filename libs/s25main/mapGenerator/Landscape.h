// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/Map.h"
#include "mapGenerator/MapSettings.h"
#include "mapGenerator/RandomUtility.h"
#include "mapGenerator/Rivers.h"

namespace rttr::mapGenerator {

/// Heights used by CreateLandscape
struct LandscapeLevels
{
    /// Land has heights in [1, landTop]
    uint8_t landTop;
    /// Mountains start at this height
    uint8_t mountainLevel;
};

/// Height levels of land and mountains for the given maximum height of the map
LandscapeLevels GetLandscapeLevels(uint8_t maxHeight);

/**
 * Creates the height map like the online map generator of the RttR website: A fractal noise field is cut at the
 * share of water (settings.water, percent of the map) and the share of mountains (settings.mountains, percent of the
 * land). Water gets height 0, land rises slowly from the coast, mountains are higher (see GetLandscapeLevels).
 * The size of the features depends on settings.landMass, the detail on settings.roughness.
 *
 * @returns the height levels used
 */
LandscapeLevels CreateLandscape(Map& map, RandomUtility& rnd, const MapSettings& settings);

/**
 * Creates a river which flows downhill from the source until it reaches the sea level, another river or a sink (where
 * it ends in a small pond). Sets the water textures along its course, marks the course in map.riverCourse and lowers
 * the heights slightly.
 *
 * @returns all nodes touched by the water textures of the river
 */
River CreateRiver(Map& map, RandomUtility& rnd, MapPoint source, unsigned maxLength);

/**
 * Creates rivers starting at random points of the upper land (below the mountains).
 *
 * @param amount 0..100 (slider "rivers")
 */
std::vector<River> CreateRivers(Map& map, RandomUtility& rnd, unsigned amount, const LandscapeLevels& levels);

} // namespace rttr::mapGenerator
