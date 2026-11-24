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
#include "civ/WareDistribution.h"
#include "figures/nofBuildingWorker.h"
#include "figures/nofCustomPlanter.h"
#include "figures/nofFarmer.h"
#include "figures/nofFarmhand.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "world/GameWorld.h"
#include "nodeObjs/noDeposit.h"
#include "nodeObjs/noFlag.h"
#include "nodeObjs/noGranite.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/GoodConsts.h"
#include "gameData/JobConsts.h"
#include "gameData/MilitaryConsts.h"
#include "gameData/ToolConsts.h"
#include "s25util/Log.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>

namespace civai {

namespace {
    using Clock = std::chrono::steady_clock;

    /// Start of the reason when the forecast of the helpers does not allow a building
    constexpr const char* HELPERS_MISSING = "keine Gehilfen";

    /// The report is written to the log every this many GFs (1 game minute)
    constexpr unsigned REPORT_GFS = 1200;

    std::string wareName(const GoodType good)
    {
        return good == GoodType::Nothing ? std::string("-") : WARE_NAMES[good];
    }

    /// A number of wares for the report: whole numbers without decimals, shares of entries with one
    std::string num(const double value)
    {
        const double rounded = std::round(value * 10.) / 10.;
        std::ostringstream out;
        // Less than 0.05 but not 0 (e.g. a shortage of 0.04): two decimals, it must not look like 0
        if(rounded == 0 && std::abs(value) >= 0.005)
        {
            out.setf(std::ios::fixed);
            out.precision(2);
            out << value;
        } else if(std::abs(rounded - std::round(rounded)) < 1e-9)
            out << std::llround(rounded);
        else
        {
            out.setf(std::ios::fixed);
            out.precision(1);
            out << rounded;
        }
        return out.str();
    }

    std::string wareNames(const std::vector<GoodType>& wares)
    {
        std::string result;
        for(const GoodType good : wares)
            result += (result.empty() ? "" : "/") + wareName(good);
        return result;
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

    std::string pointName(const MapPoint pt)
    {
        return "(" + std::to_string(pt.x) + "," + std::to_string(pt.y) + ")";
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

    /// Take the construction material of the building from `free` (any ware of an entry). Returns false if there is
    /// not enough. `taken` gets what was really there
    bool takeMaterial(AIJH::ConstructionMaterial& free, const BuildingType bld, AIJH::ConstructionMaterial& taken)
    {
        bool enough = true;
        const BuildingCosts costs = AIJH::getMaterialOrder(bld);
        for(const unsigned i : AIJH::getEntryOrder(costs))
        {
            const CostEntry& entry = costs[i];
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

    /// How good the tool is: work speed and output bonus first, then durability. 0 for wares which are no tools
    unsigned getToolQuality(const GoodType good)
    {
        const civ::ToolInfo* info = civ::getActiveContent().GetTool(good);
        if(!info)
            return 0;
        const int bonus = std::max(0, static_cast<int>(info->speed) + info->GetBestOutputBonus());
        return static_cast<unsigned>(bonus) * 1000u + std::min(info->durability, 999u);
    }

    std::vector<GoodType> sorted(std::vector<GoodType> wares)
    {
        std::sort(wares.begin(), wares.end());
        wares.erase(std::unique(wares.begin(), wares.end()), wares.end());
        return wares;
    }

    /// Sources harvested from the surface of the map which are used up (trees, stone piles, map objects of lua)
    bool isDeposit(const Source& source)
    {
        return source.kind == SourceKind::Trees || source.kind == SourceKind::StonePiles
               || source.kind == SourceKind::MapObject;
    }

    bool sameSource(const Source& lhs, const Source& rhs)
    {
        return lhs.kind == rhs.kind && lhs.index == rhs.index;
    }

    /// Resources on the map a harvester takes (Nutzer, 2026-11-18: the limit per amount holds for all of them)
    bool isMapResource(const Source& source)
    {
        return isDeposit(source) || source.kind == SourceKind::Ore || source.kind == SourceKind::Fish;
    }

    /// Units of the source at the node: a tree 1, a stone pile its stones, a map object of lua its wares, ore and fish
    /// their amount in the ground
    unsigned getSourceUnits(const GameWorldBase& world, const MapPoint pt, const Source& source)
    {
        switch(source.kind)
        {
            case SourceKind::Trees: return world.GetNO(pt)->GetType() == NodalObjectType::Tree ? 1u : 0u;
            case SourceKind::StonePiles:
            {
                const auto* granite = world.GetSpecObj<noGranite>(pt);
                return granite ? granite->GetSize() + 1u : 0u;
            }
            case SourceKind::MapObject:
            {
                const auto* deposit = world.GetSpecObj<noDeposit>(pt);
                return deposit && deposit->GetObjectIdx() == source.index ? deposit->GetSize() + 1u : 0u;
            }
            case SourceKind::Ore:
            {
                const Resource resources = world.GetNode(pt).resources;
                return resources.getType() == static_cast<ResourceType>(source.index) ? resources.getAmount() : 0u;
            }
            case SourceKind::Fish:
                return world.GetNode(pt).resources.has(ResourceType::Fish) ? world.GetNode(pt).resources.getAmount() :
                                                                             0u;
            case SourceKind::None:
            case SourceKind::Water:
            case SourceKind::Animals:
            case SourceKind::Land:
            case SourceKind::Plantable: return 0;
        }
        return 0;
    }

    const char* causeName(const OrderCause cause)
    {
        switch(cause)
        {
            case OrderCause::Shortage: return "Mangel";
            case OrderCause::Input: return "Eingang";
            case OrderCause::Territory: return "Gebiet";
            case OrderCause::Soldiers: return "für Soldaten";
            case OrderCause::Helpers: return "für Gehilfen";
            case OrderCause::Storage: return "Lager voll";
            case OrderCause::Renewal: return "erneuert";
            case OrderCause::Expansion: return "Expansion";
            case OrderCause::Deposits: return "nach Vorkommen";
        }
        return "-";
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
    forecast_ = std::make_unique<civ::EconomyForecast>(graph);
    const std::vector<Recipe>& recipes = graph.GetRecipes();
    for(unsigned i = 0; i < recipes.size(); ++i)
    {
        const Recipe& recipe = recipes[i];
        managed_[recipe.building] = true;
        for(const GoodType input : recipe.inputWares)
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
    for(const auto good : civ::getActiveContent().GetGoods())
    {
        if(IsTool(good))
            hasUse_[good] = true;
    }
    for(const GoodType good : SOLDIER_RECRUIT_WARES)
        hasUse_[good] = true;
    for(const GoodType good : getMilitaryWares())
        hasUse_[good] = true;
    for(unsigned i = 0; i < techTree.size(); ++i)
    {
        for(const civ::TechCost& cost : techTree.get(i).cost)
            hasUse_[cost.good] = true;
    }
    for(const GoodType medium : content.GetKnowledgeMedia())
        hasUse_[medium] = true;
    hasUse_[GoodType::Nothing] = false;
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        for(const CostEntry& entry : BUILDING_COSTS[bld])
        {
            for(const GoodType good : entry.wares)
                isMaterial_[good] = true;
        }
    }
    isMaterial_[GoodType::Nothing] = false;
    RegisterGroups();
}

void EconomyPlanner::RunGF(const unsigned gf)
{
    const unsigned offset = gf + player_.GetPlayerId() * 7;
    // While an order waits for the building site of an order placed for it: checked more often than the reaction time
    if(offset % reactionGFs_ != 0 && !(followUp_ && offset % FOLLOW_UP_GFS == 0))
        return;
    Step();
    if(gf / REPORT_GFS != lastReportMinute_)
    {
        lastReportMinute_ = gf / REPORT_GFS;
        LOG.write("ECONOMY player %u, GF %u:\n%s") % player_.GetPlayerId() % gf % GetReport();
    }
}

void EconomyPlanner::Step()
{
    const auto start = Clock::now();
    currentGF_ = player_.GetGameWorld().GetEvMgr().GetCurrentGF();
    snapshot_ = takeEconomySnapshot(player_);
    wayCache_.clear();
    chainCache_.clear();
    diagnosisCache_.clear();
    followUp_ = false;
    shortageNotes_.clear();
    inputNotes_.clear();
    soldierNotes_.clear();
    cancelledThisStep_ = false;
    // Ways without a place are tried again when the territory changed or something was researched
    situation_ = ComputeSituation();
    const Situation situation = situation_;
    helpers::erase_if(failedWays_, [&](const FailedWay& way) {
        return way.situation.land != situation.land || way.situation.researched != situation.researched;
    });
    UpdateWorkingBuildings();
    UpdatePlacedOrders();
    UpdatePlanned();
    RecomputeForecast();
    CreateOrders();
    freeMaterial_ = aijh_.GetConstruction().GetFreeConstructionMaterial();
    stepFreeMaterial_ = freeMaterial_;
    reservedMaterial_ = {};
    MarkSelfSolving();
    ProcessList();
    // Orders blocked in this step count for nothing from now on, orders no longer blocked count: the forecast of the
    // states the list has now (the report shows both)
    UpdatePlanned();
    RecomputeForecast();
    SendPlannedDemand();
    // Material needed orders wait for: no other building takes it (also not the military buildings of the old AI)
    aijh_.GetConstruction().SetPlannerReservation(reservedMaterial_);
    ++numSteps_;
    lastStepUs_ = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
    maxStepUs_ = std::max(maxStepUs_, lastStepUs_);
}

// ---------------------------------------------------------------------------------------------------------------------
// What the AI knows: groups of wares, their state and the state of the buildings

bool EconomyPlanner::IsTool(const GoodType good)
{
    return good != GoodType::Nothing && getToolOf(good).has_value();
}

unsigned EconomyPlanner::GetGroupIdx(std::vector<GoodType> wares, const bool demanded)
{
    wares = sorted(std::move(wares));
    helpers::erase(wares, GoodType::Nothing);
    RTTR_Assert(!wares.empty());
    const auto it = groupIndex_.find(wares);
    if(it != groupIndex_.end())
    {
        groups_[it->second].demanded |= demanded;
        return it->second;
    }
    WareGroup group;
    group.wares = wares;
    group.demanded = demanded;
    const auto idx = static_cast<unsigned>(groups_.size());
    for(const GoodType good : wares)
        groupsOf_[good].push_back(idx);
    groupIndex_.emplace(std::move(wares), idx);
    groups_.push_back(std::move(group));
    return idx;
}

void EconomyPlanner::RegisterGroups()
{
    // Every ware alone (shown in the log, used for its own consumers)
    for(const auto good : civ::getActiveContent().GetGoods())
    {
        if(good != GoodType::Nothing)
            singleGroup_[good] = GetGroupIdx({good}, false);
    }
    // What the consumers take: inputs (each one with all its wares), construction costs, tools of a slot and of a
    // kind, soldier equipment, the wares of the military buildings, research costs, knowledge media
    for(const Recipe& recipe : civAI_.GetGraph().GetRecipes())
    {
        for(const RecipeInput& input : recipe.inputs)
            GetGroupIdx(input.wares, true);
    }
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        for(const CostEntry& entry : BUILDING_COSTS[bld])
            GetGroupIdx(entry.wares, true);
    }
    const civ::Content& content = civ::getActiveContent();
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        for(const civ::BuildingTools::Slot& slot : content.GetBuildingTools(bld).slots)
        {
            if(!slot.wares.empty())
                GetGroupIdx(slot.wares, true);
        }
        // The tools of a slot giving work speed (CheckToolsAndHelpers)
        for(const std::vector<GoodType>& wares : EconomyGraph::GetWorkTools(content, bld))
            GetGroupIdx(wares, true);
    }
    helpers::EnumArray<std::vector<GoodType>, Tool> toolKinds;
    for(const auto good : civ::getActiveContent().GetGoods())
    {
        if(good == GoodType::Nothing)
            continue;
        if(const auto kind = getToolOf(good))
            toolKinds[*kind].push_back(good);
    }
    for(const auto tool : helpers::enumRange<Tool>())
    {
        toolKindGroup_[tool] = toolKinds[tool].empty() ? NO_GROUP : GetGroupIdx(toolKinds[tool], true);
    }
    for(const GoodType good : SOLDIER_RECRUIT_WARES)
        GetGroupIdx({good}, true);
    for(const GoodType good : getMilitaryWares())
        GetGroupIdx({good}, true);
    const civ::TechTree& techTree = player_.GetGameWorld().GetTechTree();
    for(unsigned i = 0; i < techTree.size(); ++i)
    {
        for(const civ::TechCost& cost : techTree.get(i).cost)
            GetGroupIdx({cost.good}, true);
    }
    for(const GoodType medium : content.GetKnowledgeMedia())
        GetGroupIdx({medium}, true);
}

const WareStatus& EconomyPlanner::GetWareStatus(const GoodType good) const
{
    // Unused ware slots have no group
    static const WareStatus unusedSlot{};
    if(!civ::getActiveContent().IsUsed(good))
        return unusedSlot;
    return groups_[singleGroup_[good]].status;
}

const WareGroup* EconomyPlanner::FindGroup(std::vector<GoodType> wares) const
{
    const auto it = groupIndex_.find(sorted(std::move(wares)));
    return it == groupIndex_.end() ? nullptr : &groups_[it->second];
}

bool EconomyPlanner::WaitsForResources(const Order& order)
{
    // Built as soon as the resources are there: construction material, a builder, a tool, helpers, soldiers. The
    // forecast must see what it asks for (otherwise nothing would make it)
    if(order.state != OrderState::Blocked)
        return false;
    switch(order.retry.kind)
    {
        case RetryCondition::Kind::Material:
        case RetryCondition::Kind::Builder:
        case RetryCondition::Kind::Helpers:
        case RetryCondition::Kind::Tool:
        case RetryCondition::Kind::Soldiers: return true;
        case RetryCondition::Kind::Now:
        case RetryCondition::Kind::Limit:
        case RetryCondition::Kind::Research:
        case RetryCondition::Kind::NoWay:
        case RetryCondition::Kind::NoPlace: return false;
    }
    return false;
}

bool EconomyPlanner::IsPlanned(const Order& order)
{
    // Needed and expanding orders ask for their construction material, tools and worker before they are placed. A
    // blocked order exists for no calculation (Nutzer, 2026-11-17): it cannot be built until the situation changes
    // (not researched, no way, no place, limit of its type, storage full, its input missing or kept for another
    // order). An order waiting for resources (material, builder, tool, helpers, soldiers) counts: shown as "wartet".
    // The state is the one of the last step (computed again at the end of a step)
    if(order.state == OrderState::Placed)
        return false;
    return order.state != OrderState::Blocked || WaitsForResources(order);
}

void EconomyPlanner::AddPlannedEntry(const OpenDemand& entry, const unsigned orderId)
{
    OpenDemand planned = entry;
    planned.wares = sorted(std::move(planned.wares));
    planned_.push_back(std::move(planned));
    plannedFor_.push_back(orderId);
}

void EconomyPlanner::AddPlanned(const BuildingType bld, const bool withCosts, const bool withTools,
                                const unsigned orderId)
{
    std::vector<OpenDemand> demand;
    if(withCosts)
    {
        for(const CostEntry& entry : BUILDING_COSTS[bld])
            demand.push_back({entry.wares, entry.amount, WareUse::Construction});
    }
    if(withTools && BuildingProperties::IsUsual(bld))
    {
        AddToolDemand(demand, bld);
        AddPlannedWorker(bld, orderId);
    }
    for(const OpenDemand& entry : demand)
        AddPlannedEntry(entry, orderId);
}

void EconomyPlanner::AddPlannedWorker(const BuildingType bld, const unsigned orderId)
{
    // Its worker: one of the job free in a warehouse, otherwise a helper
    if(!BuildingProperties::IsUsual(bld))
        return;
    if(const auto job = getJob(bld))
    {
        if(NeedsHelper(bld))
            plannedHelpers_.push_back(orderId);
        else
            ++workersPlanned_[*job];
    }
}

GoodType EconomyPlanner::GetRecruitTool(const BuildingType bld) const
{
    return civ::getRecruitTool(snapshot_, bld);
}

void EconomyPlanner::AddToolDemand(std::vector<OpenDemand>& demand, const BuildingType bld) const
{
    civ::addToolDemand(snapshot_, demand, bld);
}

void EconomyPlanner::UpdatePlanned()
{
    planned_.clear();
    plannedFor_.clear();
    plannedOrderIds_.clear();
    plannedHelpers_.clear();
    workersPlanned_ = {};
    // No input spaces (Nutzer, 2026-11-17): what a building will use counts as its consumption in the forecast (the
    // coming buildings by the measured value per building of their type, otherwise by the work time).
    // Building sites: the tools and the worker they will ask for. Their construction costs the game asks for (open)
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        for(unsigned i = snapshot_.buildings[bld].numSites; i > 0; --i)
            AddPlannedWorker(bld, 0);
    }
    // Building sites ordered whose commands are on their way and not on the map yet (also those of the old AI): their
    // construction costs, tools and worker
    const AIJH::AIConstruction& construction = aijh_.GetConstruction();
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        for(unsigned i = construction.GetNumPendingSites(bld); i > 0; --i)
            AddPlanned(bld, true, true, 0);
    }
    // Orders of the list not placed and not blocked, needed and expanding ones: their construction material, tools and
    // worker, in the order of the list. Their inputs count once they are placed (as the consumption of a coming
    // building)
    for(const Order* order : GetList())
    {
        if(!IsPlanned(*order))
            continue;
        AddPlanned(order->building, true, true, order->id);
        plannedOrderIds_.push_back(order->id);
    }
}

void EconomyPlanner::SendPlannedDemand()
{
    // "geplant" for the distribution of all wares, fully (Nutzer, 2026-11-21): the construction costs and tools of the
    // orders of the list not placed, in the order of the list. The game counts the tools of the building sites and the
    // tools in stock itself. Building sites ordered whose commands are on their way are on the map when this command is
    // executed (it is sent with them): then the game asks for their construction costs
    std::vector<civ::PlannedDemand> planned;
    for(unsigned i = 0; i < planned_.size(); ++i)
    {
        const Order* order = plannedFor_[i] != 0 ? Find(plannedFor_[i]) : nullptr;
        if(!order || order->state == OrderState::Placed)
            continue;
        civ::PlannedDemand demand;
        demand.wares = planned_[i].wares;
        helpers::erase(demand.wares, GoodType::Nothing);
        demand.amount = static_cast<uint16_t>(std::min<unsigned>(planned_[i].amount, UINT16_MAX));
        if(!demand.wares.empty() && demand.amount > 0)
            planned.push_back(std::move(demand));
    }
    if(planned != sentPlanned_ && aijh_.GetInterface().SetPlannedDemand(planned))
        sentPlanned_ = std::move(planned);
}

void EconomyPlanner::FinishStatus(WareStatus& status, const std::optional<double> evaluated) const
{
    status.forecast = status.free + status.forecastProduction - status.forecastConsumption;
    // The shares of the entries are no whole numbers, the production of a balance length neither: rounding errors are
    // no shortage (e.g. -30 + 29.99999). Below what the report shows (num: 0.005) it is 0
    if(std::abs(status.forecast) < 0.005)
        status.forecast = 0;
    const double threshold = std::max(static_cast<double>(MIN_STOCK), SURPLUS_FACTOR * status.consumptionPerMinute);
    if(status.forecast < 0)
        status.state = SupplyState::Shortage;
    else if(status.forecast - static_cast<int>(status.startStockLeft) > threshold)
        status.state = SupplyState::Surplus;
    else
        status.state = SupplyState::Normal;
    status.missingPerMinute =
      status.state == SupplyState::Shortage ? -status.forecast / static_cast<double>(BALANCE_MINUTES) : 0.;
    status.evaluated = evaluated.value_or(status.forecast);
    if(std::abs(status.evaluated) < 0.005)
        status.evaluated = 0;
    status.evaluatedShortage = status.evaluated < 0;
    status.evaluatedMissingPerMinute =
      status.evaluatedShortage ? -status.evaluated / static_cast<double>(BALANCE_MINUTES) : 0.;
}

void EconomyPlanner::RecomputeForecast()
{
    // Ways and chains depend on the forecast
    wayCache_.clear();
    chainCache_.clear();
    // What the game asks for and what is planned: every entry is a group a consumer takes (registered if it is new).
    // Tools asked for by workers and tool slots count for the tools of their kind
    const auto isWare = [](const GoodType good) { return good != GoodType::Nothing; };
    for(const std::vector<OpenDemand>* entries : {&snapshot_.openDemand, &planned_})
    {
        for(const OpenDemand& demand : *entries)
        {
            if(demand.purpose != WareUse::Tool && std::any_of(demand.wares.begin(), demand.wares.end(), isWare))
                GetGroupIdx(demand.wares, true);
        }
    }
    ComputeForecast();
    UpdateSoldierStatus();
    UpdateHelperStatus();
}

EconomyPlanner::Diagnosis EconomyPlanner::GetDiagnosis(const nobUsual& building, const Recipe& recipe,
                                                       std::vector<GoodType>& missingInputs)
{
    const auto cached = diagnosisCache_.find(building.GetPos());
    if(cached != diagnosisCache_.end())
    {
        missingInputs = cached->second.second;
        return cached->second.first;
    }
    const Diagnosis result = Diagnose(building, recipe, missingInputs);
    diagnosisCache_[building.GetPos()] = {result, missingInputs};
    return result;
}

double EconomyPlanner::GetMeasuredMinutes() const
{
    // The measured counts are extrapolated to the balance length. Less than a minute at the start of the game counts as
    // one minute: a burst in the first seconds (carriers for the first roads) is no rate per minute
    return std::max(1., player_.GetEconomyStatistics().GetCoveredMinutes(BALANCE_MINUTES, currentGF_));
}

void EconomyPlanner::ComputeForecast()
{
    // The calculation of the game (civ::EconomyForecast, also for the distribution of all wares) with what only the AI
    // knows: its building sites ordered whose commands are on their way and the orders of its list
    civ::EconomyForecast::Extra extra;
    const AIJH::AIConstruction& construction = aijh_.GetConstruction();
    for(const auto bld : helpers::enumRange<BuildingType>())
        extra.pendingSites[bld] = construction.GetNumPendingSites(bld);
    for(unsigned i = 0; i < planned_.size(); ++i)
    {
        const Order* order = plannedFor_[i] != 0 ? Find(plannedFor_[i]) : nullptr;
        extra.planned.push_back({planned_[i].wares, static_cast<civ::Amount>(planned_[i].amount) * civ::AMOUNT_UNIT,
                                 order && order->state != OrderState::Placed,
                                 planned_[i].purpose == WareUse::Construction});
    }
    extra.diagnose = [this](const nobUsual& building, const Recipe& recipe) {
        std::vector<GoodType> missing;
        return GetDiagnosis(building, recipe, missing);
    };
    extra.counts = [this](const BuildingType bld) { return managed_[bld]; };
    const auto toDouble = [](const civ::Amount amount) { return static_cast<double>(amount) / civ::AMOUNT_UNIT; };
    // The evaluation of shortages: all construction costs (open and planned) x BUILDING_MATERIAL_FACTOR. Then the
    // forecast with everything counted fully (the state of forecast_ afterwards)
    static_assert(BUILDING_MATERIAL_FACTOR >= 0. && BUILDING_MATERIAL_FACTOR <= 1.);
    const auto share = static_cast<uint32_t>(std::lround(BUILDING_MATERIAL_FACTOR * civ::CONSTRUCTION_SHARE_FULL));
    extra.constructionShare = share;
    forecast_->Compute(player_, currentGF_, snapshot_, extra);
    std::vector<double> evaluated(groups_.size());
    for(unsigned i = 0; i < groups_.size(); ++i)
        evaluated[i] = toDouble(forecast_->GetGroup(groups_[i].wares).forecast);
    if(share != civ::CONSTRUCTION_SHARE_FULL)
    {
        extra.constructionShare = civ::CONSTRUCTION_SHARE_FULL;
        forecast_->RecomputeStatuses(player_, snapshot_, extra);
    }
    for(unsigned i = 0; i < groups_.size(); ++i)
    {
        WareGroup& group = groups_[i];
        const civ::ForecastStatus core = forecast_->GetGroup(group.wares);
        WareStatus status;
        status.stock = core.stock;
        status.balance = core.balance;
        status.consumptionPerMinute = toDouble(core.consumptionPerMinute);
        status.openDemand = toDouble(core.openDemand);
        status.inTransit = core.inTransit;
        status.onTheWay = toDouble(core.onTheWay);
        status.planned = toDouble(core.planned);
        status.plannedByOrders = toDouble(core.plannedByOrders);
        status.free = toDouble(core.free);
        status.missing = toDouble(core.missing);
        status.startStockLeft = core.startStockLeft;
        status.forecastProduction = toDouble(core.production);
        status.forecastConsumption = toDouble(core.consumption);
        FinishStatus(status, evaluated[i]);
        group.status = status;
    }
}

void EconomyPlanner::UpdateSoldierStatus()
{
    // Soldiers count like a ware: the stock in the warehouses, made by recruiting, used when they leave for a military
    // building, asked for by the military buildings
    const EconomyStatistics& stats = player_.GetEconomyStatistics();
    const double coveredMinutes = GetMeasuredMinutes();
    WareStatus status;
    for(const Job job : SOLDIER_JOBS)
        status.stock += snapshot_.inWarehousesPeople[job];
    const unsigned created = stats.GetSoldiersCreated(BALANCE_MINUTES, currentGF_);
    const unsigned used = stats.GetSoldiersUsed(BALANCE_MINUTES, currentGF_);
    status.balance = static_cast<int>(created) - static_cast<int>(used);
    status.consumptionPerMinute = used / coveredMinutes;
    status.forecastProduction = created / coveredMinutes * BALANCE_MINUTES;
    status.forecastConsumption = used / coveredMinutes * BALANCE_MINUTES;
    status.openDemand = snapshot_.openSoldiers;
    status.free = static_cast<double>(status.stock) - status.openDemand;
    FinishStatus(status);
    soldiers_ = status;
}

void EconomyPlanner::UpdateHelperStatus()
{
    // Helpers like a ware: in the warehouses, requested by the workers no warehouse could send yet and by the coming
    // buildings and orders whose worker must be recruited, created by the warehouses, used for soldiers and carriers
    // (workers are requested before)
    const EconomyStatistics& stats = player_.GetEconomyStatistics();
    const double coveredMinutes = GetMeasuredMinutes();
    WareStatus status;
    status.stock = snapshot_.inWarehousesPeople[Job::Helper];
    for(const auto job : helpers::enumRange<Job>())
        status.openDemand += snapshot_.jobsWanted[job];
    for(const unsigned orderId : plannedHelpers_)
    {
        ++status.planned;
        if(orderId != 0)
        {
            const Order* order = Find(orderId);
            if(order && order->state != OrderState::Placed)
                ++status.plannedByOrders;
        }
    }
    const unsigned created = stats.GetHelpersCreated(BALANCE_MINUTES, currentGF_);
    const unsigned used = stats.GetHelpersUsed(BALANCE_MINUTES, currentGF_);
    const unsigned forJobs = stats.GetHelpersForJobs(BALANCE_MINUTES, currentGF_);
    const unsigned usedOther = used > forJobs ? used - forJobs : 0u;
    status.balance = static_cast<int>(created) - static_cast<int>(used);
    status.consumptionPerMinute = usedOther / coveredMinutes;
    status.forecastProduction = created / coveredMinutes * BALANCE_MINUTES;
    status.forecastConsumption = usedOther / coveredMinutes * BALANCE_MINUTES;
    status.free = static_cast<double>(status.stock) - status.openDemand - status.planned;
    FinishStatus(status);
    helpers_ = status;
}

void EconomyPlanner::UpdateWorkingBuildings()
{
    // Finished and working buildings are measured by the game (civ::EconomyForecast, finish and worker GF of the
    // buildings)
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
    // Made woodcutters for good: until the mark is removed in the game or the woodcutter is gone
    helpers::erase_if(unclearingPending_,
                      [&](const MapPoint pt) { return !player_.IsClearingWoodcutter(pt) || !isWoodcutter(pt); });
    // Sites placed by the planner: forgotten when the building or site is gone
    for(auto it = placements_.begin(); it != placements_.end();)
    {
        const auto* building = world.GetSpecObj<noBaseBuilding>(it->first);
        const bool there = building && building->GetPlayer() == player_.GetPlayerId()
                           && building->GetBuildingType() == it->second.building;
        it = (there || construction.IsSitePending(it->first)) ? std::next(it) : placements_.erase(it);
    }
}

double EconomyPlanner::ProducedPerBuilding(const BuildingType bld, const GoodType good) const
{
    return static_cast<double>(forecast_->ProducedPerBuilding(bld, good)) / civ::AMOUNT_UNIT;
}

double EconomyPlanner::UsedPerBuilding(const BuildingType bld, const GoodType good) const
{
    return static_cast<double>(forecast_->UsedPerBuilding(bld, good)) / civ::AMOUNT_UNIT;
}

double EconomyPlanner::OutputPerWorkingMinute(const BuildingType bld) const
{
    return static_cast<double>(forecast_->OutputPerWorkingMinute(bld)) / civ::AMOUNT_UNIT;
}

namespace {
    /// Does the status suffice for one more consumer which uses `perMinute` of it?
    bool statusSuffices(const WareStatus& status, const double perMinute)
    {
        // A surplus lies there because consumers are missing: it suffices for one more. Otherwise the forecast must
        // cover what the new building uses in one balance length by its work time
        if(status.state == SupplyState::Surplus)
            return true;
        return status.forecast - perMinute * EconomyPlanner::BALANCE_MINUTES >= 0;
    }
} // namespace

bool EconomyPlanner::InputSuffices(const GoodType input, const Recipe& consumer) const
{
    // What one more building of the type uses: the measured value per building, as it counts in the forecast once the
    // building is placed
    return statusSuffices(GetWareStatus(input), UsedPerBuilding(consumer.building, input));
}

bool EconomyPlanner::IsToolMaker(const Recipe& recipe)
{
    return !recipe.outputs.empty() && std::all_of(recipe.outputs.begin(), recipe.outputs.end(), IsTool);
}

bool EconomyPlanner::IsMultiProduct(const Recipe& recipe)
{
    // Several products chosen by the distribution of all wares (not knowledge media: regulator "Wissen")
    return recipe.outputs.size() > 1u && civ::isInDistribution(recipe.outputs.front());
}

double EconomyPlanner::GetMultiProductUnmet(const Recipe& recipe, const std::vector<GoodType>& groupWares,
                                            const double groupForecast, const bool evaluated)
{
    // Regel A (Nutzer, 2026-11-21): the capacity of a building with several products counts once over all of them.
    // What its buildings must make of each product in the balance length: what they make of it now, plus what is
    // missing of it (forecast below 0), minus what is left over (forecast above 0). A group short of wares counts only
    // for the type chosen for it (`groupWares`, `groupForecast`). What all of them can make: the buildings that can
    // work, the stopped ones (Nutzer, 2026-11-22: with their full work) and the coming ones, by what one makes per
    // minute while it works. Returns what they cannot make, per minute
    const double length = BALANCE_MINUTES;
    const EconomyStatistics& stats = player_.GetEconomyStatistics();
    const double measured = GetMeasuredMinutes();
    const AIJH::AIConstruction& construction = aijh_.GetConstruction();
    const double coming =
      snapshot_.buildings[recipe.building].numSites + construction.GetNumPendingSites(recipe.building);
    // The stopped minutes count in the forecast with what one makes per minute (as the measured production)
    const double stopped =
      static_cast<double>(forecast_->GetMeasure(recipe.building).stoppedGFs) / EconomyStatistics::GetGFsPerMinute();
    double needed = 0;
    for(const GoodType good : recipe.outputs)
    {
        const double made = stats.GetProducedBy(recipe.building, good, BALANCE_MINUTES, currentGF_) / measured * length
                            + stopped / measured * length * ProducedPerBuilding(recipe.building, good)
                            + coming * ProducedPerBuilding(recipe.building, good) * length;
        const WareStatus& status = GetWareStatus(good);
        double forecast = evaluated ? status.evaluated : status.forecast;
        if(helpers::contains(groupWares, good))
            forecast = std::min(forecast, groupForecast);
        needed += std::max(0., made - forecast);
    }
    double num = coming;
    const unsigned windowGFs = BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    for(const nobUsual* building : player_.GetBuildingRegister().GetBuildings(recipe.building))
    {
        std::vector<GoodType> missing;
        const Diagnosis diagnosis = GetDiagnosis(*building, recipe, missing);
        const unsigned age = currentGF_ > building->GetFinishedGF() ? currentGF_ - building->GetFinishedGF() : 0u;
        if(age < windowGFs || diagnosis == Diagnosis::Working || diagnosis == Diagnosis::Stopped
           || diagnosis == Diagnosis::BadTool || diagnosis == Diagnosis::MissingTool
           || diagnosis == Diagnosis::SourceFar || diagnosis == Diagnosis::FewFields)
            num += 1;
    }
    const double capacity = num * OutputPerWorkingMinute(recipe.building) * length;
    return std::max(0., needed - capacity) / length;
}

bool EconomyPlanner::InputSlotSuffices(const RecipeInput& input, const Recipe& recipe, const GoodType given) const
{
    if(helpers::contains(input.wares, given))
        return true;
    if(input.wares.size() == 1u)
        return InputSuffices(input.wares.front(), recipe);
    // Several wares: the group of all of them
    const WareGroup* group = FindGroup(input.wares);
    double used = 0;
    for(const GoodType good : input.wares)
        used += UsedPerBuilding(recipe.building, good);
    return group && statusSuffices(group->status, used);
}

bool EconomyPlanner::InputsSuffice(const Recipe& recipe, const GoodType given) const
{
    return std::all_of(recipe.inputs.begin(), recipe.inputs.end(),
                       [&](const RecipeInput& input) { return InputSlotSuffices(input, recipe, given); });
}

unsigned EconomyPlanner::GetBuildingLimit() const
{
    const civ::TechTree& techTree = player_.GetGameWorld().GetTechTree();
    const unsigned land = player_.GetStatisticCurrentValue(StatisticType::Country);
    const unsigned numAreas = land / techTree.GetAIBuildingLimitAreaSize();
    return techTree.GetAIBuildingLimitBase() + techTree.GetAIBuildingLimitPerArea() * numAreas;
}

unsigned EconomyPlanner::GetNumBuildings(const BuildingType bld) const
{
    return static_cast<unsigned>(player_.GetBuildingRegister().GetBuildings(bld).size())
           + snapshot_.buildings[bld].numSites + aijh_.GetConstruction().GetNumPendingSites(bld);
}

Situation EconomyPlanner::GetSituation() const
{
    return situation_;
}

Situation EconomyPlanner::ComputeSituation() const
{
    Situation result;
    result.land = player_.GetStatisticCurrentValue(StatisticType::Country);
    result.researched = player_.GetResearch().GetNumResearched();
    const BuildingRegister& buildings = player_.GetBuildingRegister();
    result.buildings = static_cast<unsigned>(buildings.GetBuildingSites().size())
                       + static_cast<unsigned>(buildings.GetMilitaryBuildings().size());
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(BuildingProperties::IsUsual(bld))
            result.buildings += static_cast<unsigned>(buildings.GetBuildings(bld).size());
    }
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
// Ways to a ware and the choice between them

namespace {
    /// Order of the ways: not blocked, more producers can be placed, better tool, fewest new buildings, uses a surplus,
    /// highest measured output per building, lowest construction costs
    template<class T_Way>
    bool isBetterWay(const T_Way& lhs, const T_Way& rhs)
    {
        if(lhs.blocked != rhs.blocked)
            return !lhs.blocked;
        if(lhs.canAdd != rhs.canAdd)
            return lhs.canAdd;
        if(lhs.quality != rhs.quality)
            return lhs.quality > rhs.quality;
        if(lhs.numNew != rhs.numNew)
            return lhs.numNew < rhs.numNew;
        if(lhs.usesSurplus != rhs.usesSurplus)
            return lhs.usesSurplus;
        if(lhs.output != rhs.output)
            return lhs.output > rhs.output;
        return lhs.cost < rhs.cost;
    }
} // namespace

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
    result.output =
      IsMultiProduct(recipe) ? OutputPerWorkingMinute(recipe.building) : ProducedPerBuilding(recipe.building, good);
    result.quality = getToolQuality(good);

