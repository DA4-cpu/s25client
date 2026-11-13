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
    helpers::erase_if(failedWays_, [this](const FailedWay& way) { return way.situation != GetSituation(); });
    UpdateWareStatus();
    UpdateWorkingBuildings();
    UpdatePlacedOrders();
    CreateOrders();
    freeMaterial_ = aijh_.GetConstruction().GetFreeConstructionMaterial();
    reservedMaterial_ = {};
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

void EconomyPlanner::UpdateWareStatus()
{
    const EconomyStatistics& stats = player_.GetEconomyStatistics();
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
        const double threshold = std::max(static_cast<double>(MIN_STOCK), SURPLUS_FACTOR * status.consumptionPerMinute);
        if(status.balance < 0 || status.openDemand > status.stock)
            status.state = SupplyState::Shortage;
        else if(status.stock > threshold)
            status.state = SupplyState::Surplus;
        else
            status.state = SupplyState::Normal;
        if(status.openDemand > status.stock)
            status.minutesUntilEmpty = 0;
        else if(status.balance < 0)
            status.minutesUntilEmpty = status.stock / (-status.balance / coveredMinutes);
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
    helpers::erase_if(clearingWoodcutters_, [this](const MapPoint pt) {
        const auto* building = player_.GetGameWorld().GetSpecObj<noBaseBuilding>(pt);
        return !building || building->GetBuildingType() != BuildingType::Woodcutter;
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
    // Consumers which are ordered or built but not measured yet will use the ware too: the balance does not show them
    const double pending = GetUnmeasuredConsumption(input);
    const double coveredMinutes = player_.GetEconomyStatistics().GetCoveredMinutes(BALANCE_MINUTES, currentGF_);
    if(status.balance > 0 && status.balance / coveredMinutes > pending)
        return true;
    // No surplus left: enough stock for what is used now, what the unmeasured consumers and the new building would use
    // by their work times
    const double needed =
      std::max(static_cast<double>(MIN_STOCK),
               SURPLUS_FACTOR * (status.consumptionPerMinute + pending + consumer.nominalStepsPerMinute));
    return status.stock >= needed;
}

double EconomyPlanner::GetUnmeasuredConsumption(const GoodType input) const
{
    // Consumers of the ware which do not work for a full balance length yet: building sites (also those whose command
    // is still on its way), buildings without worker or starting up. Each by the work time of its recipe
    const unsigned startupGFs = BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    const std::vector<Recipe>& recipes = civAI_.GetGraph().GetRecipes();
    double result = 0;
    std::vector<BuildingType> counted;
    for(const unsigned idx : consumers_[input])
    {
        const Recipe& recipe = recipes[idx];
        if(helpers::contains(counted, recipe.building))
            continue;
        counted.push_back(recipe.building);
        unsigned num = snapshot_.buildings[recipe.building].numSites;
        for(const nobUsual* building : player_.GetBuildingRegister().GetBuildings(recipe.building))
        {
            const auto it = workingSince_.find(building->GetPos());
            if(it == workingSince_.end() || it->second + startupGFs > currentGF_)
                ++num;
        }
        num += aijh_.GetConstruction().GetNumPendingSites(recipe.building);
        result += num * recipe.nominalStepsPerMinute;
    }
    return result;
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
    if(numWorking > 0 && productivity < GOOD_PRODUCTIVITY)
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

bool EconomyPlanner::HasOrder(const BuildingType bld, const GoodType forWare, const bool consumer) const
{
    return helpers::contains_if(orders_, [&](const Order& order) {
        return order.building == bld && order.forWare == forWare && order.consumer == consumer
               && order.kind == OrderKind::Needed;
    });
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
    return helpers::contains(clearingWoodcutters_, pt);
}

std::vector<const Order*> EconomyPlanner::GetList() const
{
    // Roots: needed before expanding. Needed: shortages by urgency, then the rest (surpluses, helpers, trees) in order
    // of creation. Expanding in order of their sequence (blocked ones moved to the end). The orders created for an
    // order come directly above it
    std::vector<const Order*> roots;
    for(const Order& order : orders_)
    {
        if(order.parent == 0)
            roots.push_back(&order);
    }
    const auto urgency = [this](const Order& order) {
        if(order.kind == OrderKind::Expanding)
            return std::numeric_limits<double>::max();
        if(order.building == BuildingType::Storehouse)
            return 0.;
        const GoodType good = order.building == BuildingType::Forester ? GoodType::Wood : order.forWare;
        if(order.consumer || good == GoodType::Nothing)
            return std::numeric_limits<double>::max() / 2;
        return wares_[good].minutesUntilEmpty;
    };
    std::stable_sort(roots.begin(), roots.end(), [&](const Order* lhs, const Order* rhs) {
        const double l = urgency(*lhs), r = urgency(*rhs);
        if(l != r)
            return l < r;
        return lhs->sequence < rhs->sequence;
    });
    std::vector<const Order*> result;
    const auto addWithChildren = [&](const auto& self, const Order& order) -> void {
        std::vector<const Order*> children;
        for(const Order& child : orders_)
        {
            if(child.parent == order.id)
                children.push_back(&child);
        }
        std::sort(children.begin(), children.end(),
                  [](const Order* lhs, const Order* rhs) { return lhs->sequence < rhs->sequence; });
        for(const Order* child : children)
            self(self, *child);
        result.push_back(&order);
    };
    for(const Order* root : roots)
        addWithChildren(addWithChildren, *root);
    return result;
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
            if(order.kind == OrderKind::Expanding && state != SupplyState::Normal)
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

    // Shortages: more producers
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(wares_[good].state == SupplyState::Shortage && !HasProducerOrder(good))
            ResolveShortage(good, 0);
    }
    // Asked for "any of" (e.g. food of a mine) and not enough of all of them in stock: the easiest one
    for(const OpenDemand& demand : snapshot_.openDemand)
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
    AddForesterOrders();
}

bool EconomyPlanner::IsAnyOfDemand(const GoodType good) const
{
    for(const OpenDemand& demand : snapshot_.openDemand)
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
        if(!blocked && !ways.empty() && parent == 0)
        {
            Order order;
            order.building = civAI_.GetGraph().GetRecipes()[ways.front().recipe].building;
            order.forWare = good;
            blocked = &Add(std::move(order));
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
    if(numWorking > 0 && productivity < GOOD_PRODUCTIVITY)
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
    // As many new producers as the measured deficit needs: deficit per minute / output per minute of one producer.
    // The output of one producer is measured (producers working now) or, without them, taken from the work time of
    // the recipe. At least one
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
          return order.building == recipe.building && order.forWare == good && !order.consumer
                 && order.kind == OrderKind::Needed && order.state != OrderState::Placed;
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
    Order order;
    order.building = civAI_.GetGraph().GetRecipes()[ways.front().recipe].building;
    order.forWare = good;
    order.kind = OrderKind::Expanding;
    order.clearing = order.building == BuildingType::Woodcutter;
    Add(std::move(order));
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
    const GameWorldBase& world = player_.GetGameWorld();
    const unsigned woodcutterRadius = nofFarmhand::GetWorkRadius(Job::Woodcutter);
    const BuildingRegister& buildings = player_.GetBuildingRegister();
    for(const nobUsual* woodcutter : buildings.GetBuildings(BuildingType::Woodcutter))
    {
        const MapPoint pos = woodcutter->GetPos();
        if(IsClearingWoodcutter(pos) || !woodcutter->HasWorker()
           || CountTrees(player_, pos, woodcutterRadius) >= FORESTER_BELOW_TREES)
            continue;
        // A forester (or its building site or order) in reach of the woodcutter serves it already
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
        order.reason = "Bäume für Holzfäller";
        Add(std::move(order));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Working on the list

void EconomyPlanner::ProcessList()
{
    unsigned searchBudget = SEARCHES_PER_STEP;
    std::vector<unsigned> ids;
    for(const Order* order : GetList())
        ids.push_back(order->id);
    for(const unsigned id : ids)
        ProcessOrder(id, searchBudget);
}

void EconomyPlanner::MoveToEnd(Order& order, const std::string& reason)
{
    order.state = OrderState::Blocked;
    order.reason = reason;
    order.sequence = nextSequence_++;
}

void EconomyPlanner::ProcessOrder(const unsigned id, unsigned& searchBudget)
{
    Order* order = Find(id);
    if(!order || order->state == OrderState::Placed)
        return;
    const bool needed = order->kind == OrderKind::Needed;
    if(needed && order->state == OrderState::Blocked && !IsRetryMet(order->retry, order->building))
        return;
    // Waits for the orders created for it
    const auto child = std::find_if(orders_.begin(), orders_.end(), [id](const Order& other) {
        return other.parent == id && other.state != OrderState::Placed;
    });
    if(child != orders_.end())
    {
        order->state = OrderState::Waiting;
        order->reason = "wartet auf " + buildingName(child->building);
        return;
    }

    // Inputs (the surplus a consumer is for suffices by definition)
    if(const Recipe* recipe = FindRecipe(order->building))
    {
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
        if(!suffices)
        {
            if(!needed)
            {
                MoveToEnd(*order, "Eingang fehlt: " + wareName(missing.front()));
                return;
            }
            order->state = OrderState::Waiting;
            order->reason = "Eingang fehlt: " + wareName(missing.front());
            const std::vector<GoodType> toResolve =
              recipe->consumeEach ? missing : std::vector<GoodType>{ChooseWare(missing)};
            if(toResolve.front() == GoodType::Nothing)
            {
                // Any of the inputs would do, but none can be made now
                std::string names;
                for(const GoodType input : missing)
                    names += (names.empty() ? "" : "/") + wareName(input);
                order->state = OrderState::Blocked;
                order->reason = "kein Weg zu " + names;
                order->retry = {RetryCondition::Kind::Situation, GetSituation()};
                return;
            }
            for(const GoodType input : toResolve)
            {
                // Made for an order above already: a loop (the ware is needed to make itself). A producer of the
                // input is ordered or starts up already (for another order): wait for its effect
                if(input == GoodType::Nothing || HasChildFor(id, input)
                   || IsInChain(id, input, BuildingType::Headquarters) || HasProducerOrder(input))
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
        if(!needed)
        {
            MoveToEnd(*order, rule);
            return;
        }
        order->state = OrderState::Blocked;
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
        if(clearing)
            clearingWoodcutters_.push_back(pos);
        if(!needed)
        {
            // Expanding orders have no start-up time
            Remove(id);
            return;
        }
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
        MoveToEnd(*order, "kein Bauplatz");
        return;
    }
    // No place for this way: the next way to the ware is tried, this one again when the situation changed
    if(order->forWare != GoodType::Nothing && !order->consumer)
    {
        failedWays_.push_back({order->forWare, order->building, GetSituation()});
        Remove(id);
        return;
    }
    order->state = OrderState::Blocked;
    order->reason = "kein Bauplatz";
    order->retry = {RetryCondition::Kind::Situation, GetSituation()};
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
    if(!HasMaterial(bld))
    {
        retry = {RetryCondition::Kind::Material, GetSituation()};
        return "Baumaterial fehlt";
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
            // Keep the material that is there for this order (orders below and other buildings must not take it), and
            // make more of what is missing
            const AIJH::ConstructionMaterial free = freeMaterial_;
            takeMaterial(freeMaterial_, bld, reservedMaterial_);
            for(const CostEntry& entry : BUILDING_COSTS[bld])
            {
                int available = 0;
                for(const GoodType good : entry.wares)
                    available += std::max(0, free[good]);
                if(available >= static_cast<int>(entry.amount))
                    continue;
                const GoodType good = entry.wares.size() == 1u ? entry.wares.front() : ChooseWare(entry.wares);
                if(good == GoodType::Nothing || HasChildFor(id, good) || IsInChain(id, good, BuildingType::Headquarters)
                   || HasProducerOrder(good))
                    continue;
                ResolveShortage(good, id);
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
        else if(order.building == BuildingType::Forester)
            pos = aijh_.FindForesterPosition(center, clearingWoodcutters_);
        else
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
            out << " " << wareName(good) << " (Bestand " << status.stock << ", Bilanz " << status.balance
                << ", angefragt " << status.openDemand << ");";
    }
    out << "\n  Überschuss:";
    for(const auto good : helpers::enumRange<GoodType>())
    {
        const WareStatus& status = wares_[good];
        if(status.state == SupplyState::Surplus && hasUse_[good])
            out << " " << wareName(good) << " (" << status.stock << ");";
    }
    out << "\n  Liste:\n";
    unsigned pos = 0;
    static const char* const stateNames[] = {"offen", "wartet", "blockiert", "platziert"};
    for(const Order* order : GetList())
    {
        out << "    " << ++pos << ". " << buildingName(order->building)
            << (order->kind == OrderKind::Needed ? " (benötigt" : " (expandierend");
        if(order->forWare != GoodType::Nothing)
            out << (order->consumer ? ", verarbeitet " : ", für ") << wareName(order->forWare);
        if(order->clearing)
            out << ", rodend";
        out << ") " << stateNames[static_cast<unsigned>(order->state)];
        if(!order->reason.empty())
            out << ": " << order->reason;
        out << "\n";
    }
    out << "  Zeit: " << static_cast<unsigned>(lastStepUs_) << " us (max " << static_cast<unsigned>(maxStepUs_)
        << " us)\n";
    return out.str();
}

} // namespace civai
