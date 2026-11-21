// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "EconomySnapshot.h"
#include "ai/aijh/AIConstruction.h"
#include "ai/civ/EconomyGraph.h"
#include "civ/EconomyForecast.h"
#include "civ/WareDistribution.h"
#include "helpers/EnumArray.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include "gameTypes/MapCoordinates.h"
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace AIJH {
class AIPlayerJH;
}
class GamePlayer;
class nobUsual;

namespace civai {

class CivAI;
struct Recipe;
struct Renewer;
struct Source;

/// State of a ware, of a group of wares or of the soldiers, from the measured statistic (DESIGN.md, "Neufassung der
/// Planung")
enum class SupplyState : uint8_t
{
    /// Forecast negative: what is free and what will be made in the next balance length does not cover what will be
    /// used. More producers are needed
    Shortage,
    /// Forecast not negative and at most the threshold: the normal state of a working economy
    Normal,
    /// Forecast above the threshold: more consumers are needed to use it
    Surplus
};

/// What the AI knows about a ware, a group of wares or the soldiers in the current planning step
struct WareStatus
{
    /// In the warehouses
    unsigned stock = 0;
    /// Produced minus consumed in the last BALANCE_MINUTES minutes
    int balance = 0;
    /// Consumed per minute in the last BALANCE_MINUTES minutes
    double consumptionPerMinute = 0;
    /// Requested ("angefragt") in two parts, and what is on the way (only shown). An entry asking for any of several
    /// wares (a construction cost by tags, "any of" inputs, a tool slot) is shared out among its wares
    /// (ComputeStatuses): a ware counts the part given to it, a group the parts given to its wares and what is missing
    /// for the entries of exactly its wares (`missing`).
    /// open: asked for by the game but not sent yet (building sites, inputs of buildings, tool slots, coins, research
    /// ...),
    double openDemand = 0;
    /// in transit: on the way to the building that asked for it (left the stock already, only shown),
    unsigned inTransit = 0;
    /// "reserviert": on the way (roads, flags) to a warehouse or without a goal: counts as free
    double onTheWay = 0;
    /// planned: tools and worker of the building sites, construction costs, tools and worker of the building sites
    /// just ordered (commands on their way) and of the orders of the list that are not blocked, the tools kept in stock
    double planned = 0;
    /// Part of planned for the orders of the list not placed yet (their construction costs and tools, the worker of
    /// their building). The rules give these to the orders in the order of the list
    double plannedByOrders = 0;
    /// Stock minus open minus planned: what is really free. Negative: more is requested than there is
    double free = 0;
    /// Of open, planned and consumption: what the entries asking for exactly these wares get from none of them
    double missing = 0;
    /// Start stock not used yet (only for the wares of rttr:SetAIStartStockNoSurplus, otherwise 0): no surplus
    unsigned startStockLeft = 0;
    SupplyState state = SupplyState::Normal;
    /// Forecast for the next balance length: wares made and used. Measured in the last balance length, plus what the
    /// coming buildings (building sites, buildings starting up) make and use by the measured value per building of
    /// their type, plus what buildings standing still (any problem) would use. Tools: what their makers make of the
    /// tools ordered and can make beyond
    double forecastProduction = 0, forecastConsumption = 0;
    /// free + forecastProduction - forecastConsumption. Decides the state, the number of producers and the order of
    /// the list
    double forecast = 0;
    /// Shortage: wares per minute missing (the negative forecast over one balance length). 0 otherwise
    double missingPerMinute = 0;
};

/// Wares a consumer takes equally ("Bedarfsgruppe"): the wares of a construction cost entry, the food of a mine, all
/// tools of one kind (iron and copper axes) ... A ware taken alone is a group of one. The groups come from the game
/// data (inputs, costs, tools, recruiting, research) and from what the game asks for, nothing is named in the code
struct WareGroup
{
    /// Sorted
    std::vector<GoodType> wares;
    /// A consumer takes exactly these wares: only then the state of the group decides about needed orders
    bool demanded = false;
    WareStatus status;
};

/// Needed: there is a measured problem and the building is its solution. Expanding: there is no problem, the building
/// makes the economy bigger (only buildings of the list of ai.lua, and harvesters of deposits)
enum class OrderKind : uint8_t
{
    Needed,
    Expanding
};

/// Why an order exists. It is removed when its cause is gone (not placed yet, no building of it placed)
enum class OrderCause : uint8_t
{
    /// Needed: makes a ware of a group in shortage (Order::group). May use a surplus (Order::usesWare)
    Shortage,
    /// Needed: makes an input the order above (Order::parent) lacks
    Input,
    /// Needed: a ware the warehouses need to recruit the soldiers that are missing
    Soldiers,
    /// Needed: a warehouse which creates helpers (none free)
    Helpers,
    /// Needed: a warehouse, the storage is full
    Storage,
    /// Needed: renews the source around a harvester for good (Order::near), e.g. a forester
    Renewal,
    /// Needed or expanding (like the order it is for): a military building for more land (Schritt 3). For an order
    /// without a place, for a source only outside the territory, or (expanding) for the region the scarcest wares come
    /// from (Order::territory)
    Territory,
    /// Expanding: makes a ware in normal state
    Expansion,
    /// Expanding: harvests deposits on the map (trees, stone piles, map objects of lua), independent of the stock
    Deposits
};

enum class OrderState : uint8_t
{
    /// To be placed when its turn comes
    Open,
    /// Waits for the orders above it (e.g. the farm the brewery needs) or for its inputs
    Waiting,
    /// A rule is not met and could not be solved now. Tried again when its retry condition is met
    Blocked,
    /// Building site placed. Stays in the list until the building is finished (renewers: until they worked one
    /// balance length)
    Placed
};

/// What a military building (OrderCause::Territory) is for
enum class TerritoryNeed : uint8_t
{
    None,
    /// An order found no place (its building, ware and source in Order::forBuilding, forWare, territorySource)
    Place,
    /// A way to a ware is blocked because its source is only outside the territory
    Source,
    /// No problem: more land where the scarcest wares come from
    Expansion
};

/// What changes when the territory grows or shrinks, a technology is researched or buildings are removed
struct Situation
{
    unsigned land = 0, researched = 0, buildings = 0;
};

/// When a blocked order is tried again (cheap to check every step, no timer)
struct RetryCondition
{
    enum class Kind : uint8_t
    {
        /// Immediately (not blocked)
        Now,
        /// Construction material for the building is there
        Material,
        /// A builder is free or can be recruited
        Builder,
        /// A helper is free in a warehouse
        Helpers,
        /// Fewer buildings of the type than the limit (expanding orders)
        Limit,
        /// The tool the building needs is in a warehouse
        Tool,
        /// A technology was researched
        Research,
        /// No way to a ware it needs: tried again every step (the ways are checked anyway)
        NoWay,
        /// Military building: a soldier is free in the warehouses for it (one for every military building site)
        Soldiers,
        /// No place: the territory changed or buildings were removed
        NoPlace
    };
    Kind kind = Kind::Now;
    /// Situation when the order was blocked
    Situation situation;
};

/// One entry of the priority list: one building to place
struct Order
{
    unsigned id = 0;
    BuildingType building = BuildingType::Headquarters;
    OrderCause cause = OrderCause::Shortage;
    /// From the cause: needed or expanding
    OrderKind kind = OrderKind::Needed;
    /// Ware the building is ordered to make. Nothing: warehouses, renewers
    GoodType forWare = GoodType::Nothing;
    /// Shortage: a surplus this producer uses (it was chosen for it). Nothing: none
    GoodType usesWare = GoodType::Nothing;
    /// Shortage: index of the group in shortage. Tools: the group of tools needing a maker
    unsigned group = 0;
    /// Renewal: the type of the harvester it renews for
    BuildingType forBuilding = BuildingType::Headquarters;
    /// Order this one was created for (Input, Tools, Helpers, Renewal for a harvester without a place). 0: none
    unsigned parent = 0;
    /// Other orders this one is the solution for too (the same building for the same ware, e.g. one sawmill for the
    /// boards of a farm and of a well). It stands above them as well
    std::vector<unsigned> alsoFor;
    OrderState state = OrderState::Open;
    /// Why it waits or is blocked, for the log, e.g. "Boards fehlt: ..."
    std::string reason;
    RetryCondition retry;
    /// Place it near this point (renewer near its harvester, harvester beside its renewer). Invalid: search near
    /// warehouses/military buildings
    MapPoint near = MapPoint::Invalid();
    /// Harvester for good without a place with enough of its source in reach: placed beside the site of the renewer
    /// ordered for it (at `near`), the source in reach does not count
    bool besideRenewer = false;
    /// Harvester of a renewable source which clears the land (no renewer, demolished when nothing is left)
    bool clearing = false;
    /// Needed order whose building is part of the chain making the construction material it lacks, and which can work
    /// (e.g. a woodcutter lacking boards). It gets the material before all other orders and buildings
    bool selfSolving = false;
    /// Inputs that are the reason it cannot be built (missing, no way). Orders below it in the list must not get a
    /// building that uses them (the wares are kept for this one)
    std::vector<GoodType> inputsMissing;
    /// How many anchors of the place search were searched in this round (the search is spread over several steps if the
    /// computing budget runs out)
    unsigned centersTried = 0;
    /// Where it was placed and when the start-up time ends (renewers)
    MapPoint placedAt = MapPoint::Invalid();
    unsigned startupEndGF = 0;
    /// Created (needed roots: sorted by urgency, expanding: by this number, blocked ones get a new one at the end)
    unsigned sequence = 0;
    /// Territory: what the military building is for, the source to reach (kind None: land to build on) and the last
    /// step a need asked for it (source outside)
    TerritoryNeed territory = TerritoryNeed::None;
    Source territorySource;
    unsigned territoryStep = 0;
};

/// Plans the production buildings of the civilization mode with a priority list (see DESIGN.md, "Neufassung der
/// Planung"). Natural limits only: construction material, builders, workers, tools, a place with something to work on
/// in reach. Everything it decides on is measured: EconomyStatistics and EconomySnapshot
class EconomyPlanner
{
public:
    /// Balance length, threshold of a surplus (SURPLUS_FACTOR x consumption per minute, at least MIN_STOCK) and the
    /// tools kept in stock: set in civ/EconomyForecast.h (the same calculation for the distribution of all wares)
    static constexpr unsigned BALANCE_MINUTES = civ::BALANCE_MINUTES;
    static constexpr unsigned SURPLUS_FACTOR = civ::SURPLUS_FACTOR;
    static constexpr unsigned MIN_STOCK = civ::MIN_STOCK;
    /// Harvesters clearing trees are placed where at least this many trees are in reach (independent of the stock)
    static constexpr unsigned CLEARING_MIN_TREES = 8;
    /// Harvesters of other deposits (stone piles, map objects of lua) are placed where at least this many deposits are
    /// in reach (independent of the stock); sollte nicht in Depos rechnen sondern in verfügbaren Ressourcen der Depots
    static constexpr unsigned MIN_DEPOSITS_IN_REACH = 1;
    /// A harvester for good gets a renewer (forester) when fewer than this many of its source (trees) are in its reach
    static constexpr unsigned RENEW_BELOW = 7;
    /// A new harvester of a source on the map (trees, stone piles, map objects of lua, fish, ore) needs this many units
    /// of it in its reach, each node shared with the other harvesters of the source reaching it (Nutzer, 2026-11-18,
    /// for all sources). None of them reaching its nodes: 1 unit suffices
    static constexpr unsigned UNITS_PER_HARVESTER = 28;
    /// A clearing woodcutter with fewer than this many trees in its reach becomes one for good when a needed forester is
    /// in the list: it gets that forester (Nutzer, 2026-11-18)
    static constexpr unsigned CLEARING_UPGRADE_BELOW = 7;
    static constexpr unsigned TOOLS_IN_STOCK = civ::TOOLS_IN_STOCK;
    /// A chain is stuck if nothing of its ware was made in this many minutes (and the warehouses do not have enough):
    /// then building sites may be cancelled for an order that solves it
    static constexpr unsigned STUCK_MINUTES = 9;
    /// Computing budget: place searches (around one warehouse or military building each) per planning step
    static constexpr unsigned SEARCHES_PER_STEP = 8;
    /// While an order waits for the building site of an order placed for it, the planner checks every this many GFs
    /// (independent of the level of the AI), so the building follows without delay
    static constexpr unsigned FOLLOW_UP_GFS = 20;
    /// At most this many orders of one building type at the same time in the list (also those placed and not finished)
    static constexpr unsigned MAX_ORDERS_PER_TYPE = 5;
    /// The list is worked on again for the orders made while working on it (producers of missing inputs ...), at most
    /// this many times per step
    static constexpr unsigned MAX_LIST_PASSES = 4;
    /// Farms and plantations: at least this many nodes for fields in their reach (fields there or free for new ones).
    /// Fewer at a building: diagnosis "Felder fehlen"; a new one is only placed where there are enough
    static constexpr unsigned MIN_FIELD_NODES = 4;
    /// The source and the fields around a building are counted again after this many GFs (they change slowly)
    static constexpr unsigned SOURCE_CHECK_GFS = 400;

