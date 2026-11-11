// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>
#include <gameTypes/Direction.h>
#include <gameTypes/MapCoordinates.h>
#include <gameTypes/MapTypes.h>
#include <helpers/StrongIdVector.h>
#include <vector>

class GameWorldBase;

/// Implements data for finding ship paths.
/// Assumes seas and land don't change so paths can be heavily cached.
/// Most paths are between harbor points and are calculated once when first needed, for others a LRU cache is provided
class ShipPathData
{
public:
    ShipPathData(const GameWorldBase& world) : world_(world) {}
    /// Get route from start to destination harbor on the given sea.
    /// Return nullptr if the destination harbor can't be reached on this sea from the start harbor.
    /// The returned route stays valid until the next call.
    const std::vector<Direction>* getHarborConnection(HarborId startHarbor, HarborId destHarbor, SeaId seaId);
    /// Find a route in the cache
    /// Return reference to it if found, else nullptr.
    /// Sets `reversed` to true if start and dest of the route are swapped
    const std::vector<Direction>* findCachedPath(MapPoint start, MapPoint dest, bool& reversed) const;
    void addToCache(MapPoint start, MapPoint dest, std::vector<Direction> route);

private:
    const GameWorldBase& world_;
    struct HarborConnection
    {
        HarborId destHarbor;
        SeaId sea;
        std::vector<Direction> route;
        struct Matcher;
    };

    /// Connections from a harbor to others which have been used already.
    /// Usually only a small number of harbors is used, so inline some.
    using HarborConnectionVector = boost::container::small_vector<HarborConnection, 8>;
    helpers::StrongIdVector<HarborConnectionVector, HarborId> harborConnections_;

    struct PathEntry
    {
        MapPoint start;
        MapPoint dest;
        unsigned lastUse;
        std::vector<Direction> route;
    };
    // LRU cache for arbitrary ship paths (start,dest).
    boost::container::static_vector<PathEntry, 16> cachedPaths_;

    /// Calculate the route between 2 neighboring harbors and store it.
    /// Return nullptr if they aren't neighbors on that sea.
    const HarborConnection* calcHarborConnection(HarborId startHarbor, HarborId destHarbor, SeaId seaId);
};
