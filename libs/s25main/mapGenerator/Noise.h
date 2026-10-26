// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/RandomUtility.h"
#include "world/NodeMapBase.h"
#include <vector>

namespace rttr::mapGenerator {

/// Visual position of a node: Rows are only half as far apart as columns (TR_H = TR_W / 2) and odd rows are shifted
/// by half a column. Using this for distances and noise makes features look round in the game.
struct VisualPos
{
    double x, y;
};
VisualPos GetVisualPos(const MapPoint& pt);
/// Visual size of the map (period of the wrapped map in visual coordinates)
VisualPos GetVisualSize(const MapExtent& size);
/// Visual distance between two nodes on the wrapped map
double GetVisualDistance(const MapPoint& p1, const MapPoint& p2, const MapExtent& size);

/// Gradient noise which wraps around (tileable) with the given number of lattice cells per direction
class TileableNoise
{
public:
    TileableNoise(RandomUtility& rnd, unsigned cellsX, unsigned cellsY);
    /// Noise value at the lattice coordinates (u, v), roughly in [-1, 1]. Wraps every cellsX/cellsY
    double Get(double u, double v) const;

private:
    unsigned cellsX_, cellsY_;
    std::vector<double> gradX_, gradY_;
    double Dot(int cellX, int cellY, double dx, double dy) const;
};

/**
 * Fractal noise (several octaves of TileableNoise) for every node of the map. The result wraps around the map edges.
 *
 * @param rnd random number generator
 * @param size map size
 * @param featureSize approximate size of the largest features in (visual) nodes
 * @param octaves number of octaves, each with twice the frequency of the previous one
 * @param persistence amplitude factor between two octaves (higher = rougher)
 * @returns raw noise values (not normalized)
 */
NodeMapBase<double> CreateFractalNoise(RandomUtility& rnd, const MapExtent& size, double featureSize,
                                       unsigned octaves = 4, double persistence = 0.5);

/**
 * Replaces each value by its rank in [0, 1): The result is uniformly distributed but keeps the spatial structure.
 * A threshold t then selects exactly the share t of all nodes.
 */
NodeMapBase<double> ToUniform(const NodeMapBase<double>& values);

} // namespace rttr::mapGenerator