    /// Radius around the anchors of a building (its renewer or harvester, an input placed for it, its partners) in
    /// which places are searched. Then the buildings holding territory are searched, each in its military radius
    static constexpr unsigned SEARCH_RADIUS = 15;
    /// Places up to this much further than the nearest place meeting all rules compete by the preferences
    static constexpr unsigned PLACE_BAND = 2;
    /// Military buildings: regions whose center is at most this far from an own military building or warehouse
    static constexpr unsigned MAX_TERRITORY_DISTANCE = 24;

    EconomyPlanner(AIJH::AIPlayerJH& aijh, const CivAI& civAI);

    /// Called every GF, plans every GetReactionGFs() GFs
    void RunGF(unsigned gf);
    /// One planning step (public for tests)
    void Step();

    /// GFs between two planning steps, after the level of the AI (reaction time: hard 1 s, medium 5 s, easy 20 s)
    unsigned GetReactionGFs() const { return reactionGFs_; }
    /// Orders in the order of the list (top first)
    std::vector<const Order*> GetList() const;
    /// Status of the ware alone (the group of one)
    const WareStatus& GetWareStatus(GoodType good) const;
    /// Status of the group with exactly these wares, nullptr if there is none
    const WareGroup* FindGroup(std::vector<GoodType> wares) const;
    const std::vector<WareGroup>& GetGroups() const { return groups_; }
    /// Soldiers in the warehouses, recruited and sent to military buildings, asked for by military buildings
    const WareStatus& GetSoldierStatus() const { return soldiers_; }
    /// The woodcutter (harvester of a renewable source) at the point clears land (no renewer, demolished when nothing
    /// is left)
    bool IsClearingWoodcutter(MapPoint pt) const;
    /// The planner decides about this building type (the old AI does not build it)
    bool IsManaged(BuildingType bld) const { return managed_[bld]; }
    /// Most buildings of one type for expanding orders: base + per start area * start areas (ai.lua,
    /// rttr:SetAIBuildingLimit). A start area is the land of an HQ; the land is counted in whole start areas, at least
    /// 1
    unsigned GetBuildingLimit() const;
    /// Buildings, building sites and ordered sites of the type
    unsigned GetNumBuildings(BuildingType bld) const;
    /// The warehouses are full: their load reached the percentage of ai.lua (rttr:SetAIStorageFull) of their capacity.
    /// Then only needed orders are built and one more warehouse is ordered
    bool IsStorageFull() const;
    /// Text for the log: shortages, surpluses and the list with reasons
    std::string GetReport() const;
    /// The order is not placed and not blocked: its construction costs, tools and worker are requested ("angefragt").
    /// A blocked order counts in no calculation (Nutzer, 2026-11-17), needed and expanding ones alike. An order waiting
    /// for resources is not blocked (WaitsForResources)
    static bool IsPlanned(const Order& order);
    /// Waits for construction material, a builder, a tool, helpers or soldiers (state Blocked with that retry
    /// condition): it is built when they are there, it counts like a waiting order ("wartet" in the report)
    static bool WaitsForResources(const Order& order);
    /// The old AI tears down a building of the player with nothing left in reach (for the report)
    void OnTornDown(MapPoint pt, BuildingType bld);
    /// Duration of the last step and the maximum (microseconds)
    double GetLastStepUs() const { return lastStepUs_; }
    double GetMaxStepUs() const { return maxStepUs_; }