    const bool failed = helpers::contains_if(
      failedWays_, [&](const FailedWay& way) { return way.ware == good && way.building == recipe.building; });
    const bool sourceThere =
      recipe.source.kind == SourceKind::None || civAI_.GetMap().GetTotalOwned().GetAmount(recipe.source) > 0;
    // A loop (the ware is needed to make one of its own inputs) is no way
    if(helpers::contains(checking_, good))
    {
        ++numLoops_;
        result.reason = wareName(good) + " im Kreis";
        return result; // Depends on the wares being checked: not cached
    }
    if(failed && GetNumBuildings(recipe.building) > 0)
    {
        // No place for another one, but the existing ones make the ware: a way, only no new building
        result.blocked = false;
        result.canAdd = false;
        result.numNew = 0;
        result.reason = buildingName(recipe.building) + ": kein Bauplatz";
        wayCache_[key] = result;
        return result;
    }
    if(failed || !sourceThere || !player_.IsBuildingEnabled(recipe.building))
    {
        result.reason = failed       ? buildingName(recipe.building) + ": kein Bauplatz" :
                        !sourceThere ? "nicht im Gebiet: " + civAI_.GetGraph().GetSourceName(recipe.source) :
                                       buildingName(recipe.building) + " nicht erforscht";
        // The source is on the map, only outside the territory: a military building can get it
        if(!failed && !sourceThere && player_.IsBuildingEnabled(recipe.building)
           && civAI_.GetMap().GetTotal().GetAmount(recipe.source) > 0)
        {
            result.outside = true;
            result.outsideBuilding = recipe.building;
            result.outsideWare = good;
            result.outsideSource = recipe.source;
        }
        wayCache_[key] = result;
        return result;
    }
    result.blocked = false;
    result.numNew = 1;
    const unsigned loopsBefore = numLoops_;
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
                if(!ways.empty() && ways.front().outside)
                {
                    result.outside = true;
                    result.outsideBuilding = ways.front().outsideBuilding;
                    result.outsideWare = ways.front().outsideWare;
                    result.outsideSource = ways.front().outsideSource;
                }
            }
            result.missingInputs.push_back(input);
            return {false, 0u};
        }
        return {true, ways.front().numNew};
    };
    for(const GoodType input : recipe.inputWares)
    {
        if(input != given && GetWareStatus(input).state == SupplyState::Surplus)
            result.usesSurplus = true;
    }
    for(const RecipeInput& input : recipe.inputs)
    {
        if(input.wares.size() == 1u)
        {
            const auto [possible, numNew] = inputCost(input.wares.front());
            if(!possible)
                result.blocked = true;
            result.numNew += numNew;
        } else if(!InputSlotSuffices(input, recipe, given))
        {
            // Several wares: the cheapest
            const std::size_t numMissingBefore = result.missingInputs.size();
            const std::string reasonBefore = inputReason;
            unsigned best = std::numeric_limits<unsigned>::max();
            for(const GoodType good : input.wares)
            {
                const auto [possible, numNew] = inputCost(good);
                if(possible)
                    best = std::min(best, numNew);
            }
            if(best == std::numeric_limits<unsigned>::max())
                result.blocked = true;
            else
            {
                result.numNew += best;
                result.missingInputs.resize(numMissingBefore);
                inputReason = reasonBefore;
            }
        }
    }
    if(result.blocked)
        result.reason = inputReason;
    checking_.pop_back();
    if(numLoops_ == loopsBefore)
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
    std::stable_sort(result.begin(), result.end(), isBetterWay<WayCheck>);
    return result;
}

