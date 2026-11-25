// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BuildingProperties.h"
#include "civ/Content.h"
#include "gameData/BuildingConsts.h"

const std::vector<BuildingType>& BuildingProperties::GetMilitaryBuildings()
{
    return civ::getActiveContent().GetMilitaryBuildings();
}

bool BuildingProperties::IsMilitary(BuildingType bld)
{
    switch(bld)
    {
        case BuildingType::Barracks:
        case BuildingType::Guardhouse:
        case BuildingType::Watchtower:
        case BuildingType::Fortress: return true;
        // Military buildings defined in lua (civilization mode)
        default: return isCustomBuilding(bld) && civ::getActiveContent().IsCustomMilitary(bld);
    }
}

bool BuildingProperties::IsMine(BuildingType bld)
{
    switch(bld)
    {
        case BuildingType::GraniteMine:
        case BuildingType::CoalMine:
        case BuildingType::IronMine:
        case BuildingType::GoldMine: return true;
        // Mines of the civilization mode (workshops with size = "mine")
        default: return isCustomBuilding(bld) && BUILDING_SIZE[bld] == BuildingQuality::Mine;
    }
}

bool BuildingProperties::IsWareHouse(BuildingType bld)
{
    switch(bld)
    {
        case BuildingType::Headquarters:
        case BuildingType::HarborBuilding:
        case BuildingType::Storehouse: return true;
        // Warehouses defined in lua (civilization mode)
        default: return isCustomBuilding(bld) && civ::getActiveContent().IsCustomWarehouse(bld);
    }
}

bool BuildingProperties::IsUsual(BuildingType bld)
{
    return IsValid(bld) && !IsMilitary(bld) && !IsWareHouse(bld);
}

bool BuildingProperties::IsValid(BuildingType bld)
{
    switch(bld)
    {
        case BuildingType::Nothing9: return false;
        default: return true;
    }
}
