// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ai/civ/EconomyPlanner.h"
#include "EconomyStatistics.h"
#include "EventManager.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "addons/const_addons.h"
#include "ai/AIInterface.h"
#include "ai/aijh/AIConstruction.h"
#include "ai/aijh/AIPlayerJH.h"
#include "ai/civ/CivAI.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobMilitary.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/PlayerResearch.h"
#include "civ/TechTree.h"
#include "figures/nofFarmhand.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "world/GameWorld.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/GoodConsts.h"
#include "gameData/JobConsts.h"
#include "gameData/ToolConsts.h"
#include "s25util/Log.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <sstream>

namespace civai {

namespace {
    using Clock = std::chrono::steady_clock;

    /// What a warehouse needs to recruit a soldier (see nobBaseWarehouse::CanRecruitSoldiers)
    constexpr std::array<GoodType, 3> SOLDIER_EQUIPMENT = {GoodType::Sword, GoodType::ShieldRomans, GoodType::Beer};
    /// The report is written to the log every this many GFs (1 game minute)
    constexpr unsigned REPORT_GFS = 1200;

    std::string wareName(const GoodType good)
    {
        return good == GoodType::Nothing ? std::string("-") : WARE_NAMES[good];
    }

    std::string buildingName(const BuildingType bld)
    {
        if(isCustomBuilding(bld))
        {
            if(const civ::CustomBuilding* building = civ::getActiveContent().GetBuilding(bld))
                return building->name;
        }
        return BUILDING_NAMES[bld];
    }

    unsigned getTotalCost(const BuildingType bld)
    {
        unsigned result = 0;
        for(const CostEntry& entry : BUILDING_COSTS[bld])
            result += entry.amount;
        return result;
    }

    /// Job of the building's worker
    std::optional<Job> getJob(const BuildingType bld)
    {
        if(isCustomBuilding(bld))
            return getCustomJob(getCustomBuildingIdx(bld));
        if(const auto& job = BLD_WORK_DESC[bld].job)
            return *job;
        return std::nullopt;
    }

    /// Take the construction material of the building from `free` (any ware of an entry, in order). Returns false if
    /// there is not enough. `taken` gets what was really there
    bool takeMaterial(AIJH::ConstructionMaterial& free, const BuildingType bld, AIJH::ConstructionMaterial& taken)
    {
        bool enough = true;
        for(const CostEntry& entry : BUILDING_COSTS[bld])
        {
            int amount = entry.amount;
            for(const GoodType good : entry.wares)
            {
                const int part = std::min(std::max(free[good], 0), amount);
                free[good] -= part;
                taken[good] += part;
                amount -= part;
            }
            enough &= amount <= 0;
        }
        return enough;
    }

    /// Tool kind of the ware (also copper tools ...), nullopt if it is no tool
    std::optional<Tool> getToolOf(const GoodType good)
    {
        for(const auto tool : helpers::enumRange<Tool>())
        {
            if(TOOL_TO_GOOD[tool] == good)
                return tool;
        }
        return civ::getActiveContent().GetToolTypeOf(good);
    }
} // namespace

EconomyPlanner::EconomyPlanner(AIJH::AIPlayerJH& aijh, const CivAI& civAI)
    : aijh_(aijh), player_(aijh.player), civAI_(civAI)
{
    // Reaction time after the level of the AI (the only difference between the levels)
    switch(aijh.GetLevel())
    {
        case AI::Level::Easy: reactionGFs_ = 400; break;
        case AI::Level::Medium: reactionGFs_ = 100; break;
        case AI::Level::Hard: reactionGFs_ = 20; break;
    }
    const EconomyGraph& graph = civAI_.GetGraph();
    const std::vector<Recipe>& recipes = graph.GetRecipes();
    for(unsigned i = 0; i < recipes.size(); ++i)
    {
        const Recipe& recipe = recipes[i];
        // The shipyard (ships) stays with the old AI
        if(recipe.building == BuildingType::Shipyard)
            continue;
        managed_[recipe.building] = true;
        for(const GoodType input : recipe.inputs)
        {
            consumers_[input].push_back(i);
            hasUse_[input] = true;
        }
    }
    for(const Renewer& renewer : graph.GetRenewers())
        managed_[renewer.building] = true;

    // Uses besides the input of a building: construction, tools, soldiers, military, research, knowledge media
    const civ::TechTree& techTree = player_.GetGameWorld().GetTechTree();
    const civ::Content& content = techTree.GetContent();
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        for(const CostEntry& entry : BUILDING_COSTS[bld])
        {
            for(const GoodType good : entry.wares)
                hasUse_[good] = true;
        }
    }
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(getToolOf(good))
            hasUse_[good] = true;
    }
    for(const GoodType good : SOLDIER_EQUIPMENT)
        hasUse_[good] = true;
    hasUse_[GoodType::Coins] = true;
    hasUse_[GoodType::Armor] = true;
    for(unsigned i = 0; i < techTree.size(); ++i)
    {
        for(const civ::TechCost& cost : techTree.get(i).cost)
            hasUse_[cost.good] = true;
    }
    for(const GoodType medium : content.GetKnowledgeMedia())
        hasUse_[medium] = true;
}

void EconomyPlanner::RunGF(const unsigned gf)
{
    if((gf + player_.GetPlayerId() * 7) % reactionGFs_ != 0)
        return;
    Step();
    if(gf / REPORT_GFS != (gf - reactionGFs_) / REPORT_GFS || gf < reactionGFs_)
        LOG.write("ECONOMY player %u, GF %u:\n%s") % player_.GetPlayerId() % gf % GetReport();
}

void EconomyPlanner::Step()
{
    const auto start = Clock::now();
    currentGF_ = player_.GetGameWorld().GetEvMgr().GetCurrentGF();
    snapshot_ = takeEconomySnapshot(player_);
    wayCache_.clear();
    chainCache_.clear();
    cancelledThisStep_ = false;
    helpers::erase_if(failedWays_, [this](const FailedWay& way) { return way.situation != GetSituation(); });
    UpdateWorkingBuildings();
    UpdatePlacedOrders();
    UpdatePlanned();
    UpdateWareStatus();
    CreateOrders();
    freeMaterial_ = aijh_.GetConstruction().GetFreeConstructionMaterial();
    stepFreeMaterial_ = freeMaterial_;
    reservedMaterial_ = {};
    MarkSelfSolving();
    ProcessList();
    // Material needed orders wait for: no other building takes it (also not the military buildings of the old AI)
    aijh_.GetConstruction().SetPlannerReservation(reservedMaterial_);
    OrderTools();
    ++numSteps_;
    lastStepUs_ = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
    maxStepUs_ = std::max(maxStepUs_, lastStepUs_);
}

// ---------------------------------------------------------------------------------------------------------------------
// What the AI knows: state of the wares and of the buildings

bool EconomyPlanner::IsPlanned(const Order& order) const
{
    // Expanding orders are wishes: planned only once they are placed (building site)
    if(order.state == OrderState::Placed || order.kind != OrderKind::Needed)
        return false;
    return !(order.state == OrderState::Blocked && order.retry.kind == RetryCondition::Kind::Situation);
}

void EconomyPlanner::AddPlanned(const BuildingType bld, const bool updateStatus, const bool withCosts,
                                const bool withInputs)
{
    std::vector<OpenDemand> demand;
    if(withCosts)
    {
        for(const CostEntry& entry : BUILDING_COSTS[bld])
            demand.push_back({entry.wares, entry.amount, WareUse::Construction});
    }
    if(withInputs && BuildingProperties::IsUsual(bld))
        addInputSpaces(demand, bld);
    for(OpenDemand& entry : demand)
    {
        if(entry.wares.size() != 1u)
        {
            plannedAnyOf_.push_back(entry);
            anyOfDemand_.push_back(std::move(entry));
            continue;
        }
        const GoodType good = entry.wares.front();
        planned_[good] += entry.amount;
        if(updateStatus)
        {
            wares_[good].planned += entry.amount;
            wares_[good].free -= static_cast<int>(entry.amount);
        }
    }
}

void EconomyPlanner::UpdatePlanned()
{
    std::fill(planned_.begin(), planned_.end(), 0u);
    plannedAnyOf_.clear();
    plannedOrderIds_.clear();
    anyOfDemand_.clear();
    for(const OpenDemand& demand : snapshot_.openDemand)
    {
        if(demand.wares.size() > 1u)
            anyOfDemand_.push_back(demand);
    }
    // Input spaces of the building sites: the buildings will ask for them
    for(const OpenDemand& demand : snapshot_.siteInputs)
    {
        if(demand.wares.size() == 1u)
            planned_[demand.wares.front()] += demand.amount;
        else
        {
            plannedAnyOf_.push_back(demand);
            anyOfDemand_.push_back(demand);
        }
    }
    // Building sites ordered whose commands are on their way (also those of the old AI)
    const AIJH::AIConstruction& construction = aijh_.GetConstruction();
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        for(unsigned i = construction.GetNumPendingSites(bld); i > 0; --i)
            AddPlanned(bld, false, true, true);
    }
    // Needed orders of the list that will be built when the resources are there: their construction material (their
    // inputs count once they are placed, otherwise several orders waiting for the same input block each other)
    for(const Order& order : orders_)
    {
        if(!IsPlanned(order))
            continue;
        AddPlanned(order.building, false, true, false);
        plannedOrderIds_.push_back(order.id);
    }
}

