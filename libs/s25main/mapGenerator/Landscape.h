// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/Map.h"
#include "mapGenerator/MapSettings.h"
#include "mapGenerator/RandomUtility.h"
#include "mapGenerator/Rivers.h"

namespace rttr::mapGenerator {

/**
 * Decides for every node whether it is water, land or mountain (map.nodeKind) like the online map generator of the
 * RttR website: A fractal noise field is cut at the share of water (settings.water, percent of the map) and the share
 * of mountains (settings.mountains, percent of the land). The size of the shapes depends on settings.landMass, their
 * detail on settings.roughness.
 */
void CreateLandMasses(Map& map, RandomUtility& rnd, const MapSettings& settings);

/**
 * Creates the heights (map.z, map.mountainLift) from map.nodeKind:
 * - Water has height 0.
 * - Land rises slowly from the coast. Hills vary by region: some regions are flat plains, others hilly. How hilly
 *   depends on settings.unevenness (0 = everything flat, 100 = very uneven).
 * - Mountains stand on the land and rise from their foot: Each mountain range has its own steepness and height, so
 *   there are flat low mountains and steep high ones. Mountains are never lower than the land around them.
 * The slopes stay small enough for mines on most mountain nodes (mines need the south east neighbor at most 3 higher).
 */
void CreateHeights(Map& map, RandomUtility& rnd, const MapSettings& settings);

/**
 * Marks the water nodes of lakes in map.lakes: Bodies of water (connected water nodes of map.nodeKind) with at most
 * maxSize nodes are lakes (fresh water), larger ones are the sea (salt water). The size is absolute, independent of the
 * size of the map.
 */
void MarkLakes(Map& map, unsigned maxSize);

/**
 * Levels for the Texturizer: water 0, land 1, mountains mountainLevel + map.mountainLift. So the textures depend on
 * the kind of the node and the mountain textures (and snow) on the height of the mountain above its foot.
 */
NodeMapBase<uint8_t> GetTextureLevels(const Map& map, uint8_t mountainLevel);

/**
 * Places small building sites in the mountains like the online map generator: The 6 triangles around nodes on a
 * regular grid (distance `spacing` in nodes) get a buildable mountain texture and the heights around are flattened,
 * so military buildings can be built there and all mountains can be conquered. A grid node which is not inside the
 * mountains is moved to a suitable node nearby if possible. Requires textures.
 *
 * @returns the positions of the building sites
 */
std::vector<MapPoint> PlaceMountainBuildingSites(Map& map, unsigned spacing);

} // namespace rttr::mapGenerator
