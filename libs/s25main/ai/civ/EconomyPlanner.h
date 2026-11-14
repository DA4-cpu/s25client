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
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace AIJH {
class AIPlayerJH;
}
class GamePlayer;

namespace civai {

class CivAI;
struct Recipe;

/// State of a ware, from the measured statistic (see DESIGN.md, "Neufassung der Planung")
enum class SupplyState : uint8_t
{
    /// Balance negative, or more wares are asked for than the warehouses have: more producers are needed
    Shortage,
    /// Balance not negative and stock at most the threshold: the normal state of a working economy
    Normal,
    /// Balance not negative and stock above the threshold: more consumers are needed to use it
    Surplus
};

/// What the AI knows about a ware in the current planning step
struct WareStatus
{
    /// In the warehouses
    unsigned stock = 0;
    /// Produced minus consumed in the last BALANCE_MINUTES minutes
    int balance = 0;
    /// Consumed per minute in the last BALANCE_MINUTES minutes
    double consumptionPerMinute = 0;
    /// Requested ("angefragt") in three parts:
    /// open: asked for by the game but not sent yet (building sites, inputs, tool slots, coins, research ...),
    unsigned openDemand = 0;
    /// in transit: on the way to the building that asked for it (left the stock already, only shown),
    unsigned inTransit = 0;
    /// planned: construction costs of the orders of the list and of the building sites just ordered, input spaces of
    /// building sites and of planned buildings (they will ask for these wares)
    unsigned planned = 0;
    /// Stock minus open minus planned: what is really free. Negative: more is requested than there is
    int free = 0;
    /// Start stock not used yet (only for the wares of rttr:SetAIStartStockNoSurplus, otherwise 0): no surplus
    unsigned startStockLeft = 0;
    SupplyState state = SupplyState::Normal;
    /// Shortage: minutes until the free stock is gone at the current balance (0 if more is requested than there is).
    /// Sorts the shortages: the most urgent first
    double minutesUntilEmpty = 0;
};

/// Needed: to remove a shortage or use a surplus. Expanding: to make the economy bigger (only buildings of ai.lua)
enum class OrderKind : uint8_t
{
    Needed,
    Expanding
};

enum class OrderState : uint8_t
{
    /// To be placed when its turn comes
    Open,
    /// Waits for the orders above it (e.g. the farm the brewery needs) or for its inputs
    Waiting,
    /// A rule is not met and could not be solved now. Tried again when its retry condition is met (needed orders) or
    /// when its turn comes again at the end of the list (expanding orders)
    Blocked,
    /// Building site placed. Stays in the list until the building worked one balance length (start-up time)
    Placed
};

/// When a blocked needed order is tried again (cheap to check every step, no timer). Also when the situation changed
/// (territory, research): a new way may be open then
struct RetryCondition
{
    enum class Kind : uint8_t
    {
        /// Immediately (not blocked)
        Now,
        /// Construction material for the building is there
        Material,
        /// A builder is free
        Builder,
        /// A helper is free in a warehouse
        Helpers,
        /// Fewer buildings of the type than the limit (land grew or one was demolished)
        Limit,
        /// The tool the building needs is in a warehouse
        Tool,
        /// The territory or the researched technologies changed (new place, new deposit, new building allowed)
        Situation
    };
    Kind kind = Kind::Now;
    /// Value of GetSituation() when the order was blocked
    unsigned situation = 0;
};

/// One entry of the priority list: one building to place
struct Order
{
    unsigned id = 0;
    BuildingType building = BuildingType::Headquarters;
    /// Ware the building is for. Nothing: for helpers (storehouse) or for trees (forester)
    GoodType forWare = GoodType::Nothing;
    OrderKind kind = OrderKind::Needed;
    /// Needed orders: true if the building should use a surplus of forWare, false if it should make forWare
    bool consumer = false;
    /// Order this one was created for (e.g. the brewery for the farm). 0: created from the statistic
    unsigned parent = 0;
    OrderState state = OrderState::Open;
    /// Why it waits or is blocked, for the log, e.g. "Bretter fehlen"
    std::string reason;
    RetryCondition retry;
    /// Place it near this point (forester near its woodcutter). Invalid: search near warehouses/military buildings
    MapPoint near = MapPoint::Invalid();
    /// Woodcutter: clears the land (expanding, no forester, demolished when no trees are left) instead of supplying
    /// wood for good
    bool clearing = false;
    /// The building is part of the chain that makes the construction material it lacks (e.g. a woodcutter lacking
    /// boards). It is not blocked by that: it gets the material before all other orders and buildings
    bool selfSolving = false;
    /// Inputs that are the reason it cannot be built (missing, no way). Orders below it in the list must not get a
    /// building that uses them (the wares are kept for this one)
    std::vector<GoodType> inputsMissing;
    /// Next warehouse/military building to search a place around and how many were searched in this round (the search
    /// is spread over several steps if the computing budget runs out)
    unsigned nextCenter = 0, centersTried = 0;
    /// Where it was placed and when the start-up time ends (0: building not working yet)
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
    /// Balance length: the statistic of the last this many minutes counts (also the start-up time of new buildings)
    static constexpr unsigned BALANCE_MINUTES = 5;
    /// The threshold of a surplus is this many times the consumption per minute ...
    static constexpr unsigned SURPLUS_FACTOR = 10;
    /// ... but at least this stock
    static constexpr unsigned MIN_STOCK = 8;
    /// High productivity (share of the time working). Producers below it do not get another one: first find why they
    /// do not work. A woodcutter for good below it gets a forester
    static constexpr double HIGH_PRODUCTIVITY = 0.8;
    /// Clearing woodcutters are placed where at least this many trees are in reach (independent of the wood stock)
    static constexpr unsigned CLEARING_MIN_TREES = 8;
    /// Quarries are placed where at least this many stone piles are in reach (independent of the stone stock)
    static constexpr unsigned QUARRY_MIN_STONE_PILES = 1;
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
    const WareStatus& GetWareStatus(GoodType good) const { return wares_[good]; }
    /// The woodcutter at the point clears land (no forester, demolished when no trees are left)
    bool IsClearingWoodcutter(MapPoint pt) const;
    /// The planner decides about this building type (the old AI does not build it)
    bool IsManaged(BuildingType bld) const { return managed_[bld]; }
    /// Most buildings of one type: base + per start area * start areas (ai.lua, rttr:SetAIBuildingLimit). A start area
    /// is the land of an HQ; the land is counted in whole start areas, at least 1
    unsigned GetBuildingLimit() const;
    /// Buildings, building sites and ordered sites of the type
    unsigned GetNumBuildings(BuildingType bld) const;
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
        /// New buildings the way needs right now (the producer and producers of its missing inputs). 0 if the
        /// existing producers lag behind (then the reason is worked on instead)
        unsigned numNew = 0;
        /// Average productivity of the existing producers (percent), 0 if there are none
        unsigned productivity = 0;
        unsigned cost = 0;
        unsigned recipe = 0;
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
        unsigned situation;
    };