void EconomyPlanner::UpdateWareStatus()
{
    const EconomyStatistics& stats = player_.GetEconomyStatistics();
    const civ::TechTree& techTree = player_.GetGameWorld().GetTechTree();
    const double coveredMinutes = stats.GetCoveredMinutes(BALANCE_MINUTES, currentGF_);
    for(const auto good : helpers::enumRange<GoodType>())
    {
        WareStatus& status = wares_[good];
        status.stock = snapshot_.inWarehouses[good];
        status.balance = stats.GetBalance(good, BALANCE_MINUTES, currentGF_);
        status.consumptionPerMinute = stats.GetConsumptionPerMinute(good, BALANCE_MINUTES, currentGF_);
        // Only what this ware alone can serve (entries "any of" are handled in CreateOrders)
        status.openDemand = 0;
        for(const OpenDemand& demand : snapshot_.openDemand)
        {
            if(demand.wares.size() == 1u && demand.wares.front() == good)
                status.openDemand += demand.amount;
        }
        status.inTransit = snapshot_.inTransit[good];
        status.planned = planned_[good];
        status.free = static_cast<int>(status.stock) - static_cast<int>(status.openDemand + status.planned);
        // Start stock (wares added, not produced) not used up yet
        status.startStockLeft = 0;
        if(techTree.IsAIStartStockNoSurplus(good))
        {
            uint32_t used = 0;
            for(const auto use : helpers::enumRange<WareUse>())
                used += stats.GetTotalUsed(good, use);
            const uint32_t added = stats.GetTotalGained(good, WareSource::Added);
            status.startStockLeft = added > used ? added - used : 0u;
        }
        const double threshold = std::max(static_cast<double>(MIN_STOCK), SURPLUS_FACTOR * status.consumptionPerMinute);
        if(status.balance < 0 || status.free < 0)
            status.state = SupplyState::Shortage;
        else if(status.free - static_cast<int>(status.startStockLeft) > threshold)
            status.state = SupplyState::Surplus;
        else
            status.state = SupplyState::Normal;
        if(status.free < 0)
            status.minutesUntilEmpty = 0;
        else if(status.balance < 0)
            status.minutesUntilEmpty = status.free / (-status.balance / coveredMinutes);
        else
            status.minutesUntilEmpty = std::numeric_limits<double>::infinity();
    }
}

void EconomyPlanner::UpdateWorkingBuildings()
{
    std::map<MapPoint, unsigned, MapPointLess> working;
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!BuildingProperties::IsUsual(bld))
            continue;
        for(const nobUsual* building : player_.GetBuildingRegister().GetBuildings(bld))
        {
            if(!building->HasWorker())
                continue;
            const auto it = workingSince_.find(building->GetPos());
            working[building->GetPos()] = it != workingSince_.end() ? it->second : currentGF_;
        }
    }
    workingSince_ = std::move(working);

    // Clearing woodcutters: marked in the game (savegame). Marks of woodcutters that are gone are removed
    const GameWorldBase& world = player_.GetGameWorld();
    const AIJH::AIConstruction& construction = aijh_.GetConstruction();
    const auto isWoodcutter = [&](const MapPoint pt) {
        const auto* building = world.GetSpecObj<noBaseBuilding>(pt);
        return building && building->GetPlayer() == player_.GetPlayerId()
               && building->GetBuildingType() == BuildingType::Woodcutter;
    };
    for(const MapPoint pt : player_.GetClearingWoodcutters())
    {
        if(!isWoodcutter(pt) && !construction.IsSitePending(pt))
            aijh_.GetInterface().SetClearingWoodcutter(pt, false);
    }
    // Placed by this planner, mark not executed yet: until it is in the game or the site is gone
    helpers::erase_if(clearingPending_, [&](const MapPoint pt) {
        return player_.IsClearingWoodcutter(pt) || (!isWoodcutter(pt) && !construction.IsSitePending(pt));
    });
}

std::pair<unsigned, unsigned> EconomyPlanner::GetProductivity(const BuildingType bld) const
{
    const unsigned startupGFs = BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    unsigned sum = 0, num = 0;
    for(const nobUsual* building : player_.GetBuildingRegister().GetBuildings(bld))
    {
        const auto it = workingSince_.find(building->GetPos());
        if(it == workingSince_.end() || it->second + startupGFs > currentGF_)
            continue; // No worker or still starting
        sum += building->GetProductivity();
        ++num;
    }
    return {num ? sum / num : 0u, num};
}

bool EconomyPlanner::InputSuffices(const GoodType input, const Recipe& consumer) const
{
    const WareStatus& status = wares_[input];
    if(status.balance < 0)
        return false;
    // Free: stock minus what is asked for and planned
    const int free = status.free;
    if(status.balance > 0 && free >= 0)
        return true;
    // Nothing more produced than used: enough free stock for what is used now and what the new building would use by
    // its work time
    const double needed = std::max(static_cast<double>(MIN_STOCK),
                                   SURPLUS_FACTOR * (status.consumptionPerMinute + consumer.nominalStepsPerMinute));
    return free >= needed;
}

unsigned EconomyPlanner::GetSituation() const
{
    // Changes when the territory grows or shrinks or a technology is researched
    const BuildingRegister& buildings = player_.GetBuildingRegister();
    return static_cast<unsigned>(buildings.GetMilitaryBuildings().size() + buildings.GetStorehouses().size())
           + 1000u * player_.GetResearch().GetNumResearched();
}

// ---------------------------------------------------------------------------------------------------------------------
// Ways to a ware and the choice between them

EconomyPlanner::WayCheck EconomyPlanner::CheckWay(const unsigned recipeIdx, const GoodType good, const unsigned depth,
                                                  const GoodType given)
{
    const auto key = std::make_tuple(recipeIdx, good, given);
    const auto cached = wayCache_.find(key);
    if(cached != wayCache_.end())
        return cached->second;

    const Recipe& recipe = civAI_.GetGraph().GetRecipes()[recipeIdx];
    WayCheck result;
    result.recipe = recipeIdx;
    result.cost = getTotalCost(recipe.building);
    const auto [productivity, numWorking] = GetProductivity(recipe.building);
    result.productivity = productivity;

    const bool failed = helpers::contains_if(
      failedWays_, [&](const FailedWay& way) { return way.ware == good && way.building == recipe.building; });
    const bool sourceThere =
      recipe.source.kind == SourceKind::None || civAI_.GetMap().GetTotalOwned().GetAmount(recipe.source) > 0;
    // A loop (the ware is needed to make one of its own inputs) is no way
    const bool loop = helpers::contains(checking_, good);
    if(failed || loop || !sourceThere || !player_.IsBuildingEnabled(recipe.building))
    {
        result.reason = failed ?
                          buildingName(recipe.building) + ": kein Bauplatz" :
                          loop ? wareName(good) + " im Kreis" :
                                 !sourceThere ? "nicht im Gebiet: " + civAI_.GetGraph().GetSourceName(recipe.source) :
                                                buildingName(recipe.building) + " nicht erforscht";
        wayCache_[key] = result;
        return result;
    }
    result.blocked = false;
    if(numWorking > 0 && productivity < HIGH_PRODUCTIVITY * 100)
    {
        // The existing ones lag behind: no new building, find out why
        result.numNew = 0;
        wayCache_[key] = result;
        return result;
    }
    result.numNew = 1;
    checking_.push_back(good);
    std::string inputReason;
    const auto inputCost = [&](const GoodType input) -> std::pair<bool, unsigned> {
        if(input == given || InputSuffices(input, recipe))
            return {true, 0u};
        const std::vector<WayCheck> ways = RankProducers(input, false, depth + 1);
        if(ways.empty() || ways.front().blocked)
        {
            if(inputReason.empty())
            {
                inputReason = "kein Weg zu " + wareName(input);
                if(!ways.empty())
                    inputReason += " (" + ways.front().reason + ")";
            }
            result.missingInputs.push_back(input);
            return {false, 0u};
        }
        return {true, ways.front().numNew};
    };
    if(recipe.consumeEach)
    {
        for(const GoodType input : recipe.inputs)
        {
            const auto [possible, numNew] = inputCost(input);
            if(!possible)
                result.blocked = true;
            result.numNew += numNew;
        }
    } else if(!recipe.inputs.empty())
    {
        // Any of the inputs: the cheapest
        unsigned best = std::numeric_limits<unsigned>::max();
        for(const GoodType input : recipe.inputs)
        {
            const auto [possible, numNew] = inputCost(input);
            if(possible)
                best = std::min(best, numNew);
        }
        if(best == std::numeric_limits<unsigned>::max())
            result.blocked = true;
        else
            result.numNew += best;
    }
    if(result.blocked)
        result.reason = inputReason;
    checking_.pop_back();
    wayCache_[key] = result;
    return result;
}

