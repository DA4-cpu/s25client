// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mapGenerator/Textures.h"
#include "RttrForeachPt.h"
#include "mapGenerator/Algorithms.h"
#include "mapGenerator/TextureHelper.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace rttr::mapGenerator {

namespace {
/// Share of the mountain height range at the top covered by snow
constexpr double peakShare = 0.08;
} // namespace

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
    // Mountain textures are grouped by their temperature (e.g. greenland, wasteland and winter mountains): The group
    // closest to the local temperature is used, within the group the height decides. Snow on the peaks.
    std::map<uint8_t, std::vector<DescIdx<TerrainDesc>>> mountainGroups;
    for(const auto texture : textureMap_.FindAll(IsMinableMountain))
        mountainGroups[textureMap_.GetDesc(texture).temperature].push_back(texture);
    if(mountainGroups.empty())
        throw std::invalid_argument("no mountain textures found");
    // Only the highest peaks get snow (it can not be used for anything)
    const auto peakTexture = textureMap_.Find(IsSnowOrLava);

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
    // Water if all corners are water, mountain if most corners are mountain, land otherwise
    auto pickTexture = [&](const Triangle& triangle) {
        const auto& corners = GetTriangleEdges(triangle, size);
        unsigned numWater = 0, numMountain = 0, mountainSum = 0;
        for(const MapPoint& corner : corners)
        {
            if(z[corner] <= seaLevel_)
                ++numWater;
            else if(z[corner] >= mountainLevel)
            {
                ++numMountain;
                mountainSum += z[corner];
            }
        }

        if(numWater == corners.size())
        {
            return waterTexture;
        }
        if(numMountain < 2)
        {
            const uint8_t climateValue = interpolate(climate, triangle);
            const uint8_t temperatureValue = interpolate(temperature, triangle);
            return pickLandTexture(climateValue, temperatureValue);
        }

        const auto zValue = static_cast<uint8_t>((mountainSum + numMountain - 1) / numMountain);
        const uint8_t temperatureValue = interpolate(temperature, triangle);
        const std::vector<DescIdx<TerrainDesc>>* mountainTextures = nullptr;
        int bestDistance = std::numeric_limits<int>::max();
        for(const auto& group : mountainGroups)
        {
            const int distance = std::abs(static_cast<int>(group.first) - temperatureValue);
            if(distance < bestDistance)
            {
                bestDistance = distance;
                mountainTextures = &group.second;
            }
        }
        const int mountainHeight = std::max(0, static_cast<int>(maxZ) - static_cast<int>(mountainLevel));
        const auto peakLevel = static_cast<uint8_t>(maxZ - static_cast<int>(mountainHeight * peakShare));
        if(zValue > peakLevel)
            return peakTexture;
        const ValueRange<uint8_t> mountainRange(static_cast<uint8_t>(mountainLevel),
                                                std::max(peakLevel, static_cast<uint8_t>(mountainLevel)));
        return (*mountainTextures)[MapValueToIndex(std::min(zValue, peakLevel), mountainRange,
                                                   mountainTextures->size())];
    };

    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(!textures_[pt].rsu)
            textures_[pt].rsu = pickTexture(Triangle(true, pt));
        if(!textures_[pt].lsd)
            textures_[pt].lsd = pickTexture(Triangle(false, pt));
    }
}

void Texturizer::ApplyBeach(unsigned rows)
{
    if(rows == 0)
        return;
    const MapExtent size = textures_.GetSize();
    const auto sand = textureMap_.Find(IsCoastTerrain);
    const auto hasExcludedCorner = [this, &size](const Triangle& triangle) {
        if(!noCoast_)
            return false;
        for(const MapPoint& corner : GetTriangleEdges(triangle, size))
        {
            if((*noCoast_)[corner] & ShoreExclusion::NoBeach)
                return true;
        }
        return false;
    };
    const auto index = [&size](const Triangle& triangle) {
        return (static_cast<unsigned>(triangle.position.y) * size.x + triangle.position.x) * 2u
               + (triangle.rsu ? 1u : 0u);
    };
    std::vector<uint8_t> visited(static_cast<size_t>(size.x) * size.y * 2u, 0);

    // Rows of triangles growing from the water, each row shares a side with the previous one
    std::vector<Triangle> row;
    RTTR_FOREACH_PT(MapPoint, size)
    {
        for(const bool rsu : {true, false})
        {
            const Triangle triangle(rsu, pt);
            if(textureMap_.Check(triangle, IsWater) && !hasExcludedCorner(triangle))
            {
                visited[index(triangle)] = 1;
                row.push_back(triangle);
            }
        }
    }
    for(unsigned i = 0; i < rows && !row.empty(); ++i)
    {
        std::vector<Triangle> nextRow;
        for(const Triangle& triangle : row)
        {
            for(const Triangle& neighbor : GetTriangleNeighbors(triangle, size))
            {
                auto& isVisited = visited[index(neighbor)];
                if(isVisited)
                    continue;
                isVisited = 1;
                if(!textureMap_.Check(neighbor, IsLand) || textureMap_.Check(neighbor, IsMountainOrSnowOrLava)
                   || hasExcludedCorner(neighbor))
                    continue;
                textureMap_.Set(neighbor, sand);
                nextRow.push_back(neighbor);
            }
        }
        row = std::move(nextRow);
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

void Texturizer::AddTextures(unsigned mountainLevel, unsigned beachWidth)
{
    ApplyTexturingByHeightMap(mountainLevel);
    ApplyBeach(beachWidth);

    const auto mountainFoot = SelectPoints(
      [this](const MapPoint& pt) {
          return this->textureMap_.Any(pt, IsMinableMountain) && !this->textureMap_.All(pt, IsMountainOrSnowOrLava);
      },
      this->textures_.GetSize());

    ApplyMountainTransitions(mountainFoot);

    auto moutainWaterTransition = SelectPoints(
      [this](const MapPoint& pt) {
          return this->textureMap_.Any(pt, IsMinableMountain) && this->textureMap_.Any(pt, IsWater);
      },
      this->textures_.GetSize());
    helpers::erase_if(moutainWaterTransition,
                      [this](const MapPoint& pt) { return noCoast_ && ((*noCoast_)[pt] & ShoreExclusion::NoSwamp); });

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
