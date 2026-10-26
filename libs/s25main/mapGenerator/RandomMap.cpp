// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/RandomMap.h"
#include "mapGenerator/Climate.h"
#include "mapGenerator/Harbors.h"
#include "mapGenerator/HeadQuarters.h"
#include "mapGenerator/Landscape.h"
#include "mapGenerator/Preview.h"
#include "mapGenerator/Resources.h"
#include "mapGenerator/Zones.h"
#include "mapGenerator/Terrain.h"
#include "mapGenerator/TextureHelper.h"
#include "mapGenerator/Textures.h"

#include "lua/GameDataLoader.h"
#include "libsiedler2/libsiedler2.h"

#include <random>
#include <stdexcept>
#include <string>

namespace rttr::mapGenerator {

unsigned GetMaximumHeight(const MapExtent& size)
{
    const unsigned combinedSize = size.x * size.y;
    if(combinedSize <= 64 * 64)
        return 32;
    else if(combinedSize <= 128 * 128)
        return 64;
    else if(combinedSize <= 256 * 256)
        return 128;
    else if(combinedSize <= 512 * 512)
        return 150;
    else if(combinedSize <= 1024 * 1024)
        return 200;
    else
        return 60;
}

unsigned GetCoastline(const MapExtent& size)
{
    const unsigned combinedSize = size.x * size.y;
    if(combinedSize <= 128 * 128)
        return 1;
    else if(combinedSize <= 512 * 512)
        return 2;
    else if(combinedSize <= 1024 * 1024)
        return 3;
    else
        return 4;
}

unsigned GetIslandRadius(const MapExtent& size)
{
    const unsigned combinedSize = size.x * size.y;
    if(combinedSize <= 128 * 128)
        return 2;
    else if(combinedSize <= 256 * 256)
        return 3;
    else if(combinedSize <= 512 * 512)
        return 4;
    else if(combinedSize <= 1024 * 1024)
        return 5;
    else
        return 6;
}

unsigned GetIslandSize(const MapExtent& size)
{
    const unsigned combinedSize = size.x * size.y;
    if(combinedSize <= 64 * 64)
        return 200;
    else if(combinedSize <= 128 * 128)
        return 400;
    else if(combinedSize <= 256 * 256)
        return 600;
    else if(combinedSize <= 512 * 512)
        return 900;
    else
        return 1200;
}

unsigned GetSmoothRadius(const MapExtent& size)
{
    const unsigned combinedSize = size.x * size.y;
    if(combinedSize <= 128 * 128)
        return 2;
    else if(combinedSize <= 256 * 256)
        return 3;
    else if(combinedSize <= 512 * 512)
        return 4;
    else if(combinedSize <= 1024 * 1024)
        return 6;
    else
        return 7;
}

unsigned GetSmoothIterations(const MapExtent& size)
{
    const unsigned combinedSize = size.x * size.y;
    if(combinedSize <= 64 * 64)
        return 10;
    else if(combinedSize <= 128 * 128)
        return 11;
    else if(combinedSize <= 256 * 256)
        return 9;
    else if(combinedSize <= 512 * 512)
        return 12;
    else if(combinedSize <= 1024 * 1024)
        return 15;
    else
        return 13;
}

void SmoothHeightMap(NodeMapBase<uint8_t>& z, const ValueRange<uint8_t>& range)
{
    int radius = GetSmoothRadius(z.GetSize());
    int iterations = GetSmoothIterations(z.GetSize());

    Smooth(iterations, radius, z);
    Scale(z, range.minimum, range.maximum);
}

RandomMap::RandomMap(RandomUtility& rnd, Map& map, const MapGenDesc& desc)
    : rnd_(rnd), map_(map), desc_(desc),
      texturizer_(map.z, map.climate, map.temperature, map.getTextures(), map.textureMap)
{}

void RandomMap::Create(const MapSettings& settings)
{
    settings_ = settings;
    const MapExtent size = settings.size;
    map_.z.Resize(size, map_.height.minimum);

    // 1) Land, water and mountains (heights) like the online generator, then rivers flowing downhill
    const LandscapeLevels levels = CreateLandscape(map_, rnd_, settings_);
    const auto rivers = CreateRivers(map_, rnd_, settings_.rivers, levels);

    // 2) Climate: humidity and temperature are independent of the height, but it gets colder further up
    ComputeHeightPercent(map_);
    map_.climate = GenerateClimateMap(rnd_, size, settings_.climateZoneSize, 2., GetHumidityRange(settings_.climateHumidity));
    map_.temperature =
      GenerateClimateMap(rnd_, size, settings_.climateZoneSize, 2., GetTemperatureRange(settings_.climateTemperature));
    ApplyHeightCooling(map_, desc_.climate.heightCooling);

    // 3) Textures by height and climate, harbors and HQs
    texturizer_.AddTextures(levels.mountainLevel, GetCoastline(size));
    PlaceHarbors(map_, rivers);
    PlaceHeadquartersOnLandmasses(map_, rnd_, map_.players, settings_.mountainDistance);
    // Heights changed a bit around the HQs
    ComputeHeightPercent(map_);

    // 4) Zones (each exactly once, connected), terrain types and then everything on the map according to lua
    map_.zones = GenerateZones(rnd_, size, std::max<unsigned>(1u, static_cast<unsigned>(desc_.zones.size())));
    ComputeTerrainFlags(map_, desc_.climate.nearWaterDistance, desc_.climate.freshwaterMaxSize);
    PlaceResources(map_, rnd_, settings_, desc_);
}

Map GenerateRandomMap(RandomUtility& rnd, const WorldDescription& worldDesc, const MapSettings& settings)
{
    constexpr unsigned maxAttempts = 5;
    const auto height = GetMaximumHeight(settings.size);
    for(unsigned attempt = 1;; ++attempt)
    {
        Map map(settings.size, settings.numPlayers, worldDesc, settings.type, height);
        RandomMap randomMap(rnd, map, worldDesc.mapGen);
        try
        {
            randomMap.Create(settings);
            return map;
        } catch(const std::runtime_error&)
        {
            // E.g. no space for all HQs: Try again with other random numbers
            if(attempt >= maxAttempts)
                throw;
        }
    }
}

boost::filesystem::path GetPreviewImagePath(const boost::filesystem::path& mapPath)
{
    return mapPath.parent_path() / (mapPath.stem().string() + "_Vorschau.bmp");
}

uint32_t CreateRandomMap(const boost::filesystem::path& filePath, const MapSettings& settings)
{
    WorldDescription worldDesc;
    loadGameData(worldDesc);

    uint32_t seed = settings.seed;
    while(seed == 0)
        seed = std::random_device{}();
    RandomUtility rnd(seed);

    Map map = GenerateRandomMap(rnd, worldDesc, settings);
    map.name = "Zufall " + std::to_string(seed);
    if(libsiedler2::Write(filePath, map.CreateArchiv()) != 0)
        throw std::runtime_error("Could not write the map file " + filePath.string());
    if(worldDesc.mapGen.climate.previewImages)
        WritePreviewImage(map, worldDesc.mapGen, GetPreviewImagePath(filePath));
    return seed;
}

} // namespace rttr::mapGenerator