std::vector<EconomyPlanner::WayCheck> EconomyPlanner::RankProducers(const GoodType good, const bool onlyExpanding,
                                                                    const unsigned depth)
{
    const EconomyGraph& graph = civAI_.GetGraph();
    const civ::TechTree& techTree = player_.GetGameWorld().GetTechTree();
    std::vector<WayCheck> result;
    for(const unsigned idx : graph.GetProducers(good))
    {
        const Recipe& recipe = graph.GetRecipes()[idx];
        if(!managed_[recipe.building] || (onlyExpanding && !techTree.IsAIExpanding(recipe.building)))
            continue;
        // A byproduct is only a way once its technology is researched
        if(!helpers::contains(recipe.outputs, good))
        {
            bool researched = false;
            for(unsigned i = 0; i < recipe.byproducts.size(); ++i)
            {
                const int tech = recipe.byproductTechs[i];
                if(recipe.byproducts[i] == good
                   && (tech < 0 || player_.GetResearch().IsResearched(static_cast<civ::TechIdx>(tech))))
                    researched = true;
            }
            if(!researched)
                continue;
        }
        result.push_back(CheckWay(idx, good, depth));
    }
    // Not blocked, fewest new buildings, highest productivity of the existing ones, lowest construction costs
    std::stable_sort(result.begin(), result.end(), [](const WayCheck& lhs, const WayCheck& rhs) {
        if(lhs.blocked != rhs.blocked)
            return !lhs.blocked;
        if(lhs.numNew != rhs.numNew)
            return lhs.numNew < rhs.numNew;
        if(lhs.productivity != rhs.productivity)
            return lhs.productivity > rhs.productivity;
        return lhs.cost < rhs.cost;
    });
    return result;
}

GoodType EconomyPlanner::ChooseWare(const std::vector<GoodType>& wares)
{
    // The ware of the list that is easiest to make now
    GoodType best = GoodType::Nothing;
    WayCheck bestWay;
    for(const GoodType good : wares)
    {
        const std::vector<WayCheck> ways = RankProducers(good, false, 0);
        if(ways.empty() || ways.front().blocked)
            continue;
        if(best == GoodType::Nothing || ways.front().numNew < bestWay.numNew)
        {
            best = good;
            bestWay = ways.front();
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// The list

Order* EconomyPlanner::Find(const unsigned id)
{
    for(Order& order : orders_)
    {
        if(order.id == id)
            return &order;
    }
    return nullptr;
}

const Order* EconomyPlanner::Find(const unsigned id) const
{
    for(const Order& order : orders_)
    {
        if(order.id == id)
            return &order;
    }
    return nullptr;
}

Order& EconomyPlanner::Add(Order order)
{
    order.id = nextId_++;
    order.sequence = nextSequence_++;
    orders_.push_back(std::move(order));
    return orders_.back();
}

void EconomyPlanner::Remove(const unsigned id)
{
    // With the orders created for it (unless they are placed already: their start-up time is still watched)
    std::vector<unsigned> toRemove{id};
    for(unsigned i = 0; i < toRemove.size(); ++i)
    {
        for(const Order& order : orders_)
        {
            if(order.parent == toRemove[i] && order.state != OrderState::Placed)
                toRemove.push_back(order.id);
        }
    }
    helpers::erase_if(orders_, [&](const Order& order) { return helpers::contains(toRemove, order.id); });
    for(Order& order : orders_)
    {
        if(helpers::contains(toRemove, order.parent))
            order.parent = 0;
    }
}

bool EconomyPlanner::HasProducerOrder(const GoodType good) const
{
    return helpers::contains_if(orders_, [good](const Order& order) {
        return order.forWare == good && !order.consumer && order.kind == OrderKind::Needed;
    });
}

bool EconomyPlanner::HasChildFor(const unsigned parent, const GoodType good) const
{
    return helpers::contains_if(orders_,
                                [&](const Order& order) { return order.parent == parent && order.forWare == good; });
}

bool EconomyPlanner::IsInChain(unsigned orderId, const GoodType good, const BuildingType bld) const
{
    while(const Order* order = Find(orderId))
    {
        if((good != GoodType::Nothing && order->forWare == good && !order->consumer) || order->building == bld)
            return true;
        orderId = order->parent;
    }
    return false;
}

bool EconomyPlanner::IsClearingWoodcutter(const MapPoint pt) const
{
    return player_.IsClearingWoodcutter(pt) || helpers::contains(clearingPending_, pt);
}

std::vector<const Order*> EconomyPlanner::GetList() const
{
    // 1. Self-solving orders (they get the material they lack before everybody else), then needed, then expanding.
    // 2. Within these the levels of the basic economy (ai.lua), then all other buildings.
    // 3. Needed: shortages by urgency (minutes until empty), then the rest (surpluses, helpers, trees) in order of
    //    creation. Expanding: in order of their sequence (blocked ones moved to the end).
    // The orders created for an order come above it (an order created for another one is needed and moves up to its
    // own level if that is better)
    const auto urgency = [this](const Order& order) {
        if(order.kind == OrderKind::Expanding)
            return 0.;
        if(order.building == BuildingType::Storehouse)
            return 0.;
        const GoodType good = order.building == BuildingType::Forester ? GoodType::Wood : order.forWare;
        if(order.consumer || good == GoodType::Nothing)
            return std::numeric_limits<double>::max() / 2;
        return wares_[good].minutesUntilEmpty;
    };
    struct Entry
    {
        const Order* order;
        unsigned group, level;
        double urgency;
        unsigned rootSequence, depth;
    };
    std::vector<Entry> entries;
    entries.reserve(orders_.size());
    for(const Order& order : orders_)
    {
        const Order* root = &order;
        unsigned depth = 0;
        while(root->parent != 0)
        {
            const Order* parent = Find(root->parent);
            if(!parent)
                break;
            root = parent;
            ++depth;
        }
        const unsigned group = order.selfSolving ? 0u : root->kind == OrderKind::Needed ? 1u : 2u;
        entries.push_back({&order, group, GetEffectiveLevel(order), urgency(*root), root->sequence, depth});
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& lhs, const Entry& rhs) {
        if(lhs.group != rhs.group)
            return lhs.group < rhs.group;
        if(lhs.level != rhs.level)
            return lhs.level < rhs.level;
        if(lhs.urgency != rhs.urgency)
            return lhs.urgency < rhs.urgency;
        if(lhs.rootSequence != rhs.rootSequence)
            return lhs.rootSequence < rhs.rootSequence;
        if(lhs.depth != rhs.depth)
            return lhs.depth > rhs.depth;
        return lhs.order->sequence < rhs.order->sequence;
    });
    std::vector<const Order*> result;
    result.reserve(entries.size());
    for(const Entry& entry : entries)
        result.push_back(entry.order);
    return result;
}

unsigned EconomyPlanner::GetEffectiveLevel(const Order& order) const
{
    // Level 0 in the game data means "not part of the basic economy": after all levels
    constexpr unsigned notBasic = std::numeric_limits<unsigned>::max();
    const civ::TechTree& techTree = player_.GetGameWorld().GetTechTree();
    unsigned result = notBasic;
    for(const Order* current = &order; current; current = current->parent ? Find(current->parent) : nullptr)
    {
        const unsigned level = techTree.GetAIBasicEconomyLevel(current->building);
        if(level > 0)
            result = std::min(result, level);
    }
    return result;
}

bool EconomyPlanner::IsChainBlocked(unsigned id) const
{
    for(const Order* order = Find(id); order; order = order->parent ? Find(order->parent) : nullptr)
    {
        if(order->state == OrderState::Blocked && order->retry.kind == RetryCondition::Kind::Situation)
            return true;
    }
    return false;
}

void EconomyPlanner::RemoveUnplacedChildren(const unsigned id)
{
    std::vector<unsigned> children;
    for(const Order& order : orders_)
    {
        if(order.parent == id && order.state != OrderState::Placed)
            children.push_back(order.id);
    }
    for(const unsigned child : children)
        Remove(child);
}

// ---------------------------------------------------------------------------------------------------------------------
// Creating orders from the statistic

void EconomyPlanner::UpdatePlacedOrders()
{
    const unsigned startupGFs = BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    const GameWorldBase& world = player_.GetGameWorld();
    std::vector<unsigned> done;
    for(Order& order : orders_)
    {
        if(order.state != OrderState::Placed)
            continue;
        const auto* building = world.GetSpecObj<noBaseBuilding>(order.placedAt);
        if(!building || building->GetPlayer() != player_.GetPlayerId() || building->GetBuildingType() != order.building)
        {
            // Not there: the command is still on its way, or it failed (place taken meanwhile) or the site was
            // destroyed. Then the order is done and a new one is made if the ware still needs it
            if(!aijh_.GetConstruction().IsSitePending(order.placedAt))
                done.push_back(order.id);
            continue;
        }
        if(building->GetGOT() == GO_Type::Buildingsite)
            continue;
        const auto it = workingSince_.find(order.placedAt);
        if(it == workingSince_.end())
        {
            // Finished without worker: the start-up time begins with the worker (storehouse: no worker)
            if(!BuildingProperties::IsUsual(order.building))
                done.push_back(order.id);
            continue;
        }
        order.startupEndGF = it->second + startupGFs;
        if(currentGF_ >= order.startupEndGF)
            done.push_back(order.id);
    }
    for(const unsigned id : done)
    {
        if(Order* order = Find(id))
        {
            // Placed orders have no unplaced children (they were done before), remove only this one
            const unsigned parent = order->parent;
            helpers::erase_if(orders_, [id](const Order& other) { return other.id == id; });
            for(Order& other : orders_)
            {
                if(other.parent == id)
                    other.parent = parent;
            }
        }
    }
}

bool EconomyPlanner::IsStartingUp(const GoodType good, const bool consumer) const
{
    return helpers::contains_if(orders_, [&](const Order& order) {
        return order.state == OrderState::Placed && order.kind == OrderKind::Needed && order.forWare == good
               && order.consumer == consumer;
    });
}

void EconomyPlanner::CreateOrders()
{
    // Orders whose reason is gone (not placed yet)
    std::vector<unsigned> obsolete;
    for(const Order& order : orders_)
    {
        if(order.state == OrderState::Placed)
            continue;
        if(order.parent == 0)
        {
            const SupplyState state =
              order.forWare == GoodType::Nothing ? SupplyState::Shortage : wares_[order.forWare].state;
            // Clearing woodcutters and quarries come from the trees and stones on the map, not from the stock
            if(order.kind == OrderKind::Expanding && state != SupplyState::Normal && !order.clearing)
                obsolete.push_back(order.id);
            else if(order.kind == OrderKind::Needed && order.consumer && state != SupplyState::Surplus)
                obsolete.push_back(order.id);
            else if(order.kind == OrderKind::Needed && !order.consumer && order.forWare != GoodType::Nothing
                    && state != SupplyState::Shortage && !IsAnyOfDemand(order.forWare))
                obsolete.push_back(order.id);
        } else if(order.forWare != GoodType::Nothing && !order.consumer)
        {
            // Input or construction material of the order above: not needed anymore once it suffices
            const Order* parent = Find(order.parent);
            if(!parent)
                obsolete.push_back(order.id);
            else if(const Recipe* recipe = FindRecipe(parent->building))
            {
                if(helpers::contains(recipe->inputs, order.forWare) && InputSuffices(order.forWare, *recipe))
                    obsolete.push_back(order.id);
            }
        }
    }
    for(const unsigned id : obsolete)
        Remove(id);

    // Shortages: more producers (as many as the deficit needs, see ResolveShortage)
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(wares_[good].state == SupplyState::Shortage)
            ResolveShortage(good, 0);
    }
    // Asked for or planned "any of" (e.g. food of a mine) and not enough of all of them in stock: the easiest one
    for(const OpenDemand& demand : anyOfDemand_)
    {
        if(demand.wares.size() < 2u)
            continue;
        unsigned stock = 0;
        for(const GoodType good : demand.wares)
            stock += wares_[good].stock;
        if(stock >= demand.amount)
            continue;
        if(helpers::contains_if(demand.wares, [this](const GoodType good) {
               return IsStartingUp(good, false) || helpers::contains_if(orders_, [good](const Order& order) {
                          return order.forWare == good && !order.consumer;
                      });
           }))
            continue;
        const GoodType best = ChooseWare(demand.wares);
        if(best != GoodType::Nothing)
            ResolveShortage(best, 0);
    }
    // Surpluses: more consumers
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(wares_[good].state == SupplyState::Surplus && !IsStartingUp(good, true)
           && !helpers::contains_if(orders_,
                                    [good](const Order& order) { return order.forWare == good && order.consumer; }))
            ResolveSurplus(good);
    }
    // Normal state: expand with the buildings of ai.lua
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(wares_[good].state == SupplyState::Normal && hasUse_[good]
           && !helpers::contains_if(orders_, [good](const Order& order) {
                  return order.forWare == good && order.kind == OrderKind::Expanding;
              }))
            AddExpandingOrder(good);
    }
    AddClearingOrders();
    AddForesterOrders();
}