GoodType EconomyPlanner::ChooseWare(const std::vector<GoodType>& wares)
{
    // The ware of the list whose best way is best (better tools first)
    GoodType best = GoodType::Nothing;
    WayCheck bestWay;
    for(const GoodType good : wares)
    {
        const std::vector<WayCheck> ways = RankProducers(good, false, 0);
        if(ways.empty() || ways.front().blocked)
            continue;
        if(best == GoodType::Nothing || isBetterWay(ways.front(), bestWay))
        {
            best = good;
            bestWay = ways.front();
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// The existing buildings: why they do not work, what they will add

EconomyPlanner::Diagnosis EconomyPlanner::Diagnose(const nobUsual& building, const Recipe& recipe,
                                                   std::vector<GoodType>& missingInputs) const
{
    // The rule of the game (civ::diagnose), the sources cached for SOURCE_CHECK_GFS
    return civ::diagnose(player_, building, recipe, missingInputs, [&]() {
        const SourceInfo& info = GetSourceInfo(building.GetPos(), recipe.building, recipe, true);
        civ::SourceInfo result;
        result.inReach = info.inReach;
        result.nearby = info.nearby;
        result.fields = info.fields;
        return result;
    });
}

const EconomyPlanner::SourceInfo& EconomyPlanner::GetSourceInfo(const MapPoint pt, const BuildingType bld,
                                                                const Recipe& recipe, const bool placed) const
{
    SourceInfo& info = sourceCache_[std::make_tuple(pt.x, pt.y, bld)];
    if(info.gf != 0 && info.gf + SOURCE_CHECK_GFS > currentGF_)
        return info;
    const GameWorldBase& world = player_.GetGameWorld();
    info.gf = std::max(1u, currentGF_);
    info.inReach = recipe.reach > 0 ? MapProfile::CountSourceNodes(world, pt, recipe.reach, recipe.source) : 0u;
    info.nearby =
      recipe.reach >= 2 ? MapProfile::CountSourceNodes(world, pt, recipe.reach / 2, recipe.source) : info.inReach;
    info.fields = NeedsFields(bld) ? CountFieldNodes(bld, pt, placed) : 0u;
    return info;
}

bool EconomyPlanner::NeedsFields(const BuildingType bld) const
{
    return civ::needsFields(bld);
}

unsigned EconomyPlanner::CountFieldNodes(const BuildingType bld, const MapPoint pt, const bool placed) const
{
    return civ::countFieldNodes(player_.GetGameWorld(), bld, pt, placed);
}

bool EconomyPlanner::CanWorkWhenBuilt(const BuildingType bld, std::string* reason) const
{
    const std::string why = CheckToolsAndHelpers(nullptr, bld, false);
    if(reason)
        *reason = why;
    return why.empty();
}

unsigned EconomyPlanner::CountWaitingBuildings(const BuildingType bld)
{
    const Recipe* recipe = FindRecipe(bld);
    if(!recipe)
        return 0;
    unsigned result = 0;
    for(const nobUsual* building : player_.GetBuildingRegister().GetBuildings(bld))
    {
        std::vector<GoodType> missing;
        const Diagnosis diagnosis = GetDiagnosis(*building, *recipe, missing);
        if(diagnosis == Diagnosis::NoInput || diagnosis == Diagnosis::NoTool || diagnosis == Diagnosis::NoWorker)
            ++result;
    }
    return result;
}

bool EconomyPlanner::MakesWare(const BuildingType bld, const GoodType good) const
{
    const Recipe* recipe = FindRecipe(bld);
    return recipe && helpers::contains(recipe->outputs, good);
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

Order* EconomyPlanner::AddOrder(Order order)
{
    // Military buildings: needed or expanding like the order they are for (set by AddTerritoryOrder)
    if(order.cause != OrderCause::Territory)
        order.kind = (order.cause == OrderCause::Expansion || order.cause == OrderCause::Deposits) ?
                       OrderKind::Expanding :
                       OrderKind::Needed;
    // Orders made to solve a problem of an expanding order are expanding too
    if(order.parent != 0)
    {
        const Order* parent = Find(order.parent);
        if(parent && parent->kind == OrderKind::Expanding)
            order.kind = OrderKind::Expanding;
    }
    const auto isSame = [&](const Order& other) {
        return other.state != OrderState::Placed && other.building == order.building && other.forWare == order.forWare;
    };
    auto replaced = orders_.end();
    if(order.kind == OrderKind::Expanding)
    {
        // Not if the same order exists (needed ones stand above it)
        if(order.forWare != GoodType::Nothing && helpers::contains_if(orders_, isSame))
            return nullptr;
    } else if(order.forWare != GoodType::Nothing)
    {
        // A needed order replaces the same expanding one (it would stand lower)
        replaced = std::find_if(orders_.begin(), orders_.end(), [&](const Order& other) {
            return isSame(other) && other.kind == OrderKind::Expanding;
        });
    }
    // Not more than MAX_ORDERS_PER_TYPE of a building type at the same time
    if(BuildingProperties::IsUsual(order.building)
       && CountOrders(order.building) - (replaced != orders_.end() ? 1u : 0u) >= MAX_ORDERS_PER_TYPE)
        return nullptr;
    if(replaced != orders_.end())
        orders_.erase(replaced);
    order.id = nextId_++;
    order.sequence = nextSequence_++;
    order.reason = "neu, im nächsten Schritt";
    orders_.push_back(std::move(order));
    return &orders_.back();
}

unsigned EconomyPlanner::CountOrders(const BuildingType bld) const
{
    return static_cast<unsigned>(
      std::count_if(orders_.begin(), orders_.end(), [bld](const Order& order) { return order.building == bld; }));
}

void EconomyPlanner::Remove(const unsigned id)
{
    // With the orders created for it (unless they are placed already: their buildings are watched until finished)
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

bool EconomyPlanner::HasChildFor(const unsigned parent, const GoodType good) const
{
    return helpers::contains_if(orders_,
                                [&](const Order& order) { return order.parent == parent && order.forWare == good; });
}

bool EconomyPlanner::HasPlacedChild(const unsigned id) const
{
    std::vector<unsigned> todo{id};
    for(unsigned i = 0; i < todo.size(); ++i)
    {
        for(const Order& order : orders_)
        {
            if(order.parent != todo[i])
                continue;
            if(order.state == OrderState::Placed)
                return true;
            todo.push_back(order.id);
        }
    }
    return false;
}

bool EconomyPlanner::IsInChain(unsigned orderId, const GoodType good, const BuildingType bld) const
{
    while(const Order* order = Find(orderId))
    {
        if((good != GoodType::Nothing && order->forWare == good) || order->building == bld)
            return true;
        orderId = order->parent;
    }
    return false;
}

bool EconomyPlanner::IsClearingWoodcutter(const MapPoint pt) const
{
    // Made one for good (command not executed yet): not clearing anymore
    if(helpers::contains(unclearingPending_, pt))
        return false;
    return player_.IsClearingWoodcutter(pt) || helpers::contains(clearingPending_, pt);
}

double EconomyPlanner::GetUrgency(const Order& root) const
{
    // The forecast of what the order is for: the lowest (most missing) first
    const auto forecastOf = [this](const GoodType good) {
        return good == GoodType::Nothing ? std::numeric_limits<double>::max() / 2 : GetWareStatus(good).forecast;
    };
    switch(root.cause)
    {
        case OrderCause::Shortage: return groups_[root.group].status.forecast;
        case OrderCause::Soldiers: return soldiers_.forecast;
        case OrderCause::Renewal:
        {
            const Recipe* harvester = FindRecipe(root.forBuilding);
            return harvester && !harvester->outputs.empty() ? forecastOf(harvester->outputs.front()) :
                                                              std::numeric_limits<double>::max() / 2;
        }
        // Without them no other building works (workers, tools, space for the wares): before every shortage
        case OrderCause::Helpers:
        case OrderCause::Storage: return -1e12;
        // Orders for another one which is gone (it was placed meanwhile): as soon as possible
        case OrderCause::Input: return -1e11;
        case OrderCause::Territory:
            return root.kind == OrderKind::Expanding ? GetExpandingScore(root) : forecastOf(root.forWare);
        case OrderCause::Expansion:
        case OrderCause::Deposits: return GetExpandingScore(root);
    }
    return 0;
}

double EconomyPlanner::GetExpandingScore(const Order& order) const
{
    const double output = order.forWare == GoodType::Nothing ? 0. : GetWareStatus(order.forWare).forecast;
    const Recipe* recipe = FindRecipe(order.building);
    if(!recipe || recipe->inputs.empty())
        return output;
    // The forecast of its inputs: per input, of its group if it has several wares
    double input = 0;
    for(const RecipeInput& slot : recipe->inputs)
    {
        if(slot.wares.size() == 1u)
            input += GetWareStatus(slot.wares.front()).forecast;
        else if(const WareGroup* group = FindGroup(slot.wares))
            input += group->status.forecast;
    }
    input /= recipe->inputs.size();
    // Weak product: up. Much of its inputs: up (it uses them). Few of its inputs: down (it would take them away)
    return (output - input) / 2;
}

EconomyPlanner::SortKey EconomyPlanner::GetSortKey(const Order& order) const
{
    // Level 0 in the game data means "not part of the basic economy": after all levels
    constexpr unsigned notBasic = std::numeric_limits<unsigned>::max();
    const civ::TechTree& techTree = player_.GetGameWorld().GetTechTree();
    SortKey key{2u, notBasic, 0., 0u, 0u, order.sequence};
    const Order* root = &order;
    for(const Order* current = &order; current; current = current->parent ? Find(current->parent) : nullptr)
    {
        // 0: needed and self-solving, 1: needed, 2: expanding. An order for another one is never below it
        const unsigned group = current->kind == OrderKind::Expanding ? 2u : current->selfSolving ? 0u : 1u;
        key.group = std::min(key.group, group);
        const unsigned level = techTree.GetAIBasicEconomyLevel(current->building);
        if(level > 0)
            key.level = std::min(key.level, level);
        if(current != &order)
            ++key.depth;
        root = current;
    }
    key.urgency = GetUrgency(*root);
    key.rootSequence = root->sequence;
    // Expanding orders: only by the economy (no levels of the basic economy, no other rule)
    if(order.kind == OrderKind::Expanding)
    {
        key.level = notBasic;
        key.rootSequence = 0;
    }
    // The solution for other orders too stands above each of them
    if(sortKeyDepth_ < 8)
    {
        ++sortKeyDepth_;
        for(const unsigned otherId : order.alsoFor)
        {
            const Order* other = Find(otherId);
            if(!other)
                continue;
            SortKey otherKey = GetSortKey(*other);
            otherKey.depth += 1;
            otherKey.sequence = order.sequence;
            if(IsAbove(otherKey, key))
                key = otherKey;
        }
        --sortKeyDepth_;
    }
    return key;
}

bool EconomyPlanner::IsAbove(const SortKey& lhs, const SortKey& rhs)
{
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
    return lhs.sequence < rhs.sequence;
}

std::vector<const Order*> EconomyPlanner::GetList() const
{
    // 1. Needed orders solving their own problem (they get the material they lack before everybody else), then the
    //    other needed orders, then the expanding ones. An expanding order never solves a problem.
    // 2. Within these the levels of the basic economy (ai.lua), then all other buildings.
    // 3. Needed: by the forecast of what they are for (the lowest first), then in order of creation.
    // Expanding orders: only by the economy, the weakest production first (GetExpandingScore).
    // The orders created for an order come directly above it
    std::vector<std::pair<SortKey, const Order*>> entries;
    entries.reserve(orders_.size());
    for(const Order& order : orders_)
        entries.emplace_back(GetSortKey(order), &order);
    std::stable_sort(entries.begin(), entries.end(),
                     [](const auto& lhs, const auto& rhs) { return IsAbove(lhs.first, rhs.first); });
    std::vector<const Order*> result;
    result.reserve(entries.size());
    for(const auto& entry : entries)
        result.push_back(entry.second);
    return result;
}

bool EconomyPlanner::IsChainBlocked(unsigned id) const
{
    // A military building for an order without a place is built because that one is blocked
    const Order* first = Find(id);
    if(first && first->cause == OrderCause::Territory)
        return first->state == OrderState::Blocked
               && (first->retry.kind == RetryCondition::Kind::Research
                   || first->retry.kind == RetryCondition::Kind::NoPlace);
    for(const Order* order = first; order; order = order->parent ? Find(order->parent) : nullptr)
    {
        if(order->state != OrderState::Blocked)
            continue;
        const RetryCondition::Kind kind = order->retry.kind;
        if(kind == RetryCondition::Kind::Research || kind == RetryCondition::Kind::NoWay
           || kind == RetryCondition::Kind::NoPlace)
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

const Recipe* EconomyPlanner::FindRecipe(const BuildingType bld) const
{
    for(const Recipe& recipe : civAI_.GetGraph().GetRecipes())
    {
        if(recipe.building == bld)
            return &recipe;
    }
    return nullptr;
}

bool EconomyPlanner::IsRenewable(const Source& source) const
{
    return helpers::contains_if(civAI_.GetGraph().GetRenewers(),
                                [&](const Renewer& renewer) { return sameSource(renewer.source, source); });
}

const Renewer* EconomyPlanner::FindRenewer(const Source& source) const
{
    for(const Renewer& renewer : civAI_.GetGraph().GetRenewers())
    {
        if(sameSource(renewer.source, source) && managed_[renewer.building]
           && player_.IsBuildingEnabled(renewer.building))
            return &renewer;
    }
    return nullptr;
}

unsigned EconomyPlanner::GetMinSourceInReach(const Order& order, const Recipe& recipe) const
{
    if(recipe.source.kind == SourceKind::Trees && order.clearing)
        return CLEARING_MIN_TREES;
    if(IsRenewable(recipe.source))
        return RENEW_BELOW;
    if(isDeposit(recipe.source))
        return MIN_DEPOSITS_IN_REACH;
    return 1;
}

BuildingType EconomyPlanner::ChooseWarehouse() const
{
    // The cheapest warehouse the player may build which needs no harbor place and stores people (helpers) and all
    // wares (storage full). Warehouses of lua store only some wares and no people: not for these two
    const civ::Content& content = civ::getActiveContent();
    BuildingType result = BuildingType::Headquarters;
    unsigned bestCost = std::numeric_limits<unsigned>::max();
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!BuildingProperties::IsWareHouse(bld) || BUILDING_COSTS[bld].empty()
           || BUILDING_SIZE[bld] == BuildingQuality::Harbor || !player_.IsBuildingEnabled(bld))
            continue;
        if(content.IsCustomWarehouse(bld))
            continue;
        if(getTotalCost(bld) < bestCost)
        {
            bestCost = getTotalCost(bld);
            result = bld;
        }
    }
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
            // destroyed. Then the order is done and a new one is made if it is still needed
            if(!aijh_.GetConstruction().IsSitePending(order.placedAt))
                done.push_back(order.id);
            continue;
        }
        if(building->GetGOT() == GO_Type::Buildingsite)
            continue;
        // Finished: from now on it counts in the statistic (its productivity after one balance length). A renewer
        // stays until it worked one balance length: the source grows slowly, so the next one comes after that
        if(order.cause != OrderCause::Renewal)
        {
            done.push_back(order.id);
            continue;
        }
        const auto* renewer = player_.GetGameWorld().GetSpecObj<nobUsual>(order.placedAt);
        if(!renewer || renewer->GetWorkerSinceGF() == nobUsual::NO_GF)
            continue;
        order.startupEndGF = renewer->GetWorkerSinceGF() + startupGFs;
        if(currentGF_ >= order.startupEndGF)
            done.push_back(order.id);
    }
    for(const unsigned id : done)
    {
        if(Order* order = Find(id))
        {
            // Placed orders have no unplaced children (they were done before), remove only this one. The renewer of a
            // harvester renews around it from now on (it belongs to no other order)
            const unsigned parent = order->parent;
            helpers::erase_if(orders_, [id](const Order& other) { return other.id == id; });
            for(Order& other : orders_)
            {
                if(other.parent == id)
                    other.parent = other.cause == OrderCause::Renewal ? 0u : parent;
            }
        }
    }
}

bool EconomyPlanner::IsCauseValid(const Order& order)
{
    switch(order.cause)
    {
        // The evaluation of shortages decides (BUILDING_MATERIAL_FACTOR), as for the orders it makes
        case OrderCause::Shortage: return groups_[order.group].status.evaluatedShortage;
        case OrderCause::Input:
        {
            const Order* parent = Find(order.parent);
            if(!parent || parent->state == OrderState::Placed)
                return false;
            const Recipe* recipe = FindRecipe(parent->building);
            if(!recipe || !helpers::contains(recipe->inputWares, order.forWare))
                return false;
            // The input with this ware does not suffice (several wares: none of them suffices, their group)
            return std::any_of(recipe->inputs.begin(), recipe->inputs.end(), [&](const RecipeInput& input) {
                return helpers::contains(input.wares, order.forWare) && !InputSlotSuffices(input, *recipe);
            });
        }
        case OrderCause::Territory: return IsTerritoryNeeded(order);
        case OrderCause::Soldiers: return soldiers_.state == SupplyState::Shortage;
        // The forecast of the helpers leaves none for one more building
        case OrderCause::Helpers: return helpers_.forecast < 1;
        case OrderCause::Storage: return IsStorageFull();
        case OrderCause::Renewal:
        {
            // For a harvester without a place (it is placed beside this one): as long as that order is there
            if(order.parent != 0 && !order.near.isValid())
                return Find(order.parent) != nullptr;
            const auto* harvester = player_.GetGameWorld().GetSpecObj<nobUsual>(order.near);
            const Recipe* recipe = FindRecipe(order.forBuilding);
            if(!harvester || harvester->GetPlayer() != player_.GetPlayerId()
               || harvester->GetBuildingType() != order.forBuilding || !recipe || IsClearingWoodcutter(order.near))
                return false;
            return MapProfile::CountSourceNodes(player_.GetGameWorld(), order.near, recipe->reach, recipe->source)
                   < RENEW_BELOW;
        }
        case OrderCause::Expansion:
            return order.forWare != GoodType::Nothing && GetWareStatus(order.forWare).state == SupplyState::Normal;
        case OrderCause::Deposits:
        {
            const Recipe* recipe = FindRecipe(order.building);
            if(!recipe)
                return false;
            const unsigned minAmount =
              recipe->source.kind == SourceKind::Trees ? CLEARING_MIN_TREES : MIN_DEPOSITS_IN_REACH;
            return civAI_.GetMap().GetTotalOwned().GetAmount(recipe->source) >= minAmount;
        }
    }
    return false;
}

void EconomyPlanner::CreateOrders()
{
    // 1. Orders whose cause is gone. Not those of which a building was placed already (the chain was started)
    std::vector<unsigned> obsolete;
    for(const Order& order : orders_)
    {
        if(order.state != OrderState::Placed && !IsCauseValid(order) && !HasPlacedChild(order.id))
            obsolete.push_back(order.id);
    }
    for(const unsigned id : obsolete)
    {
        if(Find(id))
            Remove(id);
    }

    // 2. Problems: needed orders. Shortages of the groups consumers take (the best ware of the group), soldiers, tools,
    //    helpers. Smaller groups first: the orders for them count for the larger groups containing their wares
    std::vector<unsigned> shortGroups;
    for(unsigned i = 0; i < groups_.size(); ++i)
    {
        const WareGroup& group = groups_[i];
        // The evaluation of shortages: construction costs x BUILDING_MATERIAL_FACTOR
        if(group.demanded && group.status.evaluatedShortage)
            shortGroups.push_back(i);
    }
    std::stable_sort(shortGroups.begin(), shortGroups.end(), [this](const unsigned lhs, const unsigned rhs) {
        return groups_[lhs].wares.size() < groups_[rhs].wares.size();
    });
    for(const unsigned i : shortGroups)
    {
        const WareGroup& group = groups_[i];
        GoodType good = group.wares.size() == 1u ? group.wares.front() : GoodType::Nothing;
        if(good == GoodType::Nothing)
        {
            // Any ware of the group: the one chosen before as long as it can be made (no orders for two of them),
            // otherwise the best one
            const auto before = std::find_if(orders_.begin(), orders_.end(), [i](const Order& order) {
                return order.cause == OrderCause::Shortage && order.group == i;
            });
            if(before != orders_.end())
            {
                const std::vector<WayCheck> ways = RankProducers(before->forWare, false, 0);
                if(!ways.empty() && !ways.front().blocked)
                    good = before->forWare;
            }
        }
        if(good == GoodType::Nothing)
            good = ChooseWare(group.wares);
        if(good == GoodType::Nothing)
            good = group.wares.front(); // No way to any of them: the first one shows why
        Need need;
        need.cause = OrderCause::Shortage;
        need.group = i;
        need.perMinute = groups_[i].status.evaluatedMissingPerMinute;
        ResolveShortage(good, need);
    }
    ResolveSoldiers();
    // The forecast of the helpers leaves none for one more building: a warehouse (it creates helpers)
    if(helpers_.forecast < 1)
        AddHelpersOrder(0);
    // Surpluses: an expanding consumer (a consumer whose product is short is chosen by the shortage)
    for(unsigned i = 0; i < groups_.size(); ++i)
    {
        if(groups_[i].demanded && groups_[i].status.state == SupplyState::Surplus)
            ResolveSurplus(i);
    }
    // 3. No problem: expanding orders with the buildings of ai.lua, harvesters of deposits
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(good != GoodType::Nothing && hasUse_[good] && GetWareStatus(good).state == SupplyState::Normal)
            AddExpandingOrder(good);
    }
    AddDepositOrders();
    AddRenewalOrders();
    UpgradeClearingWoodcutters();
    AddStorageOrder();
    AddExpandingTerritoryOrder();
}

void EconomyPlanner::ResolveShortage(const GoodType good, const Need& need)
{
    if(helpers::contains(resolving_, good))
        return; // Already working on it further up (loop)
    const auto isOwn = [&](const Order& order) {
        if(order.cause != need.cause || order.forWare != good)
            return false;
        switch(need.cause)
        {
            case OrderCause::Shortage: return order.group == need.group;
            case OrderCause::Soldiers: return true;
            default: return order.parent == need.parent;
        }
    };
    const std::vector<WayCheck> ways = RankProducers(good, false, 0);
    if(ways.empty() || ways.front().blocked)
    {
        // No way now. An order for another one: that one cannot be built. Otherwise one blocked order shows why the
        // ware is missing (tried again every step: the ways are checked anyway)
        std::string reason = "kein Weg zu " + wareName(good);
        if(!ways.empty())
            reason += " (" + ways.front().reason + ")";
        // The source is only outside the territory: a military building toward it (needed for a needed need,
        // expanding for an expanding one)
        if(!ways.empty() && ways.front().outside)
        {
            const Order* forOrder = need.parent ? Find(need.parent) : nullptr;
            const bool expanding = need.expanding || (forOrder && forOrder->kind == OrderKind::Expanding);
            AddTerritoryOrder(TerritoryNeed::Source, expanding ? OrderKind::Expanding : OrderKind::Needed, 0,
                              ways.front().outsideWare, ways.front().outsideBuilding, ways.front().outsideSource);
        }
        Order* blocked = need.parent ? Find(need.parent) : nullptr;
        if(blocked)
        {
            const Recipe* recipe = FindRecipe(blocked->building);
            if(recipe && helpers::contains(recipe->inputWares, good)
               && !helpers::contains(blocked->inputsMissing, good))
                blocked->inputsMissing.push_back(good);
        } else if(!need.parent && !ways.empty())
        {
            const BuildingType wayBld = civAI_.GetGraph().GetRecipes()[ways.front().recipe].building;
            auto own = std::find_if(orders_.begin(), orders_.end(), [&](const Order& order) {
                return isOwn(order) && order.state != OrderState::Placed;
            });
            // The same building for the same ware ordered for another need (another group or cause) is the solution
            // for this one too: one blocked order shows why
            if(own == orders_.end())
            {
                own = std::find_if(orders_.begin(), orders_.end(), [&](const Order& order) {
                    return order.state != OrderState::Placed && order.kind == OrderKind::Needed
                           && order.building == wayBld && order.forWare == good;
                });
            }
            if(own != orders_.end())
                blocked = &*own;
            else
            {
                Order order;
                order.building = wayBld;
                order.cause = need.cause;
                order.forWare = good;
                order.group = need.group;
                blocked = AddOrder(std::move(order));
            }
            if(blocked)
            {
                // The inputs it cannot get are kept for it (orders below must not use them)
                blocked->inputsMissing = ways.front().missingInputs;
            }
        }
        if(!blocked)
            return;
        if(blocked->state == OrderState::Blocked && blocked->retry.kind != RetryCondition::Kind::NoWay
           && blocked->retry.kind != RetryCondition::Kind::Now)
        {
            // Blocked by a rule (e.g. construction material) whose solution has no way now: both reasons
            if(blocked->reason.find(reason) == std::string::npos)
                blocked->reason += ", " + reason;
        } else
        {
            blocked->state = OrderState::Blocked;
            blocked->reason = std::move(reason);
            blocked->retry = {RetryCondition::Kind::NoWay, GetSituation()};
        }
        return;
    }
    // A way exists: orders of this need blocked for want of a way are made again with the best producer
    std::vector<unsigned> noWay;
    for(const Order& order : orders_)
    {
        if(isOwn(order) && order.state == OrderState::Blocked && order.retry.kind == RetryCondition::Kind::NoWay
           && !HasPlacedChild(order.id))
            noWay.push_back(order.id);
    }
    for(const unsigned id : noWay)
        Remove(id);

    // Why a shortage (of the statistic) gets no order in this step, for the report
    const auto setNote = [&](const std::string& note) {
        if(need.parent != 0)
        {
            // An input of an order: shown at the order (Nutzer, 2026-11-18)
            const std::string text = wareName(good) + ": " + note;
            std::vector<std::string>& notes = inputNotes_[need.parent];
            if(!helpers::contains(notes, text))
                notes.push_back(text);
            return;
        }
        if(need.cause == OrderCause::Shortage)
            shortageNotes_[need.group] = wareName(good) + ": " + note;
        else if(need.cause == OrderCause::Soldiers)
            soldierNotes_[good] = note;
    };
    // The best way where another producer can be placed. None: the existing producers make it, more do not fit now
    const auto way = std::find_if(ways.begin(), ways.end(), [](const WayCheck& w) { return !w.blocked && w.canAdd; });
    if(way == ways.end())
    {
        const BuildingType bld = civAI_.GetGraph().GetRecipes()[ways.front().recipe].building;
        setNote(ways.front().reason + ", es kommt keiner dazu (" + std::to_string(GetNumBuildings(bld)) + " stehen)");
        return;
    }
    const Recipe& recipe = civAI_.GetGraph().GetRecipes()[way->recipe];
    // What a building of the type makes per minute: the measured value per building (tool makers: while they work)
    const auto outputOf = [this](const BuildingType bld, const GoodType ware) {
        const Recipe* producer = FindRecipe(bld);
        return producer && IsMultiProduct(*producer) ? OutputPerWorkingMinute(bld) : ProducedPerBuilding(bld, ware);
    };
    // The wares of the need: the group of a shortage (any of them will do), otherwise the ware
    std::vector<GoodType> wares{good};
    if(need.cause == OrderCause::Shortage && need.group < groups_.size())
        wares = groups_[need.group].wares;
    // Orders of this need, and the other orders not placed whose building makes one of the wares (needed ones; for an
    // expanding order also the expanding ones). They count with what they make: one solution for several needs. A
    // blocked order of another need counts for nothing (Nutzer, 2026-11-17). The own ones count also when blocked:
    // they are the orders of this need (otherwise the same order would be added in every step)
    std::vector<const Order*> own;
    std::vector<std::pair<const Order*, GoodType>> others;
    for(const Order& order : orders_)
    {
        if(order.state == OrderState::Placed || (order.kind == OrderKind::Expanding && !need.expanding))
            continue;
        if(isOwn(order))
        {
            own.push_back(&order);
            continue;
        }
        if(!IsPlanned(order))
            continue;
        for(const GoodType ware : wares)
        {
            if(MakesWare(order.building, ware))
            {
                others.emplace_back(&order, ware);
                break;
            }
        }
    }
    std::ostringstream coveredBy;
    coveredBy.setf(std::ios::fixed);
    coveredBy.precision(1);
    // A building with several products (Regel A, Nutzer 2026-11-21): what its buildings cannot make of all their
    // products together. The distribution of all wares moves their work to the missing ware first
    const bool multiProduct = IsMultiProduct(recipe);
    double needPerMinute = need.perMinute;
    if(multiProduct)
    {
        const bool forGroup = need.cause == OrderCause::Shortage && need.group < groups_.size();
        needPerMinute = GetMultiProductUnmet(
          recipe, forGroup ? groups_[need.group].wares : std::vector<GoodType>{good},
          forGroup ? groups_[need.group].status.evaluated : GetWareStatus(good).forecast, forGroup);
    }
    double remaining = needPerMinute;
    for(const auto& [other, ware] : others)
        remaining -= outputOf(other->building, ware);
    const double perProducer = outputOf(recipe.building, good);
    // As many orders as the forecast needs to reach 0 (the forecast counts the coming and standing buildings already),
    // at least one for a need no other order is for
    unsigned wanted = 0;
    if(remaining > 1e-9)
        wanted = perProducer > 0 ? static_cast<unsigned>(std::ceil(remaining / perProducer - 1e-9)) : 1u;
    if(needPerMinute > 0 && wanted == 0 && own.empty() && others.empty())
        wanted = 1;
    coveredBy << "gedeckt: fehlen " << need.perMinute << "/min, andere Aufträge " << others.size() << " (je "
              << perProducer << "/min)";
    // An order for another one whose need the others cover: they are its solution too and stand above it
    if(need.parent != 0 && wanted == 0 && own.empty())
    {
        for(const auto& [other, ware] : others)
        {
            Order* solution = Find(other->id);
            if(solution && solution->id != need.parent && !helpers::contains(solution->alsoFor, need.parent)
               && solution->parent != need.parent)
            {
                solution->alsoFor.push_back(need.parent);
                break;
            }
        }
    }
    // Not more orders waiting than builders could build them (natural limit: more would only wait and plan their
    // construction costs). Counted over all needs: the needed orders waiting to be built (not placed, not blocked) and
    // the builders free for them. One order per need comes anyway (it shows what is missing)
    const unsigned numWaiting = static_cast<unsigned>(std::count_if(
      orders_.begin(), orders_.end(), [&](const Order& o) { return o.kind == OrderKind::Needed && IsPlanned(o); }));
    const unsigned builders = GetAvailableBuilders();
    const unsigned room =
      std::max(builders > numWaiting ? builders - numWaiting : 0u, (own.empty() && others.empty()) ? 1u : 0u);
    wanted = std::min(wanted, static_cast<unsigned>(own.size()) + room);
    // Not more than MAX_ORDERS_PER_TYPE of the building type at the same time (the own ones of the type included). A
    // needed order replaces the same expanding one (AddOrder): those do not count
    const unsigned ownOfType = static_cast<unsigned>(
      std::count_if(own.begin(), own.end(), [&](const Order* o) { return o->building == recipe.building; }));
    const unsigned replaceable =
      need.expanding ? 0u : static_cast<unsigned>(std::count_if(orders_.begin(), orders_.end(), [&](const Order& o) {
          return o.state != OrderState::Placed && o.kind == OrderKind::Expanding && o.building == recipe.building
                 && o.forWare == good;
      }));
    const unsigned ofType = CountOrders(recipe.building) - std::min(replaceable, CountOrders(recipe.building));
    const unsigned maxOwn =
      static_cast<unsigned>(own.size()) - ownOfType
      + (MAX_ORDERS_PER_TYPE > ofType - ownOfType ? MAX_ORDERS_PER_TYPE - (ofType - ownOfType) : 0u);
    if(wanted > maxOwn)
    {
        wanted = maxOwn;
        coveredBy.str("höchstens " + std::to_string(MAX_ORDERS_PER_TYPE) + " Aufträge je Gebäudetyp ("
                      + buildingName(recipe.building) + ")");
    }
    // Its inputs do not suffice, or the forecast of its tools or helpers does not allow one more: one waits (orders for
    // the inputs, makers for the tools and a warehouse for helpers come for it), more once they are there. A building
    // of the type standing still for an input, a tool or a worker counts as the one waiting
    std::string cannotWork;
    if(!InputsSuffice(recipe) || !CanWorkWhenBuilt(recipe.building, &cannotWork))
    {
        const unsigned waiting =
          static_cast<unsigned>(std::count_if(others.begin(), others.end(),
                                              [&](const auto& o) { return o.first->building == recipe.building; }))
          + CountWaitingBuildings(recipe.building);
        if(waiting > 0 && wanted > 0)
        {
            coveredBy.str("ein " + buildingName(recipe.building) + " wartet schon ("
                          + (cannotWork.empty() ? std::string("Eingang fehlt") : cannotWork) + ")");
        }
        wanted = std::min(wanted, waiting > 0 ? 0u : 1u);
    }
    if(wanted == 0 && own.empty())
        setNote(coveredBy.str());
    if(own.size() > wanted)
    {
        // Fewer are needed now: the newest ones go (not those with placed buildings for them)
        std::vector<const Order*> removable;
        for(const Order* order : own)
        {
            if(!HasPlacedChild(order->id))
                removable.push_back(order);
        }
        std::sort(removable.begin(), removable.end(),
                  [](const Order* lhs, const Order* rhs) { return lhs->sequence > rhs->sequence; });
        std::vector<unsigned> toRemove;
        for(unsigned i = 0; i < removable.size() && own.size() - toRemove.size() > wanted; ++i)
            toRemove.push_back(removable[i]->id);
        for(const unsigned id : toRemove)
            Remove(id);
        return;
    }
    for(size_t i = own.size(); i < wanted; ++i)
    {
        Order order;
        order.building = recipe.building;
        order.cause = need.cause;
        order.forWare = good;
        order.group = need.group;
        order.parent = need.parent;
        if(!AddOrder(std::move(order)))
            break;
    }
}

void EconomyPlanner::ResolveSurplus(const unsigned groupIdx)
{
    // A surplus for the soldiers (recruiting) is used once soldiers are short (ResolveSoldiers)
    const std::vector<GoodType> wares = groups_[groupIdx].wares;
    if(std::all_of(wares.begin(), wares.end(),
                   [](const GoodType good) { return helpers::contains(SOLDIER_RECRUIT_WARES, good); }))
        return;
    // The consumer whose product is normal (a short product gets its producer from the shortage, preferring those
    // using a surplus; a product in surplus needs no more). Then like the producers: not blocked, fewest new buildings,
    // productivity, costs. Expanding: only buildings of ai.lua
    const civ::TechTree& techTree = player_.GetGameWorld().GetTechTree();
    const std::vector<Recipe>& recipes = civAI_.GetGraph().GetRecipes();
    struct Candidate
    {
        GoodType uses, product;
        WayCheck way;
    };
    std::vector<Candidate> candidates;
    for(const GoodType good : wares)
    {
        for(const unsigned idx : consumers_[good])
        {
            const Recipe& recipe = recipes[idx];
            if(!managed_[recipe.building] || !player_.IsBuildingEnabled(recipe.building)
               || !techTree.IsAIExpanding(recipe.building))
                continue;
            for(const GoodType output : recipe.outputs)
            {
                if(!hasUse_[output] || IsTool(output) || GetWareStatus(output).state != SupplyState::Normal)
                    continue;
                // The surplus is there for the consumer: only its other inputs must be made
                const WayCheck way = CheckWay(idx, output, 0, good);
                if(!way.blocked)
                    candidates.push_back({good, output, way});
            }
        }
    }
    if(candidates.empty())
        return;
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& lhs, const Candidate& rhs) { return isBetterWay(lhs.way, rhs.way); });
    const Candidate& best = candidates.front();
    const BuildingType bld = recipes[best.way.recipe].building;
    if(helpers::contains_if(orders_, [bld](const Order& order) {
           return order.kind == OrderKind::Expanding && order.building == bld && order.state != OrderState::Placed;
       }))
        return; // At most one expanding order per building type
    Order order;
    order.building = bld;
    order.cause = OrderCause::Expansion;
    order.forWare = best.product;
    order.usesWare = best.uses;
    AddOrder(std::move(order));
}

