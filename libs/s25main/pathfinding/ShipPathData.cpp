// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ShipPathData.h"
#include "EventManager.h"
#include "helpers/containerUtils.h"
#include "pathfinding/FreePathFinder.h"
#include "pathfinding/PathConditionShip.h"
#include "world/GameWorldBase.h"
#include <algorithm>
#include <set>

struct ShipPathData::HarborConnection::Matcher
{
    HarborId harbor;
    SeaId sea;
    constexpr bool operator()(const ShipPathData::HarborConnection& con) const
    {
        return con.destHarbor == harbor && con.sea == sea;
    }
};

const std::vector<Direction>* ShipPathData::getHarborConnection(HarborId startHarbor, HarborId destHarbor, SeaId seaId)
{
    if(harborConnections_.empty())
        harborConnections_.resize(world_.GetNumHarborPoints());
    const auto& connections = harborConnections_[startHarbor];
    const auto it = helpers::find_if(connections, HarborConnection::Matcher{destHarbor, seaId});
    const HarborConnection* connection =
      (it != connections.end()) ? &*it : calcHarborConnection(startHarbor, destHarbor, seaId);
    return connection ? &connection->route : nullptr;
}

const std::vector<Direction>* ShipPathData::findCachedPath(MapPoint start, MapPoint dest, bool& reversed) const
{
    for(const PathEntry& e : cachedPaths_)
    {
        if(e.start == start && e.dest == dest)
        {
            const_cast<PathEntry&>(e).lastUse = world_.GetEvMgr().GetCurrentGF();
            reversed = false;
            return &e.route;
        }
        if(e.start == dest && e.dest == start)
        {
            const_cast<PathEntry&>(e).lastUse = world_.GetEvMgr().GetCurrentGF();
            reversed = true;
            return &e.route;
        }
    }
    return nullptr;
}

void ShipPathData::addToCache(MapPoint start, MapPoint dest, std::vector<Direction> route)
{
    PathEntry entry{start, dest, world_.GetEvMgr().GetCurrentGF(), std::move(route)};
    if(cachedPaths_.size() < cachedPaths_.max_size())
        cachedPaths_.emplace_back(std::move(entry));
    else
    {
        const auto itOldest =
          std::min_element(cachedPaths_.begin(), cachedPaths_.end(),
                           [](const PathEntry& a, const PathEntry& b) { return a.lastUse < b.lastUse; });
        *itOldest = std::move(entry);
    }
}

const ShipPathData::HarborConnection* ShipPathData::calcHarborConnection(HarborId startHarbor, HarborId destHarbor,
                                                                         SeaId seaId)
{
    RTTR_Assert(startHarbor != destHarbor);
    RTTR_Assert(seaId);
    // Only neighbors are connected, they also tell the length of the shortest route
    const HarborPos::Neighbor* neighbor = nullptr;
    for(const auto dir : helpers::EnumRange<ShipDirection>{})
    {
        for(const auto& nb : world_.GetHarborNeighbors(startHarbor, dir))
        {
            if(nb.id == destHarbor && nb.sea == seaId)
                neighbor = &nb;
        }
    }
    if(!neighbor)
        return nullptr;

    HarborConnection connection{destHarbor, seaId, {}};
    const auto startPoint = world_.GetCoastalPoint(startHarbor, seaId);
    const auto destPoint = world_.GetCoastalPoint(destHarbor, seaId);
    if(startPoint != destPoint)
    {
        unsigned len = 0;
        if(!world_.FindShipPath(startPoint, destPoint, neighbor->distance, &connection.route, &len))
        {
            RTTR_Assert(false);
            return nullptr;
        }
        RTTR_Assert(len == neighbor->distance);
    }
    auto& connections = harborConnections_[startHarbor];
    connections.push_back(std::move(connection));
    return &connections.back();
}
