// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/Map.h"
#include "mapGenerator/RandomUtility.h"
#include <set>
#include <vector>

namespace rttr::mapGenerator {

/// All nodes touching the water of a river
using River = std::set<MapPoint, MapPointLess>;

/// Settings for CreateRivers
struct RiverSettings
{
    /// 0..100 (slider "rivers"): At 100 % about one river per 2000 nodes
    unsigned amount = 15;
    /// Rivers start at nodes with at least this height in percent (0 = sea level, 100 = highest point)
    unsigned minHeight = 30;
    /// Shorter rivers are not created (steps along the edges of the map)
    unsigned minLength = 15;
};

/**
 * Creates rivers which flow from high sources (in the mountains or high land) down to the sea or a lake:
 * Every node gets a downstream neighbor from a "priority flood" starting at all water nodes, so following the
 * downstream neighbors always reaches the water on the lowest way (over the lowest pass if there is a basin). This is
 * the route of a river. Rivers never end in nowhere and never cross themselves. A river ends when it reaches another
 * river (tributary).
 * The water is placed like in the original generator: The course goes along the edges of the map close to the route
 * and turns by 60 degrees at every step. The 2 triangles beside each edge become water, so the river is continuous and
 * one triangle wide. The course is carved (heights never rise downstream).
 * Requires map.nodeKind and the heights. Marks the course nodes (except at the mouth) in map.riverCourse.
 *
 * @returns all nodes touching the water of each river (used to keep harbors away from rivers)
 */
std::vector<River> CreateRivers(Map& map, RandomUtility& rnd, const RiverSettings& settings);

} // namespace rttr::mapGenerator