    /// Trees (any size) in the radius around the point
    static unsigned CountTrees(const GamePlayer& player, MapPoint pt, unsigned radius);
    /// Stone piles in the radius around the point
    static unsigned CountStonePiles(const GamePlayer& player, MapPoint pt, unsigned radius);

private:
    /// How a way to a ware looks now (for the choice between alternatives)
    struct WayCheck
    {
        bool blocked = true;
        /// New buildings the way needs right now (the producer and producers of its missing inputs)
        unsigned numNew = 0;
        /// Measured output per building of the producer type (wares per minute), by the work time if none worked yet
        double output = 0;
        unsigned cost = 0;
        unsigned recipe = 0;
        /// Tools: how good the tool is (work speed and output bonus, durability), higher is better. 0 for other wares
        unsigned quality = 0;
        /// An input of the producer is in surplus (preferred among equal ways: the surplus is used)
        bool usesSurplus = false;
        /// Another producer can be placed (false: no place was found, the existing ones make the ware)
        bool canAdd = true;
        /// Blocked: why, for the list (e.g. "kein Lehmvorkommen im Gebiet")
        std::string reason;
        /// Blocked because of these inputs (no way to make them, not enough there)
        std::vector<GoodType> missingInputs;
        /// Blocked because the source of a producer (this one or one of an input) is only outside the territory: that
        /// producer, its ware and source
        bool outside = false;
        BuildingType outsideBuilding = BuildingType::Headquarters;
        GoodType outsideWare = GoodType::Nothing;
        Source outsideSource;
    };
    /// A way to a ware for which no place was found. Not tried again before the situation changed
    struct FailedWay
    {
        GoodType ware;
        BuildingType building;
        Situation situation;
    };
    /// What a need for more of a ware is (ResolveShortage)
    struct Need
    {
        OrderCause cause = OrderCause::Shortage;
        /// Input, Helpers: the order it is for
        unsigned parent = 0;
        /// Shortage, Tools: the group
        unsigned group = 0;
        /// Wares per minute that are missing by the forecast (it counts the coming producers and consumers): as many
        /// producers as they need (the orders of other needs for the ware count)
        double perMinute = 0;
        /// For an expanding order: the orders made for it are expanding too, expanding orders for the ware count
        bool expanding = false;
    };
    /// What keeps an existing building from working, checked for every building in every step (the productivity is a
    /// symptom only, it decides nothing). The first problem found (civ::diagnose)
    using Diagnosis = civ::Diagnosis;
    /// Counted around a building or a place (cached for SOURCE_CHECK_GFS)
    struct SourceInfo
    {
        unsigned gf = 0;
        /// Nodes of its source in its reach and in the near half of it, nodes for fields
        unsigned inReach = 0, nearby = 0, fields = 0;
    };