    /// Planned wares (see WareStatus::planned) from the snapshot, the orders of the list and the sites just ordered
    void UpdatePlanned();
    /// Construction costs and/or input spaces of one more building of the type. updateStatus: also in the state of the
    /// wares (a building placed in this step)
    void AddPlanned(BuildingType bld, bool updateStatus, bool withCosts, bool withInputs);
    /// The unplaced order will be built when the resources are there (not blocked until the situation changes): its
    /// construction costs and inputs are planned
    bool IsPlanned(const Order& order) const;
    void UpdateWareStatus();
    void UpdateWorkingBuildings();
    void UpdatePlacedOrders();
    void CreateOrders();
    /// Orders whose building is part of the chain making the construction material they lack
    void MarkSelfSolving();
    /// Military buildings are placed by the old AI until the planner does it (step 3 of DESIGN.md): the material for
    /// its next one (the cheapest military building) is kept free before the expanding orders
    void KeepMaterialForMilitary();
    void ProcessList();
    void OrderTools();

    /// More of the ware: choose a producer and add the order (or work on why the existing ones lag)
    void ResolveShortage(GoodType good, unsigned parent);
    /// Use the surplus of the ware: choose a consumer and add the order
    void ResolveSurplus(GoodType good);
    void AddExpandingOrder(GoodType good);
    /// Clearing woodcutters and quarries: from the trees and stone piles in the territory, not from the stock
    void AddClearingOrders();
    /// Woodcutters for good with low productivity get a forester
    void AddForesterOrders();
    /// Storehouse for more helpers (the warehouses create them)
    void AddStorehouseOrder(unsigned parent);

