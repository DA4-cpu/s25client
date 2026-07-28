// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Textures.h"
#include "mapGenerator/Algorithms.h"
#include "mapGenerator/TextureHelper.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace rttr::mapGenerator {

void TextureMap::Set(const MapPoint& pt, DescIdx<TerrainDesc> texture)
{
    const auto& triangles = GetTriangles(pt, textures_.GetSize());

    for(const Triangle& triangle : triangles)
    {
        Set(triangle, texture);
    }
}

void TextureMap::Set(const Triangle& triangle, DescIdx<TerrainDesc> texture)
{
    if(triangle.rsu)
    {
        textures_[triangle.position].rsu = texture;
    } else
    {
        textures_[triangle.position].lsd = texture;
    }
}

DescIdx<TerrainDesc> TextureMap::Get(const Triangle& triangle) const
{
    if(triangle.rsu)
    {
        return textures_[triangle.position].rsu;
    } else
    {
        return textures_[triangle.position].lsd;
    }
}

void Texturizer::ApplyTexturingByHeightMap(unsigned mountainLevel)
{
    if(seaLevel_ + 2 > mountainLevel)
    {
        throw std::invalid_argument("sea level must be below mountain level by at least 2");
    }

    const uint8_t maxZ = *std::max_element(z_.begin(), z_.end());

    const auto waterTexture = textureMap_.Find(IsShipableWater);
    const auto landTextures = textureMap_.FindAll(IsBuildableLand);
    auto mountainTextures = textureMap_.FindAll(IsMinableMountain);
    mountainTextures.push_back(textureMap_.Find(IsSnowOrLava));

    const MapExtent size = z_.GetSize();
    const auto& z = z_;
    const auto& climate = climate_;
    const auto& temperature = temperature_;

    // Assumptions (same as before):
    // 1) sum of 3 height/climate/temperature values is always < 256 (1 byte)
    // 2) we always want to round to the next integer (ceil)
    auto interpolate = [&size](const NodeMapBase<uint8_t>& values, const Triangle& triangle) {
        const auto& edges = GetTriangleEdges(triangle, size);
        return static_cast<uint8_t>(
          std::ceil(static_cast<double>(values[edges[0]] + values[edges[1]] + values[edges[2]]) / 3));
    };

    // Picks the buildable-land terrain whose (humidity, temperature) is closest (squared distance)
    // to the given local climate/temperature reading - a simple 2D "biome diagram" lookup, e.g. a
    // point that is both dry and hot ends up close to a desert/savanna terrain, while a point that
    // is wet and warm ends up close to a swamp, regardless of terrains in between.
    auto pickLandTexture = [&](uint8_t climateValue, uint8_t temperatureValue) {
        DescIdx<TerrainDesc> best;
        int bestDistance = std::numeric_limits<int>::max();
        for(const auto& candidate : landTextures)
        {
            const TerrainDesc& desc = textureMap_.GetDesc(candidate);
            const int dHumidity = static_cast<int>(desc.humidity) - climateValue;
            const int dTemperature = static_cast<int>(desc.temperature) - temperatureValue;
            const int distance = dHumidity * dHumidity + dTemperature * dTemperature;
            if(distance < bestDistance)
            {
                bestDistance = distance;
                best = candidate;
            }
        }
        return best;
    };

    // Combines the height map with the climate/temperature maps into a texture: the height decides
    // *which band* a point falls into (water / buildable land / mountain), exactly as before. But
    // *within* the land band, the texture is now picked by local humidity and temperature instead of
    // by height, so e.g. a desert region and a swamp region can appear at the same altitude.
    // Mountain/snow/lava textures stay purely height-driven, same as before.
    auto pickTexture = [&](const Triangle& triangle) {
        const uint8_t zValue = interpolate(z, triangle);

        if(zValue <= seaLevel_)
        {
            return waterTexture;
        }
        if(zValue < mountainLevel)
        {
            const uint8_t climateValue = interpolate(climate, triangle);
            const uint8_t temperatureValue = interpolate(temperature, triangle);
            return pickLandTexture(climateValue, temperatureValue);
        }

        const ValueRange<uint8_t> mountainRange(mountainLevel, maxZ);
        return mountainTextures[MapValueToIndex(zValue, mountainRange, mountainTextures.size())];
    };

    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(!textures_[pt].rsu)
            textures_[pt].rsu = pickTexture(Triangle(true, pt));
        if(!textures_[pt].lsd)
            textures_[pt].lsd = pickTexture(Triangle(false, pt));
    }
}

