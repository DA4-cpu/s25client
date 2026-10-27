// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/MapSettings.h" // ClimateTemperature, ClimateHumidity
#include "mapGenerator/RandomUtility.h"
#include "world/NodeMapBase.h"

namespace rttr::mapGenerator {

/// Mean temperature [0, 100] of a preset of the map generator window ("Kalt" ... "Heiß")
double GetTemperatureMean(ClimateTemperature temperature);

/// Mean humidity [0, 100] of a preset of the map generator window ("Feucht", "Gemäßigt", "Trocken")
double GetHumidityMean(ClimateHumidity humidity);

/**
 * Generates a spatially smooth climate value (temperature or humidity) for every node, independent of the height.
 * The values follow a normal distribution: most of the map is close to the mean, extreme values are rare
 * (about 68 % of the nodes are within mean +- spread, 95 % within mean +- 2 * spread). Clamped to [0, 100].
 *
 * @param rnd random utility
 * @param size size of the map
 * @param zoneSize approximate size (in nodes) of areas with similar climate
 * @param mean mean value
 * @param spread standard deviation
 */
NodeMapBase<uint8_t> GenerateClimateMap(RandomUtility& rnd, const MapExtent& size, unsigned zoneSize, double mean,
                                         double spread);

/// Inverse of the cumulative standard normal distribution for p in (0, 1)
double InverseNormalCDF(double p);

} // namespace rttr::mapGenerator
