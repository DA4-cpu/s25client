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
#include <stdexcept>

namespace rttr::mapGenerator {

ValueRange<uint8_t> GetTemperatureRange(ClimateTemperature temperature)
{
    switch(temperature)
    {
        // clang-format off
        case ClimateTemperature::Cold:      return ValueRange<uint8_t>(0, 55);
        case ClimateTemperature::Cool:      return ValueRange<uint8_t>(0, 75);
        case ClimateTemperature::Temperate: return ValueRange<uint8_t>(0, 100); // original, unbiased range
        case ClimateTemperature::Warm:      return ValueRange<uint8_t>(25, 100);
        case ClimateTemperature::Hot:       return ValueRange<uint8_t>(45, 100);
        // clang-format on
    }
    throw std::invalid_argument("Invalid climate temperature");
}

ValueRange<uint8_t> GetHumidityRange(ClimateHumidity humidity)
{
    switch(humidity)
    {
        // clang-format off
        case ClimateHumidity::Humid:      return ValueRange<uint8_t>(45, 100);
        case ClimateHumidity::Temperate:  return ValueRange<uint8_t>(0, 100); // original, unbiased range
        case ClimateHumidity::Dry:        return ValueRange<uint8_t>(0, 55);
        // clang-format on
    }
    throw std::invalid_argument("Invalid climate humidity");
}

NodeMapBase<uint8_t> GenerateClimateMap(RandomUtility& rnd, const MapExtent& size, unsigned zoneSize, double weight,
                                         const ValueRange<uint8_t>& outputRange)
{
    NodeMapBase<uint8_t> climate;

    // start at the middle of the output range everywhere - mirrors how map.z starts at a mid-range
    // defaultHeight in RandomMap::Create() before Restructure() reshapes it
    climate.Resize(size, static_cast<uint8_t>(outputRange.minimum + outputRange.GetDifference() / 2));

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
      climate, outputRange, [&poles](const MapPoint& pt) { return helpers::contains(poles, pt); }, weight);

    // same smoothing as the height map, so climate zones get soft, natural-looking borders instead
    // of sharp rings around each pole
    Smooth(GetSmoothIterations(size), GetSmoothRadius(size), climate);
    Scale(climate, outputRange.minimum, outputRange.maximum);

    return climate;
}

} // namespace rttr::mapGenerator