void Texturizer::ApplyCoastTexturing(const std::vector<MapPoint>& coast, unsigned width)
{
    std::set<MapPoint, MapPointLess> visited;
    std::copy(coast.begin(), coast.end(), std::inserter(visited, visited.begin()));

    // setup transition textures from water to land
    auto sand = textureMap_.Find(IsCoastTerrain);
    auto sandGrass = textureMap_.FindAll(IsBuildableCoast);
    textureMap_.Sort(sandGrass, ByHumidity);
    std::array<DescIdx<TerrainDesc>, 1> transitionTextures{sand};

    // exclude mountain & water textures from being replaced by water-land transition
    std::set<DescIdx<TerrainDesc>> excludedTextures;
    const auto mountainTextures = textureMap_.FindAll(IsMountainOrSnowOrLava);
    const auto water = textureMap_.Find(IsShipableWater);
    excludedTextures.insert(mountainTextures.begin(), mountainTextures.end());
    excludedTextures.insert(water);

    for(unsigned i = 0; i < transitionTextures.size(); ++i)
    {
        // for nodes covered by water & coast, replace only neighboring coast textures
        unsigned appliedWidth = width - (i == 0 ? 1 : 0);
        ReplaceTextures(textures_, appliedWidth, visited, transitionTextures[i], excludedTextures);

        // don't replace already applied transition textures
        excludedTextures.insert(transitionTextures[i]);
    }
}

void Texturizer::ApplyMountainTransitions(const std::vector<MapPoint>& mountainFoot)
{
    std::set<DescIdx<TerrainDesc>> excludedTextures{textureMap_.Find(IsShipableWater)};

    const auto& mountainTextures = textureMap_.FindAll(IsMountainOrSnowOrLava);

    for(const auto& mountainTexture : mountainTextures)
    {
        excludedTextures.insert(mountainTexture);
    }

    auto mountainTransition = textureMap_.Find(IsBuildableMountain);

    for(const MapPoint& point : mountainFoot)
    {
        ReplaceTextureForPoint(textures_, point, mountainTransition, excludedTextures);
    }
}

void Texturizer::ApplyMountainWaterTransitions(const std::vector<MapPoint>& transitions)
{
    std::set<MapPoint, MapPointLess> nodes;
    nodes.insert(transitions.begin(), transitions.end());
    const auto water = textureMap_.Find(IsWater);
    const auto boulder = textureMap_.Find(IsBuildableMountain);
    const auto swamp = textureMap_.Find(IsSwamp);
    ReplaceTextures(textures_, 0, nodes, swamp, {water});
    ReplaceTextures(textures_, 1, nodes, boulder, {swamp, water});
}

void Texturizer::AddTextures(unsigned mountainLevel, unsigned coastline)
{
    ApplyTexturingByHeightMap(mountainLevel);

    auto coast = SelectPoints(
      [this](const MapPoint& pt) { return this->textureMap_.Any(pt, IsLand) && this->textureMap_.Any(pt, IsWater); },
      this->textures_.GetSize());

    ApplyCoastTexturing(coast, 1); // small for tiny rivers

    auto isRiver = [this](const MapPoint& pt) {
        auto suroundedByWater = [this](const MapPoint& pt) { return this->textureMap_.All(pt, IsWater); };
        return !helpers::contains_if(this->textures_.GetNeighbours(pt), suroundedByWater);
    };
    helpers::erase_if(coast, isRiver);

    ApplyCoastTexturing(coast, coastline);

    const auto mountainFoot = SelectPoints(
      [this](const MapPoint& pt) {
          return this->textureMap_.Any(pt, IsMinableMountain) && !this->textureMap_.All(pt, IsMountainOrSnowOrLava);
      },
      this->textures_.GetSize());

    ApplyMountainTransitions(mountainFoot);

    const auto moutainWaterTransition = SelectPoints(
      [this](const MapPoint& pt) {
          return this->textureMap_.Any(pt, IsMinableMountain) && this->textureMap_.Any(pt, IsWater);
      },
      this->textures_.GetSize());

    ApplyMountainWaterTransitions(moutainWaterTransition);
}

void ReplaceTextureForPoint(NodeMapBase<TexturePair>& textures, const MapPoint& point, DescIdx<TerrainDesc> texture,
                            const std::set<DescIdx<TerrainDesc>>& excluded)
{
    auto triangles = GetTriangles(point, textures.GetSize());

    for(const Triangle& triangle : triangles)
    {
        auto& pair = textures[triangle.position];
        auto& currentTexture = triangle.rsu ? pair.rsu : pair.lsd;

        if(!helpers::contains(excluded, currentTexture))
        {
            currentTexture = texture;
        }
    }
}

void ReplaceTextures(NodeMapBase<TexturePair>& textures, unsigned radius, std::set<MapPoint, MapPointLess>& nodes,
                     DescIdx<TerrainDesc> texture, const std::set<DescIdx<TerrainDesc>>& excluded)
{
    if(radius == 0)
    {
        for(const MapPoint& pt : nodes)
        {
            ReplaceTextureForPoint(textures, pt, texture, excluded);
        }

        return;
    }

    std::set<MapPoint, MapPointLess> initialNodes(nodes);

    for(const MapPoint& pt : initialNodes)
    {
        auto points = textures.GetPointsInRadius(pt, radius);

        for(const MapPoint& p : points)
        {
            ReplaceTextureForPoint(textures, p, texture, excluded);
            nodes.insert(p);
        }
    }
}

} // namespace rttr::mapGenerator
