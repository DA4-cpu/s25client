// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/RandomMap.h"
#include "mapGenerator/Climate.h"
#include "mapGenerator/Harbors.h"
#include "mapGenerator/HeadQuarters.h"
#include "mapGenerator/Islands.h"
#include "mapGenerator/Resources.h"
#include "mapGenerator/Terrain.h"
#include "mapGenerator/TextureHelper.h"
#include "mapGenerator/Textures.h"

#include "lua/GameDataLoader.h"
#include "libsiedler2/libsiedler2.h"

#include <algorithm>
#include <stdexcept>

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

RandomMap::RandomMap(RandomUtility& rnd, Map& map)
    : rnd_(rnd), map_(map), texturizer_(map.z, map.climate, map.temperature, map.getTextures(), map.textureMap)
{}

void RandomMap::Create(const MapSettings& settings)
{
    auto defaultHeight = map_.height.minimum + map_.height.GetDifference() / 2;

    settings_ = settings;
    map_.z.Resize(settings.size, defaultHeight);
    map_.climate =
      GenerateClimateMap(rnd_, settings.size, settings.climateZoneSize, 2., GetHumidityRange(settings.climateHumidity));
    map_.temperature = GenerateClimateMap(rnd_, settings.size, settings.climateZoneSize, 2.,
                                          GetTemperatureRange(settings.climateTemperature));

    switch(settings.style)
    {
        case MapStyle::Water: CreateWaterMap(); break;

        case MapStyle::Mixed: CreateMixedMap(); break;

        case MapStyle::Land: CreateLandMap(); break;
    }

    AddObjects(map_, rnd_, settings_);
    AddResources(map_, rnd_, settings_);
    AddAnimals(map_, rnd_);
}

std::vector<River> RandomMap::CreateRivers(const MapPoint source)
{
    std::vector<River> rivers;

    const MapExtent size = settings_.size;

    // Distance (in nodes) from every point to the closest point at the map's minimum height (i.e. the
    // sea/a lake). This lets every river get just enough of a length budget to actually reach the
    // water, instead of the previous flat "width + height" guess, which could be far too short for
    // sources near the middle of a large map and needlessly long for sources already close to the
    // coast.
    const auto distanceToSea =
      DistancesTo(size, [this](const MapPoint& pt) { return map_.z[pt] == map_.height.minimum; });

    // River sources look more natural coming from elevated terrain ("mountain springs") instead of an
    // arbitrary point anywhere on the map, so prefer picking them from the upper ~15% of the height
    // distribution.
    const auto elevationThreshold = LimitFor(map_.z, 0.85, map_.height.minimum);
    const auto elevatedPoints =
      SelectPoints([this, elevationThreshold](const MapPoint& pt) { return map_.z[pt] > elevationThreshold; }, size);

    // Keep multiple independent river sources spaced apart so they don't all start crowded into the
    // same corner of the map.
    const unsigned minSourceDistance = std::max(3u, static_cast<unsigned>(size.x + size.y) / 8);
    std::vector<MapPoint> chosenSources;

    const auto pickLandSource = [&]() {
        if(elevatedPoints.empty())
        {
            // No meaningfully elevated terrain to speak of (e.g. a tiny or perfectly flat map) - fall
            // back to picking any point, as before.
            return rnd_.Point(size);
        }

        MapPoint candidate = rnd_.RandomItem(elevatedPoints);
        for(unsigned attempt = 0; attempt < 10; ++attempt)
        {
            const bool farEnough =
              std::all_of(chosenSources.begin(), chosenSources.end(), [&](const MapPoint& existing) {
                  return map_.z.CalcDistance(candidate, existing) >= minSourceDistance;
              });
            if(farEnough)
            {
                break;
            }
            candidate = rnd_.RandomItem(elevatedPoints);
        }
        return candidate;
    };

    // For maps with a shared, given source (the central lake/mountain of mixed & water maps), every
    // river direction gets its own point close to that source instead of the exact same node, so
    // multiple rivers don't look like perfectly symmetric spokes radiating out of a single pixel.
    const unsigned jitterRadius = std::max(2u, static_cast<unsigned>(std::min(size.x, size.y)) / 20);
    const auto jitterSource = [&](const MapPoint& anchor) {
        const auto nearby = map_.z.GetPointsInRadiusWithCenter(anchor, jitterRadius);
        return nearby.empty() ? anchor : rnd_.RandomItem(nearby);
    };

    for(const auto dir : helpers::EnumRange<Direction>())
    {
        if(!rnd_.ByChance(settings_.rivers))
        {
            continue;
        }

        const MapPoint riverSource = source.isValid() ? jitterSource(source) : pickLandSource();
        chosenSources.push_back(riverSource);

        const unsigned toSea = distanceToSea[riverSource];
        // Generous but bounded budget: comfortably enough steps for a downhill-biased stream to reach
        // the sea from here, with headroom left for meandering. Falls back to the old estimate if the
        // map doesn't have any sea at all (e.g. in isolated unit tests).
        const unsigned length = (toSea == unsigned(-1)) ? (size.x + size.y) : toSea * 2 + 4;

        const unsigned splitRate = rnd_.RandomValue(0u, 2u);
        rivers.push_back(CreateStream(rnd_, map_, riverSource, dir, length, splitRate));
    }
    return rivers;
}

