// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "EconomySnapshot.h"
#include "ai/civ/EconomyGraph.h"
#include "helpers/EnumArray.h"
#include "helpers/OptionalEnum.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include "gameTypes/MapCoordinates.h"
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

class GamePlayer;
class GameWorld;
class nobUsual;

namespace civ {

/// Amounts of wares in hundredths. Whole numbers: all computers of a network game must compute the same (the
/// distribution of all wares is a rule of the game, DESIGN.md "Gesamtverteilung")
using Amount = int64_t;
constexpr Amount AMOUNT_UNIT = 100;

/// Balance length in minutes: production and consumption are measured over it and forecast for it (DESIGN.md
/// "Prognose"). Also the time a new building counts as coming
constexpr unsigned BALANCE_MINUTES = 10;
/// Surplus: the forecast is above SURPLUS_FACTOR x consumption per minute, at least MIN_STOCK
constexpr unsigned SURPLUS_FACTOR = 10;
constexpr unsigned MIN_STOCK = 8;
/// Tools kept in stock per tool kind (asked for as planned, so they are made before they are missing)
constexpr unsigned TOOLS_IN_STOCK = 2;

/// The first problem of a building (DESIGN.md "Diagnose jedes Gebäudes")
enum class Diagnosis : uint8_t
{
    /// No problem found
    Working,
    /// Production stopped by the player or by the distribution of all wares (not examined)
    Stopped,
    /// No road at its flag
    NoRoad,
    /// No worker and none coming (no warehouse can send one)
    NoWorker,
    /// Its worker is on his way
    WorkerComing,
    /// Tool reform: it does not work without tools (base speed 0) and has none giving work speed: it waits
    NoTool,
    /// Tool reform: a slot is empty, but it works without that tool (slowly). Only reported, it counts as working
    MissingTool,
    /// Inputs missing
    NoInput,
    /// Its flag is full
    FlagFull,
    /// Nothing left in its reach
    Exhausted,
    /// Farm or plantation with fewer than MIN_FIELD_NODES field nodes in its reach
    FewFields,
    /// Nothing in the near half of its reach (only reported)
    SourceFar,
    /// A tool in use is worse than the best one of its slot (only reported)
    BadTool
};

/// Farms and plantations need this many field nodes in their reach
constexpr unsigned MIN_FIELD_NODES = 4;

/// Sources around a building: in its reach, in the near half of its reach, and the field nodes (farms, plantations)
struct SourceInfo
{
    unsigned inReach = 0, nearby = 0, fields = 0;
};

/// Does the building type need fields (farm, plantation of lua)?
bool needsFields(BuildingType bld);
/// Field nodes in the reach of a farm or plantation at `pt`: a field or free for a new one. Not `placed` yet: the nodes
/// next to the building and its flag stay free
unsigned countFieldNodes(const GameWorld& world, BuildingType bld, MapPoint pt, bool placed);
/// Count the sources of the recipe around `pt`
SourceInfo countSources(const GameWorld& world, MapPoint pt, const civai::Recipe& recipe, bool placed);
/// The first problem of the building. `getSources` is only called if the sources are needed
Diagnosis diagnose(const GamePlayer& player, const nobUsual& building, const civai::Recipe& recipe,
                   std::vector<GoodType>& missingInputs, const std::function<SourceInfo()>& getSources);

/// Tool kind of the ware (also copper tools ...), nullopt if it is no tool
helpers::OptionalEnum<Tool> getToolKindOf(GoodType good);
/// How good the tool is: work speed and output bonus first, then durability. 0 for wares which are no tools
unsigned getToolQuality(GoodType good);
/// The tool needed to recruit the worker of the building, unless a worker of the job waits in a warehouse
GoodType getRecruitTool(const EconomySnapshot& snapshot, BuildingType bld);
/// The tools a new building of the type will ask for: the tool to recruit its worker, one per tool slot
void addToolDemand(const EconomySnapshot& snapshot, std::vector<OpenDemand>& demand, BuildingType bld);

/// What is known about a ware or a group of wares (wares one consumer takes equally) for the next balance length.
/// Every request is an entry (wares, one of them will do, and an amount), shared out among the wares that can serve it
/// (DESIGN.md "Warengruppen", "Zuteilung der Einträge")
struct ForecastStatus
{
    /// In the warehouses
    unsigned stock = 0;
    /// "Reserviert": wares on their way (roads, flags) to a warehouse or without a goal. Wares on their way to a
    /// consumer are not counted: their request is not open any more
    Amount onTheWay = 0;
    /// On their way to the building that asked for them (only shown)
    unsigned inTransit = 0;
    /// Forecast: requested, open (asked for by the game, not sent yet) and planned (tools and workers of building
    /// sites, sites ordered, orders of the AI, tools kept in stock), and the part of the AI orders not placed yet
    Amount openDemand = 0, planned = 0, plannedByOrders = 0;
    /// stock + onTheWay - openDemand - planned
    Amount free = 0;
    /// Of open, planned and consumption: what the entries asking for exactly these wares get from none of them
    Amount missing = 0;
    /// Forecast production and consumption in the balance length
    Amount production = 0, consumption = 0;
    /// free + production - consumption
    Amount forecast = 0;
    /// Number of wares ("Warenzahl"): stock + onTheWay - open requests (shared out the same way, only the open ones)
    Amount count = 0;
    /// Of the open requests: what the entries asking for exactly these wares get from none of them
    Amount countMissing = 0;
    /// Consumed (all uses) per minute in the balance length
    Amount consumptionPerMinute = 0;
    /// Produced minus consumed in the balance length
    int balance = 0;
    /// Start stock not used yet (only for the wares of rttr:SetAIStartStockNoSurplus, otherwise 0)
    unsigned startStockLeft = 0;
};

/// Measured values of a building type in the balance length
struct TypeMeasure
{
    /// GFs the buildings of the type stood finished in the window (also those gone since)
    unsigned buildingGFs = 0;
    /// Of these: GFs they stood stopped (by the player or the distribution of all wares; EconomyStatistics::
    /// GetStoppedBy). They do not count for the measured values per building, and the buildings count with what they
    /// would have made in them (Nutzer, 2026-11-22)
    unsigned stoppedGFs = 0;
    /// GFs their workers were there, weighted by the productivity (percent)
    uint64_t workingGFsPercent = 0;
    /// A building of the type worked (has a worker, or made or used anything)
    bool worked = false;
};

/// Construction costs are counted with a share in 1/CONSTRUCTION_SHARE_FULL (EconomyForecast::Extra::constructionShare)
constexpr uint32_t CONSTRUCTION_SHARE_FULL = 10000;

/// Computes forecast and number of wares of a player (DESIGN.md "Prognose", "Gesamtverteilung"). The same calculation
/// for the distribution of all wares (rule of the game, every computer for every player) and for the economy planner
/// of the AI (which adds its building sites ordered and the orders of its list)
class EconomyForecast
{
public:
    /// What the AI adds: its building sites ordered whose commands are on their way, the orders of its list
    struct PlannedEntry
    {
        std::vector<GoodType> wares;
        Amount amount = 0;
        /// For an order of the list not placed yet
        bool byOrder = false;
        /// Construction costs (counted with `Extra::constructionShare`)
        bool construction = false;
    };
    struct Extra
    {
        helpers::EnumArray<unsigned, BuildingType> pendingSites{};
        /// Construction costs, tools of the sites ordered and of the orders of the list, in the order of the list
        std::vector<PlannedEntry> planned;
        /// Diagnosis of a building (the AI caches the sources). Empty: computed here
        std::function<Diagnosis(const nobUsual&, const civai::Recipe&)> diagnose;
        /// Building types whose coming and standing buildings count. Empty: all
        std::function<bool(BuildingType)> counts;
        /// Share of all construction costs (open and planned) counted, in 1/CONSTRUCTION_SHARE_FULL. The game counts
        /// them fully; the AI evaluates its shortages with less (EconomyPlanner::BUILDING_MATERIAL_FACTOR)
        uint32_t constructionShare = CONSTRUCTION_SHARE_FULL;
    };