bool EconomyPlanner::IsAnyOfDemand(const GoodType good) const
{
    for(const OpenDemand& demand : anyOfDemand_)
    {
        if(demand.wares.size() < 2u || !helpers::contains(demand.wares, good))
            continue;
        unsigned stock = 0;
        for(const GoodType ware : demand.wares)
            stock += wares_[ware].stock;
        if(stock < demand.amount)
            return true;
    }
    return false;
}

const Recipe* EconomyPlanner::FindRecipe(const BuildingType bld) const
{
    for(const Recipe& recipe : civAI_.GetGraph().GetRecipes())
    {
        if(recipe.building == bld)
            return &recipe;
    }
    return nullptr;
}

void EconomyPlanner::ResolveShortage(const GoodType good, const unsigned parent)
{
    if(helpers::contains(resolving_, good))
        return; // Already working on it further up (loop)
    const std::vector<WayCheck> ways = RankProducers(good, false, 0);
    if(ways.empty() || ways.front().blocked)
    {
        // No way now: the order above waits for a new situation (territory, research). A shortage of the statistic
        // gets the order for its best producer, blocked, so the list shows why the ware is missing
        std::string reason = "kein Weg zu " + wareName(good);
        if(!ways.empty())
            reason += " (" + ways.front().reason + ")";
        Order* blocked = Find(parent);
        if(!blocked && !ways.empty() && parent == 0 && !HasProducerOrder(good))
        {
            Order order;
            order.building = civAI_.GetGraph().GetRecipes()[ways.front().recipe].building;
            order.forWare = good;
            // The inputs it cannot get are kept for it (orders below must not use them)
            order.inputsMissing = ways.front().missingInputs;
            blocked = &Add(std::move(order));
        } else if(blocked && !helpers::contains(blocked->inputsMissing, good))
        {
            const Recipe* recipe = FindRecipe(blocked->building);
            if(recipe && helpers::contains(recipe->inputs, good))
                blocked->inputsMissing.push_back(good);
        }
        if(blocked)
        {
            if(blocked->state == OrderState::Blocked && blocked->retry.kind != RetryCondition::Kind::Situation)
            {
                // Blocked by a rule (e.g. construction material) whose solution has no way now: both reasons, tried
                // again when the rule is met or the situation changed
                blocked->reason += ", " + reason;
            } else
            {
                blocked->state = OrderState::Blocked;
                blocked->reason = std::move(reason);
                blocked->retry = {RetryCondition::Kind::Situation, GetSituation()};
            }
        }
        return;
    }
    const Recipe& recipe = civAI_.GetGraph().GetRecipes()[ways.front().recipe];
    const auto [productivity, numWorking] = GetProductivity(recipe.building);
    if(numWorking > 0 && productivity < HIGH_PRODUCTIVITY * 100)
    {
        // The existing producers lag behind: work on why instead of building more
        resolving_.push_back(good);
        const BuildingTypeState& state = snapshot_.buildings[recipe.building];
        for(const WaitingForInput& waiting : state.waitingForInput)
        {
            if(helpers::contains_if(waiting.wares, [this](const GoodType input) {
                   return IsStartingUp(input, false) || helpers::contains_if(orders_, [input](const Order& order) {
                              return order.forWare == input && !order.consumer;
                          });
               }))
                continue;
            const GoodType input = waiting.wares.size() == 1u ? waiting.wares.front() : ChooseWare(waiting.wares);
            if(input != GoodType::Nothing)
                ResolveShortage(input, parent);
        }
        if(state.withoutWorker > 0)
            AddStorehouseOrder(parent);
        // Missing tools are ordered in OrderTools. Otherwise the buildings stand badly (placement, growth): reported
        // in the log
        resolving_.pop_back();
        return;
    }
    // As many producers as the measured deficit needs: deficit per minute / output per minute of one producer, at
    // least one. The output of one producer is measured (producers working now) or, without them, taken from the work
    // time of the recipe. Producers ordered already or starting up count (their output is not measured yet)
    unsigned count = 1;
    const WareStatus& status = wares_[good];
    if(status.balance < 0)
    {
        const EconomyStatistics& stats = player_.GetEconomyStatistics();
        const double minutes = stats.GetCoveredMinutes(BALANCE_MINUTES, currentGF_);
        const unsigned numProducers = GetNumWorkingProducers(good);
        const unsigned produced = stats.GetProduced(good, BALANCE_MINUTES, currentGF_);
        const double outputPerMinute =
          (numProducers > 0 && produced > 0) ? produced / minutes / numProducers : recipe.nominalStepsPerMinute;
        if(outputPerMinute > 0)
            count = std::max(1u, static_cast<unsigned>(std::ceil(-status.balance / minutes / outputPerMinute)));
    }
    const unsigned existing =
      static_cast<unsigned>(std::count_if(orders_.begin(), orders_.end(), [&](const Order& order) {
          return order.forWare == good && !order.consumer && order.kind == OrderKind::Needed;
      }));
    for(unsigned i = existing; i < count; ++i)
    {
        Order order;
        order.building = recipe.building;
        order.forWare = good;
        order.parent = parent;
        Add(std::move(order));
    }
}

unsigned EconomyPlanner::GetNumWorkingProducers(const GoodType good) const
{
    unsigned result = 0;
    std::vector<BuildingType> counted;
    for(const unsigned idx : civAI_.GetGraph().GetProducers(good))
    {
        const BuildingType bld = civAI_.GetGraph().GetRecipes()[idx].building;
        if(helpers::contains(counted, bld))
            continue;
        counted.push_back(bld);
        result += GetProductivity(bld).second;
    }
    return result;
}

