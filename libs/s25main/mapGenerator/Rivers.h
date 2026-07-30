// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/Map.h"
#include "mapGenerator/RandomUtility.h"

namespace rttr::mapGenerator {

using River = std::set<MapPoint, MapPointLess>;

/**
 * Creates a small stream of water for the specified map, starting at "source" and flowing towards the sea.
 *
 * At every step the stream looks at all 6 neighboring nodes (except the one it just came from, unless that is
 * the only way to keep moving towards the sea) and picks among those that get it strictly closer to the sea -
 * i.e. it always makes guaranteed progress and reliably reaches an actual sea/lake node (a node at the map's
 * minimum height) rather than petering out on dry land or stopping short of the water. Where more than one
 * direction gets it equally close, it's biased towards the neighbor with the lower terrain height, purely for a
 * more natural-looking meander - "length" needs to be at least the actual distance from "source" to the sea
 * *plus one* (one node is drawn per step, including the starting node itself, so reaching a node at distance D
 * takes D+1 drawn steps) for the stream to be guaranteed to arrive; see RandomMap::CreateRivers, which sizes it
 * with a comfortable safety margin on top of that. "length" exists mainly as a safety cap for degenerate inputs
 * (e.g. a "source" that cannot reach any sea node at all).
 *
 * @param rnd random number generator used to pick directions and split points
 * @param map reference to the map to place the stream on
 * @param distanceToSea distance (in nodes) from every point of the map to the nearest node at the map's minimum
 *        height, e.g. as returned by DistancesTo(map.size, isSeaLevel) - see RandomMap::CreateRivers
 * @param source source of the stream
 * @param direction initial direction of the stream, used only to seed which direction counts as "just came
 *        from" for the very first step
 * @param length maximum length of the ditch in triangles
 * @param splitRate chance of the ditch to split up into two streams (0 by default, a number between 0 and 100)
 *
 * @returns all nodes the stream (incl. split up streams) is covering.
 */
River CreateStream(RandomUtility& rnd, Map& map, const NodeMapBase<unsigned>& distanceToSea, const MapPoint& source,
                   Direction direction, unsigned length, unsigned splitRate = 0);

} // namespace rttr::mapGenerator