    /// Producers of the ware usable as a way, the best first: not blocked, fewest new buildings, highest productivity
    /// of the existing ones, lowest construction costs. onlyExpanding: only buildings of ai.lua
    std::vector<WayCheck> RankProducers(GoodType good, bool onlyExpanding, unsigned depth);
    /// `given`: input that is there anyway (the surplus a consumer is ordered for)
    WayCheck CheckWay(unsigned recipeIdx, GoodType good, unsigned depth, GoodType given = GoodType::Nothing);
    /// The ware of the list which is easiest to make now (any of them would do). Nothing if none can be made
    GoodType ChooseWare(const std::vector<GoodType>& wares);
    /// True if the input suffices for one more consumer of the recipe
    bool InputSuffices(GoodType input, const Recipe& consumer) const;
    /// Buildings of the chain that makes the ware now: the best producer, the best producers of its inputs ... (and
    /// the forester if a woodcutter is part of it)
    std::vector<BuildingType> GetChain(GoodType good);
    /// Why the ware is missing, following its producers: "Holz: frei 0, Bilanz -3; Holzfäller: keiner arbeitet"
    std::string ExplainWare(GoodType good, unsigned depth);
    /// The construction material missing for the building in detail, e.g. "Bretter fehlen: 2 nötig, 0 frei (Lager 6,
    /// für Baustellen 6, für Aufträge darüber 0)". Empty if nothing is missing. `missing`: the wares that are missing
    std::string DescribeMissingMaterial(BuildingType bld, std::vector<GoodType>& missing) const;
    /// Self-solving order lacking material (ware and amount): cancel building sites outside the chain whose material is
    /// not built in yet until enough comes back. Returns what was cancelled, for the reason
    std::string CancelSitesFor(const std::vector<std::pair<GoodType, unsigned>>& lacking);
    /// Average productivity (percent) of the buildings of the type which worked longer than the start-up time, and
    /// their number
    std::pair<unsigned, unsigned> GetProductivity(BuildingType bld) const;
    /// A producer (or consumer) of the ware was placed and did not finish its start-up time yet
    bool IsStartingUp(GoodType good, bool consumer) const;
    /// The ware is one of several asked for ("any of", e.g. food of a mine) and there are not enough of all of them
    bool IsAnyOfDemand(GoodType good) const;
    const Recipe* FindRecipe(BuildingType bld) const;
    /// The order or one above it makes the ware or is of the building type (one more would be a loop). Headquarters as
    /// building: only the ware is checked
    bool IsInChain(unsigned orderId, GoodType good, BuildingType bld) const;