void EconomyPlanner::ResolveSurplus(const GoodType good)
{
    // Soldier equipment is used by the warehouses (recruiting): the other pieces are missing
    if(helpers::contains(SOLDIER_EQUIPMENT, good))
    {
        for(const GoodType other : SOLDIER_EQUIPMENT)
        {
            if(other != good && wares_[other].stock == 0 && !IsStartingUp(other, false)
               && !helpers::contains_if(
                 orders_, [other](const Order& order) { return order.forWare == other && !order.consumer; }))
                ResolveShortage(other, 0);
        }
        return;
    }
    // The consumer whose product is needed most: shortage before normal, never one whose product is in surplus.
    // Then like the producers: not blocked, fewest new buildings, productivity, costs
    const std::vector<Recipe>& recipes = civAI_.GetGraph().GetRecipes();
    struct Candidate
    {
        unsigned recipe;
        unsigned stateRank;
        WayCheck way;
    };
    std::vector<Candidate> candidates;
    for(const unsigned idx : consumers_[good])
    {
        const Recipe& recipe = recipes[idx];
        if(!managed_[recipe.building] || !player_.IsBuildingEnabled(recipe.building))
            continue;
        unsigned bestRank = 3;
        WayCheck bestWay;
        for(const GoodType output : recipe.outputs)
        {
            if(!hasUse_[output])
                continue;
            const unsigned rank = wares_[output].state == SupplyState::Shortage ?
                                    0u :
                                    wares_[output].state == SupplyState::Normal ? 1u : 2u;
            // The surplus is there for the consumer: only its other inputs must be made
            const WayCheck way = CheckWay(idx, output, 0, good);
            if(rank < bestRank)
            {
                bestRank = rank;
                bestWay = way;
            }
        }
        if(bestRank < 2u && !bestWay.blocked)
            candidates.push_back({idx, bestRank, bestWay});
    }
    if(candidates.empty())
        return;
    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if(lhs.stateRank != rhs.stateRank)
            return lhs.stateRank < rhs.stateRank;
        if(lhs.way.numNew != rhs.way.numNew)
            return lhs.way.numNew < rhs.way.numNew;
        if(lhs.way.productivity != rhs.way.productivity)
            return lhs.way.productivity > rhs.way.productivity;
        return lhs.way.cost < rhs.way.cost;
    });
    Order order;
    order.building = recipes[candidates.front().recipe].building;
    order.forWare = good;
    order.consumer = true;
    Add(std::move(order));
}

void EconomyPlanner::AddExpandingOrder(const GoodType good)
{
    const std::vector<WayCheck> ways = RankProducers(good, true, 0);
    if(ways.empty() || ways.front().blocked)
        return;
    const BuildingType bld = civAI_.GetGraph().GetRecipes()[ways.front().recipe].building;
    // Woodcutters and quarries expand where trees and stones are (AddClearingOrders), not by the stock
    if((bld == BuildingType::Woodcutter || bld == BuildingType::Quarry))
        return;
    Order order;
    order.building = bld;
    order.forWare = good;
    order.kind = OrderKind::Expanding;
    Add(std::move(order));
}

void EconomyPlanner::AddClearingOrders()
{
    // Clearing woodcutters and quarries clear the land: one order each as long as there are trees / stone piles in the
    // territory (a place with enough in reach is searched when its turn comes)
    const ResourceProfile owned = civAI_.GetMap().GetTotalOwned();
    const auto add = [&](const BuildingType bld, const GoodType good, const Source source, const unsigned minAmount) {
        if(!managed_[bld] || !player_.IsBuildingEnabled(bld) || owned.GetAmount(source) < minAmount)
            return;
        if(helpers::contains_if(orders_, [bld](const Order& order) {
               return order.building == bld && order.clearing && order.state != OrderState::Placed;
           }))
            return;
        Order order;
        order.building = bld;
        order.forWare = good;
        order.kind = OrderKind::Expanding;
        order.clearing = true;
        Add(std::move(order));
    };
    add(BuildingType::Woodcutter, GoodType::Wood, Source{SourceKind::Trees, 0}, CLEARING_MIN_TREES);
    add(BuildingType::Quarry, GoodType::Stones, Source{SourceKind::StonePiles, 0}, QUARRY_MIN_STONE_PILES);
}

void EconomyPlanner::AddStorehouseOrder(const unsigned parent)
{
    if(IsInChain(parent, GoodType::Nothing, BuildingType::Storehouse)
       || helpers::contains_if(orders_, [](const Order& order) { return order.building == BuildingType::Storehouse; }))
        return;
    Order order;
    order.building = BuildingType::Storehouse;
    order.parent = parent;
    order.reason = "für Gehilfen";
    Add(std::move(order));
}

