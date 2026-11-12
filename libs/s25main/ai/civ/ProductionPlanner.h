// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "civ/Knowledge.h"
#include "helpers/EnumArray.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include <cstdint>
#include <string>
#include <vector>

class GameWorldBase;
class Inventory;

namespace civai {

class EconomyGraph;
class MapProfile;

/// Why the player wants wares. The order is the priority when the stock is shared
enum class DemandKind : uint8_t
{
    /// Material for building sites and for the buildings the plan adds
    Construction,
    /// Tools missing in buildings (tool reform) or to recruit their workers
    Tools,
    /// Wares and knowledge for the current research
    Research,
    /// Equipment of the soldiers missing in military buildings
    Soldiers,
    /// Coins and armor the military buildings take
    Military,
    /// Construction material and soldiers for the ongoing expansion: a flow, the stock does not cover it (otherwise the
    /// large stock at the start would delay the production until it is used up)
    Expansion,
    /// Stocks (construction material, tools, soldiers) to bridge the time until production follows
    Reserve
};
constexpr auto maxEnumValue(DemandKind)
{
    return DemandKind::Reserve;
}

/// The player needs `amount` wares, any of `wares` will do (e.g. a cost entry by tags)
struct DemandItem
{
    DemandKind kind = DemandKind::Reserve;
    std::vector<GoodType> wares;
    double amount = 0;
};

/// What the player needs right now, read from the game: building sites, tool slots, soldiers, research
struct Demand
{
    /// Before the stock (see applyStock)
    std::vector<DemandItem> items;
    /// Knowledge still missing for the current research (tenths)
    civ::KnowledgePoints knowledge{};
    /// Tenths a knowledge point of a medium counts (in a library full, otherwise half)
    unsigned tenthsPerPoint = civ::MEDIA_IN_WAREHOUSE_TENTHS;
};

/// Read the current demand of the player. Only meaningful in the civilization mode
Demand collectDemand(const GameWorldBase& world, unsigned playerId);
/// Subtract the stock from the demand: each list of wares (e.g. all wares of a cost entry) uses its stock once, in the
/// order of the demand kinds. Items fully in stock are removed. The expansion is a flow and stays
std::vector<DemandItem> applyStock(std::vector<DemandItem> items, const Inventory& inventory);

/// Result of the planning: how many buildings of each type the player wants and why
struct ProductionPlan
{
    /// The planner decides about these buildings (all which make wares and the forester). Others (military,
    /// warehouses, harbor, shipyard, library ...) are planned by the old AI
    helpers::EnumArray<bool, BuildingType> managed{};
    /// Number of buildings (with building sites) wanted
    helpers::EnumArray<unsigned, BuildingType> wanted{};
    /// Number of buildings needed for the demand (with fractions)
    helpers::EnumArray<double, BuildingType> work{};
    /// The ware the building is mostly needed for (Nothing: none)
    helpers::EnumArray<GoodType, BuildingType> mainWare{};
    /// Buildings added for the knowledge points they give (current research)
    helpers::EnumArray<unsigned, BuildingType> forKnowledge{};
    /// Wares per minute the player wants directly (demand) and in total (with the inputs of the chain)
    helpers::EnumArray<double, GoodType> demandPerMinute{};
    helpers::EnumArray<double, GoodType> needPerMinute{};
    /// Cost of one ware made with the cheapest recipe usable now (see planProduction). Infinite: cannot be made now
    helpers::EnumArray<double, GoodType> unitCost{};
    /// Recipe chosen for the ware (index into EconomyGraph::GetRecipes) or -1
    helpers::EnumArray<int, GoodType> recipe{};
    /// Demand per kind (wares per minute) for the log
    helpers::EnumArray<double, DemandKind> perKind{};
    /// Demands no usable recipe can serve, e.g. "Steine (Bau)"
    std::vector<std::string> unmet;

    /// Text for the log: demand, wanted buildings and why
    std::string GetReport() const;
};

/// Plan the production buildings for the demand (see DESIGN.md, "Neue KI", step 2):
/// 1. Cost of each ware: the cheapest recipe usable now (building enabled, its source in the territory, construction
///    material and tools in stock or producible). Cost of a recipe = occupancy of the building (plus its construction
///    costs, spread over BUILD_AMORTIZATION wares) per ware + its inputs. A recipe without source is unusable, so other
///    ways to the ware (tags) are chosen automatically.
/// 2. Backwards through the chosen recipes: wares per minute -> buildings -> inputs per minute.
/// 3. Knowledge for the current research: more buildings giving knowledge points, the rest from knowledge media.
/// 4. The buildings added by the plan need material and tools: plan once more with them and the media.
/// 5. Not more new buildings at once than helpers (they become the workers).
ProductionPlan planProduction(const GameWorldBase& world, unsigned playerId, const EconomyGraph& graph,
                              const MapProfile& map, const Demand& demand);

/// Minutes in which a demand of the kind should be produced (rate = amount / horizon)
double getHorizonMinutes(DemandKind kind);

} // namespace civai