void EconomyPlanner::ResolveSoldiers()
{
    // Soldiers are short: each soldier takes one of each recruiting ware. What is missing of them per minute (beyond
    // what is produced and lies free) is ordered
    for(const GoodType good : SOLDIER_RECRUIT_WARES)
    {
        const WareStatus& status = GetWareStatus(good);
        double missing = 0;
        if(soldiers_.state == SupplyState::Shortage)
        {
            // What the forecast of the ware leaves
            missing = soldiers_.missingPerMinute - std::max(0., status.forecast) / BALANCE_MINUTES;
        }
        Need need;
        need.cause = OrderCause::Soldiers;
        need.perMinute = std::max(0., missing);
        if(missing > 0 || helpers::contains_if(orders_, [good](const Order& order) {
               return order.cause == OrderCause::Soldiers && order.forWare == good;
           }))
            ResolveShortage(good, need);
    }
    // The helpers to recruit them
    if(soldiers_.state == SupplyState::Shortage && helpers_.forecast < 1)
        AddHelpersOrder(0);
}

void EconomyPlanner::AddExpandingOrder(const GoodType good)
{
    const std::vector<WayCheck> ways = RankProducers(good, true, 0);
    if(ways.empty() || ways.front().blocked)
        return;
    const Recipe& recipe = civAI_.GetGraph().GetRecipes()[ways.front().recipe];
    const BuildingType bld = recipe.building;
    // Harvesters of deposits expand where the deposits are (AddDepositOrders), not by the stock
    if(isDeposit(recipe.source))
        return;
    // At most one expanding order per building type in the list (e.g. one metalworks, not one per tool)
    if(helpers::contains_if(orders_, [bld](const Order& order) {
           return order.kind == OrderKind::Expanding && order.building == bld && order.state != OrderState::Placed;
       }))
        return;
    // Buildings of the type wait for an input, a tool or a worker: one more would wait too
    if(CountWaitingBuildings(bld) > 0)
        return;
    // Several products (Regel A): no more of them while its buildings can make what all its products need
    if(IsMultiProduct(recipe) && GetMultiProductUnmet(recipe, {}, 0, false) <= 0)
        return;
    Order order;
    order.building = bld;
    order.cause = OrderCause::Expansion;
    order.forWare = good;
    AddOrder(std::move(order));
}

void EconomyPlanner::AddDepositOrders()
{
    // Harvesters of deposits (trees, stone piles, map objects of lua) clear the land: one order each as long as there
    // are enough deposits in the territory (a place with enough in reach is searched when its turn comes)
    const ResourceProfile owned = civAI_.GetMap().GetTotalOwned();
    std::vector<BuildingType> added;
    for(const Recipe& recipe : civAI_.GetGraph().GetRecipes())
    {
        const BuildingType bld = recipe.building;
        if(!isDeposit(recipe.source) || !managed_[bld] || !player_.IsBuildingEnabled(bld)
           || helpers::contains(added, bld) || recipe.outputs.empty() || !hasUse_[recipe.outputs.front()])
            continue;
        added.push_back(bld);
        const unsigned minAmount = recipe.source.kind == SourceKind::Trees ? CLEARING_MIN_TREES : MIN_DEPOSITS_IN_REACH;
        if(owned.GetAmount(recipe.source) < minAmount)
            continue;
        if(helpers::contains_if(orders_, [bld](const Order& order) {
               return order.building == bld && order.cause == OrderCause::Deposits && order.state != OrderState::Placed;
           }))
            continue;
        Order order;
        order.building = bld;
        order.cause = OrderCause::Deposits;
        order.forWare = recipe.outputs.front();
        // Trees (a renewable source): this one clears, renewers are for the harvesters for good
        order.clearing = recipe.source.kind == SourceKind::Trees && IsRenewable(recipe.source);
        AddOrder(std::move(order));
    }
}

void EconomyPlanner::AddHelpersOrder(const unsigned parent)
{
    const BuildingType bld = ChooseWarehouse();
    if(bld == BuildingType::Headquarters || IsInChain(parent, GoodType::Nothing, bld)
       || helpers::contains_if(orders_, [](const Order& order) { return order.cause == OrderCause::Helpers; }))
        return;
    Order order;
    order.building = bld;
    order.cause = OrderCause::Helpers;
    order.parent = parent;
    AddOrder(std::move(order));
}

bool EconomyPlanner::IsStorageFull() const
{
    const unsigned percent = player_.GetGameWorld().GetTechTree().GetAIStorageFullPercent();
    return snapshot_.storageCapacity > 0 && snapshot_.storageLoad * 100u >= snapshot_.storageCapacity * percent;
}

void EconomyPlanner::AddStorageOrder()
{
    if(!IsStorageFull())
        return; // An order that is there is removed with its cause
    const BuildingType bld = ChooseWarehouse();
    if(bld == BuildingType::Headquarters)
        return;
    // One at a time: the next one when this one is finished and the storage is still full
    if(helpers::contains_if(orders_, [](const Order& order) { return order.cause == OrderCause::Storage; })
       || snapshot_.buildings[bld].numSites > 0 || aijh_.GetConstruction().GetNumPendingSites(bld) > 0)
        return;
    Order order;
    order.building = bld;
    order.cause = OrderCause::Storage;
    AddOrder(std::move(order));
}