void EconomyPlanner::AddForesterOrders()
{
    if(!managed_[BuildingType::Forester])
        return;
    // A woodcutter for good (not clearing) that worked a balance length with a productivity below HIGH_PRODUCTIVITY
    // gets a forester in its reach, unless one is there already (or its site or order)
    const GameWorldBase& world = player_.GetGameWorld();
    const unsigned woodcutterRadius = nofFarmhand::GetWorkRadius(Job::Woodcutter);
    const unsigned startupGFs = BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    const BuildingRegister& buildings = player_.GetBuildingRegister();
    for(const nobUsual* woodcutter : buildings.GetBuildings(BuildingType::Woodcutter))
    {
        const MapPoint pos = woodcutter->GetPos();
        const auto working = workingSince_.find(pos);
        if(IsClearingWoodcutter(pos) || working == workingSince_.end() || working->second + startupGFs > currentGF_
           || woodcutter->GetProductivity() >= HIGH_PRODUCTIVITY * 100)
            continue;
        const auto isNear = [&](const MapPoint pt) { return world.CalcDistance(pt, pos) <= woodcutterRadius; };
        if(helpers::contains_if(buildings.GetBuildings(BuildingType::Forester),
                                [&](const nobUsual* forester) { return isNear(forester->GetPos()); })
           || helpers::contains_if(buildings.GetBuildingSites(),
                                   [&](const noBuildingSite* site) {
                                       return site->GetBuildingType() == BuildingType::Forester
                                              && isNear(site->GetPos());
                                   })
           || helpers::contains_if(orders_, [&](const Order& order) {
                  return order.building == BuildingType::Forester && order.near == pos;
              }))
            continue;
        Order order;
        order.building = BuildingType::Forester;
        order.near = pos;
        order.reason = "Holzfäller unter hoher Produktivität";
        Add(std::move(order));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Working on the list

void EconomyPlanner::ProcessList()
{
    unsigned searchBudget = SEARCHES_PER_STEP;
    keptInputs_.clear();
    militaryKept_ = BuildingType::Headquarters;
    std::vector<const Order*> list = GetList();
    std::vector<unsigned> ids;
    for(const Order* order : list)
        ids.push_back(order->id);
    bool militaryDone = false;
    for(const unsigned id : ids)
    {
        const Order* order = Find(id);
        // Military buildings are expanding (ai.lua) and are still placed by the old AI: before the first expanding
        // order the material for its next military building is kept free
        if(!militaryDone && order && order->kind == OrderKind::Expanding && !order->selfSolving)
        {
            KeepMaterialForMilitary();
            militaryDone = true;
        }
        ProcessOrder(id, searchBudget);
    }
    if(!militaryDone)
        KeepMaterialForMilitary();
}

void EconomyPlanner::KeepMaterialForMilitary()
{
    const AIJH::AIConstruction& construction = aijh_.GetConstruction();
    if(!construction.CanStartMilitarySite())
        return;
    // The cheapest military building the player may build
    unsigned bestCost = std::numeric_limits<unsigned>::max();
    for(const BuildingType bld : BuildingProperties::militaryBldTypes)
    {
        if(player_.IsBuildingEnabled(bld) && getTotalCost(bld) < bestCost)
        {
            bestCost = getTotalCost(bld);
            militaryKept_ = bld;
        }
    }
    if(militaryKept_ == BuildingType::Headquarters)
        return;
    AIJH::ConstructionMaterial taken{};
    takeMaterial(freeMaterial_, militaryKept_, taken);
}

void EconomyPlanner::MoveToEnd(Order& order, const std::string& reason, const RetryCondition& retry)
{
    order.state = OrderState::Blocked;
    order.reason = reason;
    order.retry = retry;
    order.sequence = nextSequence_++;
}

void EconomyPlanner::ProcessOrder(const unsigned id, unsigned& searchBudget)
{
    Order* order = Find(id);
    if(!order || order->state == OrderState::Placed)
        return;
    // Created for an order that cannot be built until the situation changes: of no use (e.g. the well for a brewery
    // without grain)
    if(order->parent != 0 && IsChainBlocked(order->parent))
    {
        Remove(id);
        return;
    }
    const bool needed = order->kind == OrderKind::Needed;
    const RetryCondition situationChanges{RetryCondition::Kind::Situation, GetSituation()};
    // The inputs this order lacks are kept for it: no order below it gets a building using them
    for(const GoodType input : order->inputsMissing)
        keptInputs_.emplace(input, id);
    // Blocked: tried again when its condition is met (material there, situation changed ...), not every step
    if(!order->selfSolving && order->state == OrderState::Blocked && !IsRetryMet(order->retry, order->building))
        return;
    // Waits for the orders created for it. A self-solving order is the solution itself and does not wait
    if(!order->selfSolving)
    {
        const auto child = std::find_if(orders_.begin(), orders_.end(), [id](const Order& other) {
            return other.parent == id && other.state != OrderState::Placed;
        });
        if(child != orders_.end())
        {
            order->state = OrderState::Waiting;
            order->reason = "wartet auf " + buildingName(child->building);
            return;
        }
    }

    // Inputs (the surplus a consumer is for suffices by definition)
    if(const Recipe* recipe = FindRecipe(order->building))
    {
        // Inputs kept for an order above: this one must wait (any of: only if all of them are kept)
        std::vector<const Order*> keptFor;
        std::vector<GoodType> keptWares;
        for(const GoodType input : recipe->inputs)
        {
            if(order->consumer && input == order->forWare)
                continue;
            const auto kept = keptInputs_.find(input);
            if(kept != keptInputs_.end() && kept->second != id)
            {
                keptWares.push_back(input);
                keptFor.push_back(Find(kept->second));
            }
        }
        const bool blockedByKept =
          recipe->consumeEach ? !keptWares.empty() : !keptWares.empty() && keptWares.size() == recipe->inputs.size();
        if(blockedByKept)
        {
            const std::string reason = wareName(keptWares.front()) + " vorbehalten für "
                                       + (keptFor.front() ? buildingName(keptFor.front()->building) : "-");
            if(needed)
            {
                order->state = OrderState::Waiting;
                order->reason = reason;
            } else
                MoveToEnd(*order, reason, RetryCondition{});
            return;
        }
        std::vector<GoodType> missing;
        bool anySuffices = recipe->inputs.empty();
        for(const GoodType input : recipe->inputs)
        {
            if((order->consumer && input == order->forWare) || InputSuffices(input, *recipe))
                anySuffices = true;
            else
                missing.push_back(input);
        }
        const bool suffices = recipe->consumeEach ? missing.empty() : anySuffices;
        order->inputsMissing = suffices ? std::vector<GoodType>() : missing;
        if(suffices)
        {
            // Not the reason anymore: free for the orders below
            for(auto it = keptInputs_.begin(); it != keptInputs_.end();)
                it = it->second == id ? keptInputs_.erase(it) : std::next(it);
        } else
        {
            // The missing inputs are the reason: kept for this order from now on
            for(const GoodType input : missing)
                keptInputs_.emplace(input, id);
            if(!needed)
            {
                // Checked again every step (cheap, no place search)
                MoveToEnd(*order, "Eingang fehlt: " + wareName(missing.front()), RetryCondition{});
                return;
            }
            // Every missing input must be possible, otherwise the building cannot work: then no orders for the
            // others (no well for a brewery that gets no grain)
            std::string noWay;
            if(recipe->consumeEach)
            {
                for(const GoodType input : missing)
                {
                    const std::vector<WayCheck> ways = RankProducers(input, false, 0);
                    if(ways.empty() || ways.front().blocked)
                    {
                        noWay = "kein Weg zu " + wareName(input);
                        if(!ways.empty())
                            noWay += " (" + ways.front().reason + ")";
                        break;
                    }
                }
            } else if(ChooseWare(missing) == GoodType::Nothing)
            {
                std::string names;
                for(const GoodType input : missing)
                    names += (names.empty() ? "" : "/") + wareName(input);
                noWay = "kein Weg zu " + names;
            }
            if(!noWay.empty())
            {
                order->state = OrderState::Blocked;
                order->reason = noWay;
                order->retry = situationChanges;
                RemoveUnplacedChildren(id);
                return;
            }
            order->state = OrderState::Waiting;
            order->reason = "Eingang fehlt: " + wareName(missing.front());
            const std::vector<GoodType> toResolve =
              recipe->consumeEach ? missing : std::vector<GoodType>{ChooseWare(missing)};
            for(const GoodType input : toResolve)
            {
                // Made for an order above already: a loop (the ware is needed to make itself)
                if(HasChildFor(id, input) || IsInChain(id, input, BuildingType::Headquarters))
                    continue;
                ResolveShortage(input, id); // May add orders: `order` is not valid anymore
            }
            return;
        }
    }

    RetryCondition retry;
    const std::string rule = CheckRules(*order, retry);
    if(!rule.empty())
    {
        if(!needed && !order->selfSolving)
        {
            MoveToEnd(*order, rule, retry);
            return;
        }
        order->state = order->selfSolving ? OrderState::Waiting : OrderState::Blocked;
        order->reason = rule;
        order->retry = retry;
        SolveRule(id, retry);
        return;
    }

    bool searchedAll = false;
    const MapPoint pos = SearchPlace(*order, searchBudget, searchedAll);
    if(pos.isValid())
    {
        const BuildingType building = order->building;
        const bool clearing = order->clearing;
        if(!aijh_.PlaceBuilding(building, pos))
            return;
        AIJH::ConstructionMaterial taken{};
        takeMaterial(freeMaterial_, building, taken);
        // Placed: its inputs are planned from now on (and its costs if it was not counted at the start of the step)
        AddPlanned(building, true, !helpers::contains(plannedOrderIds_, id), true);
        if(clearing && building == BuildingType::Woodcutter)
        {
            aijh_.GetInterface().SetClearingWoodcutter(pos, true);
            clearingPending_.push_back(pos);
        }
        if(!needed)
        {
            // Expanding orders have no start-up time
            Remove(id);
            return;
        }
        order = Find(id);
        order->state = OrderState::Placed;
        order->placedAt = pos;
        order->reason.clear();
        return;
    }
    if(!searchedAll)
    {
        order->state = OrderState::Open;
        order->reason = "suche Bauplatz";
        return;
    }
    if(!needed)
    {
        MoveToEnd(*order, "kein Bauplatz", situationChanges);
        return;
    }
    // No place for this way: the next way to the ware is tried, this one again when the situation changed
    if(order->forWare != GoodType::Nothing && !order->consumer && !order->clearing)
    {
        failedWays_.push_back({order->forWare, order->building, GetSituation()});
        Remove(id);
        return;
    }
    order->state = OrderState::Blocked;
    order->reason = "kein Bauplatz";
    order->retry = situationChanges;
    RemoveUnplacedChildren(id);
}

std::string EconomyPlanner::CheckRules(const Order& order, RetryCondition& retry) const
{
    const BuildingType bld = order.building;
    if(!player_.IsBuildingEnabled(bld))
    {
        retry = {RetryCondition::Kind::Situation, GetSituation()};
        return "nicht erforscht";
    }
    const AIJH::AIConstruction& construction = aijh_.GetConstruction();
    std::vector<GoodType> missingMaterial;
    std::string material = DescribeMissingMaterial(bld, missingMaterial);
    if(!material.empty())
    {
        retry = {RetryCondition::Kind::Material, GetSituation()};
        return material;
    }
    if(!construction.HasEnoughBuildersAvailable())
    {
        retry = {RetryCondition::Kind::Builder, GetSituation()};
        return "kein Bauarbeiter";
    }
    if(BuildingProperties::IsUsual(bld))
    {
        if(!IsWorkerAvailable(bld))
        {
            retry = {GetFreeHelpers() > 0 ? RetryCondition::Kind::Tool : RetryCondition::Kind::Helpers, GetSituation()};
            return GetFreeHelpers() > 0 ? "Werkzeug für Arbeiter fehlt" : "keine Gehilfen";
        }
        if(!HasRequiredTools(bld))
        {
            retry = {RetryCondition::Kind::Tool, GetSituation()};
            return "Werkzeug fehlt";
        }
    }
    return std::string();
}

int EconomyPlanner::GetFreeHelpers() const
{
    // Helpers in the warehouses minus those already promised: workers asked for and building sites which will need
    // a worker
    int promised = 0;
    for(const auto job : helpers::enumRange<Job>())
        promised += snapshot_.jobsWanted[job];
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(BuildingProperties::IsUsual(bld))
            promised += snapshot_.buildings[bld].numSites;
    }
    return static_cast<int>(snapshot_.inWarehousesPeople[Job::Helper]) - promised;
}

bool EconomyPlanner::IsWorkerAvailable(const BuildingType bld) const
{
    const auto job = getJob(bld);
    if(!job)
        return true;
    if(snapshot_.inWarehousesPeople[*job] > snapshot_.jobsWanted[*job])
        return true;
    if(GetFreeHelpers() <= 0)
        return false;
    const civ::Content& content = civ::getActiveContent();
    if(content.IsRecruitedWithoutTool(*job))
        return true;
    GoodType tool = GoodType::Nothing;
    if(isCustomJob(*job))
    {
        if(const civ::CustomWorker* worker = content.GetWorker(*job))
            tool = worker->tool;
    } else
        tool = JOB_CONSTS[*job].tool.value_or(GoodType::Nothing);
    return tool == GoodType::Nothing || GetToolStock(tool) > 0;
}

unsigned EconomyPlanner::GetToolStock(const GoodType tool) const
{
    // All wares of the kind of tool (e.g. iron and copper axes)
    const auto kind = getToolOf(tool);
    if(!kind)
        return snapshot_.inWarehouses[tool];
    unsigned result = 0;
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(snapshot_.inWarehouses[good] > 0 && getToolOf(good) == kind)
            result += snapshot_.inWarehouses[good];
    }
    return result;
}

