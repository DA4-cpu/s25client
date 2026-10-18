// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/Identifiers.h"
#include "helpers/EnumArray.h"
#include "helpers/EnumRange.h"
#include "gameData/ShieldConsts.h"

namespace civ {
namespace {
    // Order must match the enum
    const helpers::EnumArray<const char*, BuildingType> buildingIdentifiers = {
      {"Headquarters", "Barracks",     "Guardhouse",    "Skinner",    "Watchtower", "Vineyard",       "Winery",
       "Temple",       "Tannery",      "Fortress",      "GraniteMine", "CoalMine",  "IronMine",       "GoldMine",
       "LookoutTower", "LeatherWorks", "Catapult",      "Woodcutter", "Fishery",    "Quarry",         "Forester",
       "Slaughterhouse", "Hunter",     "Brewery",       "Armory",     "Metalworks", "Ironsmelter",    "Charburner",
       "PigFarm",      "Storehouse",   "Nothing9",      "Mill",       "Bakery",     "Sawmill",        "Mint",
       "Well",         "Shipyard",     "Farm",          "DonkeyBreeder", "HarborBuilding"}};

    // Order must match the enum
    const helpers::EnumArray<const char*, GoodType> goodIdentifiers = {
      {"Beer",       "Tongs",  "Hammer",  "Axe",        "Saw",        "PickAxe",   "Shovel",  "Crucible",
       "RodAndLine", "Scythe", "WaterEmpty", "Water",   "Cleaver",    "Rollingpin", "Bow",     "Boat",
       "Sword",      "Iron",   "Flour",   "Fish",       "Bread",      "Shield",    "Wood",    "Boards",
       "Stones",     "ShieldVikings", "ShieldAfricans", "Grain", "Coins", "Gold",     "IronOre", "Coal",
       "Meat",       "Ham",    "ShieldJapanese", "Grapes", "Wine",    "Skins",     "Leather", "Armor"}};

    bool isUsableBuilding(BuildingType bld)
    {
        return bld != BuildingType::Headquarters && bld != BuildingType::Nothing9;
    }
    bool isUsableGood(GoodType good)
    {
        // Only canonical shields and no empty water
        return good != GoodType::WaterEmpty && ConvertShields(good) == good;
    }
} // namespace

const char* getIdentifier(BuildingType bld)
{
    return buildingIdentifiers[bld];
}

const char* getIdentifier(GoodType good)
{
    return goodIdentifiers[ConvertShields(good)];
}

std::optional<BuildingType> buildingFromIdentifier(const std::string& identifier)
{
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(isUsableBuilding(bld) && identifier == buildingIdentifiers[bld])
            return bld;
    }
    return std::nullopt;
}

std::optional<GoodType> goodFromIdentifier(const std::string& identifier)
{
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(isUsableGood(good) && identifier == goodIdentifiers[good])
            return good;
    }
    return std::nullopt;
}

} // namespace civ
