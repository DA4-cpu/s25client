// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/Climate.h"
#include "RttrForeachPt.h"
#include "SerializedGameData.h"
#include "civ/MapCodes.h"
#include "mapGenerator/TextureHelper.h"
#include "mapGenerator/Zones.h"
#include "world/World.h"
#include "gameData/MapGenDesc.h"
#include "gameData/WorldDescription.h"
#include "libsiedler2/ArchivItem_Map.h"
#include <algorithm>
#include <vector>

namespace civ {

namespace {
    void pushNodeMap(SerializedGameData& sgd, const NodeMapBase<uint8_t>& nodes)
    {
        const std::vector<uint8_t> data(nodes.begin(), nodes.end());
        sgd.PushRawData(data.data(), data.size());
    }
    void popNodeMap(SerializedGameData& sgd, NodeMapBase<uint8_t>& nodes, const MapExtent& size)
    {
        std::vector<uint8_t> data(prodOfComponents(size));
        sgd.PopRawData(data.data(), data.size());
        nodes.Resize(size);
        for(unsigned i = 0; i < data.size(); ++i)
            nodes[i] = data[i];
    }
} // namespace

void ClimateMap::Init(const World& world, const libsiedler2::ArchivItem_Map* map)
{
    using libsiedler2::MapLayer;
    const MapExtent size = world.GetSize();
    temperature_.Resize(size, 0);
    humidity_.Resize(size, 0);
    height_.Resize(size, 0);
    water_.Resize(size, 0);
    hasClimate_ = false;
    if(map && map->hasLayer(MapLayer::Unknown7) && map->hasLayer(MapLayer::Unknown9)
       && map->hasLayer(MapLayer::Unknown10) && map->getHeader().getWidth() == size.x
       && map->getHeader().getHeight() == size.y)
    {
        // The generator writes the marker at every node
        hasClimate_ = true;
        RTTR_FOREACH_PT(MapPoint, size)
        {
            if((map->getMapDataAt(MapLayer::Unknown7, pt.x, pt.y) & CLIMATE_MARKER) == 0)
            {
                hasClimate_ = false;
                break;
            }
        }
    }
    if(hasClimate_)
    {
        unsigned maxAltitude = 0;
        RTTR_FOREACH_PT(MapPoint, size)
            maxAltitude = std::max<unsigned>(maxAltitude, world.GetNode(pt).altitude);
        RTTR_FOREACH_PT(MapPoint, size)
        {
            temperature_[pt] = static_cast<uint8_t>(std::min(
              100u, static_cast<unsigned>(map->getMapDataAt(MapLayer::Unknown7, pt.x, pt.y) & ~CLIMATE_MARKER)));
            humidity_[pt] = static_cast<uint8_t>(
              std::min(100u, static_cast<unsigned>(map->getMapDataAt(MapLayer::Unknown9, pt.x, pt.y))));
            water_[pt] =
              static_cast<uint8_t>(map->getMapDataAt(MapLayer::Unknown10, pt.x, pt.y) & (WATER_FRESH | WATER_RIVER));
            // Like the generator: 0 = sea level, 100 = highest point of the map
            const unsigned altitude = world.GetNode(pt).altitude;
            height_[pt] = static_cast<uint8_t>(maxAltitude == 0 ? 0u : altitude * 100u / maxAltitude);
        }
    }
    ComputeTerrain(world);
}

void ClimateMap::ComputeTerrain(const World& world)
{
    using namespace rttr::mapGenerator;
    const MapExtent size = world.GetSize();
    const auto& desc = world.GetDescription();
    NodeMapBase<uint8_t> info;
    info.Resize(size, 0);
    RTTR_FOREACH_PT(MapPoint, size)
    {
        uint8_t curInfo = 0;
        if(world.IsOfTerrain(pt, IsWater))
            curInfo |= TERRAIN_INFO_ALL_WATER;
        if(world.HasTerrain(pt, IsWater))
            curInfo |= TERRAIN_INFO_ANY_WATER;
        if(world.HasTerrain(pt, IsMountainOrSnowOrLava))
            curInfo |= TERRAIN_INFO_ANY_MOUNTAIN;
        if(world.IsOfTerrain(pt, IsLand))
            curInfo |= TERRAIN_INFO_ALL_LAND;
        info[pt] = curInfo;
    }

    if(!hasClimate_)
    {
        // Lakes: Bodies of water up to lakeMaxSize nodes (like the generator). No rivers
        const unsigned lakeMaxSize = desc.mapGen.climate.lakeMaxSize;
        NodeMapBase<uint8_t> visited;
        visited.Resize(size, 0);
        RTTR_FOREACH_PT(MapPoint, size)
        {
            water_[pt] = 0;
        }
        RTTR_FOREACH_PT(MapPoint, size)
        {
            if(!(info[pt] & TERRAIN_INFO_ALL_WATER) || visited[pt])
                continue;
            std::vector<MapPoint> body{pt};
            visited[pt] = 1;
            for(unsigned i = 0; i < body.size(); ++i)
            {
                for(const MapPoint& neighbor : world.GetNeighbours(body[i]))
                {
                    if((info[neighbor] & TERRAIN_INFO_ALL_WATER) && !visited[neighbor])
                    {
                        visited[neighbor] = 1;
                        body.push_back(neighbor);
                    }
                }
            }
            if(body.size() <= lakeMaxSize)
            {
                for(const MapPoint& node : body)
                    water_[node] = WATER_FRESH;
            }
        }
    }
    RTTR_FOREACH_PT(MapPoint, size)
    {
        if(water_[pt] & WATER_FRESH)
            info[pt] |= TERRAIN_INFO_FRESH;
        if(water_[pt] & WATER_RIVER)
            info[pt] |= TERRAIN_INFO_RIVER;
    }
    terrain_ = ComputeTerrainFlags(info, desc.mapGen.climate.nearWaterDistance);
}

bool ClimateMap::AllowsNaturally(const MapPoint pt, const MapResourceDesc& rule) const
{
    if((terrain_[pt] & rule.terrain) == 0)
        return false;
    if(!hasClimate_)
        return true;
    const unsigned temperature = temperature_[pt], humidity = humidity_[pt], height = height_[pt];
    return rule.climate.matches(temperature, humidity, height) && rule.getWeight(temperature, humidity, height) > 0.;
}

void ClimateMap::Serialize(SerializedGameData& sgd) const
{
    sgd.PushBool(hasClimate_);
    if(!hasClimate_)
        return;
    pushNodeMap(sgd, temperature_);
    pushNodeMap(sgd, humidity_);
    pushNodeMap(sgd, height_);
    pushNodeMap(sgd, water_);
}

void ClimateMap::Deserialize(SerializedGameData& sgd, const World& world)
{
    const MapExtent size = world.GetSize();
    hasClimate_ = sgd.PopBool();
    if(hasClimate_)
    {
        popNodeMap(sgd, temperature_, size);
        popNodeMap(sgd, humidity_, size);
        popNodeMap(sgd, height_, size);
        popNodeMap(sgd, water_, size);
    } else
    {
        temperature_.Resize(size, 0);
        humidity_.Resize(size, 0);
        height_.Resize(size, 0);
        water_.Resize(size, 0);
    }
    ComputeTerrain(world);
}

} // namespace civ