    explicit EconomyForecast(const civai::EconomyGraph& graph);

    /// Compute everything for the player at `gf`
    void Compute(const GamePlayer& player, unsigned gf, const EconomySnapshot& snapshot, const Extra& extra);
    /// Compute the statuses again with other planned entries or another share of the construction costs, with the
    /// measured values, production and consumption of the last Compute (same player, same GF, same snapshot)
    void RecomputeStatuses(const GamePlayer& player, const EconomySnapshot& snapshot, const Extra& extra);

    const civai::EconomyGraph& GetGraph() const { return graph_; }
    const ForecastStatus& GetWare(GoodType good) const { return wares_[good]; }
    /// The status of a group of wares (sorted, without duplicates): its wares with the shares given to them by all
    /// entries, and what is missing for the entries of exactly its wares. A ware alone is a group of one
    ForecastStatus GetGroup(const std::vector<GoodType>& wares) const;
    /// The ware sets of all entries (what a consumer asks for exactly)
    const std::vector<std::vector<GoodType>>& GetEntrySets() const { return entrySets_; }

    /// Measured: made in the balance length per building of the type and minute, in hundredths. By the work time if no
    /// building of the type worked yet
    Amount ProducedPerBuilding(BuildingType bld, GoodType good) const;
    Amount UsedPerBuilding(BuildingType bld, GoodType good) const;
    /// Wares (all products together) a building of the type makes per minute while it works, in hundredths
    Amount OutputPerWorkingMinute(BuildingType bld) const;
    const TypeMeasure& GetMeasure(BuildingType bld) const { return measures_[bld]; }
    /// Work steps per minute of a building of the type by the work times of the game data, in hundredths
    Amount GetNominalSteps(BuildingType bld) const;
    const civai::Recipe* FindRecipe(BuildingType bld) const;
    /// Minutes of the balance length measured so far (at least one), in GFs
    unsigned GetMeasuredGFs() const { return measuredGFs_; }

private:
    void ComputeMeasures(const GamePlayer& player);
    void ComputeFlows(const GamePlayer& player, const EconomySnapshot& snapshot, const Extra& extra);
    void ComputeStatuses(const GamePlayer& player, const EconomySnapshot& snapshot, const Extra& extra);