bool EconomyPlanner::HasRequiredTools(const BuildingType bld) const
{
    // Tool reform: a building with work speed 0 without tools does not work without the tool of its first slot
    const civ::Content& content = civ::getActiveContent();
    if(!content.HasToolReform())
        return true;
    const civ::BuildingTools& tools = content.GetBuildingTools(bld);
    if(tools.baseSpeed > 0 || tools.slots.empty())
        return true;
    return helpers::contains_if(tools.slots.front().wares,
                                [this](const GoodType good) { return snapshot_.inWarehouses[good] > 0; });
}

void EconomyPlanner::SolveRule(const unsigned id, const RetryCondition& retry)
{
    Order* order = Find(id);
    if(!order)
        return;
    const BuildingType bld = order->building;
    switch(retry.kind)
    {
        case RetryCondition::Kind::Material:
        {
            // Keep the material that is there for this order (orders below and other buildings must not take it)
            const AIJH::ConstructionMaterial free = freeMaterial_;
            takeMaterial(freeMaterial_, bld, reservedMaterial_);
            std::vector<std::pair<GoodType, unsigned>> lacking;
            for(const CostEntry& entry : BUILDING_COSTS[bld])
            {
                int available = 0;
                for(const GoodType good : entry.wares)
                    available += std::max(0, free[good]);
                if(available >= static_cast<int>(entry.amount))
                    continue;
                GoodType good = entry.wares.size() == 1u ? entry.wares.front() : ChooseWare(entry.wares);
                if(good == GoodType::Nothing)
                    good = entry.wares.front();
                lacking.emplace_back(good, entry.amount - available);
            }
            if(lacking.empty())
                break;
            // Why it is missing, e.g. "Bretter: frei -4, Bilanz -3; Sägewerk: ... -> Holz: ..."
            const std::string why = ExplainWare(lacking.front().first, 2);
            // Material made by a chain the building is part of (a woodcutter lacking boards): it is the solution
            // itself. It gets what comes back to the warehouses before everybody else, building sites outside the chain
            // whose material is not built in yet are cancelled. Other material: its producers are ordered for it
            std::vector<std::pair<GoodType, unsigned>> selfSolved;
            std::vector<GoodType> toOrder;
            for(const auto& [good, amount] : lacking)
            {
                if(order->selfSolving && helpers::contains(GetChain(good), bld))
                {
                    selfSolved.emplace_back(good, amount);
                    reservedMaterial_[good] += amount;
                } else
                    toOrder.push_back(good);
            }
            const std::string cancelled = selfSolved.empty() ? std::string() : CancelSitesFor(selfSolved);
            order = Find(id);
            if(!selfSolved.empty())
                order->reason = "Vorrang (löst den Mangel selbst): " + order->reason;
            order->reason += " – " + why;
            if(!cancelled.empty())
                order->reason += "; abgebrochen: " + cancelled;
            for(const GoodType good : toOrder)
            {
                if(HasChildFor(id, good) || IsInChain(id, good, BuildingType::Headquarters))
                    continue;
                ResolveShortage(good, id); // May add orders: `order` is not valid anymore
            }
            break;
        }
        case RetryCondition::Kind::Helpers: AddStorehouseOrder(id); break;
        case RetryCondition::Kind::Now:
        case RetryCondition::Kind::Builder:
        case RetryCondition::Kind::Tool:
        case RetryCondition::Kind::Situation:
            // Builders and tools: the tools are ordered (OrderTools). Situation: research or territory needed
            break;
    }
}

std::string EconomyPlanner::DescribeMissingMaterial(const BuildingType bld, std::vector<GoodType>& missing) const
{
    std::string result;
    for(const CostEntry& entry : BUILDING_COSTS[bld])
    {
        int available = 0, stock = 0, forSites = 0, forOrdersAbove = 0;
        std::string names;
        for(const GoodType good : entry.wares)
        {
            available += std::max(0, freeMaterial_[good]);
            stock += static_cast<int>(snapshot_.inWarehouses[good]);
            forSites += static_cast<int>(snapshot_.inWarehouses[good]) - stepFreeMaterial_[good];
            forOrdersAbove += std::max(0, stepFreeMaterial_[good]) - std::max(0, freeMaterial_[good]);
            names += (names.empty() ? "" : "/") + wareName(good);
        }
        if(available >= static_cast<int>(entry.amount))
            continue;
        missing.push_back(entry.wares.front());
        std::ostringstream out;
        out << names << " fehlt: " << static_cast<unsigned>(entry.amount) << " nötig, " << available << " frei (Lager "
            << stock << ", Baustellen brauchen noch " << forSites << ", für Aufträge darüber " << forOrdersAbove << ")";
        result += (result.empty() ? "" : "; ") + out.str();
    }
    return result;
}

std::string EconomyPlanner::ExplainWare(const GoodType good, const unsigned depth)
{
    const WareStatus& status = wares_[good];
    std::ostringstream out;
    out << wareName(good) << ": frei " << status.free << ", Bilanz " << status.balance;
    const std::vector<WayCheck> ways = RankProducers(good, false, 0);
    if(ways.empty())
        return out.str() + ", kein Erzeuger";
    if(ways.front().blocked)
        return out.str() + ", " + ways.front().reason;
    const Recipe& recipe = civAI_.GetGraph().GetRecipes()[ways.front().recipe];
    const BuildingTypeState& state = snapshot_.buildings[recipe.building];
    const unsigned working = state.numBuildings - state.withoutWorker;
    out << "; " << buildingName(recipe.building) << ": " << working << " arbeiten";
    if(working > 0)
        out << " (Produktivität " << state.GetProductivity() << " %)";
    if(state.numSites > 0)
        out << ", " << state.numSites << " Baustellen";
    if(state.withoutWorker > 0)
        out << ", " << state.withoutWorker << " ohne Arbeiter";
    if(depth > 0)
    {
        for(const GoodType input : recipe.inputs)
        {
            if(!InputSuffices(input, recipe))
                return out.str() + " -> " + ExplainWare(input, depth - 1);
        }
    }
    return out.str();
}

std::vector<BuildingType> EconomyPlanner::GetChain(const GoodType good)
{
    const auto cached = chainCache_.find(good);
    if(cached != chainCache_.end())
        return cached->second;
    // The best producer of the ware, and for each of its inputs that does not suffice the best producer of that input
    // and so on. Example boards: sawmill; wood does not suffice: woodcutter and forester
    const std::vector<Recipe>& recipes = civAI_.GetGraph().GetRecipes();
    std::vector<BuildingType> chain;
    std::vector<GoodType> todo{good}, seen;
    while(!todo.empty())
    {
        const GoodType current = todo.back();
        todo.pop_back();
        if(helpers::contains(seen, current))
            continue;
        seen.push_back(current);
        const std::vector<WayCheck> ways = RankProducers(current, false, 0);
        if(ways.empty() || ways.front().blocked)
            continue;
        const Recipe& recipe = recipes[ways.front().recipe];
        if(!helpers::contains(chain, recipe.building))
            chain.push_back(recipe.building);
        for(const Renewer& renewer : civAI_.GetGraph().GetRenewers())
        {
            if(renewer.source.kind == recipe.source.kind && renewer.source.index == recipe.source.index
               && recipe.source.kind != SourceKind::None && !helpers::contains(chain, renewer.building))
                chain.push_back(renewer.building);
        }
        for(const GoodType input : recipe.inputs)
        {
            if(!InputSuffices(input, recipe))
                todo.push_back(input);
        }
    }
    chainCache_[good] = chain;
    return chain;
}

void EconomyPlanner::MarkSelfSolving()
{
    for(Order& order : orders_)
        order.selfSolving = false;
    for(Order& order : orders_)
    {
        if(order.state == OrderState::Placed)
            continue;
        std::vector<GoodType> missing;
        if(DescribeMissingMaterial(order.building, missing).empty())
            continue;
        for(const GoodType good : missing)
        {
            const std::vector<BuildingType> chain = GetChain(good);
            if(helpers::contains(chain, order.building))
                order.selfSolving = true;
        }
    }
}