    // Status
    void RegisterGroups();
    /// Index of the group with exactly these wares (created if it does not exist)
    unsigned GetGroupIdx(std::vector<GoodType> wares, bool demanded);
    void UpdatePlanned();
    /// Send the planned demand of the orders of the list to the game (distribution of all wares) if it changed
    void SendPlannedDemand();
    /// Construction costs and/or tools and worker of one more building of the type, for the order (0: a building
    /// site). No input spaces: what a building will use counts as its consumption in the forecast. The status is
    /// computed again by RecomputeForecast
    void AddPlanned(BuildingType bld, bool withCosts, bool withTools, unsigned orderId);
    void AddPlannedEntry(const OpenDemand& entry, unsigned orderId);
    void AddPlannedWorker(BuildingType bld, unsigned orderId);
    /// Forecast of all wares, tools, soldiers and helpers. At the start of the step and after every building placed
    void RecomputeForecast();
    void UpdateSoldierStatus();
    /// Helpers like a ware: in the warehouses, requested by workers that are wanted and by coming buildings, created
    /// by the warehouses and used for soldiers and carriers
    void UpdateHelperStatus();
    /// Forecast, state and missing amount from free, forecast production and consumption
    void FinishStatus(WareStatus& status) const;
    /// Game minutes the measured counts of the last balance length cover (at least 1)
    double GetMeasuredMinutes() const;
    /// Forecast of every ware and group by the calculation of the game (civ::EconomyForecast) with the building sites
    /// ordered and the orders of the list
    void ComputeForecast();
    /// Diagnose with the result kept for the step
    Diagnosis GetDiagnosis(const nobUsual& building, const Recipe& recipe, std::vector<GoodType>& missingInputs);
    /// The source and the fields around the point for a building of the type (cached for SOURCE_CHECK_GFS)
    const SourceInfo& GetSourceInfo(MapPoint pt, BuildingType bld, const Recipe& recipe, bool placed) const;
    /// The building grows fields around it (farm, plantation of lua)
    bool NeedsFields(BuildingType bld) const;
    /// Nodes for fields of the building type around the point: fields there and nodes free for new ones
    unsigned CountFieldNodes(BuildingType bld, MapPoint pt, bool placed) const;
    /// Expanding orders are sorted by this only, lowest first: the forecast of the product, and for a consumer the
    /// mean of it and the negative forecast of its inputs (a consumer of a surplus comes first)
    double GetExpandingScore(const Order& order) const;
    /// Where the building of the order should stand close to: the site of an input ordered for it, or the site of
    /// another input ordered for the same order. Invalid: nowhere special
    MapPoint GetPreferredPoint(const Order& order) const;
    /// A plantation of the player has or could put a field on the point (or on the flag of a building there)
    bool IsFieldPlace(MapPoint pt) const;
    void UpdateWorkingBuildings();
    void UpdatePlacedOrders();

