// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "EconomyStatistics.h"
#include "helpers/EnumArray.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include "gameTypes/JobTypes.h"
#include <vector>

class GamePlayer;

/// Wares something asked for which have not been sent yet (none in a warehouse that could deliver them).
/// Any of `wares` will do, e.g. all wares of a construction cost entry or the food of a mine
struct OpenDemand
{
    std::vector<GoodType> wares;
    unsigned amount = 0;
    /// What the wares are for (only Production, Construction, Tool, Military and Research occur)
    WareUse purpose = WareUse::Production;
};

/// Buildings of one type standing still because inputs are missing. Any of `wares` would let them work
struct WaitingForInput
{
    std::vector<GoodType> wares;
    unsigned numBuildings = 0;
};

/// State of all buildings of one type
struct BuildingTypeState
{
    /// Finished buildings and building sites
    unsigned numBuildings = 0, numSites = 0;
    /// Finished buildings without worker (none found yet or on his way)
    unsigned withoutWorker = 0;
    /// Production stopped by the player
    unsigned stopped = 0;
    /// Tool reform: the building cannot work at all without its tools
    unsigned withoutTool = 0;
    /// Standing still because inputs are missing, by the missing wares
    std::vector<WaitingForInput> waitingForInput;
    /// Sum of the productivity (percent) of the buildings with worker
    unsigned productivitySum = 0;

    unsigned GetNumWaitingForInput() const;
    /// Average productivity (percent) of the buildings with a worker. 0 if there are none
    unsigned GetProductivity() const;
};

/// What the economy of a player looks like right now. Taken when needed (not stored, not part of the savegame)
struct EconomySnapshot
{
    std::vector<OpenDemand> openDemand;
    /// Soldiers military buildings asked for which no warehouse has sent yet
    unsigned openSoldiers = 0;
    helpers::EnumArray<BuildingTypeState, BuildingType> buildings;
    /// Wares in the warehouses (the inventory of the player also counts wares on the roads and in buildings)
    helpers::EnumArray<unsigned, GoodType> inWarehouses{};
    /// Helpers and workers without work in the warehouses
    helpers::EnumArray<unsigned, Job> inWarehousesPeople{};
    /// Building sites and buildings waiting for a worker of the job
    helpers::EnumArray<unsigned, Job> jobsWanted{};
    /// Wares on their way to the building that asked for them (not to a warehouse). They left the stock already
    helpers::EnumArray<unsigned, GoodType> inTransit{};
    /// Input spaces of the building sites: the finished buildings will ask for these wares (any of `wares` for the
    /// entries "any of", e.g. the food of a mine)
    std::vector<OpenDemand> siteInputs;

    /// Sum of the open demand of all entries the ware can serve
    unsigned GetOpenDemand(GoodType good) const;
};

EconomySnapshot takeEconomySnapshot(const GamePlayer& player);
/// Input spaces of a building of the type: the wares it will ask for when it works (one entry "any of" for buildings
/// using any of their inputs, e.g. the food of a mine)
void addInputSpaces(std::vector<OpenDemand>& demand, BuildingType bld);
