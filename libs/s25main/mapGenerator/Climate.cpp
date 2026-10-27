// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Climate.h"
#include "RttrForeachPt.h"
#include "mapGenerator/Noise.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rttr::mapGenerator {

double GetTemperatureMean(ClimateTemperature temperature)
{
    // Centers of the former value ranges of the presets
    switch(temperature)
    {
        case ClimateTemperature::Cold: return 27.5;
        case ClimateTemperature::Cool: return 37.5;
        case ClimateTemperature::Temperate: return 50.;
        case ClimateTemperature::Warm: return 62.5;
        case ClimateTemperature::Hot: return 72.5;
    }
    throw std::invalid_argument("Invalid climate temperature");
}

double GetHumidityMean(ClimateHumidity humidity)
{
    switch(humidity)
    {
        case ClimateHumidity::Humid: return 72.5;
        case ClimateHumidity::Temperate: return 50.;
        case ClimateHumidity::Dry: return 27.5;
    }
    throw std::invalid_argument("Invalid climate humidity");
}

double InverseNormalCDF(double p)
{
    // Rational approximation by Peter J. Acklam (relative error < 1.15e-9)
    constexpr double a1 = -3.969683028665376e+01, a2 = 2.209460984245205e+02, a3 = -2.759285104469687e+02,
                     a4 = 1.383577518672690e+02, a5 = -3.066479806614716e+01, a6 = 2.506628277459239e+00;
    constexpr double b1 = -5.447609879822406e+01, b2 = 1.615858368580409e+02, b3 = -1.556989798598866e+02,
                     b4 = 6.680131188771972e+01, b5 = -1.328068155288572e+01;
    constexpr double c1 = -7.784894002430293e-03, c2 = -3.223964580411365e-01, c3 = -2.400758277161838e+00,
                     c4 = -2.549732539343734e+00, c5 = 4.374664141464968e+00, c6 = 2.938163982698783e+00;
    constexpr double d1 = 7.784695709041462e-03, d2 = 3.224671290700398e-01, d3 = 2.445134137142996e+00,
                     d4 = 3.754408661907416e+00;
    constexpr double pLow = 0.02425;
    p = std::clamp(p, 1e-9, 1. - 1e-9);
    if(p < pLow)
    {
        const double q = std::sqrt(-2. * std::log(p));
        return (((((c1 * q + c2) * q + c3) * q + c4) * q + c5) * q + c6) / ((((d1 * q + d2) * q + d3) * q + d4) * q + 1.);
    }
    if(p > 1. - pLow)
    {
        const double q = std::sqrt(-2. * std::log(1. - p));
        return -(((((c1 * q + c2) * q + c3) * q + c4) * q + c5) * q + c6)
               / ((((d1 * q + d2) * q + d3) * q + d4) * q + 1.);
    }
    const double q = p - 0.5;
    const double r = q * q;
    return (((((a1 * r + a2) * r + a3) * r + a4) * r + a5) * r + a6) * q
           / (((((b1 * r + b2) * r + b3) * r + b4) * r + b5) * r + 1.);
}

NodeMapBase<uint8_t> GenerateClimateMap(RandomUtility& rnd, const MapExtent& size, unsigned zoneSize, double mean,
                                         double spread)
{
    // Smooth noise, made uniform by rank and then mapped to a normal distribution
    const auto uniform = ToUniform(CreateFractalNoise(rnd, size, std::max(4u, zoneSize), 3, 0.45));
    const unsigned numNodes = static_cast<unsigned>(size.x) * size.y;
    NodeMapBase<uint8_t> climate;
    climate.Resize(size, 0);
    RTTR_FOREACH_PT(MapPoint, size)
    {
        // Center of the rank interval, so the distribution is symmetric
        const double p = uniform[pt] + 0.5 / numNodes;
        const double value = mean + spread * InverseNormalCDF(p);
        climate[pt] = static_cast<uint8_t>(std::clamp(std::lround(value), 0l, 100l));
    }
    return climate;
}

} // namespace rttr::mapGenerator
