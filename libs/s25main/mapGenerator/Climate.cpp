// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Climate.h"
#include "helpers/containerUtils.h"
#include "mapGenerator/Algorithms.h"
#include "mapGenerator/RandomMap.h" // GetSmoothRadius / GetSmoothIterations
#include "mapGenerator/Terrain.h"   // generic Restructure()

#include <algorithm>
#include <set>

namespace rttr::mapGenerator {

NodeMapBase<uint8_t> GenerateClimateMap(RandomUtility& rnd, const MapExtent& size, unsigned zoneSize, double weight)
{
    NodeMapBase<uint8_t> climate;

    // start at "medium humid" everywhere - mirrors how map.z starts at a mid-range defaultHeight
    // in RandomMap::Create() before Restructure() reshapes it
    climate.Resize(size, 50);

    const ValueRange<uint8_t> humidityRange(0, 100);

    // scatter roughly one climate "pole" per zoneSize x zoneSize nodes across the whole map - same
    // idea as CreateWaterMap()/CreateIsland() using a fixed number of focus points instead of a
    // flat per-node chance, which avoids the chance rounding down to 0 on small maps
    const unsigned totalNodes = static_cast<unsigned>(size.x) * size.y;
    const unsigned numPoles = std::min(totalNodes, std::max(2u, totalNodes / (zoneSize * zoneSize)));
    std::set<MapPoint, MapPointLess> poles;
    while(poles.size() < numPoles)
    {
        poles.insert(rnd.Point(size));
    }

    Restructure(
      climate, humidityRange, [&poles](const MapPoint& pt) { return helpers::contains(poles, pt); }, weight);

    // same smoothing as the height map, so climate zones get soft, natural-looking borders instead
    // of sharp rings around each pole
    Smooth(GetSmoothIterations(size), GetSmoothRadius(size), climate);
    Scale(climate, humidityRange.minimum, humidityRange.maximum);

    return climate;
}

} // namespace rttr::mapGenerator
