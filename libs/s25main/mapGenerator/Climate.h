// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/MapSettings.h" // ClimateTemperature, ClimateHumidity
#include "mapGenerator/NodeMapUtilities.h"
#include "mapGenerator/RandomUtility.h"
#include "world/NodeMapBase.h"

namespace rttr::mapGenerator {

/**
 * Maps a ClimateTemperature preset (as chosen in the map generator UI) to the value range
 * GenerateClimateMap() should scale its result to. "Temperate" spans the full [0, 100] range - the
 * original, unbiased behaviour - while "Cold"/"Hot" progressively shift and narrow the range towards
 * the low/high end so the average generated temperature moves accordingly.
 */
ValueRange<uint8_t> GetTemperatureRange(ClimateTemperature temperature);

/**
 * Maps a ClimateHumidity preset (as chosen in the map generator UI) to the value range
 * GenerateClimateMap() should scale its result to. "Temperate" (=moderate) spans the full [0, 100]
 * range - the original, unbiased behaviour - while "Dry"/"Humid" shift the range towards the low/high
 * end so the average generated humidity moves accordingly.
 */
ValueRange<uint8_t> GetHumidityRange(ClimateHumidity humidity);

/**
 * Generates a spatially smooth "climate" value for every node of the map - the spatial counterpart
 * to TerrainDesc::humidity. Where TerrainDesc::humidity is a static property of a terrain *type*
 * (used to sort/rank terrains), this map describes the local humidity of a *location*, completely
 * independent of the height map (map.z). It uses the exact same seed-point + distance-transform +
 * smoothing technique as the height map (see Terrain::Restructure and RandomMap::SmoothHeightMap),
 * just applied to its own NodeMapBase<uint8_t>, so that e.g. a dry region and a humid region can
 * appear side by side at the same altitude instead of climate being fully determined by height.
 *
 * @param rnd random utility used for scattering climate seed points ("poles")
 * @param size size of the map
 * @param zoneSize approximate width/height (in nodes) of a single climate zone; roughly one seed
 * point is scattered per zoneSize x zoneSize nodes. Smaller values produce more, smaller zones.
 * @param weight factor influencing how sharply climate drops off with distance from a seed point
 * (default: 2 - quadratic drop, same default as Restructure())
 * @param outputRange value range the result is scaled to - use GetTemperatureRange()/
 * GetHumidityRange() to derive this from a user-facing preset. Defaults to the full [0, 100] range.
 *
 * @returns a node map with values in outputRange, directly comparable to TerrainDesc::humidity (or,
 * once TerrainDesc::temperature is populated per terrain, TerrainDesc::temperature).
 */
NodeMapBase<uint8_t> GenerateClimateMap(RandomUtility& rnd, const MapExtent& size, unsigned zoneSize = 40,
                                         double weight = 2.,
                                         const ValueRange<uint8_t>& outputRange = ValueRange<uint8_t>(0, 100));

} // namespace rttr::mapGenerator
