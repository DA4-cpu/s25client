// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ai/civ/MapProfile.h"
#include "RttrForeachPt.h"
#include "civ/Content.h"
#include "civ/TechTree.h"
#include "figures/nofCustomPlanter.h"
#include "helpers/EnumRange.h"
#include "world/GameWorldBase.h"
#include "nodeObjs/noDeposit.h"
#include "nodeObjs/noGranite.h"
#include "gameTypes/MapCoordinates.h"
#include "gameTypes/MapNode.h"
#include "gameData/TerrainDesc.h"
#include <algorithm>

namespace civai {

unsigned ResourceProfile::GetAmount(const Source& source) const
{
    switch(source.kind)
    {
        case SourceKind::None: return 1;
        case SourceKind::Trees: return trees;
        case SourceKind::StonePiles: return stones;
        case SourceKind::Animals: return animals;
        case SourceKind::Land: return land;
        case SourceKind::Ore: return resources[static_cast<ResourceType>(source.index)];
        case SourceKind::Fish: return resources[ResourceType::Fish];
        case SourceKind::Water: return resources[ResourceType::Water];
        case SourceKind::MapObject: return source.index < mapObjects.size() ? mapObjects[source.index] : 0u;
        case SourceKind::Plantable: return source.index < plantable.size() ? plantable[source.index] : 0u;
    }
    return 0;
}

void ResourceProfile::Add(const ResourceProfile& other)
{
    trees += other.trees;
    stones += other.stones;
    animals += other.animals;
    land += other.land;
    for(const auto type : helpers::enumRange<ResourceType>())
        resources[type] += other.resources[type];
    mapObjects.resize(std::max(mapObjects.size(), other.mapObjects.size()), 0u);
    for(unsigned i = 0; i < other.mapObjects.size(); ++i)
        mapObjects[i] += other.mapObjects[i];
    plantable.resize(std::max(plantable.size(), other.plantable.size()), 0u);
    for(unsigned i = 0; i < other.plantable.size(); ++i)
        plantable[i] += other.plantable[i];
}

MapProfile::MapProfile(const GameWorldBase& world, const unsigned playerId)
    : world_(world), numRegionsX_((world.GetWidth() + REGION_SIZE - 1) / REGION_SIZE),
      numRegionsY_((world.GetHeight() + REGION_SIZE - 1) / REGION_SIZE), playerId_(playerId),
      regions_(numRegionsX_ * numRegionsY_), ownedRegions_(regions_.size())
{
    const civ::Content& content = world.GetTechTree().GetContent();
    for(unsigned i = 0; i < content.GetMapObjects().size() && plantableObjects_.size() < 31u; ++i)
    {
        if(content.GetMapObjects()[i].CanBePlanted())
            plantableObjects_.push_back(i);
    }
    staticInfo_.resize(static_cast<size_t>(world.GetWidth()) * world.GetHeight(), 0u);
    RTTR_FOREACH_PT(MapPoint, world.GetSize())
    {
        if(!world.IsOfTerrain(pt, [](const TerrainDesc& t) { return t.Is(ETerrain::Buildable); }))
            continue;
        uint32_t info = 1u;
        for(unsigned i = 0; i < plantableObjects_.size(); ++i)
        {
            if(nofCustomPlanter::GrowsNaturallyAt(world, pt, plantableObjects_[i]))
                info |= 1u << (i + 1u);
        }
        staticInfo_[world.GetIdx(pt)] = info;
    }
    for(unsigned i = 0; i < regions_.size(); ++i)
        ScanRegion(i);
}

void MapProfile::Update(const unsigned numRegions)
{
    for(unsigned i = 0; i < numRegions && !regions_.empty(); ++i)
    {
        ScanRegion(nextRegion_);
        nextRegion_ = (nextRegion_ + 1) % regions_.size();
    }
}

unsigned MapProfile::GetRegionIdx(const MapPoint pt) const
{
    return (pt.y / REGION_SIZE) * numRegionsX_ + pt.x / REGION_SIZE;
}

ResourceProfile MapProfile::GetTotal() const
{
    ResourceProfile result;
    for(const ResourceProfile& region : regions_)
        result.Add(region);
    return result;
}

ResourceProfile MapProfile::GetTotalOwned() const
{
    ResourceProfile result;
    for(const ResourceProfile& region : ownedRegions_)
        result.Add(region);
    return result;
}

void MapProfile::ScanRegion(const unsigned idx)
{
    const auto numMapObjects = static_cast<unsigned>(world_.GetTechTree().GetContent().GetMapObjects().size());

    ResourceProfile& region = regions_[idx];
    ResourceProfile& owned = ownedRegions_[idx];
    region = ResourceProfile();
    owned = ResourceProfile();
    region.mapObjects.assign(numMapObjects, 0u);
    region.plantable.assign(numMapObjects, 0u);
    owned.mapObjects.assign(numMapObjects, 0u);
    owned.plantable.assign(numMapObjects, 0u);

    const unsigned x0 = (idx % numRegionsX_) * REGION_SIZE, y0 = (idx / numRegionsX_) * REGION_SIZE;
    const unsigned x1 = std::min<unsigned>(x0 + REGION_SIZE, world_.GetWidth());
    const unsigned y1 = std::min<unsigned>(y0 + REGION_SIZE, world_.GetHeight());
    for(unsigned y = y0; y < y1; ++y)
    {
        for(unsigned x = x0; x < x1; ++x)
        {
            const MapPoint pt(x, y);
            const MapNode& node = world_.GetNode(pt);
            const bool isOwned = node.owner == playerId_ + 1;
            const auto add = [&](auto&& addTo) {
                addTo(region);
                if(isOwned)
                    addTo(owned);
            };

            if(node.resources.getAmount() > 0)
            {
                const ResourceType type = node.resources.getType();
                const unsigned amount = node.resources.getAmount();
                add([&](ResourceProfile& p) { p.resources[type] += amount; });
            }
            for(const auto& figure : node.figures)
            {
                if(figure->GetType() == NodalObjectType::Animal)
                    add([](ResourceProfile& p) { ++p.animals; });
            }
            const noBase* obj = world_.GetNO(pt);
            switch(obj->GetType())
            {
                case NodalObjectType::Tree: add([](ResourceProfile& p) { ++p.trees; }); break;
                case NodalObjectType::Granite:
                {
                    const unsigned stones = static_cast<const noGranite*>(obj)->GetSize() + 1u;
                    add([stones](ResourceProfile& p) { p.stones += stones; });
                    break;
                }
                case NodalObjectType::Deposit:
                {
                    const auto* deposit = static_cast<const noDeposit*>(obj);
                    const unsigned objIdx = deposit->GetObjectIdx();
                    if(objIdx < numMapObjects)
                    {
                        const unsigned pieces = deposit->GetSize() + 1u;
                        add([objIdx, pieces](ResourceProfile& p) { p.mapObjects[objIdx] += pieces; });
                    }
                    break;
                }
                case NodalObjectType::Nothing:
                case NodalObjectType::Environment:
                {
                    const uint32_t info = staticInfo_[world_.GetIdx(pt)];
                    if(info & 1u)
                    {
                        add([](ResourceProfile& p) { ++p.land; });
                        for(unsigned i = 0; i < plantableObjects_.size(); ++i)
                        {
                            if(info & (1u << (i + 1u)))
                            {
                                const unsigned objIdx = plantableObjects_[i];
                                add([objIdx](ResourceProfile& p) { ++p.plantable[objIdx]; });
                            }
                        }
                    }
                    break;
                }
                default: break;
            }
        }
    }
}

} // namespace civai
