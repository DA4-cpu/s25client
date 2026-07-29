// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/Map.h"
#include "mapGenerator/RandomUtility.h"

namespace rttr::mapGenerator {

using River = std::set<MapPoint, MapPointLess>;

/**
 * Creates a small stream of water for the specified map with the specified initial direction, length and split
 * rate. Starting at "direction", the stream is free to continue straight ahead or turn by 60 degrees to either
 * side on every step (never further, and never back towards where it came from), and is biased towards
 * neighboring nodes with lower terrain height, i.e. it tends to flow downhill rather than wander randomly. It
 * stops as soon as it reaches a node at the map's minimum height (e.g. sea level) or after "length" steps,
 * whichever happens first - callers that want a stream to reliably reach the sea should size "length" generously
 * based on the actual distance from "source" to the sea (see RandomMap::CreateRivers for an example).
 *
 * @param rnd random number generator used to pick directions and split points
 * @param map reference to the map to place the stream on
 * @param source source of the stream
 * @param direction initial direction of the stream
 * @param length maximum length of the ditch in triangles
 * @param splitRate chance of the ditch to split up into two streams (0 by default, a number between 0 and 100)
 *
 * @returns all nodes the stream (incl. split up streams) is covering.
 */
River CreateStream(RandomUtility& rnd, Map& map, const MapPoint& source, Direction direction, unsigned length,
                   unsigned splitRate = 0);

} // namespace rttr::mapGenerator
