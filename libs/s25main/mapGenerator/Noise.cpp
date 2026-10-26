// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Noise.h"
#include "RttrForeachPt.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace rttr::mapGenerator {

VisualPos GetVisualPos(const MapPoint& pt)
{
    return VisualPos{pt.x + ((pt.y & 1) ? 0.5 : 0.), pt.y * 0.5};
}

VisualPos GetVisualSize(const MapExtent& size)
{
    return VisualPos{static_cast<double>(size.x), size.y * 0.5};
}

double GetVisualDistance(const MapPoint& p1, const MapPoint& p2, const MapExtent& size)
{
    const VisualPos a = GetVisualPos(p1);
    const VisualPos b = GetVisualPos(p2);
    const VisualPos period = GetVisualSize(size);
    double dx = std::abs(a.x - b.x);
    double dy = std::abs(a.y - b.y);
    dx = std::min(dx, period.x - dx);
    dy = std::min(dy, period.y - dy);
    return std::sqrt(dx * dx + dy * dy);
}

TileableNoise::TileableNoise(RandomUtility& rnd, unsigned cellsX, unsigned cellsY)
    : cellsX_(std::max(1u, cellsX)), cellsY_(std::max(1u, cellsY))
{
    const unsigned numCells = cellsX_ * cellsY_;
    gradX_.resize(numCells);
    gradY_.resize(numCells);
    constexpr double pi = 3.14159265358979323846;
    for(unsigned i = 0; i < numCells; i++)
    {
        const double angle = rnd.RandomDouble(0., 2. * pi);
        gradX_[i] = std::cos(angle);
        gradY_[i] = std::sin(angle);
    }
}

double TileableNoise::Dot(int cellX, int cellY, double dx, double dy) const
{
    const auto wrap = [](int value, unsigned period) {
        const int p = static_cast<int>(period);
        return static_cast<unsigned>(((value % p) + p) % p);
    };
    const unsigned idx = wrap(cellY, cellsY_) * cellsX_ + wrap(cellX, cellsX_);
    return gradX_[idx] * dx + gradY_[idx] * dy;
}

double TileableNoise::Get(double u, double v) const
{
    const auto fade = [](double t) { return t * t * t * (t * (t * 6. - 15.) + 10.); };
    const double fu = std::floor(u);
    const double fv = std::floor(v);
    const int x0 = static_cast<int>(fu);
    const int y0 = static_cast<int>(fv);
    const double dx = u - fu;
    const double dy = v - fv;

    const double n00 = Dot(x0, y0, dx, dy);
    const double n10 = Dot(x0 + 1, y0, dx - 1., dy);
    const double n01 = Dot(x0, y0 + 1, dx, dy - 1.);
    const double n11 = Dot(x0 + 1, y0 + 1, dx - 1., dy - 1.);

    const double wu = fade(dx);
    const double wv = fade(dy);
    const double nx0 = n00 + wu * (n10 - n00);
    const double nx1 = n01 + wu * (n11 - n01);
    // Gradient noise in 2D is within [-sqrt(0.5), sqrt(0.5)], scale to roughly [-1, 1]
    return (nx0 + wv * (nx1 - nx0)) * 1.41421356;
}

NodeMapBase<double> CreateFractalNoise(RandomUtility& rnd, const MapExtent& size, double featureSize,
                                       unsigned octaves, double persistence)
{
    NodeMapBase<double> result;
    result.Resize(size, 0.);
    const VisualPos period = GetVisualSize(size);
    featureSize = std::max(featureSize, 1.);
    double amplitude = 1.;
    for(unsigned octave = 0; octave < std::max(1u, octaves); octave++)
    {
        const double scale = static_cast<double>(1u << octave);
        const auto cellsX = static_cast<unsigned>(std::max(1., std::round(period.x / featureSize * scale)));
        const auto cellsY = static_cast<unsigned>(std::max(1., std::round(period.y / featureSize * scale)));
        // Stop when the cells get smaller than about 2 nodes, they would only add noise per node
        if(octave > 0 && (period.x / cellsX < 2. || period.y / cellsY < 1.))
            break;
        const TileableNoise noise(rnd, cellsX, cellsY);
        RTTR_FOREACH_PT(MapPoint, size)
        {
            const VisualPos pos = GetVisualPos(pt);
            result[pt] += amplitude * noise.Get(pos.x / period.x * cellsX, pos.y / period.y * cellsY);
        }
        amplitude *= persistence;
    }
    return result;
}

NodeMapBase<double> ToUniform(const NodeMapBase<double>& values)
{
    const MapExtent size = values.GetSize();
    const unsigned numNodes = static_cast<unsigned>(size.x) * size.y;
    std::vector<unsigned> order(numNodes);
    std::iota(order.begin(), order.end(), 0u);
    // Stable to be deterministic for equal values
    std::stable_sort(order.begin(), order.end(),
                     [&values](unsigned a, unsigned b) { return values[a] < values[b]; });
    NodeMapBase<double> result;
    result.Resize(size, 0.);
    for(unsigned rank = 0; rank < numNodes; rank++)
        result[order[rank]] = static_cast<double>(rank) / numNodes;
    return result;
}

} // namespace rttr::mapGenerator