void RandomMap::CreateFreeIslands(unsigned waterNodes)
{
    const auto islandRadius = GetIslandRadius(map_.size);
    const auto islandAmount = static_cast<double>(settings_.islands) / 100;
    const auto maxIslandSize = GetIslandSize(map_.size);
    const auto minIslandSize = std::min(200u, maxIslandSize);
    auto islandNodes = static_cast<unsigned>(islandAmount * waterNodes);
    auto islandSize = rnd_.RandomValue(minIslandSize, maxIslandSize);
    while(islandNodes >= islandSize)
    {
        islandNodes -= islandSize;
        CreateIsland(map_, rnd_, islandSize, islandRadius, .2);
        islandSize = rnd_.RandomValue(minIslandSize, maxIslandSize);
    }
}

void RandomMap::CreateMixedMap()
{
    const auto center = rnd_.Point(map_.size);
    const unsigned maxDistance = map_.z.CalcMaxDistance();

    Restructure(map_, [this, &center, maxDistance](const MapPoint& pt) {
        auto weight = 1. - static_cast<float>(map_.z.CalcDistance(pt, center)) / maxDistance;
        auto percentage = static_cast<unsigned>(12 * weight);
        return rnd_.ByChance(percentage);
    });
    SmoothHeightMap(map_.z, map_.height);

    const double sea = 0.5;
    const double mountain = 0.1;
    const double land = 1. - sea - mountain;

    ResetSeaLevel(map_, rnd_, LimitFor(map_.z, sea, map_.height.minimum));

    const auto mountainLevel = LimitFor(map_.z, land, static_cast<uint8_t>(map_.height.minimum + 1)) + 1;
    const auto rivers = CreateRivers(center);
    const unsigned waterNodes = helpers::count(map_.z, map_.height.minimum);

    CreateFreeIslands(waterNodes);

    texturizer_.AddTextures(mountainLevel, GetCoastline(map_.size));

    PlaceHarbors(map_, rivers);
    PlaceHeadquarters(map_, rnd_, map_.players, settings_.mountainDistance);
}

void RandomMap::CreateWaterMap()
{
    const auto center = rnd_.Point(map_.size);
    const unsigned maxDistance = map_.z.CalcMaxDistance();

    Restructure(map_, [this, &center, maxDistance](const MapPoint& pt) {
        const auto weight = 1. - static_cast<float>(map_.z.CalcDistance(pt, center)) / maxDistance;
        const auto percentage = static_cast<unsigned>(15 * weight * weight);
        return rnd_.ByChance(percentage);
    });
    SmoothHeightMap(map_.z, map_.height);

    const double sea = 0.80;      // 20% of map is center island (100% - 80% water)
    const double mountain = 0.05; // 20% of center island is mountain (5% of 20% land)
    const auto seaLevel = LimitFor(map_.z, sea, map_.height.minimum);

    ResetSeaLevel(map_, rnd_, seaLevel);

    const unsigned waterNodes = helpers::count(map_.z, map_.height.minimum);
    const auto land = 1. - static_cast<double>(waterNodes) / (map_.size.x * map_.size.y) - mountain;
    const auto mountainLevel = LimitFor(map_.z, land, static_cast<uint8_t>(1)) + 1;
    const auto islandSize = GetIslandSize(map_.size);
    const auto islandRadius = GetIslandRadius(map_.size);

    std::vector<Island> islands(map_.players);

    for(unsigned i = 0; i < map_.players; i++)
    {
        islands[i] = CreateIsland(map_, rnd_, islandSize, islandRadius, .2);
    }

    if(waterNodes > map_.players * islandSize)
    {
        CreateFreeIslands(waterNodes - map_.players * islandSize);
    }

    const auto rivers = CreateRivers(center);

    texturizer_.AddTextures(mountainLevel, GetCoastline(map_.size));

    PlaceHarbors(map_, rivers);

    for(unsigned i = 0; i < map_.players; i++)
    {
        PlaceHeadquarter(map_, islands[i], settings_.mountainDistance);
    }
}

void RandomMap::CreateLandMap()
{
    Restructure(map_, [this](auto&&) { return rnd_.ByChance(5); });
    SmoothHeightMap(map_.z, map_.height);

    const double sea = rnd_.RandomDouble(0.1, 0.2);
    const double mountain = rnd_.RandomDouble(0.15, 0.4 - sea);
    const double land = 1. - sea - mountain;

    ResetSeaLevel(map_, rnd_, LimitFor(map_.z, sea, map_.height.minimum));

    const auto mountainLevel = LimitFor(map_.z, land, static_cast<uint8_t>(1)) + 1;
    CreateRivers();

    texturizer_.AddTextures(mountainLevel, GetCoastline(map_.size));

    PlaceHeadquarters(map_, rnd_, map_.players, settings_.mountainDistance);
}

Map GenerateRandomMap(RandomUtility& rnd, const WorldDescription& worldDesc, const MapSettings& settings)
{
    auto height = GetMaximumHeight(settings.size);
    Map map(settings.size, settings.numPlayers, worldDesc, settings.type, height);
    RandomMap randomMap(rnd, map);
    randomMap.Create(settings);
    return map;
}

void CreateRandomMap(const boost::filesystem::path& filePath, const MapSettings& settings)
{
    RandomUtility rnd;
    WorldDescription worldDesc;
    loadGameData(worldDesc);

    Map map = GenerateRandomMap(rnd, worldDesc, settings);
    libsiedler2::Write(filePath, map.CreateArchiv());
}

} // namespace rttr::mapGenerator
