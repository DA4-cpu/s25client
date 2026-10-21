// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/Identifiers.h"
#include "civ/Content.h"
#include "helpers/EnumRange.h"
#include "gameData/ShieldConsts.h"
#include <array>

namespace civ {
namespace {
    // Order must match the enums (built-in part only)
    const std::array<const char*, NUM_BUILTIN_BUILDINGS> buildingIdentifiers = {
      {"Headquarters",   "Barracks",     "Guardhouse", "Skinner",       "Watchtower",    "Vineyard",    "Winery",
       "Temple",         "Tannery",      "Fortress",   "GraniteMine",   "CoalMine",      "IronMine",    "GoldMine",
       "LookoutTower",   "LeatherWorks", "Catapult",   "Woodcutter",    "Fishery",       "Quarry",      "Forester",
       "Slaughterhouse", "Hunter",       "Brewery",    "Armory",        "Metalworks",    "Ironsmelter", "Charburner",
       "PigFarm",        "Storehouse",   "Nothing9",   "Mill",          "Bakery",        "Sawmill",     "Mint",
       "Well",           "Shipyard",     "Farm",       "DonkeyBreeder", "HarborBuilding"}};

    const std::array<const char*, NUM_BUILTIN_GOODS> goodIdentifiers = {
      {"Beer",       "Tongs",         "Hammer",         "Axe",    "Saw",     "PickAxe",    "Shovel",  "Crucible",
       "RodAndLine", "Scythe",        "WaterEmpty",     "Water",  "Cleaver", "Rollingpin", "Bow",     "Boat",
       "Sword",      "Iron",          "Flour",          "Fish",   "Bread",   "Shield",     "Wood",    "Boards",
       "Stones",     "ShieldVikings", "ShieldAfricans", "Grain",  "Coins",   "Gold",       "IronOre", "Coal",
       "Meat",       "Ham",           "ShieldJapanese", "Grapes", "Wine",    "Skins",      "Leather", "Armor"}};

    const std::array<const char*, NUM_BUILTIN_JOBS> jobIdentifiers = {{"Helper",
                                                                       "Woodcutter",
                                                                       "Fisher",
                                                                       "Forester",
                                                                       "Carpenter",
                                                                       "Stonemason",
                                                                       "Hunter",
                                                                       "Farmer",
                                                                       "Miller",
                                                                       "Baker",
                                                                       "Butcher",
                                                                       "Miner",
                                                                       "Brewer",
                                                                       "PigBreeder",
                                                                       "DonkeyBreeder",
                                                                       "IronFounder",
                                                                       "Minter",
                                                                       "Metalworker",
                                                                       "Armorer",
                                                                       "Builder",
                                                                       "Planer",
                                                                       "Private",
                                                                       "PrivateFirstClass",
                                                                       "Sergeant",
                                                                       "Officer",
                                                                       "General",
                                                                       "Geologist",
                                                                       "Shipwright",
                                                                       "Scout",
                                                                       "PackDonkey",
                                                                       "BoatCarrier",
                                                                       "CharBurner",
                                                                       "Winegrower",
                                                                       "Vintner",
                                                                       "TempleServant",
                                                                       "Skinner",
                                                                       "Tanner",
                                                                       "LeatherWorker"}};

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

std::string getIdentifier(BuildingType bld)
{
    if(!isCustomBuilding(bld))
        return buildingIdentifiers[static_cast<unsigned>(bld)];
    if(const CustomBuilding* custom = getActiveContent().GetBuilding(bld))
        return custom->name;
    return "Custom" + std::to_string(getCustomBuildingIdx(bld));
}

std::string getIdentifier(GoodType good)
{
    if(good == GoodType::Nothing)
        return "Nothing";
    if(!isCustomGood(good))
        return goodIdentifiers[static_cast<unsigned>(ConvertShields(good))];
    if(const CustomWare* custom = getActiveContent().GetWare(good))
        return custom->name;
    return "Custom" + std::to_string(getCustomGoodIdx(good));
}

std::string getIdentifier(Job job)
{
    if(!isCustomJob(job))
        return jobIdentifiers[static_cast<unsigned>(job)];
    if(const CustomWorker* custom = getActiveContent().GetWorker(job))
        return custom->name;
    return "Custom" + std::to_string(getCustomJobIdx(job));
}

std::optional<BuildingType> buildingFromIdentifier(const std::string& identifier, bool allowHeadquarters)
{
    for(unsigned i = 0; i < NUM_BUILTIN_BUILDINGS; ++i)
    {
        const auto bld = BuildingType(i);
        const bool usable = isUsableBuilding(bld) || (allowHeadquarters && bld == BuildingType::Headquarters);
        if(usable && identifier == buildingIdentifiers[i])
            return bld;
    }
    return std::nullopt;
}

std::optional<GoodType> goodFromIdentifier(const std::string& identifier)
{
    for(unsigned i = 0; i < NUM_BUILTIN_GOODS; ++i)
    {
        const auto good = GoodType(i);
        if(isUsableGood(good) && identifier == goodIdentifiers[i])
            return good;
    }
    return std::nullopt;
}

std::optional<Job> jobFromIdentifier(const std::string& identifier)
{
    for(unsigned i = 0; i < NUM_BUILTIN_JOBS; ++i)
    {
        if(identifier == jobIdentifiers[i])
            return Job(i);
    }
    return std::nullopt;
}

} // namespace civ