    Order* Find(unsigned id);
    const Order* Find(unsigned id) const;
    Order& Add(Order order);
    /// Removes the order and the not yet placed orders created for it
    void Remove(unsigned id);
    /// A needed order makes the ware (not placed yet, or placed and starting up)
    bool HasProducerOrder(GoodType good) const;
    bool HasChildFor(unsigned parent, GoodType good) const;
    /// Removes the not yet placed orders created for this one (it cannot be built, they would be of no use)
    void RemoveUnplacedChildren(unsigned id);
    /// The order or one above it is blocked until the situation changes (no way, no place, not researched)
    bool IsChainBlocked(unsigned id) const;
    /// Rank of the order in the basic economy for sorting (ai.lua; 1 = first level, 2 = second ..., then all others).
    /// An order created for another one moves up to its own level if that is better, otherwise it stays with the
    /// order it is for (it must come above it)
    unsigned GetEffectiveLevel(const Order& order) const;

    void ProcessOrder(unsigned id, unsigned& searchBudget);
    /// Checks the rules. Returns the reason of the first rule not met (empty: all met) and sets the retry condition
    std::string CheckRules(const Order& order, RetryCondition& retry) const;
    /// Needed order: work on the unmet rule (e.g. order the producer of the missing material)
    void SolveRule(unsigned id, const RetryCondition& retry);
    MapPoint SearchPlace(Order& order, unsigned& searchBudget, bool& searchedAll);
    /// Expanding order which cannot be placed now: to the end of the list. `retry`: what it waits for (material ...:
    /// its costs stay planned; situation: not)
    void MoveToEnd(Order& order, const std::string& reason, const RetryCondition& retry);

    /// The construction material for the building is there (after what the orders above in the list took)
    bool HasMaterial(BuildingType bld) const;
    /// Buildings making the ware which have a worker and finished their start-up time
    unsigned GetNumWorkingProducers(GoodType good) const;
    /// Helpers in the warehouses not yet promised to building sites or asked for as workers
    int GetFreeHelpers() const;
    /// A worker for the building can come: one is in a warehouse, or a helper (and the tool to recruit him)
    bool IsWorkerAvailable(BuildingType bld) const;
    /// Tools of the kind of the ware in the warehouses (all materials)
    unsigned GetToolStock(GoodType tool) const;
    /// Tool reform: the tool without which the building does not work at all is in a warehouse
    bool HasRequiredTools(BuildingType bld) const;
    /// Number that changes when the territory or the researched technologies change
    unsigned GetSituation() const;
    bool IsRetryMet(const RetryCondition& retry, BuildingType bld) const;

    AIJH::AIPlayerJH& aijh_;
    const GamePlayer& player_;
    const CivAI& civAI_;
    unsigned reactionGFs_;
    unsigned currentGF_ = 0;

    EconomySnapshot snapshot_;
    helpers::EnumArray<WareStatus, GoodType> wares_;
    /// The ware is used by anything (input of a building, construction, tool, soldier equipment, coins, research)
    helpers::EnumArray<bool, GoodType> hasUse_{};
    /// Recipes taking the ware as input
    helpers::EnumArray<std::vector<unsigned>, GoodType> consumers_;
    /// The planner decides about these buildings. Military buildings, harbor, shipyard ... stay with the old AI
    helpers::EnumArray<bool, BuildingType> managed_{};
    /// First GF a building (by its position) was seen with a worker: start-up time and productivity
    std::map<MapPoint, unsigned, MapPointLess> workingSince_;
    /// Clearing woodcutters placed whose mark (game command, kept in the savegame) is not executed yet
    std::vector<MapPoint> clearingPending_;
    /// Planned wares this step (see WareStatus::planned) and the planned entries "any of"
    helpers::EnumArray<unsigned, GoodType> planned_{};
    std::vector<OpenDemand> plannedAnyOf_;
    /// Orders whose construction costs are counted as planned at the start of the step
    std::vector<unsigned> plannedOrderIds_;
    /// Entries "any of" asked for (open) or planned: food of mines, construction costs with several materials
    std::vector<OpenDemand> anyOfDemand_;
    /// GetChain per ware in this step
    std::map<GoodType, std::vector<BuildingType>> chainCache_;
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

    double lastStepUs_ = 0, maxStepUs_ = 0;
    unsigned numSteps_ = 0;
};

} // namespace civai
