// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "EconomySnapshot.h"
#include "ai/aijh/AIConstruction.h"
#include "helpers/EnumArray.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include "gameTypes/MapCoordinates.h"
#include <cstdint>
#include <limits>
#include <map>
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
struct Source;

/// State of a ware, of a group of wares or of the soldiers, from the measured statistic (DESIGN.md, "Neufassung der
/// Planung")
enum class SupplyState : uint8_t
{
    /// Balance negative, or more is asked for than the warehouses have: more producers are needed
    Shortage,
    /// Balance not negative and stock at most the threshold: the normal state of a working economy
    Normal,
    /// Balance not negative and stock above the threshold: more consumers are needed to use it
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
    /// Requested ("angefragt") in three parts:
    /// open: asked for by the game but not sent yet (building sites, inputs, tool slots, coins, research, soldiers ...),
    unsigned openDemand = 0;
    /// in transit: on the way to the building that asked for it (left the stock already, only shown),
    unsigned inTransit = 0;
    /// planned: construction costs of the needed orders of the list and of the building sites just ordered, input
    /// spaces of building sites and of placed buildings (they will ask for these wares)
    unsigned planned = 0;
    /// Stock minus open minus planned: what is really free. Negative: more is requested than there is
    int free = 0;
    /// Start stock not used yet (only for the wares of rttr:SetAIStartStockNoSurplus, otherwise 0): no surplus
    unsigned startStockLeft = 0;
    SupplyState state = SupplyState::Normal;
    /// Shortage: minutes until the free stock is gone at the current balance (0 if more is requested than there is).
    /// Sorts the shortages: the most urgent first
    double minutesUntilEmpty = 0;
    /// Shortage: wares per minute missing, the deficit of the balance plus what is asked for beyond the stock (to be
    /// delivered within one balance length). 0 otherwise
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
    /// All wares are tools: they are made on order (EconomyPlanner::OrderTools), not by the state of the group
    bool tools = false;
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
    /// Needed: makes construction material the order above lacks
    Material,
    /// Needed: makes an input buildings of a type (Order::forBuilding) stand still for
    Idle,
    /// Needed: a tool maker. Tools are ordered and there is none that makes them, or all of them are busy
    Tools,
    /// Needed: a ware the warehouses need to recruit the soldiers that are missing
    Soldiers,
    /// Needed: a warehouse which creates helpers (none free)
    Helpers,
    /// Needed: a warehouse, the storage is full
    Storage,
    /// Needed: renews the source around a harvester for good (Order::near), e.g. a forester
    Renewal,
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
    /// Idle: the type of the buildings standing still
    BuildingType forBuilding = BuildingType::Headquarters;
    /// Order this one was created for (Input, Material, Tools, Helpers). 0: none
    unsigned parent = 0;
    OrderState state = OrderState::Open;
    /// Why it waits or is blocked, for the log, e.g. "Boards fehlt: ..."
    std::string reason;
    RetryCondition retry;
    /// Place it near this point (renewer near its harvester). Invalid: search near warehouses/military buildings
    MapPoint near = MapPoint::Invalid();
    /// Harvester of a renewable source which clears the land (no renewer, demolished when nothing is left)
    bool clearing = false;
    /// Needed order whose building is part of the chain making the construction material it lacks, and which can work
    /// (e.g. a woodcutter lacking boards). It gets the material before all other orders and buildings
    bool selfSolving = false;
    /// Inputs that are the reason it cannot be built (missing, no way). Orders below it in the list must not get a
    /// building that uses them (the wares are kept for this one)
    std::vector<GoodType> inputsMissing;
    /// Next warehouse/military building to search a place around and how many were searched in this round (the search
    /// is spread over several steps if the computing budget runs out)
    unsigned nextCenter = 0, centersTried = 0;
    /// Where it was placed and when the start-up time ends (renewers)
    MapPoint placedAt = MapPoint::Invalid();
    unsigned startupEndGF = 0;
    /// Created (needed roots: sorted by urgency, expanding: by this number, blocked ones get a new one at the end)
    unsigned sequence = 0;
};