void EconomyPlanner::AddRenewalOrders()
{
    // A harvester for good (not clearing) gets a renewer when fewer than RENEW_BELOW of its source are in its reach.
    // One order per harvester at a time: the next one after the renewer ordered for it worked one balance length
    const EconomyGraph& graph = civAI_.GetGraph();
    for(const Renewer& renewer : graph.GetRenewers())
    {
        if(!managed_[renewer.building] || !player_.IsBuildingEnabled(renewer.building))
            continue;
        // The forecast of its tools or helpers does not allow one more: one order waits for them. Otherwise the limit
        // per building type holds
        const unsigned maxWaiting = CanWorkWhenBuilt(renewer.building) ? MAX_ORDERS_PER_TYPE : 1u;
        const auto isWaiting = [&](const Order& order) {
            return order.cause == OrderCause::Renewal && order.building == renewer.building
                   && order.state != OrderState::Placed;
        };
        std::vector<BuildingType> harvesters;
        for(const Recipe& recipe : graph.GetRecipes())
        {
            if(!sameSource(recipe.source, renewer.source) || recipe.reach == 0
               || helpers::contains(harvesters, recipe.building))
                continue;
            harvesters.push_back(recipe.building);
            for(const nobUsual* building : player_.GetBuildingRegister().GetBuildings(recipe.building))
            {
                const MapPoint pos = building->GetPos();
                if(IsClearingWoodcutter(pos) || helpers::contains_if(orders_, [&](const Order& order) {
                       return order.cause == OrderCause::Renewal && order.near == pos;
                   }))
                    continue;
                if(static_cast<unsigned>(std::count_if(orders_.begin(), orders_.end(), isWaiting)) >= maxWaiting)
                    continue;
                const unsigned amount =
                  MapProfile::CountSourceNodes(player_.GetGameWorld(), pos, recipe.reach, recipe.source);
                if(amount >= RENEW_BELOW)
                    continue;
                Order order;
                order.building = renewer.building;
                order.cause = OrderCause::Renewal;
                order.forBuilding = recipe.building;
                order.near = pos;
                AddOrder(std::move(order));
            }
        }
    }
}

