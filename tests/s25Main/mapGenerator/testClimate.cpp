// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Climate.h"
#include "mapGenerator/MapSettings.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>

using namespace rttr::mapGenerator;

namespace {
double Midpoint(const ValueRange<uint8_t>& range)
{
    return (static_cast<double>(range.minimum) + range.maximum) / 2.;
}
} // namespace

BOOST_AUTO_TEST_SUITE(ClimateTests)

BOOST_AUTO_TEST_CASE(GetTemperatureRange_temperate_reproduces_the_original_unbiased_range)
{
    const auto range = GetTemperatureRange(ClimateTemperature::Temperate);
    BOOST_TEST(range.minimum == 0);
    BOOST_TEST(range.maximum == 100);
}

BOOST_AUTO_TEST_CASE(GetTemperatureRange_midpoint_increases_from_cold_to_hot)
{
    // Each preset should shift the average generated value further towards the hot end than the
    // previous one - this is what makes the UI preset (Kalt/Kühl/Gemäßigt/Warm/Heiß) meaningful.
    const auto cold = GetTemperatureRange(ClimateTemperature::Cold);
    const auto cool = GetTemperatureRange(ClimateTemperature::Cool);
    const auto temperate = GetTemperatureRange(ClimateTemperature::Temperate);
    const auto warm = GetTemperatureRange(ClimateTemperature::Warm);
    const auto hot = GetTemperatureRange(ClimateTemperature::Hot);

    BOOST_TEST(Midpoint(cold) < Midpoint(cool));
    BOOST_TEST(Midpoint(cool) < Midpoint(temperate));
    BOOST_TEST(Midpoint(temperate) < Midpoint(warm));
    BOOST_TEST(Midpoint(warm) < Midpoint(hot));
}

BOOST_AUTO_TEST_CASE(GetHumidityRange_temperate_reproduces_the_original_unbiased_range)
{
    const auto range = GetHumidityRange(ClimateHumidity::Temperate);
    BOOST_TEST(range.minimum == 0);
    BOOST_TEST(range.maximum == 100);
}

BOOST_AUTO_TEST_CASE(GetHumidityRange_midpoint_increases_from_dry_to_humid)
{
    // "Trocken" -> "Gemäßigt" -> "Feucht" should shift the average generated value upwards.
    const auto dry = GetHumidityRange(ClimateHumidity::Dry);
    const auto temperate = GetHumidityRange(ClimateHumidity::Temperate);
    const auto humid = GetHumidityRange(ClimateHumidity::Humid);

    BOOST_TEST(Midpoint(dry) < Midpoint(temperate));
    BOOST_TEST(Midpoint(temperate) < Midpoint(humid));
}

BOOST_AUTO_TEST_CASE(GenerateClimateMap_stays_within_the_requested_output_range)
{
    RandomUtility rnd(1337);
    const MapExtent size(16, 16);
    const auto range = GetTemperatureRange(ClimateTemperature::Cold);

    const auto climate = GenerateClimateMap(rnd, size, 4, 2., range);

    RTTR_FOREACH_PT(MapPoint, size)
    {
        BOOST_TEST_REQUIRE(climate[pt] >= range.minimum);
        BOOST_TEST_REQUIRE(climate[pt] <= range.maximum);
    }
}

BOOST_AUTO_TEST_CASE(GenerateClimateMap_fills_the_requested_output_range)
{
    // Scale() always stretches the generated variation to fill the full output range - this is
    // what makes the "climate zone size" slider produce visible zones instead of a flat value and
    // what makes the min/max of a preset (e.g. "Kalt": [0, 55]) actually get used.
    RandomUtility rnd(1337);
    const MapExtent size(32, 32);
    const ValueRange<uint8_t> range(20, 80);

    const auto climate = GenerateClimateMap(rnd, size, 8, 2., range);

    uint8_t minVal = 255;
    uint8_t maxVal = 0;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        minVal = std::min(minVal, climate[pt]);
        maxVal = std::max(maxVal, climate[pt]);
    }

    BOOST_TEST(minVal == range.minimum);
    BOOST_TEST(maxVal == range.maximum);
}

BOOST_AUTO_TEST_CASE(GenerateClimateMap_defaults_to_the_full_range)
{
    // Backwards-compat check: calling without an explicit outputRange must behave exactly like
    // before the climate presets were introduced.
    RandomUtility rnd(1337);
    const MapExtent size(32, 32);

    const auto climate = GenerateClimateMap(rnd, size, 8);

    uint8_t minVal = 255;
    uint8_t maxVal = 0;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        minVal = std::min(minVal, climate[pt]);
        maxVal = std::max(maxVal, climate[pt]);
    }

    BOOST_TEST(minVal == 0);
    BOOST_TEST(maxVal == 100);
}

BOOST_AUTO_TEST_SUITE_END()
