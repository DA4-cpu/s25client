// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "mapGenerator/Map.h"
#include "mapGenerator/MapSettings.h"
#include "mapGenerator/RandomUtility.h"
#include <string>
#include <vector>

struct MapGenDesc;
struct MapResourceDesc;
struct MapZonePackageDesc;

namespace rttr::mapGenerator {

/// Object type and first object index of a tree species in the S2 map format
struct TreeCode
{
    uint8_t type;
    uint8_t index;
};
/// Map format code of a tree species name (see getMapGenTreeNames()). Throws for unknown names
TreeCode GetTreeCode(const std::string& species);
/// Map format code of an animal species name (see getMapGenAnimalNames()). Throws for unknown names
libsiedler2::Animal GetAnimalCode(const std::string& species);
/// True if the animal lives on water (ducks), false if it lives on land
bool IsWaterAnimal(const std::string& species);

/// Amount (percent) of a resource rule: Its own value or the value of the referenced slider of the settings
double GetResourceAmount(const MapResourceDesc& resource, const MapSettings& settings);

/// Package (plants and animals) of the zone of the node for its climate or nullptr if none matches
const MapZonePackageDesc* GetZonePackage(const Map& map, const MapGenDesc& desc, const MapPoint& pt);

/// Map object (rttr:AddMapObject) or ore (rttr:AddOre) of the civilization mode the generator can place, stored as
/// described in civ/MapCodes.h. The index of an object/ore is its position among the entries of the same kind.
struct CustomMapObjectInfo
{
    std::string name;
    /// Map objects: Number of sizes
    unsigned sizes = 1;
    /// True: Ore in the mountains (placed together with coal, iron, gold and granite)
    bool isOre = false;
};

/**
 * Places all objects (trees, stone piles, decoration, map objects of the civilization mode), animals and resources
 * (fish, ground water, coal, iron, gold, granite) according to the resource rules of the lua data
 * (rttr:AddMapResource). The rules are applied in order.
 * Requires textures, zones, climate (map.temperature, map.climate, map.heightPercent), map.terrainFlags, HQs and
 * harbors to be set. The area around HQs and harbors stays free of objects.
 * Rules with a map object which is not in customObjects are skipped (with a message in the log).
 */
void PlaceResources(Map& map, RandomUtility& rnd, const MapSettings& settings, const MapGenDesc& desc,
                    const std::vector<CustomMapObjectInfo>& customObjects = {});

} // namespace rttr::mapGenerator
