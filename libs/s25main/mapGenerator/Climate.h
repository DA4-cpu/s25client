// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/NodeMapUtilities.h"
#include "mapGenerator/RandomUtility.h"
#include "world/NodeMapBase.h"

namespace rttr::mapGenerator {

/**
 * Generates a spatially smooth "climate" value in [0, 100] for every node of the map - the spatial
 * counterpart to TerrainDesc::humidity. Where TerrainDesc::humidity is a static property of a terrain
 * *type* (used to sort/rank terrains), this map describes the local humidity of a *location*,
 * completely independent of the height map (map.z). It uses the exact same seed-point +
 * distance-transform + smoothing technique as the height map (see Terrain::Restructure and
 * RandomMap::SmoothHeightMap), just applied to its own NodeMapBase<uint8_t>, so that e.g. a dry
 * region and a humid region can appear side by side at the same altitude instead of climate being
 * fully determined by height.
 *
 * @param rnd random utility used for scattering climate seed points ("poles")
 * @param size size of the map
 * @param zoneSize approximate width/height (in nodes) of a single climate zone; roughly one seed
 * point is scattered per zoneSize x zoneSize nodes. Smaller values produce more, smaller zones.
 * @param weight factor influencing how sharply climate drops off with distance from a seed point
 * (default: 2 - quadratic drop, same default as Restructure())
 *
 * @returns a node map with values in [0, 100], directly comparable to TerrainDesc::humidity.
 */
NodeMapBase<uint8_t> GenerateClimateMap(RandomUtility& rnd, const MapExtent& size, unsigned zoneSize = 40,
                                         double weight = 2.);

} // namespace rttr::mapGenerator