    // Creating orders
    void CreateOrders();
    /// The cause of the order still holds (it is removed otherwise)
    bool IsCauseValid(const Order& order);
    /// More of the ware: choose a producer and add orders until the forecast is 0 or above (the orders of other needs
    /// for the wares count)
    void ResolveShortage(GoodType good, const Need& need);
    /// Use the surplus of a group: a needed producer if the product of its consumer is short, an expanding consumer
    /// if it is normal
    void ResolveSurplus(unsigned groupIdx);
    /// Soldiers are short: the wares the warehouses need to recruit them
    void ResolveSoldiers();
    /// Tools a building of the type will ask for: the tool to recruit its worker (only jobs recruited with a tool),
    /// the tools of its slots
    void AddToolDemand(std::vector<OpenDemand>& demand, BuildingType bld) const;
    /// The tool to recruit the worker of a building of the type (none if a worker waits in a warehouse or the job is
    /// recruited without tool)
    GoodType GetRecruitTool(BuildingType bld) const;
    void AddExpandingOrder(GoodType good);
    /// Harvesters of deposits: from the trees, stone piles and map objects in the territory, not from the stock
    void AddDepositOrders();
    /// Harvesters for good with few of their source in reach get a renewer
    void AddRenewalOrders();
    /// A military building for the need (Schritt 3). Needed or expanding like the order it is for; one per need
    void AddTerritoryOrder(TerritoryNeed need, OrderKind kind, unsigned parent, GoodType ware, BuildingType bld,
                           const Source& source);
    /// No problem: one expanding military building at a time, for the region the scarcest wares come from
    void AddExpandingTerritoryOrder();
    /// The need of a military order still holds
    bool IsTerritoryNeeded(const Order& order) const;
    /// Wares with a source on the map, their source and how scarce they are (low forecast = high scarcity)
    struct ScarceSource
    {
        Source source;
        GoodType ware;
        double scarcity;
    };
    std::vector<ScarceSource> GetScarceSources() const;
    /// The region outside the territory for the military order: value = scarcity × amount (both normalized) − distance
    /// − danger (enemy military buildings). -1: none. `ware`: the ware the value comes from most
    int ChooseTargetRegion(const Order& order, GoodType* ware) const;
    /// All candidate regions for the military order, the best first, and the ware each one is for
    std::vector<unsigned> RankTargetRegions(const Order& order, std::vector<GoodType>* wares) const;
    /// Place of a military building toward the target region (type by the rules of the old AI at that place)
    MapPoint SearchMilitaryPlace(Order& order, bool& searchedAll);
    /// Place near the anchor: the nearest place meeting all rules, among those up to PLACE_BAND further the best by the
    /// preferences of the user (more of the source, fewer of the same type nearby, near the partners)
    MapPoint FindPlaceNear(const Order& order, const Recipe* recipe, MapPoint anchor, unsigned radius,
                           const std::vector<MapPoint>& partners);
    /// All rules for the place of the building of the order
    bool IsPlaceValid(const Order& order, const Recipe* recipe, MapPoint pt);
    /// Enough units of the source of the harvester in its reach at the place: only nodes its worker can walk to (path
    /// of at most 20 like the worker), each node shared with the other harvesters of the source reaching it (buildings,
    /// sites, sites ordered). At least UNITS_PER_HARVESTER, 1 if no other harvester reaches its nodes
    bool HasUnitsForHarvester(const Recipe& recipe, MapPoint pt) const;
    /// A clearing woodcutter with few trees left becomes one for good and gets a needed forester of the list
    void UpgradeClearingWoodcutters();
    /// Buildings and sites making an input of the recipe or using its product, the most central first (nearest to a
    /// warehouse), at most `max`
    std::vector<MapPoint> GetPartners(const Recipe& recipe, unsigned max) const;
    /// Warehouse for more helpers (the warehouses create them)
    void AddHelpersOrder(unsigned parent);
    /// Storage full: a needed warehouse (one at a time)
    void AddStorageOrder();
    /// Add the order. A needed order replaces the same expanding one (same building and ware, it would stand lower); an
    /// expanding order is not added if the same one exists. Not more than MAX_ORDERS_PER_TYPE of a building type
    Order* AddOrder(Order order);
    /// Orders of the building type in the list (any state)
    unsigned CountOrders(BuildingType bld) const;

