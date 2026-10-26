// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/Map.h"
#include "mapGenerator/MapSettings.h"
#include "mapGenerator/RandomUtility.h"
#include "mapGenerator/Rivers.h"
#include "mapGenerator/Textures.h"

#include <boost/filesystem/path.hpp>

struct MapGenDesc;

namespace rttr::mapGenerator {

unsigned GetMaximumHeight(const MapExtent& size);
unsigned GetCoastline(const MapExtent& size);
unsigned GetIslandRadius(const MapExtent& size);
unsigned GetIslandSize(const MapExtent& size);
unsigned GetSmoothRadius(const MapExtent& size);
unsigned GetSmoothIterations(const MapExtent& size);

void SmoothHeightMap(NodeMapBase<uint8_t>& z, const ValueRange<uint8_t>& range);

class RandomMap
{
private:
    RandomUtility& rnd_;
    Map& map_;
    const MapGenDesc& desc_;
    Texturizer texturizer_;
    MapSettings settings_;

public:
    RandomMap(RandomUtility& rnd, Map& map, const MapGenDesc& desc);
    /**
     * Creates the map: land and water like the online map generator of the RttR website (CreateLandscape), rivers,
     * climate (temperature, humidity, height), textures, harbors, HQs, zones and finally all objects and resources
     * according to the lua data (PlaceResources).
     */
    void Create(const MapSettings& settings);
};

/// Generate a map. Retries a few times (with further random numbers) if no valid HQ positions are found
Map GenerateRandomMap(RandomUtility& rnd, const WorldDescription& worldDesc, const MapSettings& settings);
/**
 * Generate a map and save it. Uses settings.seed for the random numbers or a random seed if it is 0.
 * Also saves a preview image next to the map (<name>_Vorschau.bmp) if enabled in the lua data.
 *
 * @returns the seed used
 */
uint32_t CreateRandomMap(const boost::filesystem::path& filePath, const MapSettings& settings);
/// Path of the preview image of a generated map
boost::filesystem::path GetPreviewImagePath(const boost::filesystem::path& mapPath);

} // namespace rttr::mapGenerator
