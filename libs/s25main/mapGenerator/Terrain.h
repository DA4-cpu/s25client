// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/Algorithms.h"
#include "mapGenerator/Map.h"
#include "mapGenerator/RandomUtility.h"
#include <functional>

namespace rttr::mapGenerator {

/**
 * Restructures the specified value map by elevating values preferably around the specified focus area.
 * Nodes closer to the focus area experience more elevation than nodes further away from the focus area.
 * This is the generic version used both for the height map (see the Map-overload below) and for any other
 * independent NodeMapBase<uint8_t> layer, e.g. a climate/humidity map.
 *
 * @param values reference to the map of values to manipulate
 * @param range minimum and maximum value to scale the result to
 * @param predicate predicate defining the focus of elevation
 * @param weight factor influencing the strength of elevation towards the focused area (default: 2 - quadratic drop
 * with distance)
 */
void Restructure(NodeMapBase<uint8_t>& values, const ValueRange<uint8_t>& range,
                  const std::function<bool(const MapPoint&)>& predicate, double weight = 2.);

/**
 * Restructures the specified height map by elevating landscape preferably around the specified focus area.
 * Nodes closer to the focus area experience more elevation than nodes further away from the focus area.
 *
 * @param map reference to the map to manipulate
 * @param predicate predicate defining the focus of elevation
 * @param weight factor influencing the strength of elevation towards the focused area (default: 2 - quadratic drop
 * with distance)
 */
void Restructure(Map& map, const std::function<bool(const MapPoint&)>& predicate, double weight = 2.);

/**
 * Resets the sea level to "0" by setting all sea nodes to "0" height and scaling the remaining nodes to a range
 * between "1" and maximum height of the map.
 *
 * @param map reference to the map
 * @param seaLevel maximum height sea can reach
 */
void ResetSeaLevel(Map& map, RandomUtility& rnd, unsigned seaLevel);

} // namespace rttr::mapGenerator