    // Ways and buildings
    /// Producers of the ware usable as a way, the best first: not blocked, better tool, fewest new buildings, uses a
    /// surplus, highest measured output per building, lowest construction costs. onlyExpanding: only buildings of
    /// ai.lua
    std::vector<WayCheck> RankProducers(GoodType good, bool onlyExpanding, unsigned depth);
    /// `given`: input that is there anyway (the surplus a consumer is ordered for)
    WayCheck CheckWay(unsigned recipeIdx, GoodType good, unsigned depth, GoodType given = GoodType::Nothing);
    /// The ware of the list which is best to make now (any of them would do). Nothing if none can be made
    GoodType ChooseWare(const std::vector<GoodType>& wares);
    /// True if the input suffices for one more consumer of the recipe: it is in surplus, or its forecast stays at 0
    /// or above with what the new building uses in one balance length (measured value per building)
    bool InputSuffices(GoodType input, const Recipe& consumer) const;
    /// The inputs of the recipe suffice for one more building (all of them, or one of them for "any of")
    bool InputsSuffice(const Recipe& recipe, GoodType given = GoodType::Nothing) const;
    Diagnosis Diagnose(const nobUsual& building, const Recipe& recipe, std::vector<GoodType>& missingInputs) const;
    /// Measured per building of the type and minute in the last balance length (all buildings of the type, also those
    /// with problems): made of the ware / used of the input. By the work time if no building of the type worked yet
    double ProducedPerBuilding(BuildingType bld, GoodType good) const;
    double UsedPerBuilding(BuildingType bld, GoodType good) const;
    /// Tool makers: tools one maker makes per minute while it works (measured, or by the work time)
    double OutputPerWorkingMinute(BuildingType bld) const;
    /// The building makes the ware (one of its products)
    bool MakesWare(BuildingType bld, GoodType good) const;
    /// A building of the type can work once finished: the forecasts of the tools it needs to work
    /// (CheckToolsAndHelpers) and of the helpers allow one more (all planned ones counted). `reason`: why not
    bool CanWorkWhenBuilt(BuildingType bld, std::string* reason = nullptr) const;
    /// Buildings of the type that wait (input or worker missing, no tool and not working without) and are not working
    unsigned CountWaitingBuildings(BuildingType bld);
    /// Buildings of the chain that makes the ware now: the best producer, the best producers of its inputs ... (and
    /// the renewer of a source it uses)
    std::vector<BuildingType> GetChain(GoodType good);
    /// Why the ware is missing, following its producers: "Holz: frei 0, Bilanz -3; Holzfäller: keiner arbeitet"
    std::string ExplainWare(GoodType good, unsigned depth);
    /// The construction material missing for the building in detail, e.g. "Bretter fehlen: 2 nötig, 0 frei (Lager 6,
    /// für Baustellen 6, für Aufträge darüber 0)". Empty if nothing is missing. `missing`: the wares that are missing
    std::string DescribeMissingMaterial(BuildingType bld, std::vector<GoodType>& missing) const;
    /// Self-solving order lacking material (ware and amount): cancel building sites outside the chain whose material is
    /// not built in yet and which are not the solution of a needed order, until enough comes back. Returns what was
    /// cancelled, for the reason
    std::string CancelSitesFor(const std::vector<std::pair<GoodType, unsigned>>& lacking);
    const Recipe* FindRecipe(BuildingType bld) const;
    /// Recipes making the source the renewer renews (harvesters)
    bool IsRenewable(const Source& source) const;
    /// The renewer of the source which the player may build, nullptr if there is none
    const Renewer* FindRenewer(const Source& source) const;
    /// How much of its source a harvester needs in reach at its place: clearing woodcutters CLEARING_MIN_TREES,
    /// harvesters for good of a renewable source RENEW_BELOW (fewer: a renewer comes first), other deposits
    /// MIN_DEPOSITS_IN_REACH, ore and fish 1
    unsigned GetMinSourceInReach(const Order& order, const Recipe& recipe) const;
    /// The order or one above it makes the ware or is of the building type (one more would be a loop). Headquarters as
    /// building: only the ware is checked
    bool IsInChain(unsigned orderId, GoodType good, BuildingType bld) const;
    /// Warehouse type to build (helpers, storage): the cheapest one allowed that stores people and all wares and needs
    /// no harbor place. Headquarters: none
    BuildingType ChooseWarehouse() const;
    /// The ware is a tool (made on order)
    static bool IsTool(GoodType good);
    /// All products of the recipe are tools: it works on order
    static bool IsToolMaker(const Recipe& recipe);
    /// Several products chosen by the distribution of all wares
    static bool IsMultiProduct(const Recipe& recipe);
    /// Regel A: what the buildings of the type with several products cannot make of all their products together, per
    /// minute. A group short of wares counts only for the type chosen for it (groupWares, groupForecast)
    double GetMultiProductUnmet(const Recipe& recipe, const std::vector<GoodType>& groupWares, double groupForecast);
    // The list
    Order* Find(unsigned id);
    const Order* Find(unsigned id) const;
    /// Removes the order and the not yet placed orders created for it
    void Remove(unsigned id);
    bool HasChildFor(unsigned parent, GoodType good) const;
    /// A building of an order created for this one is placed (the chain was started)
    bool HasPlacedChild(unsigned id) const;
    /// Removes the not yet placed orders created for this one (it cannot be built, they would be of no use)
    void RemoveUnplacedChildren(unsigned id);
    /// The order or one above it is blocked until the situation changes (no way, no place, not researched)
    bool IsChainBlocked(unsigned id) const;
    /// Sort key of an order: group (needed self-solving, needed, expanding), level of the basic economy, urgency ...
    /// An order created for another one is never below it
    struct SortKey
    {
        unsigned group, level;
        double urgency;
        unsigned rootSequence, depth, sequence;
    };
    SortKey GetSortKey(const Order& order) const;
    static bool IsAbove(const SortKey& lhs, const SortKey& rhs);
    double GetUrgency(const Order& root) const;