std::string EconomyPlanner::CancelSitesFor(const std::vector<std::pair<GoodType, unsigned>>& lacking)
{
    // Once per step: the cancelled sites need some time to give their material back
    if(cancelledThisStep_)
        return std::string();
    cancelledThisStep_ = true;
    std::ostringstream result;
    const EconomyStatistics& stats = player_.GetEconomyStatistics();
    for(const auto& [good, amount] : lacking)
    {
        // Only when stuck: nothing of it made in the balance length and not enough in the warehouses. Otherwise the
        // material comes by itself and the other sites keep theirs
        if(stats.GetProduced(good, BALANCE_MINUTES, currentGF_) > 0 || snapshot_.inWarehouses[good] >= amount)
            continue;
        const std::vector<BuildingType> chain = GetChain(good);
        // Sites outside the chain whose builder has not built in anything yet. What comes back: the wares on the way
        // there and those not ordered yet (wares already lying at the site are lost)
        std::vector<std::pair<unsigned, const noBuildingSite*>> candidates;
        for(const noBuildingSite* site : player_.GetBuildingRegister().GetBuildingSites())
        {
            const BuildingType bld = site->GetBuildingType();
            if(helpers::contains(chain, bld) || site->IsHarborBuildingSiteFromSea())
                continue;
            const std::vector<noBuildingSite::Material>& materials = site->GetMaterials();
            const BuildingCosts& costs = BUILDING_COSTS[bld];
            bool builtIn = false;
            unsigned comesBack = 0;
            for(unsigned i = 0; i < materials.size() && i < costs.size(); ++i)
            {
                builtIn |= materials[i].used > 0;
                if(helpers::contains(costs[i].wares, good))
                    comesBack += costs[i].amount - std::min<unsigned>(costs[i].amount, materials[i].delivered.size());
            }
            if(!builtIn && comesBack > 0)
                candidates.emplace_back(comesBack, site);
        }
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const auto& lhs, const auto& rhs) { return lhs.first > rhs.first; });
        unsigned freed = 0;
        for(const auto& [comesBack, site] : candidates)
        {
            if(freed >= amount)
                break;
            aijh_.GetInterface().DestroyBuilding(site->GetPos());
            freed += comesBack;
            result << (result.tellp() > 0 ? ", " : "") << buildingName(site->GetBuildingType()) << " (" << comesBack
                   << " " << wareName(good) << ")";
        }
    }
    return result.str();
}

bool EconomyPlanner::HasMaterial(const BuildingType bld) const
{
    AIJH::ConstructionMaterial free = freeMaterial_;
    AIJH::ConstructionMaterial taken{};
    return takeMaterial(free, bld, taken);
}

bool EconomyPlanner::IsRetryMet(const RetryCondition& retry, const BuildingType bld) const
{
    if(retry.kind != RetryCondition::Kind::Now && retry.situation != GetSituation())
        return true;
    switch(retry.kind)
    {
        case RetryCondition::Kind::Now: return true;
        case RetryCondition::Kind::Material: return HasMaterial(bld);
        case RetryCondition::Kind::Builder: return aijh_.GetConstruction().HasEnoughBuildersAvailable();
        case RetryCondition::Kind::Helpers: return GetFreeHelpers() > 0;
        case RetryCondition::Kind::Tool: return IsWorkerAvailable(bld) && HasRequiredTools(bld);
        case RetryCondition::Kind::Situation: return false;
    }
    return true;
}

MapPoint EconomyPlanner::SearchPlace(Order& order, unsigned& searchBudget, bool& searchedAll)
{
    searchedAll = false;
    std::vector<MapPoint> centers;
    if(order.near.isValid())
        centers.push_back(order.near);
    else
    {
        // Processing near the warehouses, buildings working on the map (and storehouses) also near the military
        // buildings (like the old AI)
        for(const nobBaseWarehouse* warehouse : player_.GetBuildingRegister().GetStorehouses())
            centers.push_back(warehouse->GetPos());
        const Recipe* recipe = FindRecipe(order.building);
        const bool worksOnMap = !recipe || recipe->source.kind != SourceKind::None;
        if(worksOnMap)
        {
            for(const nobMilitary* military : player_.GetBuildingRegister().GetMilitaryBuildings())
                centers.push_back(military->GetPos());
        }
    }
    if(centers.empty())
    {
        searchedAll = true;
        return MapPoint::Invalid();
    }
    while(searchBudget > 0)
    {
        if(order.centersTried >= centers.size())
        {
            order.centersTried = 0;
            searchedAll = true;
            return MapPoint::Invalid();
        }
        const MapPoint center = centers[(order.nextCenter++) % centers.size()];
        ++order.centersTried;
        --searchBudget;
        aijh_.UpdateNodesAround(center, 15);
        MapPoint pos;
        if(order.building == BuildingType::Woodcutter)
            pos = aijh_.FindWoodcutterPosition(center, order.clearing);
        else if(order.building == BuildingType::Quarry && order.clearing)
            pos = aijh_.FindQuarryPosition(center);
        else if(order.building == BuildingType::Forester)
        {
            std::vector<MapPoint> clearing = player_.GetClearingWoodcutters();
            clearing.insert(clearing.end(), clearingPending_.begin(), clearingPending_.end());
            pos = aijh_.FindForesterPosition(center, clearing);
        } else
            pos = aijh_.FindPositionForBuildingAround(order.building, center);
        if(pos.isValid())
        {
            order.centersTried = 0;
            return pos;
        }
    }
    return MapPoint::Invalid();
}

// ---------------------------------------------------------------------------------------------------------------------
// Tools: only orders, at least TOOLS_IN_STOCK of each kind beyond what is asked for. The regulator stays at 0

void EconomyPlanner::OrderTools()
{
    if(!player_.GetGameWorld().GetGGS().isEnabled(AddonId::TOOL_ORDERING))
        return;
    helpers::EnumArray<unsigned, Tool> asked{};
    for(const OpenDemand& demand : snapshot_.openDemand)
    {
        if(demand.purpose != WareUse::Tool || demand.wares.empty())
            continue;
        if(const auto tool = getToolOf(demand.wares.front()))
            asked[*tool] += demand.amount;
    }
    helpers::EnumArray<int8_t, Tool> orders{};
    bool any = false;
    for(const auto tool : helpers::enumRange<Tool>())
    {
        const unsigned have = GetToolStock(TOOL_TO_GOOD[tool]) + player_.GetToolsOrdered(tool);
        const unsigned wanted = asked[tool] + TOOLS_IN_STOCK;
        if(wanted > have)
        {
            orders[tool] = static_cast<int8_t>(std::min(wanted - have, 100u));
            any = true;
        }
    }
    bool regulatorAtZero = true;
    for(const auto tool : helpers::enumRange<Tool>())
        regulatorAtZero &= player_.GetToolPriority(tool) == 0;
    if(any || !regulatorAtZero)
        aijh_.GetInterface().ChangeTools(ToolSettings{}, orders.data());
}

// ---------------------------------------------------------------------------------------------------------------------

unsigned EconomyPlanner::CountStonePiles(const GamePlayer& player, const MapPoint pt, const unsigned radius)
{
    const GameWorldBase& world = player.GetGameWorld();
    unsigned result = 0;
    world.CheckPointsInRadius(
      pt, radius,
      [&](const MapPoint curPt, unsigned) {
          if(world.GetNO(curPt)->GetType() == NodalObjectType::Granite)
              ++result;
          return false;
      },
      true);
    return result;
}

unsigned EconomyPlanner::CountTrees(const GamePlayer& player, const MapPoint pt, const unsigned radius)
{
    const GameWorldBase& world = player.GetGameWorld();
    unsigned result = 0;
    world.CheckPointsInRadius(
      pt, radius,
      [&](const MapPoint curPt, unsigned) {
          if(world.GetNO(curPt)->GetType() == NodalObjectType::Tree)
              ++result;
          return false;
      },
      true);
    return result;
}

std::string EconomyPlanner::GetReport() const
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    out << "  Mangel:";
    for(const auto good : helpers::enumRange<GoodType>())
    {
        const WareStatus& status = wares_[good];
        if(status.state == SupplyState::Shortage)
        {
            out << " " << wareName(good) << " (Bestand " << status.stock << ", frei " << status.free << ", Bilanz "
                << status.balance << "; angefragt: offen " << status.openDemand << ", unterwegs " << status.inTransit
                << ", geplant " << status.planned << ");";
        }
    }
    out << "\n  Überschuss:";
    for(const auto good : helpers::enumRange<GoodType>())
    {
        const WareStatus& status = wares_[good];
        if(status.state == SupplyState::Surplus && hasUse_[good])
            out << " " << wareName(good) << " (frei " << status.free << ");";
    }
    out << "\n  Liste:\n";
    unsigned pos = 0;
    static const char* const stateNames[] = {"offen", "wartet", "blockiert", "platziert"};
    for(const Order* order : GetList())
    {
        out << "    " << ++pos << ". " << buildingName(order->building)
            << (order->kind == OrderKind::Needed ? " (benötigt" : " (expandierend");
        const unsigned level = player_.GetGameWorld().GetTechTree().GetAIBasicEconomyLevel(order->building);
        if(level > 0)
            out << ", Stufe " << level;
        if(order->forWare != GoodType::Nothing)
            out << (order->consumer ? ", verarbeitet " : ", für ") << wareName(order->forWare);
        // What it is for: "Water für Brewery"
        if(const Order* parent = order->parent ? Find(order->parent) : nullptr)
            out << " für " << buildingName(parent->building);
        if(order->clearing)
            out << (order->building == BuildingType::Woodcutter ? ", rodend" : ", nach Vorkommen");
        out << ") " << stateNames[static_cast<unsigned>(order->state)];
        if(!order->reason.empty())
            out << ": " << order->reason;
        out << "\n";
    }
    if(militaryKept_ != BuildingType::Headquarters)
        out << "    Material für " << buildingName(militaryKept_)
            << " (Militär der alten KI) vor den expandierenden Aufträgen freigehalten\n";
    out << "  Zeit: " << static_cast<unsigned>(lastStepUs_) << " us (max " << static_cast<unsigned>(maxStepUs_)
        << " us)\n";
    return out.str();
}

} // namespace civai
