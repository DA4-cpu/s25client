// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "RttrForeachPt.h"
#include "mapGenerator/Climate.h"
#include "mapGenerator/MapSettings.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <cmath>

using namespace rttr::mapGenerator;

BOOST_AUTO_TEST_SUITE(ClimateTests)

BOOST_AUTO_TEST_CASE(Means_increase_from_cold_to_hot_and_dry_to_humid)
{
    BOOST_TEST(GetTemperatureMean(ClimateTemperature::Temperate) == 50.);
    BOOST_TEST(GetTemperatureMean(ClimateTemperature::Cold) < GetTemperatureMean(ClimateTemperature::Cool));
    BOOST_TEST(GetTemperatureMean(ClimateTemperature::Cool) < GetTemperatureMean(ClimateTemperature::Temperate));
    BOOST_TEST(GetTemperatureMean(ClimateTemperature::Temperate) < GetTemperatureMean(ClimateTemperature::Warm));
    BOOST_TEST(GetTemperatureMean(ClimateTemperature::Warm) < GetTemperatureMean(ClimateTemperature::Hot));
    BOOST_TEST(GetHumidityMean(ClimateHumidity::Temperate) == 50.);
    BOOST_TEST(GetHumidityMean(ClimateHumidity::Dry) < GetHumidityMean(ClimateHumidity::Temperate));
    BOOST_TEST(GetHumidityMean(ClimateHumidity::Temperate) < GetHumidityMean(ClimateHumidity::Humid));
}

BOOST_AUTO_TEST_CASE(InverseNormalCDF_matches_known_values)
{
    BOOST_TEST(InverseNormalCDF(0.5) == 0., boost::test_tools::tolerance(1e-6));
    BOOST_TEST(InverseNormalCDF(0.8413447) == 1., boost::test_tools::tolerance(1e-4));
    BOOST_TEST(InverseNormalCDF(0.0227501) == -2., boost::test_tools::tolerance(1e-4));
    BOOST_TEST(InverseNormalCDF(0.9986501) == 3., boost::test_tools::tolerance(1e-4));
}

BOOST_AUTO_TEST_CASE(GenerateClimateMap_is_normally_distributed_around_the_mean)
{
    RandomUtility rnd(1337);
    const MapExtent size(128, 128);
    for(const double mean : {27.5, 50., 72.5})
    {
        const auto climate = GenerateClimateMap(rnd, size, 40, mean, 15.);
        double sum = 0.;
        unsigned within1 = 0, within2 = 0;
        RTTR_FOREACH_PT(MapPoint, size)
        {
            BOOST_TEST_REQUIRE(climate[pt] <= 100u);
            sum += climate[pt];
            const double diff = std::abs(climate[pt] - mean);
            within1 += diff <= 15.5 ? 1 : 0;
            within2 += diff <= 30.5 ? 1 : 0;
        }
        const double numNodes = size.x * size.y;
        BOOST_TEST(sum / numNodes == mean, boost::test_tools::tolerance(0.05));
        // Most of the map is close to the mean, extreme values are rare
        BOOST_TEST(within1 / numNodes == 0.68, boost::test_tools::tolerance(0.05));
        BOOST_TEST(within2 / numNodes >= 0.93);
    }
}

BOOST_AUTO_TEST_CASE(GenerateClimateMap_is_smooth)
{
    RandomUtility rnd(7);
    const MapExtent size(64, 64);
    const auto climate = GenerateClimateMap(rnd, size, 30, 50., 15.);
    double sumDiff = 0.;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        sumDiff += std::abs(climate[pt] - climate[climate.GetNeighbour(pt, Direction::East)]);
    }
    // Neighbors differ only little on average
    BOOST_TEST(sumDiff / (size.x * size.y) < 3.);
}

BOOST_AUTO_TEST_CASE(GenerateClimateMap_with_zero_spread_is_constant)
{
    RandomUtility rnd(3);
    const auto climate = GenerateClimateMap(rnd, MapExtent(16, 16), 10, 40., 0.);
    BOOST_TEST(*std::min_element(climate.begin(), climate.end()) == 40u);
    BOOST_TEST(*std::max_element(climate.begin(), climate.end()) == 40u);
}

BOOST_AUTO_TEST_SUITE_END()