    // Working on the list
    /// Orders whose building is part of the chain making the construction material they lack
    void MarkSelfSolving();
    void ProcessList();
    void ProcessOrder(unsigned id, unsigned& searchBudget);
    /// Checks the rules. Returns the reason of the first rule not met (empty: all met) and sets the retry condition
    std::string CheckRules(const Order& order, RetryCondition& retry) const;
    /// Needed order: work on the unmet rule (e.g. order the producer of the missing material)
    void SolveRule(unsigned id, const RetryCondition& retry);
    /// Needed order lacking material: the material that is there is kept for it (orders below and other buildings
    /// must not take it)
    void KeepMaterial(const Order& order);
    MapPoint SearchPlace(Order& order, unsigned& searchBudget, bool& searchedAll);
    /// Expanding order which cannot be placed now: to the end of the list
    void MoveToEnd(Order& order, const std::string& reason, const RetryCondition& retry);

    // Rules
    /// The construction material for the building is there (after what the orders above in the list took)
    bool HasMaterial(BuildingType bld) const;
    /// Helpers in the warehouses not yet promised to building sites or asked for as workers
    int GetFreeHelpers() const;
    /// Builders in the warehouses and helpers who can become builders now
    unsigned GetAvailableBuilders() const;
    /// A building of the type needs a helper for its worker: no worker of its job is free in a warehouse
    bool NeedsHelper(BuildingType bld) const;
    /// Tools of the kind of the ware in the warehouses (all materials)
    unsigned GetToolStock(GoodType tool) const;
    /// The forecast of the tools the building needs to work and of the helpers allows it. Tools it needs: the tool to
    /// recruit its worker; tool reform: a tool of any of its slots if it does not work without tools (base speed 0),
    /// none otherwise. withList: only the planned entries of the building sites and of the orders above it in the list
    /// count (rules of the list); otherwise all planned ones
    std::string CheckToolsAndHelpers(const Order* order, BuildingType bld, bool withList) const;
    /// The helper part of CheckToolsAndHelpers (counted: the order is planned)
    std::string CheckHelpers(const Order* order, BuildingType bld, bool withList, bool counted) const;
    /// A needed order processed without being placed keeps its tools and helper for itself (orders below must not take
    /// them)
    void ClaimToolsAndHelpers(const Order& order);
    /// The situation at the start of the step
    Situation GetSituation() const;
    Situation ComputeSituation() const;
    bool IsRetryMet(const RetryCondition& retry, const Order& order) const;

    AIJH::AIPlayerJH& aijh_;
    const GamePlayer& player_;
    const CivAI& civAI_;
    unsigned reactionGFs_;
    unsigned currentGF_ = 0;
    Situation situation_;