/// Plans the production buildings of the civilization mode with a priority list (see DESIGN.md, "Neufassung der
/// Planung"). Natural limits only: construction material, builders, workers, tools, a place with something to work on
/// in reach. Everything it decides on is measured: EconomyStatistics and EconomySnapshot
class EconomyPlanner
{
public:
    /// Balance length: the statistic of the last this many minutes counts (also the time after which the productivity
    /// of a new building counts)
    static constexpr unsigned BALANCE_MINUTES = 5;
    /// The threshold of a surplus is this many times the consumption per minute ...
    static constexpr unsigned SURPLUS_FACTOR = 10;
    /// ... but at least this stock
    static constexpr unsigned MIN_STOCK = 8;
    /// High productivity (share of the time working). A building below it is examined: why does it not work?
    static constexpr double HIGH_PRODUCTIVITY = 0.8;
    /// Harvesters clearing trees are placed where at least this many trees are in reach (independent of the stock)
    static constexpr unsigned CLEARING_MIN_TREES = 8;
    /// Harvesters of other deposits (stone piles, map objects of lua) are placed where at least this many deposits are
    /// in reach (independent of the stock)
    static constexpr unsigned MIN_DEPOSITS_IN_REACH = 1;
    /// A harvester for good gets a renewer (forester) when fewer than this many of its source (trees) are in its reach
    static constexpr unsigned RENEW_BELOW = 11;
    /// Tools of each kind the player keeps in stock beyond what is asked for (tool orders)
    static constexpr unsigned TOOLS_IN_STOCK = 1;
    /// A chain is stuck if nothing of its ware was made in this many minutes (and the warehouses do not have enough):
    /// then building sites may be cancelled for an order that solves it
    static constexpr unsigned STUCK_MINUTES = 3;
    /// Computing budget: place searches (around one warehouse or military building each) per planning step
    static constexpr unsigned SEARCHES_PER_STEP = 8;

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
    /// rttr:SetAIBuildingLimit). A start area is the land of an HQ; the land is counted in whole start areas, at least 1
    unsigned GetBuildingLimit() const;
    /// Buildings, building sites and ordered sites of the type
    unsigned GetNumBuildings(BuildingType bld) const;
    /// The warehouses are full: their load reached the percentage of ai.lua (rttr:SetAIStorageFull) of their capacity.
    /// Then only needed orders are built and one more warehouse is ordered
    bool IsStorageFull() const;
    /// Text for the log: shortages, surpluses and the list with reasons
    std::string GetReport() const;
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
        /// Average productivity of the existing producers (percent), 0 if there are none
        unsigned productivity = 0;
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
        /// Input, Material, Tools: the order it is for
        unsigned parent = 0;
        /// Shortage, Tools: the group
        unsigned group = 0;
        /// Idle: the type of the buildings standing still
        BuildingType forBuilding = BuildingType::Headquarters;
        /// Wares per minute that are missing: as many producers as they need (the orders of other needs for the ware
        /// count). Negative: one producer unless one is coming (tool makers)
        double perMinute = 0;
        /// At most this many orders for the need (one for an order or for buildings standing still). 0: no limit
        unsigned maxOrders = 0;
    };
    /// Why an existing building does not work at high productivity
    enum class Diagnosis : uint8_t
    {
        /// Works at high productivity (or was stopped by the player: not examined)
        Working,
        /// Has its worker for less than a balance length: its productivity does not count yet
        StartingUp,
        /// Waits for a worker
        NoWorker,
        /// Tool reform: does not work without its tool
        NoTool,
        /// Waits for input wares
        NoInput,
        /// Its flag is full: the worker waits with the product
        FlagFull,
        /// Nothing of its source left in its reach
        Exhausted,
        /// Low productivity and no reason found: counted as it works (more buildings may be ordered)
        Unknown
    };
    /// What the existing and coming producers of a ware will add (wares per minute), see ProducerCapacity
    struct Capacity
    {
        /// Building sites, ordered sites and buildings starting up, by their work time (until their output is in the
        /// balance)
        double coming = 0;
        /// Buildings standing still for a reason that is worked on (input, worker, tool, flag): what they add once it
        /// is solved
        double reserve = 0;
        /// Buildings standing still for a reason that is worked on, and those of them with a worker
        unsigned numIdle = 0, numIdleWorking = 0;
        /// Buildings with nothing of their source left in reach
        unsigned numExhausted = 0;
        /// Buildings of the producers (finished, sites, ordered)
        unsigned numBuildings = 0;
        /// Buildings working at high productivity
        unsigned numBusy = 0;
    };

    // Status
    void RegisterGroups();
    /// Index of the group with exactly these wares (created if it does not exist)
    unsigned GetGroupIdx(std::vector<GoodType> wares, bool demanded);
    void UpdatePlanned();
    /// Construction costs and/or input spaces of one more building of the type. updateStatus: also in the state of the
    /// wares (a building placed in this step)
    void AddPlanned(BuildingType bld, bool updateStatus, bool withCosts, bool withInputs);
    void AddPlannedEntry(const OpenDemand& entry, bool updateStatus);
    /// The unplaced order will be built when the resources are there: its construction costs are planned
    bool IsPlanned(const Order& order) const;
    void UpdateWareStatus();
    void UpdateSoldierStatus();
    /// State and urgency from stock, balance, free and consumption
    void FinishStatus(WareStatus& status, double coveredMinutes) const;
    void UpdateWorkingBuildings();
    void UpdatePlacedOrders();

    // Creating orders
    void CreateOrders();
    /// The cause of the order still holds (it is removed otherwise)
    bool IsCauseValid(const Order& order);
    /// More of the ware: choose a producer and add as many orders as the need asks for (or work on why the existing
    /// producers stand still)
    void ResolveShortage(GoodType good, const Need& need);
    /// Use the surplus of a group: a needed producer if the product of its consumer is short, an expanding consumer
    /// if it is normal
    void ResolveSurplus(unsigned groupIdx);
    /// Soldiers are short: the wares the warehouses need to recruit them
    void ResolveSoldiers();
    /// Tools are ordered: a tool maker if there is none or all are busy, inputs for those standing still
    void ResolveTools();
    /// The group of tools needs a maker: asked for beyond the stock, or tools of its kind are ordered or missing
    bool IsToolMakerNeeded(unsigned groupIdx) const;
    /// Tools missing per kind: asked for and TOOLS_IN_STOCK more, minus the stock and the tools ordered
    void UpdateToolNeeds();
    void AddExpandingOrder(GoodType good);
    /// Harvesters of deposits: from the trees, stone piles and map objects in the territory, not from the stock
    void AddDepositOrders();
    /// Harvesters for good with few of their source in reach get a renewer
    void AddRenewalOrders();
    /// Warehouse for more helpers (the warehouses create them)
    void AddHelpersOrder(unsigned parent);
    /// Storage full: a needed warehouse (one at a time)
    void AddStorageOrder();
    /// Add the order. A needed order replaces the same expanding one (same building and ware, it would stand lower); an
    /// expanding order is not added if the same one exists
    Order* AddOrder(Order order);

    // Ways and buildings
    /// Producers of the ware usable as a way, the best first: not blocked, better tool, fewest new buildings, highest
    /// productivity of the existing ones, lowest construction costs. onlyExpanding: only buildings of ai.lua
    std::vector<WayCheck> RankProducers(GoodType good, bool onlyExpanding, unsigned depth);
    /// `given`: input that is there anyway (the surplus a consumer is ordered for)
    WayCheck CheckWay(unsigned recipeIdx, GoodType good, unsigned depth, GoodType given = GoodType::Nothing);
    /// The ware of the list which is best to make now (any of them would do). Nothing if none can be made
    GoodType ChooseWare(const std::vector<GoodType>& wares);
    /// True if the input suffices for one more consumer of the recipe: with a positive balance and free stock, or
    /// enough free stock for 10 minutes of its work time (SURPLUS_FACTOR). A tool maker: the stock covers the tools
    /// ordered and missing of its kinds
    bool InputSuffices(GoodType input, const Recipe& consumer) const;
    /// The inputs of the recipe suffice for one more building (all of them, or one of them for "any of")
    bool InputsSuffice(const Recipe& recipe, GoodType given = GoodType::Nothing) const;
    /// What the existing and coming producers of the ware add. Works on the reasons buildings stand still (orders for
    /// their inputs, warehouse for helpers)
    Capacity ProducerCapacity(GoodType good, bool solveReasons);
    Diagnosis Diagnose(const nobUsual& building, const Recipe& recipe, std::vector<GoodType>& missingInputs) const;
    /// Wares per minute one producer of the recipe makes of the ware: measured, or by the work time
    double OutputPerProducer(GoodType good, const Recipe& recipe) const;
    /// The building makes the ware (one of its products)
    bool MakesWare(BuildingType bld, GoodType good) const;
    /// A building of the type would work once finished: a worker for it can come and its tools are there
    bool CanWorkWhenBuilt(BuildingType bld) const;
    /// Made more than used of the ware per minute (positive balance), 0 if not
    double GetSparePerMinute(GoodType good) const;
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
    /// Average productivity (percent) of the buildings of the type which worked longer than the start-up time, and
    /// their number
    std::pair<unsigned, unsigned> GetProductivity(BuildingType bld) const;
    const Recipe* FindRecipe(BuildingType bld) const;
    /// Recipes making the source the renewer renews (harvesters)
    bool IsRenewable(const Source& source) const;
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
    /// Military buildings are placed by the old AI until the planner does it (step 3 of DESIGN.md): the material for
    /// its next one (the cheapest military building) is kept free before the expanding orders
    void KeepMaterialForMilitary();
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
    void OrderTools();

    // Rules
    /// The construction material for the building is there (after what the orders above in the list took)
    bool HasMaterial(BuildingType bld) const;
    /// Buildings making the ware which have a worker and finished their start-up time
    unsigned GetNumWorkingProducers(GoodType good) const;
    /// Helpers in the warehouses not yet promised to building sites or asked for as workers
    int GetFreeHelpers() const;
    /// Builders in the warehouses and helpers who can become builders now
    unsigned GetAvailableBuilders() const;
    /// A worker for the building can come: one is in a warehouse, or a helper (and the tool to recruit him)
    bool IsWorkerAvailable(BuildingType bld) const;
    /// Tools of the kind of the ware in the warehouses (all materials)
    unsigned GetToolStock(GoodType tool) const;
    /// Tool reform: the tool without which the building does not work at all is in a warehouse
    bool HasRequiredTools(BuildingType bld) const;
    /// The situation at the start of the step
    Situation GetSituation() const;
    Situation ComputeSituation() const;
    bool IsRetryMet(const RetryCondition& retry, BuildingType bld) const;

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
    /// Tools missing of each kind in this step (UpdateToolNeeds)
    helpers::EnumArray<unsigned, Tool> toolsMissing_{};
    /// Groups containing the ware
    helpers::EnumArray<std::vector<unsigned>, GoodType> groupsOf_;
    WareStatus soldiers_;
    /// The ware is used by anything (input of a building, construction, tool, soldier equipment, military, research)
    helpers::EnumArray<bool, GoodType> hasUse_{};
    /// Recipes taking the ware as input
    helpers::EnumArray<std::vector<unsigned>, GoodType> consumers_;
    /// The planner decides about these buildings. Military buildings, harbor, shipyard ... stay with the old AI
    helpers::EnumArray<bool, BuildingType> managed_{};
    /// First GF a building (by its position) was seen with a worker: start-up time and productivity
    std::map<MapPoint, unsigned, MapPointLess> workingSince_;
    /// Clearing woodcutters placed whose mark (game command, kept in the savegame) is not executed yet
    std::vector<MapPoint> clearingPending_;
    /// Orders whose construction costs are counted as planned at the start of the step
    std::vector<unsigned> plannedOrderIds_;
    /// Planned entries this step (costs, input spaces), any number of wares each
    std::vector<OpenDemand> planned_;
    /// GetChain per ware in this step
    std::map<GoodType, std::vector<BuildingType>> chainCache_;
    /// ProducerCapacity per ware in this step, and the wares whose reasons were worked on in this step
    std::map<GoodType, Capacity> capacityCache_;
    std::vector<GoodType> reasonsSolved_;
    /// Construction material in the warehouses not needed by building sites at the start of the step (for the reasons)
    AIJH::ConstructionMaterial stepFreeMaterial_{};
    /// Building sites cancelled for self-solving orders this step (one cancel round per step)
    bool cancelledThisStep_ = false;
    /// Military building whose material was kept free in this step (Headquarters: none)
    BuildingType militaryKept_ = BuildingType::Headquarters;
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

    double lastStepUs_ = 0, maxStepUs_ = 0;
    unsigned numSteps_ = 0;
};

} // namespace civai