void EconomyPlanner::UpgradeClearingWoodcutters()
{
    // Nutzer, 2026-11-18: a clearing woodcutter with fewer than CLEARING_UPGRADE_BELOW trees in its reach becomes one
    // for good when a needed forester is in the list, and gets that forester. The woodcutter the forester was for waits
    // for the next one. A forester for a woodcutter without a place (it would be placed beside it): the upgraded
    // woodcutter is that woodcutter, the order goes. Of several clearing woodcutters the nearest to the woodcutter the
    // forester was for (without one: the nearest to a warehouse)
    const Recipe* woodcutter = FindRecipe(BuildingType::Woodcutter);
    const Renewer* renewer = woodcutter ? FindRenewer(woodcutter->source) : nullptr;
    if(!woodcutter || woodcutter->reach == 0 || !renewer)
        return;
    const GameWorldBase& world = player_.GetGameWorld();
    std::vector<MapPoint> candidates;
    for(const nobUsual* building : player_.GetBuildingRegister().GetBuildings(BuildingType::Woodcutter))
    {
        const MapPoint pos = building->GetPos();
        if(IsClearingWoodcutter(pos)
           && MapProfile::CountSourceNodes(world, pos, woodcutter->reach, woodcutter->source) < CLEARING_UPGRADE_BELOW)
            candidates.push_back(pos);
    }
    if(candidates.empty())
        return;
    // Needed foresters not placed yet (not those waiting for research: no forester can come)
    std::vector<unsigned> foresters;
    for(const Order& order : orders_)
    {
        if(order.cause == OrderCause::Renewal && order.building == renewer->building && order.kind == OrderKind::Needed
           && order.forBuilding == BuildingType::Woodcutter && order.state != OrderState::Placed
           && !(order.state == OrderState::Blocked && order.retry.kind == RetryCondition::Kind::Research))
            foresters.push_back(order.id);
    }
    const auto distanceToWarehouse = [&](const MapPoint pt) {
        unsigned result = std::numeric_limits<unsigned>::max();
        for(const nobBaseWarehouse* warehouse : player_.GetBuildingRegister().GetStorehouses())
            result = std::min(result, world.CalcDistance(pt, warehouse->GetPos()));
        return result;
    };
    for(const unsigned id : foresters)
    {
        if(candidates.empty())
            break;
        Order* forester = Find(id);
        if(!forester)
            continue;
        const MapPoint origin = forester->near;
        const auto best =
          std::min_element(candidates.begin(), candidates.end(), [&](const MapPoint lhs, const MapPoint rhs) {
              if(origin.isValid())
                  return world.CalcDistance(lhs, origin) < world.CalcDistance(rhs, origin);
              return distanceToWarehouse(lhs) < distanceToWarehouse(rhs);
          });
        const MapPoint pos = *best;
        candidates.erase(best);
        aijh_.GetInterface().SetClearingWoodcutter(pos, false);
        unclearingPending_.push_back(pos);
        LOG.write("ECONOMY player %u: rodender Holzfäller %s wird dauerhaft, bekommt den Förster%s\n")
          % player_.GetPlayerId() % pointName(pos)
          % (origin.isValid() ? " für den Holzfäller " + pointName(origin) :
                                std::string(" für einen Holzfäller ohne Bauplatz (Auftrag entfällt)"));
        // The forester is for it now (renews around it). A woodcutter order without a place it was for: not needed
        const unsigned harvesterId = forester->parent;
        forester->near = pos;
        forester->parent = 0;
        forester->state = OrderState::Open;
        forester->retry = RetryCondition{};
        forester->reason = "für den Holzfäller " + pointName(pos) + " (rodend, jetzt dauerhaft)";
        const Order* harvester = harvesterId ? Find(harvesterId) : nullptr;
        if(harvester && harvester->building == BuildingType::Woodcutter && harvester->state != OrderState::Placed)
            Remove(harvesterId);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Working on the list

void EconomyPlanner::ProcessList()
{
    // Links to orders which do not lack the ware anymore (or are gone, placed) are removed. They are made again while
    // the orders are worked on
    for(Order& order : orders_)
    {
        helpers::erase_if(order.alsoFor, [&](const unsigned otherId) {
            const Order* other = Find(otherId);
            if(!other || other->state == OrderState::Placed)
                return true;
            // Placed for the other one: it is its input until it is finished
            return order.state != OrderState::Placed && !helpers::contains(other->inputsMissing, order.forWare);
        });
    }
    unsigned searchBudget = SEARCHES_PER_STEP;
    keptInputs_.clear();
    toolsClaimed_.assign(groups_.size(), 0u);
    helpersClaimed_ = 0;
    // The orders made while the list is worked on (e.g. the producer of a missing input) are worked on in the same
    // step: every order has its state (blocked or not) when the forecast is computed again at the end of the step
    std::vector<unsigned> done;
    for(unsigned pass = 0; pass < MAX_LIST_PASSES; ++pass)
    {
        std::vector<unsigned> ids;
        for(const Order* order : GetList())
        {
            if(!helpers::contains(done, order->id))
                ids.push_back(order->id);
        }
        if(ids.empty())
            break;
        for(const unsigned id : ids)
        {
            ProcessOrder(id, searchBudget);
            done.push_back(id);
        }
    }
}

void EconomyPlanner::MoveToEnd(Order& order, const std::string& reason, const RetryCondition& retry)
{
    order.state = OrderState::Blocked;
    order.reason = reason;
    order.retry = retry;
    order.sequence = nextSequence_++;
}

void EconomyPlanner::KeepMaterial(const Order& order)
{
    takeMaterial(freeMaterial_, order.building, reservedMaterial_);
}

void EconomyPlanner::ProcessOrder(const unsigned id, unsigned& searchBudget)
{
    Order* order = Find(id);
    if(!order || order->state == OrderState::Placed)
        return;
    // Created for an order that cannot be built until the situation changes: of no use (e.g. the well for a brewery
    // without grain)
    if(order->parent != 0 && order->cause != OrderCause::Territory && IsChainBlocked(order->parent))
    {
        Remove(id);
        return;
    }
    const bool needed = order->kind == OrderKind::Needed;
    const BuildingType bld = order->building;
    // The inputs a needed order lacks are kept for it: no order below it gets a building using them (expanding orders
    // have no rights)
    if(needed)
    {
        for(const GoodType input : order->inputsMissing)
            keptInputs_.emplace(input, id);
    }
    // Storage full: only needed orders
    if(!needed && IsStorageFull())
    {
        MoveToEnd(*order,
                  "Lager voll (" + std::to_string(snapshot_.storageLoad) + " von "
                    + std::to_string(snapshot_.storageCapacity) + ")",
                  RetryCondition{});
        return;
    }
    // Blocked: tried again when its condition is met (material there, situation changed ...). A self-solving order and
    // an order without a way are checked every step
    if(order->state == OrderState::Blocked && !order->selfSolving && !IsRetryMet(order->retry, *order))
    {
        // An order works on its unmet rule in every step: a needed one keeps the material that is there, and the
        // solution is ordered again if it is gone (e.g. no place for the sawmill ordered for it). A needed one keeps
        // its tools and helper too
        const RetryCondition::Kind kind = order->retry.kind;
        if(needed)
            ClaimToolsAndHelpers(*order);
        if(kind == RetryCondition::Kind::Material || kind == RetryCondition::Kind::Builder
           || kind == RetryCondition::Kind::Helpers)
        {
            if(kind == RetryCondition::Kind::Material)
            {
                std::vector<GoodType> missing;
                order->reason = DescribeMissingMaterial(bld, missing);
            }
            SolveRule(id, order->retry);
            return;
        }
        // No way to its inputs: checked again below. No way to the ware itself: ResolveShortage checks it
        if(kind != RetryCondition::Kind::NoWay || order->inputsMissing.empty())
            return;
    }
    // A harvester without a place with enough of its source waits for the renewer ordered for it and is placed beside
    // its site. No place for the renewer either: no place for this way (tried again when the situation changed)
    const auto renewer = std::find_if(orders_.begin(), orders_.end(), [id, bld](const Order& other) {
        return other.parent == id && other.cause == OrderCause::Renewal && other.forBuilding == bld;
    });
    if(renewer != orders_.end())
    {
        if(renewer->state == OrderState::Blocked && renewer->retry.kind == RetryCondition::Kind::NoPlace)
        {
            if(order->forWare != GoodType::Nothing)
                failedWays_.push_back({order->forWare, bld, GetSituation()});
            Remove(id);
            return;
        }
        if(renewer->state != OrderState::Placed)
        {
            order->state = OrderState::Waiting;
            order->reason = "wartet auf " + buildingName(renewer->building);
            return;
        }
        if(!order->besideRenewer)
        {
            const auto* site = player_.GetGameWorld().GetSpecObj<noBaseBuilding>(renewer->placedAt);
            if(!site || site->GetPlayer() != player_.GetPlayerId())
            {
                followUp_ = true;
                order->state = OrderState::Waiting;
                order->reason = "wartet auf die Baustelle von " + buildingName(renewer->building);
                return;
            }
            order->near = renewer->placedAt;
            order->besideRenewer = true;
        }
    }
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
    if(const Recipe* recipe = FindRecipe(bld))
    {
        // Inputs kept for an order above: this one must wait (an input with several wares: only if all of them are
        // kept)
        std::vector<const Order*> keptFor;
        std::vector<GoodType> keptWares;
        for(const RecipeInput& input : recipe->inputs)
        {
            std::vector<const Order*> keptForHere;
            std::vector<GoodType> keptHere;
            for(const GoodType good : input.wares)
            {
                if(good == order->usesWare)
                    continue;
                const auto kept = keptInputs_.find(good);
                if(kept != keptInputs_.end() && kept->second != id)
                {
                    keptHere.push_back(good);
                    keptForHere.push_back(Find(kept->second));
                }
            }
            const bool blocks =
              input.wares.size() == 1u ? !keptHere.empty() : !keptHere.empty() && keptHere.size() == input.wares.size();
            if(blocks)
            {
                keptWares.insert(keptWares.end(), keptHere.begin(), keptHere.end());
                keptFor.insert(keptFor.end(), keptForHere.begin(), keptForHere.end());
            }
        }
        const bool blockedByKept = !keptWares.empty();
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
        // A producer placed for an input of this order counts in the forecast: the order is checked again at once and
        // follows beside its site if the forecast allows it. Its site must be in the game (the command takes up to 2
        // network frames)
        bool waitsForSite = false;
        std::string waitsForSiteOf;
        const auto hasPlacedProducer = [&](const GoodType input) {
            for(const Order& other : orders_)
            {
                if(other.state != OrderState::Placed || other.forWare != input
                   || (other.parent != id && !helpers::contains(other.alsoFor, id)))
                    continue;
                const auto* site = player_.GetGameWorld().GetSpecObj<noBaseBuilding>(other.placedAt);
                if(site && site->GetPlayer() == player_.GetPlayerId())
                    return true;
                waitsForSite = true;
                waitsForSiteOf = buildingName(other.building);
            }
            return false;
        };
        // Inputs that do not suffice. An input with several wares: its group does not suffice, then none of them does
        // (each one alone may look enough, the building would use all of them together)
        std::vector<GoodType> missing, missingNames;
        std::vector<std::vector<GoodType>> missingGroups;
        for(const RecipeInput& input : recipe->inputs)
        {
            if(input.wares.size() == 1u)
            {
                const GoodType good = input.wares.front();
                if(good == order->usesWare || InputSuffices(good, *recipe))
                    continue;
                missing.push_back(good);
                missingNames.push_back(good);
            } else if(!InputSlotSuffices(input, *recipe, order->usesWare))
            {
                std::vector<GoodType> wares;
                for(const GoodType good : input.wares)
                {
                    if(good != order->usesWare)
                        wares.push_back(good);
                }
                missing.insert(missing.end(), wares.begin(), wares.end());
                missingNames.insert(missingNames.end(), input.wares.begin(), input.wares.end());
                missingGroups.push_back(std::move(wares));
            }
        }
        const bool suffices = InputsSuffice(*recipe, order->usesWare);
        if(!suffices)
        {
            for(const GoodType input : missing)
                hasPlacedProducer(input);
        }
        if(!suffices && waitsForSite)
        {
            // Checked again soon (independent of the reaction time of the AI)
            followUp_ = true;
            order->state = OrderState::Waiting;
            order->reason = "wartet auf die Baustelle von " + waitsForSiteOf;
            return;
        }
        order->inputsMissing = suffices ? std::vector<GoodType>() : missing;
        if(suffices)
        {
            // Not the reason anymore: free for the orders below
            for(auto it = keptInputs_.begin(); it != keptInputs_.end();)
                it = it->second == id ? keptInputs_.erase(it) : std::next(it);
        } else
        {
            // Every missing input must be possible, otherwise the building cannot work: then no orders for the
            // others (no well for a brewery that gets no grain)
            std::string noWay;
            for(const RecipeInput& input : recipe->inputs)
            {
                if(input.wares.size() != 1u || !helpers::contains(missing, input.wares.front()))
                    continue;
                const GoodType good = input.wares.front();
                const std::vector<WayCheck> ways = RankProducers(good, false, 0);
                if(ways.empty() || ways.front().blocked)
                {
                    noWay = "kein Weg zu " + wareName(good);
                    if(!ways.empty())
                        noWay += " (" + ways.front().reason + ")";
                    if(!ways.empty() && ways.front().outside)
                        AddTerritoryOrder(TerritoryNeed::Source, order->kind, 0, ways.front().outsideWare,
                                          ways.front().outsideBuilding, ways.front().outsideSource);
                    order = Find(id);
                    break;
                }
            }
            for(const std::vector<GoodType>& wares : missingGroups)
            {
                if(noWay.empty() && ChooseWare(wares) == GoodType::Nothing)
                    noWay = "kein Weg zu " + wareNames(wares);
            }
            if(!noWay.empty())
            {
                // It cannot be built: it keeps none of its inputs
                for(auto it = keptInputs_.begin(); it != keptInputs_.end();)
                    it = it->second == id ? keptInputs_.erase(it) : std::next(it);
                order->state = OrderState::Blocked;
                order->reason = noWay;
                order->retry = {RetryCondition::Kind::NoWay, GetSituation()};
                RemoveUnplacedChildren(id);
                return;
            }
            // The missing inputs are the reason: kept for a needed order from now on (orders below must not use them)
            if(needed)
            {
                for(const GoodType input : missing)
                    keptInputs_.emplace(input, id);
            }
            const std::string reason = "Eingang fehlt: " + wareNames(missingNames);
            // Expanding: to the end of the list, with expanding orders for its inputs
            if(needed)
            {
                order->state = OrderState::Waiting;
                order->reason = reason;
            } else
                MoveToEnd(*order, reason, RetryCondition{});
            // Every missing input with one ware, of an input with several wares the one chosen
            std::vector<GoodType> toResolve;
            for(const RecipeInput& input : recipe->inputs)
            {
                if(input.wares.size() == 1u && helpers::contains(missing, input.wares.front()))
                    toResolve.push_back(input.wares.front());
            }
            for(const std::vector<GoodType>& wares : missingGroups)
                toResolve.push_back(ChooseWare(wares));
            for(const GoodType input : toResolve)
            {
                // Made for an order above already: a loop (the ware is needed to make itself)
                if(HasChildFor(id, input) || IsInChain(id, input, BuildingType::Headquarters))
                    continue;
                // What the building will use per minute (measured value per building), beyond what the forecast leaves:
                // producers until the forecast reaches 0
                Need need;
                need.cause = OrderCause::Input;
                need.parent = id;
                need.expanding = !needed;
                need.perMinute =
                  std::max(0., UsedPerBuilding(bld, input) - GetWareStatus(input).forecast / BALANCE_MINUTES);
                ResolveShortage(input, need); // May add orders: `order` is not valid anymore
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
            // Expanding: to the end of the list. Its problem is worked on with expanding orders (material, helpers)
            MoveToEnd(*order, rule, retry);
            SolveRule(id, retry);
            return;
        }
        order->state = order->selfSolving ? OrderState::Waiting : OrderState::Blocked;
        order->reason = rule;
        order->retry = retry;
        ClaimToolsAndHelpers(*order);
        SolveRule(id, retry);
        return;
    }

    bool searchedAll = false;
    const MapPoint pos = SearchPlace(*order, searchBudget, searchedAll);
    if(pos.isValid())
    {
        const bool clearing = order->clearing;
        // A military building gets its type at the place
        const BuildingType bld = order->building;
        if(!aijh_.PlaceBuilding(bld, pos))
            return;
        AIJH::ConstructionMaterial taken{};
        takeMaterial(freeMaterial_, bld, taken);
        {
            const Recipe* placedRecipe = FindRecipe(bld);
            const int inReach = placedRecipe && placedRecipe->reach > 0 ?
                                  static_cast<int>(MapProfile::CountSourceNodes(
                                    player_.GetGameWorld(), pos, placedRecipe->reach, placedRecipe->source)) :
                                  -1;
            placements_[pos] = Placement{currentGF_, bld, order->cause, order->kind, inReach};
        }
        // Placed: its costs, tools and worker are planned if it was not counted at the start of the step. What it will
        // make and use is part of the forecast from now on (a site ordered)
        const bool counted = helpers::contains(plannedOrderIds_, id);
        if(!counted)
            AddPlanned(bld, true, true, 0);
        if(clearing && bld == BuildingType::Woodcutter)
        {
            aijh_.GetInterface().SetClearingWoodcutter(pos, true);
            clearingPending_.push_back(pos);
        }
        order = Find(id);
        order->state = OrderState::Placed;
        RecomputeForecast();
        // Expanding orders are done with the building site. Military buildings stay until they are finished: one
        // expanding at a time, at most MAX_ORDERS_PER_TYPE of a type with their sites
        if(!needed && order->cause != OrderCause::Territory)
        {
            Remove(id);
            return;
        }
        order = Find(id);
        order->placedAt = pos;
        order->reason.clear();
        // The renewer ordered for it renews around it from now on (no other one for it while it starts up)
        for(Order& other : orders_)
        {
            if(other.parent == id && other.cause == OrderCause::Renewal)
                other.near = pos;
        }
        return;
    }
    if(!searchedAll)
    {
        order->state = OrderState::Open;
        order->reason = "suche Bauplatz";
        return;
    }
    const RetryCondition noPlace{RetryCondition::Kind::NoPlace, GetSituation()};
    const Recipe* noPlaceRecipe = FindRecipe(bld);
    const Source noPlaceSource = noPlaceRecipe && noPlaceRecipe->reach > 0 ? noPlaceRecipe->source : Source{};
    if(order->cause == OrderCause::Territory)
    {
        order->state = OrderState::Blocked;
        order->reason = "kein Bauplatz an der Grenze";
        order->retry = noPlace;
        return;
    }
    if(!needed)
    {
        MoveToEnd(*order, "kein Bauplatz", noPlace);
        // A military building for it (expanding like it)
        AddTerritoryOrder(TerritoryNeed::Place, OrderKind::Expanding, id, order->forWare, bld, noPlaceSource);
        return;
    }
    // A harvester for good without a place with enough of its source in reach: a renewer is ordered for it, the
    // harvester is placed beside its site (the source in reach does not count there)
    if(!order->clearing && !order->besideRenewer && order->cause != OrderCause::Renewal)
    {
        const Recipe* recipe = FindRecipe(bld);
        const Renewer* renewerOfSource = recipe && recipe->reach > 0 ? FindRenewer(recipe->source) : nullptr;
        if(renewerOfSource)
        {
            const std::string reason = "kein Bauplatz mit " + std::to_string(GetMinSourceInReach(*order, *recipe)) + " "
                                       + civAI_.GetGraph().GetSourceName(recipe->source)
                                       + " in Reichweite: kommt neben " + buildingName(renewerOfSource->building);
            Order planter;
            planter.building = renewerOfSource->building;
            planter.cause = OrderCause::Renewal;
            planter.parent = id;
            planter.forBuilding = bld;
            AddOrder(std::move(planter));
            order = Find(id);
            order->state = OrderState::Waiting;
            order->reason = reason;
            return;
        }
    }
    // No place for this way: the next way to the ware is tried, this one again when the situation changed
    if(order->forWare != GoodType::Nothing && !order->clearing
       && (order->cause == OrderCause::Shortage || order->cause == OrderCause::Input
           || order->cause == OrderCause::Soldiers))
    {
        failedWays_.push_back({order->forWare, bld, GetSituation()});
        // A military building for the way (needed like the order; it is for the way, the order goes)
        AddTerritoryOrder(TerritoryNeed::Place, order->kind, 0, order->forWare, bld, noPlaceSource);
        Remove(id);
        return;
    }
    order->state = OrderState::Blocked;
    order->reason = "kein Bauplatz";
    order->retry = noPlace;
    // Removing the children moves the orders in the list: `order` is not valid anymore
    const GoodType forWare = order->forWare;
    RemoveUnplacedChildren(id);
    AddTerritoryOrder(TerritoryNeed::Place, OrderKind::Needed, id, forWare, bld, noPlaceSource);
}

std::string EconomyPlanner::CheckRules(const Order& order, RetryCondition& retry) const
{
    const BuildingType bld = order.building;
    if(!player_.IsBuildingEnabled(bld))
    {
        retry = {RetryCondition::Kind::Research, GetSituation()};
        return "nicht erforscht";
    }
    // The limit per building type holds for expanding orders. Needed orders solve a problem and ignore it
    if(order.kind == OrderKind::Expanding && BuildingProperties::IsUsual(bld))
    {
        const unsigned limit = GetBuildingLimit();
        const unsigned num = GetNumBuildings(bld);
        if(num >= limit)
        {
            retry = {RetryCondition::Kind::Limit, GetSituation()};
            return "Höchstzahl erreicht: " + std::to_string(num) + " von " + std::to_string(limit);
        }
    }
    std::vector<GoodType> missingMaterial;
    std::string material = DescribeMissingMaterial(bld, missingMaterial);
    if(!material.empty())
    {
        retry = {RetryCondition::Kind::Material, GetSituation()};
        return material;
    }
    if(!aijh_.GetConstruction().HasEnoughBuildersAvailable())
    {
        retry = {RetryCondition::Kind::Builder, GetSituation()};
        return "kein Bauarbeiter";
    }
    // Military building: a soldier free in the warehouses for every military building site and this one (rule of
    // the old AI, without its limit of building sites)
    if(BuildingProperties::IsMilitary(bld) && !aijh_.GetConstruction().HasSoldiersForMilitarySite())
    {
        retry = {RetryCondition::Kind::Soldiers, GetSituation()};
        return "keine Soldaten frei";
    }
    // The forecast of its tools and of the helpers allows it (the building sites and the orders above it count)
    const std::string toolsAndHelpers = CheckToolsAndHelpers(&order, bld, true);
    if(!toolsAndHelpers.empty())
    {
        retry = {toolsAndHelpers.rfind(HELPERS_MISSING, 0) == 0 ? RetryCondition::Kind::Helpers :
                                                                  RetryCondition::Kind::Tool,
                 GetSituation()};
        return toolsAndHelpers;
    }
    return std::string();
}

unsigned EconomyPlanner::GetAvailableBuilders() const
{
    // Builders in the warehouses and the free helpers who can become builders (with the tool of the builder, if he
    // needs one)
    const int freeHelpers = std::max(0, GetFreeHelpers());
    const GoodType tool = JOB_CONSTS[Job::Builder].tool.value_or(GoodType::Nothing);
    const unsigned recruitable =
      tool == GoodType::Nothing ? freeHelpers : std::min<unsigned>(freeHelpers, GetToolStock(tool));
    return snapshot_.inWarehousesPeople[Job::Builder] + recruitable;
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

bool EconomyPlanner::NeedsHelper(const BuildingType bld) const
{
    const auto job = getJob(bld);
    if(!job || !BuildingProperties::IsUsual(bld))
        return false;
    // Workers of the job free in the warehouses (not wanted elsewhere, not planned for other buildings yet)
    return snapshot_.inWarehousesPeople[*job] <= snapshot_.jobsWanted[*job] + workersPlanned_[*job];
}

unsigned EconomyPlanner::GetToolStock(const GoodType tool) const
{
    // All wares of the kind of tool (e.g. iron and copper axes)
    const auto kind = getToolOf(tool);
    if(!kind)
        return snapshot_.inWarehouses[tool];
    unsigned result = 0;
    for(const auto good : civ::getActiveContent().GetGoods())
    {
        if(good != GoodType::Nothing && snapshot_.inWarehouses[good] > 0 && getToolOf(good) == kind)
            result += snapshot_.inWarehouses[good];
    }
    return result;
}

std::string EconomyPlanner::CheckToolsAndHelpers(const Order* order, const BuildingType bld, const bool withList) const
{
    if(!BuildingProperties::IsUsual(bld))
        return std::string();
    // Rules of the list: the forecast without the orders not placed yet, minus what the orders above keep for
    // themselves, must leave what this one needs. Otherwise (one more building of the type): the forecast with all
    // planned ones
    const bool counted = order && helpers::contains(plannedOrderIds_, order->id);
    // What the forecast of the tools leaves for this building, text if it does not leave enough
    const auto checkTools = [&](const std::vector<GoodType>& wares, const unsigned amount) {
        const auto it = groupIndex_.find(sorted(wares));
        if(it == groupIndex_.end())
            return std::string();
        const WareStatus& status = groups_[it->second].status;
        // The stock of TOOLS_IN_STOCK per kind is there to be used by new buildings: it makes the makers work ahead,
        // but it does not keep a building from taking the tools
        const unsigned reserve = helpers::contains(toolKindGroup_, it->second) ? TOOLS_IN_STOCK : 0u;
        // A maker of the tool makes it for itself once it is placed (it works slowly without it)
        double ownMaking = 0;
        if(const Recipe* recipe = FindRecipe(bld); recipe && IsMultiProduct(*recipe))
        {
            for(const GoodType good : recipe->outputs)
            {
                if(helpers::contains(wares, good))
                    ownMaking = OutputPerWorkingMinute(bld) * BALANCE_MINUTES;
            }
        }
        const double byOrders = withList ? status.plannedByOrders : 0.;
        const unsigned claimed = withList && it->second < toolsClaimed_.size() ? toolsClaimed_[it->second] : 0u;
        const double available = status.forecast + reserve + ownMaking + byOrders - claimed;
        // Its own tools are in the forecast if it is planned (all planned ones) or added back (rules of the list)
        const double own = (counted && !withList) ? 0. : static_cast<double>(amount);
        if(available - own >= 0)
            return std::string();
        std::ostringstream out;
        out << wareNames(groups_[it->second].wares) << " (Prognose " << num(status.forecast) << ", Vorrat " << reserve;
        if(withList)
            out << ", für Aufträge " << num(byOrders) << ", davon darüber " << claimed;
        out << ")";
        return out.str();
    };
    // The tool to recruit its worker (jobs recruited with a tool): without it there is no worker
    if(const GoodType tool = GetRecruitTool(bld); tool != GoodType::Nothing)
    {
        const std::string missing = checkTools({tool}, 1);
        if(!missing.empty())
            return "Werkzeug fehlt: " + missing;
    }
    // Tool reform: only a building that does not work without tools (base speed 0) waits, for a tool of any of its
    // slots giving work speed. Its other tools, and all tools of a building working slowly without them, are a bonus:
    // they are asked for (planned) and their makers come by the forecast, but the building does not wait for them
    // (Nutzer, 2026-11-18)
    const std::vector<std::vector<GoodType>> workTools = EconomyGraph::GetWorkTools(civ::getActiveContent(), bld);
    if(!workTools.empty())
    {
        std::string missing;
        for(const std::vector<GoodType>& wares : workTools)
        {
            const std::string slotMissing = checkTools(wares, 1);
            if(slotMissing.empty())
                return CheckHelpers(order, bld, withList, counted);
            missing += (missing.empty() ? "" : " oder ") + slotMissing;
        }
        return "Werkzeug fehlt: " + missing;
    }
    return CheckHelpers(order, bld, withList, counted);
}

std::string EconomyPlanner::CheckHelpers(const Order* order, const BuildingType bld, const bool withList,
                                         const bool counted) const
{
    const bool needsHelper = counted ? helpers::contains(plannedHelpers_, order->id) : NeedsHelper(bld);
    if(needsHelper)
    {
        double available = helpers_.forecast;
        if(withList)
            available += static_cast<double>(helpers_.plannedByOrders) - helpersClaimed_;
        const double own = (counted && !withList) ? 0. : 1.;
        if(available - own < 0)
        {
            std::ostringstream out;
            out << HELPERS_MISSING << " (Prognose " << num(helpers_.forecast);
            if(withList)
                out << ", für Aufträge " << num(helpers_.plannedByOrders) << ", davon darüber " << helpersClaimed_;
            out << ")";
            return out.str();
        }
    }
    return std::string();
}

void EconomyPlanner::ClaimToolsAndHelpers(const Order& order)
{
    // Only what is planned for it (orders not planned do not count in the forecast)
    if(order.kind != OrderKind::Needed || !BuildingProperties::IsUsual(order.building)
       || !helpers::contains(plannedOrderIds_, order.id))
        return;
    std::vector<OpenDemand> demand;
    AddToolDemand(demand, order.building);
    for(const OpenDemand& entry : demand)
    {
        const auto it = groupIndex_.find(sorted(entry.wares));
        if(it == groupIndex_.end())
            continue;
        if(toolsClaimed_.size() <= it->second)
            toolsClaimed_.resize(groups_.size(), 0u);
        toolsClaimed_[it->second] += entry.amount;
    }
    if(helpers::contains(plannedHelpers_, order.id))
        ++helpersClaimed_;
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
            // An expanding order has no rights: it waits at the end of the list (the reason is set by MoveToEnd). No
            // orders are made for construction material (Nutzer, 2026-11-17): its producers come from the forecast
            if(order->kind != OrderKind::Needed)
                break;
            // A needed order keeps the material that is there (orders below and other buildings must not take it)
            AIJH::ConstructionMaterial free = freeMaterial_;
            KeepMaterial(*order);
            std::vector<std::pair<GoodType, unsigned>> lacking;
            // The wares of each entry lacking (any of them would do)
            std::vector<std::vector<GoodType>> lackingWares;
            const BuildingCosts costs = AIJH::getMaterialOrder(bld);
            for(const unsigned i : AIJH::getEntryOrder(costs))
            {
                const CostEntry& entry = costs[i];
                int amount = entry.amount;
                for(const GoodType good : entry.wares)
                {
                    const int part = std::min(std::max(free[good], 0), amount);
                    free[good] -= part;
                    amount -= part;
                }
                if(amount <= 0)
                    continue;
                GoodType good = entry.wares.size() == 1u ? entry.wares.front() : ChooseWare(entry.wares);
                if(good == GoodType::Nothing)
                    good = entry.wares.front();
                lacking.emplace_back(good, static_cast<unsigned>(amount));
                lackingWares.push_back(entry.wares);
            }
            if(lacking.empty())
                break;
            // Why it is missing, e.g. "Bretter: frei -4, Prognose -3; Sägewerk: ... -> Holz: ..."
            const std::string why = ExplainWare(lacking.front().first, 2);
            // Material made by a chain the building is part of (a woodcutter lacking boards): it is the solution
            // itself. It gets what comes back to the warehouses before everybody else, building sites outside the chain
            // whose material is not built in yet are cancelled
            std::vector<std::pair<GoodType, unsigned>> selfSolved;
            if(order->selfSolving)
            {
                for(unsigned i = 0; i < lacking.size(); ++i)
                {
                    // The ware of the entry its chain makes (a quarry lacking "walls" of wood, boards or stones)
                    const auto ware =
                      std::find_if(lackingWares[i].begin(), lackingWares[i].end(),
                                   [&](const GoodType good) { return helpers::contains(GetChain(good), bld); });
                    if(ware == lackingWares[i].end())
                        continue;
                    selfSolved.emplace_back(*ware, lacking[i].second);
                    reservedMaterial_[*ware] += lacking[i].second;
                }
            }
            const std::string cancelled = selfSolved.empty() ? std::string() : CancelSitesFor(selfSolved);
            order = Find(id);
            if(!order)
                break;
            if(!selfSolved.empty())
                order->reason = "Vorrang (löst den Mangel selbst): " + order->reason;
            order->reason += " – " + why;
            if(!cancelled.empty())
                order->reason += "; abgebrochen: " + cancelled;
            break;
        }
        case RetryCondition::Kind::Helpers: AddHelpersOrder(id); break;
        case RetryCondition::Kind::Builder:
            // A builder is recruited from a helper and his tool (kept in stock by the tool orders)
            if(helpers_.forecast < 1)
                AddHelpersOrder(id);
            break;
        case RetryCondition::Kind::Limit:
        case RetryCondition::Kind::Now:
        case RetryCondition::Kind::Tool:
        case RetryCondition::Kind::Research:
        case RetryCondition::Kind::NoWay:
        case RetryCondition::Kind::Soldiers:
        case RetryCondition::Kind::NoPlace:
            // Tools: ordered (OrderTools), their makers come with ResolveTools. Research or territory needed
            break;
    }
}

std::string EconomyPlanner::DescribeMissingMaterial(const BuildingType bld, std::vector<GoodType>& missing) const
{
    // The entries take their wares one after another from the free material (the wares fewer other entries take first,
    // the entries with fewest alternatives first): entries sharing wares (wood for walls and roof) cannot both count
    // the same wares. "frei" is what is left for the entry
    AIJH::ConstructionMaterial free = freeMaterial_;
    const BuildingCosts costs = AIJH::getMaterialOrder(bld);
    std::string result;
    for(const unsigned i : AIJH::getEntryOrder(costs))
    {
        const CostEntry& entry = costs[i];
        int available = 0, stock = 0, forSites = 0, forOrdersAbove = 0;
        for(const GoodType good : entry.wares)
        {
            available += std::max(0, free[good]);
            stock += static_cast<int>(snapshot_.inWarehouses[good]);
            forSites += static_cast<int>(snapshot_.inWarehouses[good]) - stepFreeMaterial_[good];
            forOrdersAbove += std::max(0, stepFreeMaterial_[good]) - std::max(0, freeMaterial_[good]);
        }
        int amount = entry.amount;
        for(const GoodType good : entry.wares)
        {
            const int part = std::min(std::max(free[good], 0), amount);
            free[good] -= part;
            amount -= part;
        }
        if(available >= static_cast<int>(entry.amount))
            continue;
        // Any ware of the entry would do
        for(const GoodType good : entry.wares)
        {
            if(!helpers::contains(missing, good))
                missing.push_back(good);
        }
        std::ostringstream out;
        out << wareNames(BUILDING_COSTS[bld][i].wares) << " fehlt: " << static_cast<unsigned>(entry.amount)
            << " nötig, " << available << " frei (Lager " << stock << ", Baustellen brauchen noch " << forSites
            << ", für Aufträge darüber " << forOrdersAbove << ")";
        result += (result.empty() ? "" : "; ") + out.str();
    }
    return result;
}

std::string EconomyPlanner::ExplainWare(const GoodType good, const unsigned depth)
{
    const WareStatus& status = GetWareStatus(good);
    std::ostringstream out;
    out << wareName(good) << ": frei " << num(status.free) << ", Prognose " << num(status.forecast);
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
        for(const GoodType input : recipe.inputWares)
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
    // and so on. Example boards: sawmill; wood does not suffice: woodcutter. Renewers do not make wares now
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
        for(const GoodType input : recipe.inputWares)
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
    // Only needed orders solve a problem. Self-solving: its building is part of the chain making the construction
    // material it lacks, and it can work (its inputs suffice), e.g. a woodcutter lacking boards when the sawmills lack
    // wood. A sawmill lacking boards and wood solves nothing
    for(Order& order : orders_)
        order.selfSolving = false;
    for(Order& order : orders_)
    {
        if(order.state == OrderState::Placed || order.kind != OrderKind::Needed)
            continue;
        std::vector<GoodType> missing;
        if(DescribeMissingMaterial(order.building, missing).empty())
            continue;
        const Recipe* recipe = FindRecipe(order.building);
        if(!recipe || !InputsSuffice(*recipe))
            continue;
        for(const GoodType good : missing)
        {
            if(helpers::contains(GetChain(good), order.building))
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
    // Sites of needed orders are the solution of a problem too: they stay
    std::vector<MapPoint> neededSites;
    for(const Order& order : orders_)
    {
        if(order.state == OrderState::Placed && order.kind == OrderKind::Needed)
            neededSites.push_back(order.placedAt);
    }
    for(const auto& [good, amount] : lacking)
    {
        // Only when stuck: nothing of it made in STUCK_MINUTES and not enough in the warehouses. Otherwise the material
        // comes by itself and the other sites keep theirs
        if(stats.GetProduced(good, STUCK_MINUTES, currentGF_) > 0 || snapshot_.inWarehouses[good] >= amount)
            continue;
        const std::vector<BuildingType> chain = GetChain(good);
        // Sites outside the chain whose builder has not built in anything yet. What comes back: the wares on the way
        // there and those not ordered yet (wares already lying at the site are lost)
        std::vector<std::pair<unsigned, const noBuildingSite*>> candidates;
        for(const noBuildingSite* site : player_.GetBuildingRegister().GetBuildingSites())
        {
            const BuildingType bld = site->GetBuildingType();
            if(helpers::contains(chain, bld) || site->IsHarborBuildingSiteFromSea()
               || helpers::contains(neededSites, site->GetPos()))
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

bool EconomyPlanner::IsRetryMet(const RetryCondition& retry, const Order& order) const
{
    const Situation situation = GetSituation();
    const BuildingType bld = order.building;
    switch(retry.kind)
    {
        case RetryCondition::Kind::Now: return true;
        case RetryCondition::Kind::Material: return HasMaterial(bld);
        case RetryCondition::Kind::Builder: return aijh_.GetConstruction().HasEnoughBuildersAvailable();
        case RetryCondition::Kind::Limit: return GetNumBuildings(bld) < GetBuildingLimit();
        // The forecasts change every step: checked again
        case RetryCondition::Kind::Helpers:
        case RetryCondition::Kind::Tool: return CheckToolsAndHelpers(&order, bld, true).empty();
        case RetryCondition::Kind::Research: return situation.researched != retry.situation.researched;
        case RetryCondition::Kind::NoWay: return false; // Checked every step anyway
        case RetryCondition::Kind::Soldiers: return aijh_.GetConstruction().HasSoldiersForMilitarySite();
        case RetryCondition::Kind::NoPlace:
            // More land, or buildings removed (their places are free)
            return situation.land != retry.situation.land || situation.buildings < retry.situation.buildings;
    }
    return true;
}

MapPoint EconomyPlanner::GetPreferredPoint(const Order& order) const
{
    // The site of an input placed for it. Otherwise the site of another input placed for the same order (the inputs
    // of a building stand together, the building follows beside them)
    for(const Order& other : orders_)
    {
        if(other.state == OrderState::Placed && other.kind == OrderKind::Needed && other.forWare != GoodType::Nothing
           && (other.parent == order.id || helpers::contains(other.alsoFor, order.id))
           && (other.cause == OrderCause::Input || other.cause == OrderCause::Shortage
               || other.cause == OrderCause::Soldiers))
        {
            const Recipe* recipe = FindRecipe(order.building);
            if(recipe && helpers::contains(recipe->inputWares, other.forWare))
                return other.placedAt;
        }
    }
    if(order.cause == OrderCause::Input && order.parent != 0)
    {
        for(const Order& other : orders_)
        {
            if(other.id != order.id && other.parent == order.parent && other.cause == OrderCause::Input
               && other.state == OrderState::Placed)
                return other.placedAt;
        }
    }
    return MapPoint::Invalid();
}

bool EconomyPlanner::IsFieldPlace(const MapPoint pt) const
{
    // Plantations of lua plant only where their object grows: these places (and those with a plant on them) stay
    // free, also for the flag of a building. The fields of farms and charburners are kept free by the old AI
    // (AIPlayerJH::SetFarmedNodes)
    const GameWorldBase& world = player_.GetGameWorld();
    const civ::Content& content = civ::getActiveContent();
    const MapPoint flagPt = world.GetNeighbour(pt, Direction::SouthEast);
    const auto isField = [&](const MapPoint center, const civ::CustomBuilding& building) {
        for(const MapPoint node : {pt, flagPt})
        {
            if(world.CalcDistance(node, center) > building.plantRadius)
                continue;
            const auto* deposit = world.GetSpecObj<noDeposit>(node);
            if((deposit && static_cast<int>(deposit->GetObjectIdx()) == building.plantObject)
               || nofCustomPlanter::CanPlantAt(world, node, building))
                return true;
        }
        return false;
    };
    for(unsigned i = 0; i < content.GetNumBuildings(); ++i)
    {
        const BuildingType bld = getCustomBuilding(i);
        const civ::CustomBuilding* building = content.GetBuilding(bld);
        if(!building || !building->IsPlanter())
            continue;
        for(const nobUsual* plantation : player_.GetBuildingRegister().GetBuildings(bld))
        {
            if(isField(plantation->GetPos(), *building))
                return true;
        }
        if(snapshot_.buildings[bld].numSites == 0)
            continue;
        for(const noBuildingSite* site : player_.GetBuildingRegister().GetBuildingSites())
        {
            if(site->GetBuildingType() == bld && isField(site->GetPos(), *building))
                return true;
        }
    }
    return false;
}

MapPoint EconomyPlanner::SearchPlace(Order& order, unsigned& searchBudget, bool& searchedAll)
{
    searchedAll = false;
    if(order.cause == OrderCause::Territory)
    {
        if(searchBudget == 0)
            return MapPoint::Invalid();
        --searchBudget;
        return SearchMilitaryPlace(order, searchedAll);
    }
    const Recipe* recipe = FindRecipe(order.building);
    const bool renewal = order.cause == OrderCause::Renewal;
    const bool warehouse = BuildingProperties::IsWareHouse(order.building);
    const bool harvester = recipe && recipe->reach > 0 && recipe->source.kind != SourceKind::None;
    const bool material = recipe
                          && std::any_of(recipe->outputs.begin(), recipe->outputs.end(),
                                         [this](const GoodType good) { return isMaterial_[good]; });
    // Anchors with their search radius (Nutzer, 2026-11-17: the rules of the placement of 15.11., implemented better).
    // First, in this order: the point given (renewer at its harvester, harvester beside its renewer), the site of an
    // input placed for it, for a processing building the partners (producers of its inputs, users of its product).
    // Then in turn the buildings holding territory (headquarters, harbors, military buildings), each in its military
    // radius: together they cover the whole territory (Nutzer, 2026-11-18; before: the warehouses in radius 15)
    std::vector<std::pair<MapPoint, unsigned>> anchors;
    std::vector<MapPoint> partners;
    if(order.near.isValid())
        anchors.emplace_back(order.near, SEARCH_RADIUS);
    else
    {
        const MapPoint preferred = (renewal || order.besideRenewer) ? MapPoint::Invalid() : GetPreferredPoint(order);
        if(preferred.isValid())
            anchors.emplace_back(preferred, SEARCH_RADIUS);
        if(recipe && !harvester && !warehouse)
        {
            partners = GetPartners(*recipe, 30);
            if(!material)
            {
                for(unsigned i = 0; i < partners.size() && i < 3u; ++i)
                    anchors.emplace_back(partners[i], SEARCH_RADIUS);
            }
        }
    }
    const unsigned first = static_cast<unsigned>(anchors.size());
    if(!order.near.isValid())
    {
        const BuildingRegister& buildings = player_.GetBuildingRegister();
        for(const nobBaseWarehouse* building : buildings.GetStorehouses())
        {
            if(building->GetMilitaryRadius() > 0)
                anchors.emplace_back(building->GetPos(), building->GetMilitaryRadius());
        }
        for(const nobMilitary* military : buildings.GetMilitaryBuildings())
            anchors.emplace_back(military->GetPos(), military->GetMilitaryRadius());
        // The nearest to a warehouse first: short ways for the wares, the whole territory if needed
        const GameWorldBase& world = player_.GetGameWorld();
        const auto toWarehouse = [&](const MapPoint pt) {
            unsigned result = std::numeric_limits<unsigned>::max();
            for(const nobBaseWarehouse* warehouse : buildings.GetStorehouses())
                result = std::min(result, world.CalcDistance(pt, warehouse->GetPos()));
            return result;
        };
        std::stable_sort(anchors.begin() + first, anchors.end(), [&](const auto& lhs, const auto& rhs) {
            return toWarehouse(lhs.first) < toWarehouse(rhs.first);
        });
    }
    if(anchors.empty())
    {
        searchedAll = true;
        return MapPoint::Invalid();
    }
    std::vector<std::pair<MapPoint, unsigned>> avoid;
    if(renewal && order.building == BuildingType::Forester)
    {
        // A forester never where its reach overlaps that of a clearing woodcutter or a charburner (Nutzer)
        const unsigned foresterReach = nofFarmhand::GetWorkRadius(Job::Forester);
        std::vector<MapPoint> clearing = player_.GetClearingWoodcutters();
        clearing.insert(clearing.end(), clearingPending_.begin(), clearingPending_.end());
        for(const MapPoint pos : clearing)
        {
            if(IsClearingWoodcutter(pos))
                avoid.emplace_back(pos, nofFarmhand::GetWorkRadius(Job::Woodcutter) + foresterReach);
        }
        const BuildingRegister& buildings = player_.GetBuildingRegister();
        for(const nobUsual* charburner : buildings.GetBuildings(BuildingType::Charburner))
            avoid.emplace_back(charburner->GetPos(), nofFarmhand::GetWorkRadius(Job::CharBurner) + foresterReach);
        for(const noBuildingSite* site : buildings.GetBuildingSites())
        {
            if(site->GetBuildingType() == BuildingType::Charburner)
                avoid.emplace_back(site->GetPos(), nofFarmhand::GetWorkRadius(Job::CharBurner) + foresterReach);
        }
    }
    while(searchBudget > 0)
    {
        if(order.centersTried >= anchors.size())
        {
            order.centersTried = 0;
            searchedAll = true;
            return MapPoint::Invalid();
        }
        const unsigned idx = order.centersTried;
        const auto [anchor, radius] = anchors[idx];
        ++order.centersTried;
        --searchBudget;
        aijh_.UpdateNodesAround(anchor, radius);
        MapPoint pos;
        if(order.besideRenewer)
        {
            // Beside the renewer planted for it (the source in its reach does not count)
            const Renewer* renewer = recipe ? FindRenewer(recipe->source) : nullptr;
            pos = aijh_.FindHarvesterPositionBeside(order.building, anchor, renewer ? renewer->reach : 2u);
            if(pos.isValid() && !IsPlaceValid(order, recipe, pos))
                pos = MapPoint::Invalid();
        } else if(renewal && order.building == BuildingType::Forester)
        {
            // In reach of its woodcutter
            pos = aijh_.FindForesterPosition(anchor, avoid, order.near.isValid() ? 0u : radius);
            if(pos.isValid() && IsFieldPlace(pos))
                pos = MapPoint::Invalid();
        } else
            pos = FindPlaceNear(order, recipe, anchor, radius, partners);
        if(pos.isValid())
        {
            order.centersTried = 0;
            return pos;
        }
    }
    return MapPoint::Invalid();
}

std::vector<MapPoint> EconomyPlanner::GetPartners(const Recipe& recipe, const unsigned max) const
{
    // Producers of an input, users of the product (buildings and sites)
    std::vector<BuildingType> types;
    for(const Recipe& other : civAI_.GetGraph().GetRecipes())
    {
        if(other.building == recipe.building || helpers::contains(types, other.building))
            continue;
        const bool makesInput = std::any_of(other.outputs.begin(), other.outputs.end(), [&](const GoodType good) {
            return helpers::contains(recipe.inputWares, good);
        });
        const bool usesProduct =
          std::any_of(other.inputWares.begin(), other.inputWares.end(),
                      [&](const GoodType good) { return helpers::contains(recipe.outputs, good); });
        if(makesInput || usesProduct)
            types.push_back(other.building);
    }
    const GameWorldBase& world = player_.GetGameWorld();
    const BuildingRegister& buildings = player_.GetBuildingRegister();
    std::vector<std::pair<unsigned, MapPoint>> found;
    const auto add = [&](const MapPoint pos) {
        unsigned distance = std::numeric_limits<unsigned>::max();
        for(const nobBaseWarehouse* warehouse : buildings.GetStorehouses())
            distance = std::min(distance, world.CalcDistance(pos, warehouse->GetPos()));
        found.emplace_back(distance, pos);
    };
    for(const BuildingType bld : types)
    {
        if(!BuildingProperties::IsUsual(bld))
            continue;
        for(const nobUsual* building : buildings.GetBuildings(bld))
            add(building->GetPos());
    }
    for(const noBuildingSite* site : buildings.GetBuildingSites())
    {
        if(helpers::contains(types, site->GetBuildingType()))
            add(site->GetPos());
    }
    std::stable_sort(found.begin(), found.end(),
                     [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    std::vector<MapPoint> result;
    for(unsigned i = 0; i < found.size() && i < max; ++i)
        result.push_back(found[i].second);
    return result;
}

bool EconomyPlanner::IsPlaceValid(const Order& order, const Recipe* recipe, const MapPoint pt)
{
    const GameWorldBase& world = player_.GetGameWorld();
    const BuildingType bld = order.building;
    const BuildingRegister& buildings = player_.GetBuildingRegister();
    // Never on the fields of plantations
    if(IsFieldPlace(pt))
        return false;
    // The reach of a clearing woodcutter never overlaps that of a forester, the reach of a charburner never that of a
    // forester (Nutzer, both ways)
    const auto nearForester = [&](const unsigned distance) {
        for(const nobUsual* building : buildings.GetBuildings(BuildingType::Forester))
        {
            if(world.CalcDistance(pt, building->GetPos()) <= distance)
                return true;
        }
        for(const noBuildingSite* site : buildings.GetBuildingSites())
        {
            if(site->GetBuildingType() == BuildingType::Forester && world.CalcDistance(pt, site->GetPos()) <= distance)
                return true;
        }
        return false;
    };
    const unsigned foresterReach = nofFarmhand::GetWorkRadius(Job::Forester);
    if(order.clearing && recipe && FindRenewer(recipe->source) && nearForester(recipe->reach + foresterReach))
        return false;
    if(bld == BuildingType::Charburner && nearForester(nofFarmhand::GetWorkRadius(Job::CharBurner) + foresterReach))
        return false;
    // Enough of its source in reach (not beside its renewer: it plants it)
    if(recipe && recipe->reach > 0 && !order.besideRenewer
       && MapProfile::CountSourceNodes(world, pt, recipe->reach, recipe->source) < GetMinSourceInReach(order, *recipe))
        return false;
    // Farms and plantations with enough nodes for fields
    if(NeedsFields(bld) && !order.besideRenewer && CountFieldNodes(bld, pt, false) < MIN_FIELD_NODES)
        return false;
    // The rules of the old AI that stay (hunter, quarry, fishery, storehouse, farm, charburner)
    if(!aijh_.IsPlaceAllowed(bld, pt))
        return false;
    // Enough units of the source for one more harvester (Nutzer, 2026-11-18): the nodes its worker can walk to, shared
    // with the other harvesters reaching them, at least UNITS_PER_HARVESTER, 1 if no other one reaches them. Not beside
    // its renewer (it plants the source)
    if(recipe && recipe->reach > 0 && !order.besideRenewer && isMapResource(recipe->source)
       && !HasUnitsForHarvester(*recipe, pt))
        return false;
    return true;
}

bool EconomyPlanner::HasUnitsForHarvester(const Recipe& recipe, const MapPoint pt) const
{
    const GameWorldBase& world = player_.GetGameWorld();
    const BuildingRegister& buildings = player_.GetBuildingRegister();
    // The other harvesters of the source and their reach: buildings, building sites and sites ordered by the planner
    std::vector<std::pair<MapPoint, unsigned>> others;
    std::vector<BuildingType> types;
    for(const Recipe& other : civAI_.GetGraph().GetRecipes())
    {
        if(other.reach == 0 || !sameSource(other.source, recipe.source) || helpers::contains(types, other.building))
            continue;
        types.push_back(other.building);
        for(const nobUsual* building : buildings.GetBuildings(other.building))
            others.emplace_back(building->GetPos(), other.reach);
        for(const noBuildingSite* site : buildings.GetBuildingSites())
        {
            if(site->GetBuildingType() == other.building)
                others.emplace_back(site->GetPos(), other.reach);
        }
        for(const auto& [pos, placement] : placements_)
        {
            if(placement.building == other.building && !world.GetSpecObj<noBaseBuilding>(pos))
                others.emplace_back(pos, other.reach);
        }
    }
    // The nodes with the source (nearest first) and their share: the units divided by the harvesters reaching them
    std::vector<std::pair<MapPoint, double>> nodes;
    double upperBound = 0;
    bool shared = false;
    world.CheckPointsInRadius(
      pt, recipe.reach,
      [&](const MapPoint curPt, unsigned) {
          const unsigned amount = getSourceUnits(world, curPt, recipe.source);
          if(amount == 0)
              return false;
          unsigned numOthers = 0;
          for(const auto& [pos, reach] : others)
          {
              if(pos != pt && world.CalcDistance(pos, curPt) <= reach)
                  ++numOthers;
          }
          shared |= numOthers > 0;
          nodes.emplace_back(curPt, static_cast<double>(amount) / (numOthers + 1));
          upperBound += nodes.back().second;
          return false;
      },
      true);
    // No other harvester reaches them: 1 unit suffices, otherwise UNITS_PER_HARVESTER
    const double needed = shared ? UNITS_PER_HARVESTER : 1e-9;
    if(upperBound < needed)
        return false;
    // Workers walk to trees, stone piles and map objects (at most 20 steps like nofFarmhand): only those count. Fish
    // and ore: no path. Paths only as long as the result is open
    if(!isDeposit(recipe.source))
        return true;
    double units = 0;
    for(const auto& [curPt, share] : nodes)
    {
        upperBound -= share;
        if(world.FindHumanPath(pt, curPt, 20))
        {
            units += share;
            if(units >= needed)
                return true;
        }
        if(units + upperBound < needed)
            return false;
    }
    return false;
}

MapPoint EconomyPlanner::FindPlaceNear(const Order& order, const Recipe* recipe, const MapPoint anchor,
                                       const unsigned radius, const std::vector<MapPoint>& partners)
{
    const GameWorldBase& world = player_.GetGameWorld();
    const BuildingType bld = order.building;
    const bool harvester = recipe && recipe->reach > 0 && recipe->source.kind != SourceKind::None;
    const BuildingRegister& buildings = player_.GetBuildingRegister();
    // Buildings of the same type nearby: harvesters within twice their reach (they share the source), others within 8
    const unsigned sameTypeRadius = harvester ? 2 * recipe->reach : 8u;
    std::vector<MapPoint> sameType;
    if(BuildingProperties::IsUsual(bld))
    {
        for(const nobUsual* building : buildings.GetBuildings(bld))
            sameType.push_back(building->GetPos());
    } else
    {
        for(const nobBaseWarehouse* building : buildings.GetStorehouses())
            sameType.push_back(building->GetPos());
    }
    for(const noBuildingSite* site : buildings.GetBuildingSites())
    {
        if(site->GetBuildingType() == bld)
            sameType.push_back(site->GetPos());
    }
    // The nearest place meeting all rules, and those up to PLACE_BAND further compete by the preferences of the user
    // (15.11.): more of the source (up to 40), fewer buildings of the same type nearby, nearer to the partners. Equal:
    // the nearest to the anchor
    MapPoint best = MapPoint::Invalid();
    double bestScore = 0;
    unsigned bandEnd = std::numeric_limits<unsigned>::max();
    for(const MapPoint pt : aijh_.GetPlacesAround(bld, anchor, radius))
    {
        const unsigned distance = world.CalcDistance(pt, anchor);
        if(distance > bandEnd)
            break;
        if(!IsPlaceValid(order, recipe, pt))
            continue;
        if(bandEnd == std::numeric_limits<unsigned>::max())
            bandEnd = distance + PLACE_BAND;
        double score = 0;
        if(harvester)
            score += std::min(40u, MapProfile::CountSourceNodes(world, pt, recipe->reach, recipe->source));
        for(const MapPoint other : sameType)
        {
            if(world.CalcDistance(pt, other) <= sameTypeRadius)
                score -= 10;
        }
        if(!partners.empty())
        {
            unsigned nearest = std::numeric_limits<unsigned>::max();
            for(const MapPoint partner : partners)
                nearest = std::min(nearest, world.CalcDistance(pt, partner));
            score -= 0.5 * nearest;
        }
        if(!best.isValid() || score > bestScore)
        {
            best = pt;
            bestScore = score;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// Military buildings (Schritt 3, Nutzer 2026-11-17): the planner decides when, where and which military buildings are
// built. Troops, attacks, coins and demolition stay with the old AI

void EconomyPlanner::AddTerritoryOrder(const TerritoryNeed need, const OrderKind kind, const unsigned parent,
                                       const GoodType ware, const BuildingType bld, const Source& source)
{
    // One for each need: the same one is still asked for
    for(Order& order : orders_)
    {
        if(order.cause == OrderCause::Territory && order.state != OrderState::Placed && order.territory == need
           && order.parent == parent && order.forWare == ware && order.forBuilding == bld)
        {
            order.territoryStep = numSteps_;
            return;
        }
    }
    const auto type = aijh_.GetConstruction().GetSmallestAllowedMilBuilding();
    if(!type)
        return;
    Order order;
    order.building = *type;
    order.cause = OrderCause::Territory;
    order.kind = kind;
    order.parent = parent;
    order.forWare = ware;
    order.forBuilding = bld;
    order.territory = need;
    order.territorySource = source;
    order.territoryStep = numSteps_;
    AddOrder(std::move(order));
}

void EconomyPlanner::AddExpandingTerritoryOrder()
{
    if(helpers::contains_if(orders_, [](const Order& order) {
           return order.cause == OrderCause::Territory && order.territory == TerritoryNeed::Expansion
                  && order.state != OrderState::Placed;
       }))
        return;
    const auto type = aijh_.GetConstruction().GetSmallestAllowedMilBuilding();
    if(!type)
        return;
    Order order;
    order.building = *type;
    order.cause = OrderCause::Territory;
    order.kind = OrderKind::Expanding;
    order.territory = TerritoryNeed::Expansion;
    order.territoryStep = numSteps_;
    // The ware the best region is for (expanding orders are sorted by its forecast)
    GoodType ware = GoodType::Nothing;
    if(ChooseTargetRegion(order, &ware) < 0)
        return;
    order.forWare = ware;
    AddOrder(std::move(order));
}

bool EconomyPlanner::IsTerritoryNeeded(const Order& order) const
{
    switch(order.territory)
    {
        case TerritoryNeed::Place:
            // For an order without a place: while it waits for one. For a way without a place: while it failed
            if(order.parent != 0)
            {
                const Order* parent = Find(order.parent);
                return parent && parent->state == OrderState::Blocked
                       && parent->retry.kind == RetryCondition::Kind::NoPlace;
            }
            return helpers::contains_if(failedWays_, [&](const FailedWay& way) {
                return way.ware == order.forWare && way.building == order.forBuilding;
            });
        case TerritoryNeed::Source:
            // Asked for in the last step and the source is still not in the territory
            return order.territoryStep + 1 >= numSteps_
                   && civAI_.GetMap().GetTotalOwned().GetAmount(order.territorySource) == 0;
        case TerritoryNeed::Expansion: return true;
        case TerritoryNeed::None: break;
    }
    return false;
}

std::vector<EconomyPlanner::ScarceSource> EconomyPlanner::GetScarceSources() const
{
    // Low forecast = high scarcity (Nutzer): 0 from MIN_STOCK on, 1 at 0, more below
    const auto scarcity = [this](const GoodType good) {
        return std::max(0., static_cast<double>(MIN_STOCK) - GetWareStatus(good).forecast) / MIN_STOCK;
    };
    std::vector<ScarceSource> result;
    for(const Recipe& recipe : civAI_.GetGraph().GetRecipes())
    {
        if(!managed_[recipe.building] || recipe.source.kind == SourceKind::None
           || !player_.IsBuildingEnabled(recipe.building))
            continue;
        for(const GoodType good : recipe.outputs)
        {
            if(!hasUse_[good])
                continue;
            const double value = scarcity(good);
            const auto it = std::find_if(result.begin(), result.end(), [&](const ScarceSource& entry) {
                return sameSource(entry.source, recipe.source);
            });
            if(it == result.end())
                result.push_back({recipe.source, good, value});
            else if(value > it->scarcity)
            {
                it->scarcity = value;
                it->ware = good;
            }
        }
    }
    return result;
}

int EconomyPlanner::ChooseTargetRegion(const Order& order, GoodType* ware) const
{
    std::vector<GoodType> wares;
    const std::vector<unsigned> regions = RankTargetRegions(order, &wares);
    if(regions.empty())
        return -1;
    if(ware)
        *ware = wares.front();
    return static_cast<int>(regions.front());
}

std::vector<unsigned> EconomyPlanner::RankTargetRegions(const Order& order, std::vector<GoodType>* wares) const
{
    const GameWorldBase& world = player_.GetGameWorld();
    const MapProfile& map = civAI_.GetMap();
    const BuildingRegister& buildings = player_.GetBuildingRegister();
    // What the region is for: the source of the need (land to build on without one), or all sources by their scarcity
    std::vector<ScarceSource> sources;
    bool land = false;
    if(order.territory == TerritoryNeed::Expansion)
        sources = GetScarceSources();
    else if(order.territorySource.kind != SourceKind::None)
        sources.push_back({order.territorySource, order.forWare, 1.});
    else
        land = true;
    // Own buildings the territory grows from
    std::vector<MapPoint> own;
    for(const nobMilitary* military : buildings.GetMilitaryBuildings())
        own.push_back(military->GetPos());
    for(const nobBaseWarehouse* warehouse : buildings.GetStorehouses())
        own.push_back(warehouse->GetPos());
    if(own.empty())
        return {};
    // Candidate regions: center near an own building, not owned by another player, something outside the territory
    struct Candidate
    {
        unsigned idx, distance, danger, land;
        std::vector<unsigned> amounts;
    };
    std::vector<Candidate> candidates;
    std::vector<unsigned> maxAmount(sources.size(), 0u);
    unsigned maxLand = 0;
    const unsigned playerId = player_.GetPlayerId();
    for(unsigned idx = 0; idx < map.GetNumRegions(); ++idx)
    {
        const MapPoint center = map.GetRegionCenter(idx);
        const unsigned owner = world.GetNode(center).owner;
        if(owner != 0 && owner != playerId + 1u)
            continue;
        unsigned distance = std::numeric_limits<unsigned>::max();
        for(const MapPoint pos : own)
            distance = std::min(distance, world.CalcDistance(center, pos));
        if(distance > MAX_TERRITORY_DISTANCE)
            continue;
        const ResourceProfile& region = map.GetRegion(idx);
        const ResourceProfile& owned = map.GetRegionOwned(idx);
        Candidate candidate{idx, distance, 0u, region.land > owned.land ? region.land - owned.land : 0u, {}};
        bool any = candidate.land > 0;
        for(unsigned i = 0; i < sources.size(); ++i)
        {
            const unsigned all = region.GetAmount(sources[i].source);
            const unsigned inside = owned.GetAmount(sources[i].source);
            candidate.amounts.push_back(all > inside ? all - inside : 0u);
            maxAmount[i] = std::max(maxAmount[i], candidate.amounts.back());
            any |= candidate.amounts.back() > 0;
        }
        if(!land && order.territory != TerritoryNeed::Expansion
           && std::all_of(candidate.amounts.begin(), candidate.amounts.end(), [](const unsigned a) { return a == 0; }))
            continue;
        if(!any)
            continue;
        // Danger: enemy military buildings near the region
        for(const nobBaseMilitary* military : world.LookForMilitaryBuildings(center, 2))
        {
            if(military->GetPlayer() != playerId && !player_.IsAlly(military->GetPlayer())
               && world.CalcDistance(center, military->GetPos()) <= MAX_TERRITORY_DISTANCE)
                ++candidate.danger;
        }
        maxLand = std::max(maxLand, candidate.land);
        candidates.push_back(std::move(candidate));
    }
    // Value = scarcity × amount (each normalized to the largest amount) − distance − danger. Land counts with 1 for a
    // building without a place and a quarter for expanding (more land is always of some use)
    std::vector<std::tuple<double, unsigned, GoodType>> ranked;
    for(const Candidate& candidate : candidates)
    {
        double value = 0, strongest = 0;
        GoodType strongestWare = GoodType::Nothing;
        for(unsigned i = 0; i < sources.size(); ++i)
        {
            if(maxAmount[i] == 0)
                continue;
            const double part = sources[i].scarcity * candidate.amounts[i] / maxAmount[i];
            value += part;
            if(part > strongest)
            {
                strongest = part;
                strongestWare = sources[i].ware;
            }
        }
        if(maxLand > 0)
            value += (land ? 1. : 0.25) * candidate.land / maxLand;
        value -= 0.5 * candidate.distance / MAX_TERRITORY_DISTANCE;
        value -= 1. * candidate.danger;
        ranked.emplace_back(value, candidate.idx,
                            order.territory == TerritoryNeed::Expansion ? strongestWare : order.forWare);
    }
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const auto& lhs, const auto& rhs) { return std::get<0>(lhs) > std::get<0>(rhs); });
    std::vector<unsigned> result;
    for(const auto& [value, idx, ware] : ranked)
    {
        result.push_back(idx);
        if(wares)
            wares->push_back(ware);
    }
    return result;
}

MapPoint EconomyPlanner::SearchMilitaryPlace(Order& order, bool& searchedAll)
{
    // The best regions in turn (at most a few per step): reachable over land from the territory, a place for the type
    // the old AI chooses there (or the smallest one) at the border nearest to the region
    constexpr unsigned MAX_REGIONS_PER_SEARCH = 4;
    searchedAll = true;
    const GameWorldBase& world = player_.GetGameWorld();
    const std::vector<unsigned> regions = RankTargetRegions(order, nullptr);
    AIJH::AIConstruction& construction = aijh_.GetConstruction();
    unsigned tried = 0;
    for(const unsigned region : regions)
    {
        if(tried++ >= MAX_REGIONS_PER_SEARCH)
            break;
        const MapPoint target = civAI_.GetMap().GetRegionCenter(region);
        // The own territory nearest to the region
        MapPoint anchor = MapPoint::Invalid();
        unsigned anchorDistance = std::numeric_limits<unsigned>::max();
        for(const MapPoint pt : world.GetPointsInRadiusWithCenter(target, MAX_TERRITORY_DISTANCE + 2))
        {
            if(world.GetNode(pt).owner != player_.GetPlayerId() + 1u)
                continue;
            const unsigned distance = world.CalcDistance(pt, target);
            if(distance < anchorDistance)
            {
                anchorDistance = distance;
                anchor = pt;
            }
        }
        if(!anchor.isValid())
            continue;
        // Reachable over land (not across water): a way to the region center or a node beside it
        bool reachable = anchorDistance == 0;
        for(const MapPoint pt : world.GetPointsInRadiusWithCenter(target, 1))
        {
            if(reachable)
                break;
            reachable = world.FindHumanPath(anchor, pt, 3 * MAX_TERRITORY_DISTANCE).has_value();
        }
        if(!reachable)
            continue;
        aijh_.UpdateNodesAround(anchor, SEARCH_RADIUS);
        std::vector<BuildingType> types;
        if(const auto chosen = construction.ChooseMilitaryBuilding(anchor))
            types.push_back(*chosen);
        if(!helpers::contains(types, order.building))
            types.push_back(order.building);
        for(const BuildingType type : types)
        {
            if(type != order.building && !HasMaterial(type))
                continue;
            const MapPoint pos = aijh_.FindPositionForBuildingAround(type, anchor);
            if(pos.isValid())
            {
                order.building = type;
                order.near = target;
                return pos;
            }
        }
    }
    return MapPoint::Invalid();
}

// ---------------------------------------------------------------------------------------------------------------------
// Tools: only orders, at least TOOLS_IN_STOCK of each kind beyond what is asked for. The regulator stays at 0

// ---------------------------------------------------------------------------------------------------------------------

unsigned EconomyPlanner::CountStonePiles(const GamePlayer& player, const MapPoint pt, const unsigned radius)
{
    return MapProfile::CountSourceNodes(player.GetGameWorld(), pt, radius, Source{SourceKind::StonePiles, 0});
}

unsigned EconomyPlanner::CountTrees(const GamePlayer& player, const MapPoint pt, const unsigned radius)
{
    return MapProfile::CountSourceNodes(player.GetGameWorld(), pt, radius, Source{SourceKind::Trees, 0});
}

void EconomyPlanner::OnTornDown(const MapPoint pt, const BuildingType bld)
{
    const unsigned gf = player_.GetGameWorld().GetEvMgr().GetCurrentGF();
    std::ostringstream out;
    out << buildingName(bld) << " (" << pt.x << "," << pt.y << ") GF " << gf;
    const auto placed = placements_.find(pt);
    if(placed != placements_.end() && placed->second.building == bld)
    {
        out << ", platziert GF " << placed->second.gf << " ("
            << (placed->second.kind == OrderKind::Needed ? "benötigt: " : "expandierend: ")
            << causeName(placed->second.cause) << ")";
        if(placed->second.sourceInReach >= 0)
            out << " mit " << placed->second.sourceInReach << " Vorkommen in Reichweite";
    } else
        out << ", nicht vom Planer platziert";
    const auto* building = player_.GetGameWorld().GetSpecObj<nobUsual>(pt);
    if(building && building->GetWorkerSinceGF() != nobUsual::NO_GF)
        out << ", Arbeiter seit GF " << building->GetWorkerSinceGF();
    tornDown_.push_back(out.str());
    if(tornDown_.size() > 5u)
        tornDown_.erase(tornDown_.begin());
    LOG.write("ECONOMY player %u: abgerissen, nichts mehr in Reichweite: %s\n") % player_.GetPlayerId() % out.str();
}

std::string EconomyPlanner::GetReport() const
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    const auto writeStatus = [&out](const std::string& name, const WareStatus& status, const bool group) {
        out << " " << name << " (Prognose " << num(status.forecast) << " = frei " << num(status.free)
            << " + Herstellung " << status.forecastProduction << " - Verbrauch " << status.forecastConsumption
            << "; Bestand " << status.stock << ", angefragt: offen " << num(status.openDemand) << ", geplant "
            << num(status.planned);
        if(group && status.missing > 1e-9)
            out << ", davon bekommt keine Ware " << num(status.missing);
        out << "; unterwegs zum Abnehmer " << status.inTransit;
        // The evaluation of shortages (construction costs x BUILDING_MATERIAL_FACTOR): it makes the orders
        if(std::abs(status.evaluated - status.forecast) > 0.005)
            out << "; bewertet " << num(status.evaluated);
        out << ");";
    };
    out << "  Mangel:";
    for(const WareGroup& group : groups_)
    {
        if(group.demanded && group.status.state == SupplyState::Shortage)
            writeStatus(wareNames(group.wares), group.status, group.wares.size() > 1u);
    }
    if(!shortageNotes_.empty())
    {
        out << "\n  Mangel ohne eigenen Auftrag:";
        for(const auto& [group, note] : shortageNotes_)
            out << " " << wareNames(groups_[group].wares) << " (" << note << ");";
    }
    out << "\n  Überschuss:";
    for(const WareGroup& group : groups_)
    {
        if(group.demanded && group.status.state == SupplyState::Surplus)
            out << " " << wareNames(group.wares) << " (Prognose " << num(group.status.forecast) << ");";
    }
    // Distribution of all wares (rule of the game): value 0..1000, number (forecast or number of wares), state
    if(civ::isDistributionActive(player_.GetGameWorld()))
    {
        const civ::WareDistribution& distribution = player_.GetWareDistribution();
        out << "\n  Gesamtverteilung (" << (distribution.UsesForecast() ? "Prognose" : "Warenzahl") << "):";
        for(const auto good : helpers::enumRange<GoodType>())
        {
            if(!civ::isInDistribution(good))
                continue;
            const char* state = "";
            switch(distribution.GetState(good))
            {
                case civ::WareDistState::NoBuilding: continue;
                case civ::WareDistState::Valued: state = ""; break;
                case civ::WareDistState::Surplus: state = " Überschuss"; break;
                case civ::WareDistState::Paused: state = " angehalten"; break;
            }
            out << " " << wareName(good) << " "
                << distribution.GetValue(good) * civ::DISTRIBUTION_SHOWN / civ::DISTRIBUTION_SCALE << " ("
                << num(static_cast<double>(distribution.GetNumber(good)) / civ::AMOUNT_UNIT) << state;
            if(distribution.GetOrders(good) > 0)
                out << ", bestellt " << distribution.GetOrders(good);
            out << ");";
        }
    }
    out << "\n  Soldaten:";
    writeStatus(soldiers_.state == SupplyState::Shortage ? "Mangel" : "kein Mangel", soldiers_, false);
    for(const auto& [good, note] : soldierNotes_)
        out << " " << wareName(good) << " ohne Auftrag: " << note << ";";
    out << "\n  Gehilfen:";
    writeStatus(helpers_.state == SupplyState::Shortage ? "Mangel" : "kein Mangel", helpers_, false);
    // Problems of the buildings (diagnosis of every building in this step), per type
    static const char* const problemNames[] = {"",
                                               "gestoppt",
                                               "keine Straße",
                                               "kein Arbeiter",
                                               "Arbeiter unterwegs",
                                               "kein Werkzeug",
                                               "Werkzeugplatz leer",
                                               "Eingang fehlt",
                                               "Fahne voll",
                                               "nichts mehr in Reichweite",
                                               "Felder fehlen",
                                               "Vorkommen weit entfernt",
                                               "schlechtes Werkzeug"};
    std::map<BuildingType, std::map<unsigned, unsigned>> problems;
    for(const auto& [pos, entry] : diagnosisCache_)
    {
        const auto* building = player_.GetGameWorld().GetSpecObj<nobUsual>(pos);
        if(entry.first != Diagnosis::Working && building)
            ++problems[building->GetBuildingType()][static_cast<unsigned>(entry.first)];
    }
    out << "\n  Probleme:";
    for(const auto& [bld, counts] : problems)
    {
        out << " " << buildingName(bld) << ":";
        bool first = true;
        for(const auto& [problem, num] : counts)
        {
            out << (first ? " " : ", ") << num << " " << problemNames[problem];
            first = false;
        }
        out << ";";
    }
    // Building sites: military (old AI) and economy, without road, waiting for material (none on the way, the
    // warehouses have none of it) and the material they still need
    {
        unsigned numMilitary = 0, numEconomy = 0, noRoad = 0, waiting = 0, needMilitary = 0, needEconomy = 0;
        for(const noBuildingSite* site : player_.GetBuildingRegister().GetBuildingSites())
        {
            const bool military = BuildingProperties::IsMilitary(site->GetBuildingType());
            (military ? numMilitary : numEconomy)++;
            if(!site->IsConnected())
                ++noRoad;
            const BuildingCosts& costs = BUILDING_COSTS[site->GetBuildingType()];
            const auto& materials = site->GetMaterials();
            unsigned need = 0;
            bool onTheWay = false;
            for(unsigned i = 0; i < costs.size() && i < materials.size(); ++i)
            {
                const unsigned have = materials[i].used + materials[i].delivered.size();
                need += costs[i].amount > have ? costs[i].amount - have : 0u;
                onTheWay |= !materials[i].ordered.empty();
            }
            (military ? needMilitary : needEconomy) += need;
            if(need > 0 && !onTheWay && site->IsConnected())
                ++waiting;
        }
        out << "\n  Baustellen: Militär " << numMilitary << " (braucht noch " << needMilitary << "), Wirtschaft "
            << numEconomy << " (braucht noch " << needEconomy << "), ohne Straße " << noRoad
            << ", ohne Material unterwegs " << waiting;
    }
    if(snapshot_.storageCapacity > 0)
    {
        out << "\n  Lager: " << snapshot_.storageLoad << " von " << snapshot_.storageCapacity
            << (IsStorageFull() ? " (voll: nur benötigte Aufträge)" : "");
    }
    out << "\n  Liste:\n";
    unsigned pos = 0;
    static const char* const stateNames[] = {"offen", "wartet", "blockiert", "platziert"};
    for(const Order* order : GetList())
    {
        out << "    " << ++pos << ". " << buildingName(order->building)
            << (order->kind == OrderKind::Needed ? " (benötigt: " : " (expandierend: ") << causeName(order->cause);
        const unsigned level = player_.GetGameWorld().GetTechTree().GetAIBasicEconomyLevel(order->building);
        if(level > 0 && order->kind == OrderKind::Needed)
            out << ", Stufe " << level;
        if(order->forWare != GoodType::Nothing)
            out << ", für " << wareName(order->forWare);
        if(order->usesWare != GoodType::Nothing)
            out << ", verarbeitet " << wareName(order->usesWare);
        // What it is for: "Water für Brewery"
        if(const Order* parent = order->parent ? Find(order->parent) : nullptr)
            out << " für " << buildingName(parent->building);
        for(const unsigned otherId : order->alsoFor)
        {
            if(const Order* other = Find(otherId))
                out << (order->parent ? ", " : " für ") << buildingName(other->building);
        }
        if(order->cause == OrderCause::Renewal && !order->parent)
            out << " für " << buildingName(order->forBuilding);
        if(order->cause == OrderCause::Territory)
        {
            if(order->territory == TerritoryNeed::Place)
                out << (order->parent ? "" : " für " + buildingName(order->forBuilding)) << ", kein Bauplatz";
            else if(order->territory == TerritoryNeed::Source)
                out << " für " << buildingName(order->forBuilding) << ", "
                    << civAI_.GetGraph().GetSourceName(order->territorySource) << " nur außerhalb";
            if(order->near.isValid())
                out << ", Ziel " << pointName(order->near);
        }
        if(order->cause == OrderCause::Shortage && groups_[order->group].wares.size() > 1u)
            out << " (Gruppe " << wareNames(groups_[order->group].wares) << ")";
        if(order->clearing)
            out << ", rodend";
        if(order->besideRenewer)
            out << ", neben " << pointName(order->near);
        // Waiting for resources: it counts in the forecast like an open or waiting order, it is not blocked
        out << ") " << (WaitsForResources(*order) ? "wartet" : stateNames[static_cast<unsigned>(order->state)]);
        if(!order->reason.empty())
            out << ": " << order->reason;
        // Its inputs without an order of their own and why (e.g. "Grain: Farm: kein Bauplatz")
        const auto notes = inputNotes_.find(order->id);
        if(order->state != OrderState::Placed && notes != inputNotes_.end())
        {
            out << " – ohne eigenen Auftrag: ";
            for(unsigned i = 0; i < notes->second.size(); ++i)
                out << (i ? "; " : "") << notes->second[i];
        }
        out << "\n";
    }
    if(!tornDown_.empty())
    {
        out << "  Zuletzt abgerissen (nichts mehr in Reichweite):";
        for(const std::string& entry : tornDown_)
            out << "\n    " << entry;
        out << "\n";
    }
    out << "  Zeit: " << static_cast<unsigned>(lastStepUs_) << " us (max " << static_cast<unsigned>(maxStepUs_)
        << " us)\n";
    return out.str();
}

} // namespace civai