    EconomySnapshot snapshot_;
    /// All groups
    std::vector<WareGroup> groups_;
    /// Group of each ware alone
    helpers::EnumArray<unsigned, GoodType> singleGroup_{};
    /// Group by its (sorted) wares
    std::map<std::vector<GoodType>, unsigned> groupIndex_;
    static constexpr unsigned NO_GROUP = std::numeric_limits<unsigned>::max();
    /// Group of all tools of a kind (iron and copper axes ...), NO_GROUP if there are none
    helpers::EnumArray<unsigned, Tool> toolKindGroup_{};
    /// Groups containing the ware
    helpers::EnumArray<std::vector<unsigned>, GoodType> groupsOf_;
    WareStatus soldiers_, helpers_;
    /// The ware is used by anything (input of a building, construction, tool, soldier equipment, military, research)
    helpers::EnumArray<bool, GoodType> hasUse_{};
    /// Recipes taking the ware as input
    helpers::EnumArray<std::vector<unsigned>, GoodType> consumers_;
    /// The planner decides about these buildings. Military buildings, harbor, shipyard ... stay with the old AI
    helpers::EnumArray<bool, BuildingType> managed_{};
    /// Clearing woodcutters placed whose mark (game command, kept in the savegame) is not executed yet
    std::vector<MapPoint> clearingPending_;
    /// Clearing woodcutters made woodcutters for good whose command is not executed yet
    std::vector<MapPoint> unclearingPending_;
    /// Orders whose construction costs are counted as planned at the start of the step
    std::vector<unsigned> plannedOrderIds_;
    /// Planned entries this step (costs, tools), any number of wares each, and the order each one is for (0: a building
    /// site)
    std::vector<OpenDemand> planned_;
    std::vector<unsigned> plannedFor_;
    /// Planned demand sent to the game last (SendPlannedDemand)
    std::vector<civ::PlannedDemand> sentPlanned_;
    /// A helper requested for the worker of each building site and planned order (the order, 0: a building site), and
    /// the workers of each job that are free in the warehouses and planned for them
    std::vector<unsigned> plannedHelpers_;
    helpers::EnumArray<unsigned, Job> workersPlanned_{};
    /// Tools (per group) and helpers kept by needed orders processed in this step without being placed
    std::vector<unsigned> toolsClaimed_;
    unsigned helpersClaimed_ = 0;
    /// GetChain per ware in this step
    std::map<GoodType, std::vector<BuildingType>> chainCache_;
    /// Construction material in the warehouses not needed by building sites at the start of the step (for the reasons)
    AIJH::ConstructionMaterial stepFreeMaterial_{};
    /// Building sites cancelled for self-solving orders this step (one cancel round per step)
    bool cancelledThisStep_ = false;
    /// Depth of GetSortKey following the orders an order is also for (guard against loops)
    mutable unsigned sortKeyDepth_ = 0;
    /// Inputs kept for an order higher in the list that cannot be built without them (ware -> order). Orders further
    /// down using one of them are not built in this step
    std::map<GoodType, unsigned> keptInputs_;

    /// Construction material in the current step: free in the warehouses, minus what the orders processed so far took
    /// (placed or kept for needed orders waiting for material)
    AIJH::ConstructionMaterial freeMaterial_{}, reservedMaterial_{};

    std::vector<Order> orders_;
    unsigned nextId_ = 1;
    unsigned nextSequence_ = 1;
    /// Results of CheckWay in the current step (by recipe, ware and given input)
    std::map<std::tuple<unsigned, GoodType, GoodType>, WayCheck> wayCache_;
    std::vector<FailedWay> failedWays_;
    /// Wares whose ways are being checked / resolved right now (to find loops)
    std::vector<GoodType> checking_, resolving_;
    /// Number of loops found while checking ways (results depending on a loop are not cached)
    unsigned numLoops_ = 0;

    /// Building sites placed by the planner: when and why (to tell needed from expanding sites, and for the report when
    /// one is torn down)
    struct Placement
    {
        unsigned gf;
        BuildingType building;
        OrderCause cause;
        OrderKind kind;
        /// Of its source in reach when it was placed, -1 if not counted (no source on the map, animals ...)
        int sourceInReach;
    };
    std::map<MapPoint, Placement, MapPointLess> placements_;
    /// The last buildings torn down because nothing was left in their reach (for the report)
    std::vector<std::string> tornDown_;
    /// Shortages (by group) and soldier wares without an order in this step: why (for the report)
    std::map<unsigned, std::string> shortageNotes_;
    std::map<GoodType, std::string> soldierNotes_;
    /// Inputs of an order (by order id) without an order in this step: why (for the report at the order)
    std::map<unsigned, std::vector<std::string>> inputNotes_;

    /// The calculation of the game (also for the distribution of all wares)
    std::unique_ptr<civ::EconomyForecast> forecast_;
    std::map<MapPoint, std::pair<Diagnosis, std::vector<GoodType>>, MapPointLess> diagnosisCache_;
    mutable std::map<std::tuple<MapCoord, MapCoord, BuildingType>, SourceInfo> sourceCache_;
    /// The ware is construction material of any building
    helpers::EnumArray<bool, GoodType> isMaterial_{};
    /// An order waits for the building site of an order placed for it: plan again soon
    bool followUp_ = false;
    unsigned lastReportMinute_ = std::numeric_limits<unsigned>::max();

    double lastStepUs_ = 0, maxStepUs_ = 0;
    unsigned numSteps_ = 0;
};

} // namespace civai