    const civai::EconomyGraph& graph_;
    /// The player of the last computation (for the measured values per building)
    const GamePlayer* player_ = nullptr;
    std::vector<Amount> nominalSteps_;
    unsigned gf_ = 0;
    unsigned measuredGFs_ = 1, coveredGFs_ = 1, windowStart_ = 0;
    helpers::EnumArray<TypeMeasure, BuildingType> measures_;
    /// Production and consumption in the balance length (as input of buildings), alone and "any of"
    helpers::EnumArray<Amount, GoodType> production_{}, consumption_{};
    std::vector<std::pair<std::vector<GoodType>, Amount>> groupConsumption_;

    struct Shares
    {
        Amount open = 0, consumption = 0, planned = 0, plannedByOrders = 0;
    };
    helpers::EnumArray<ForecastStatus, GoodType> wares_;
    /// Per ware: what the entries gave it (forecast) and what the open requests gave it (number of wares)
    helpers::EnumArray<Shares, GoodType> given_{};
    helpers::EnumArray<Amount, GoodType> givenCount_{};
    helpers::EnumArray<Amount, GoodType> available_{}, availableCount_{};
    std::map<std::vector<GoodType>, Shares> missing_;
    std::map<std::vector<GoodType>, Amount> missingCount_;
    std::vector<std::vector<GoodType>> entrySets_;
};

} // namespace civ
