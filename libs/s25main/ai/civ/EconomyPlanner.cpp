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
#include "figures/nofBuildingWorker.h"
#include "figures/nofCustomPlanter.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "world/GameWorld.h"
#include "nodeObjs/noDeposit.h"
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
#include <limits>
#include <sstream>

namespace civai {

namespace {
    using Clock = std::chrono::steady_clock;

    /// The report is written to the log every this many GFs (1 game minute)
    constexpr unsigned REPORT_GFS = 1200;

    std::string wareName(const GoodType good)
    {
        return good == GoodType::Nothing ? std::string("-") : WARE_NAMES[good];
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

    /// How good the tool is: work speed and output bonus first, then durability. 0 for wares which are no tools
    unsigned getToolQuality(const GoodType good)
    {
        const civ::ToolInfo* info = civ::getActiveContent().GetTool(good);
        if(!info)
            return 0;
        return (info->speed + info->outputRatio) * 1000u + std::min(info->durability, 999u);
    }

    std::vector<GoodType> sorted(std::vector<GoodType> wares)
    {
        std::sort(wares.begin(), wares.end());
        wares.erase(std::unique(wares.begin(), wares.end()), wares.end());
        return wares;
    }

    /// Every ware of `entry` is part of `group` (both sorted)
    bool isSubset(const std::vector<GoodType>& entry, const std::vector<GoodType>& group)
    {
        return std::includes(group.begin(), group.end(), entry.begin(), entry.end());
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

    /// Wares per minute one producer of the recipe makes of the ware by the work times of the game data
    double nominalOutput(const Recipe& recipe)
    {
        return recipe.nominalStepsPerMinute / std::max<size_t>(1u, recipe.outputs.size());
    }

    const char* causeName(const OrderCause cause)
    {
        switch(cause)
        {
            case OrderCause::Shortage: return "Mangel";
            case OrderCause::Input: return "Eingang";
            case OrderCause::Material: return "Baumaterial";
            case OrderCause::Idle: return "steht still";
            case OrderCause::Tools: return "Werkzeughersteller";
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
    const std::vector<Recipe>& recipes = graph.GetRecipes();
    for(unsigned i = 0; i < recipes.size(); ++i)
    {
        const Recipe& recipe = recipes[i];
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
    capacityCache_.clear();
    reasonsSolved_.clear();
    diagnosisCache_.clear();
    followUp_ = false;
    shortageNotes_.clear();
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
    UpdateWareStatus();
    UpdateSoldierStatus();
    UpdateToolNeeds();
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
    group.tools = !wares.empty() && std::all_of(wares.begin(), wares.end(), IsTool);
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
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(good != GoodType::Nothing)
            singleGroup_[good] = GetGroupIdx({good}, false);
    }
    // What the consumers take: inputs (each one, or all of them for "any of"), construction costs, tools of a slot and
    // of a kind, soldier equipment, the wares of the military buildings, research costs, knowledge media
    for(const Recipe& recipe : civAI_.GetGraph().GetRecipes())
    {
        if(recipe.consumeEach)
        {
            for(const GoodType input : recipe.inputs)
                GetGroupIdx({input}, true);
        } else if(!recipe.inputs.empty())
            GetGroupIdx(recipe.inputs, true);
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
    }
    helpers::EnumArray<std::vector<GoodType>, Tool> toolKinds;
    for(const auto good : helpers::enumRange<GoodType>())
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
    return groups_[singleGroup_[good]].status;
}

const WareGroup* EconomyPlanner::FindGroup(std::vector<GoodType> wares) const
{
    const auto it = groupIndex_.find(sorted(std::move(wares)));
    return it == groupIndex_.end() ? nullptr : &groups_[it->second];
}

bool EconomyPlanner::IsPlanned(const Order& order) const
{
    // Expanding orders are wishes: planned only once they are placed (building site)
    if(order.state == OrderState::Placed || order.kind != OrderKind::Needed)
        return false;
    return !IsChainBlocked(order.id);
}

void EconomyPlanner::AddPlannedEntry(const OpenDemand& entry, const bool updateStatus)
{
    OpenDemand planned = entry;
    planned.wares = sorted(std::move(planned.wares));
    if(updateStatus)
    {
        for(WareGroup& group : groups_)
        {
            if(isSubset(planned.wares, group.wares))
            {
                group.status.planned += planned.amount;
                group.status.free -= static_cast<int>(planned.amount);
            }
        }
    }
    planned_.push_back(std::move(planned));
}

void EconomyPlanner::AddPlanned(const BuildingType bld, const bool updateStatus, const bool withCosts,
                                const bool withInputs, const bool withTools)
{
    std::vector<OpenDemand> demand;
    if(withCosts)
    {
        for(const CostEntry& entry : BUILDING_COSTS[bld])
            demand.push_back({entry.wares, entry.amount, WareUse::Construction});
    }
    if(withInputs && BuildingProperties::IsUsual(bld))
        addInputSpaces(demand, bld);
    if(withTools)
        AddToolDemand(demand, bld);
    for(const OpenDemand& entry : demand)
        AddPlannedEntry(entry, updateStatus);
}

void EconomyPlanner::AddToolDemand(std::vector<OpenDemand>& demand, const BuildingType bld) const
{
    if(!BuildingProperties::IsUsual(bld))
        return;
    const civ::Content& content = civ::getActiveContent();
    // The tool to recruit its worker, unless a worker waits in a warehouse
    const auto job = getJob(bld);
    if(job && snapshot_.inWarehousesPeople[*job] <= snapshot_.jobsWanted[*job] && !content.IsRecruitedWithoutTool(*job))
    {
        GoodType tool = GoodType::Nothing;
        if(isCustomJob(*job))
        {
            if(const civ::CustomWorker* worker = content.GetWorker(*job))
                tool = worker->tool;
        } else
            tool = JOB_CONSTS[*job].tool.value_or(GoodType::Nothing);
        if(tool != GoodType::Nothing)
            demand.push_back({{tool}, 1, WareUse::Tool});
    }
    // Tool reform: one tool per slot
    for(const civ::BuildingTools::Slot& slot : content.GetBuildingTools(bld).slots)
    {
        if(!slot.wares.empty())
            demand.push_back({slot.wares, 1, WareUse::Tool});
    }
}

unsigned EconomyPlanner::GetNumEquippable(const BuildingType bld) const
{
    std::vector<OpenDemand> demand;
    AddToolDemand(demand, bld);
    unsigned result = std::numeric_limits<unsigned>::max();
    for(const OpenDemand& entry : demand)
    {
        unsigned stock = 0;
        for(const GoodType good : entry.wares)
            stock += snapshot_.inWarehouses[good];
        // A tool of any material will do for the worker
        if(entry.wares.size() == 1u)
            stock = GetToolStock(entry.wares.front());
        result = std::min(result, stock);
    }
    return std::max(1u, result);
}

void EconomyPlanner::UpdatePlanned()
{
    planned_.clear();
    plannedOrderIds_.clear();
    // Input spaces of the building sites: the buildings will ask for them. And the tools they will ask for
    for(const OpenDemand& demand : snapshot_.siteInputs)
        AddPlannedEntry(demand, false);
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        for(unsigned i = snapshot_.buildings[bld].numSites; i > 0; --i)
            AddPlanned(bld, false, false, false, true);
    }
    // Building sites ordered whose commands are on their way (also those of the old AI)
    const AIJH::AIConstruction& construction = aijh_.GetConstruction();
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        for(unsigned i = construction.GetNumPendingSites(bld); i > 0; --i)
            AddPlanned(bld, false, true, true, true);
    }
    // Needed orders of the list that will be built when the resources are there: their construction material (their
    // inputs count once they are placed, otherwise several orders waiting for the same input block each other)
    for(const Order& order : orders_)
    {
        if(!IsPlanned(order))
            continue;
        AddPlanned(order.building, false, true, false, true);
        plannedOrderIds_.push_back(order.id);
    }
}

void EconomyPlanner::FinishStatus(WareStatus& status) const
{
    status.forecast = status.free + status.forecastProduction - status.forecastConsumption;
    const double threshold = std::max(static_cast<double>(MIN_STOCK), SURPLUS_FACTOR * status.consumptionPerMinute);
    if(status.forecast < 0)
        status.state = SupplyState::Shortage;
    else if(status.forecast - static_cast<int>(status.startStockLeft) > threshold)
        status.state = SupplyState::Surplus;
    else
        status.state = SupplyState::Normal;
    status.missingPerMinute =
      status.state == SupplyState::Shortage ? -status.forecast / static_cast<double>(BALANCE_MINUTES) : 0.;
}

void EconomyPlanner::UpdateWareStatus()
{
    // What the game asks for: every entry is a group a consumer takes (registered if it is new). Tools asked for by
    // workers and tool slots count for the tools of their kind
    const auto isWare = [](const GoodType good) { return good != GoodType::Nothing; };
    for(const std::vector<OpenDemand>* entries : {&snapshot_.openDemand, &planned_})
    {
        for(const OpenDemand& demand : *entries)
        {
            if(demand.purpose != WareUse::Tool && std::any_of(demand.wares.begin(), demand.wares.end(), isWare))
                GetGroupIdx(demand.wares, true);
        }
    }
    // First without the buildings standing still: whether those waiting for inputs will get them depends on it
    baseReady_ = false;
    ComputeFlows(false);
    ComputeStatuses();
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(good != GoodType::Nothing)
            baseForecast_[good] = groups_[singleGroup_[good]].status.forecast;
    }
    baseReady_ = true;
    ComputeFlows(true);
    ComputeStatuses();
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

bool EconomyPlanner::WillGetInputs(const Recipe& recipe, const std::vector<GoodType>& missingInputs) const
{
    if(!baseReady_)
        return false;
    if(recipe.consumeEach)
    {
        return std::all_of(missingInputs.begin(), missingInputs.end(),
                           [this](const GoodType good) { return baseForecast_[good] >= 0; });
    }
    // Any of its inputs
    const WareGroup* group = FindGroup(recipe.inputs);
    return group && group->status.forecast >= 0;
}

void EconomyPlanner::ComputeFlows(const bool withStanding)
{
    const EconomyStatistics& stats = player_.GetEconomyStatistics();
    const double coveredMinutes = stats.GetCoveredMinutes(BALANCE_MINUTES, currentGF_);
    const double length = BALANCE_MINUTES;
    // Measured: what was made and used in the last balance length
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(good == GoodType::Nothing)
            continue;
        wareProduction_[good] = stats.GetProduced(good, BALANCE_MINUTES, currentGF_) / coveredMinutes * length;
        // Used as input of buildings. What is used for construction, tools, soldiers, research ... is asked for
        // before it is used: it is part of "requested" (counting it here too would count it twice)
        wareConsumption_[good] =
          stats.GetUsed(good, WareUse::Production, BALANCE_MINUTES, currentGF_) / coveredMinutes * length;
    }
    groupConsumption_.clear();
    // Coming: building sites, sites ordered and buildings starting up (the part of their work not in the statistic
    // yet), by their work time. Standing: buildings standing still for a reason that is worked on, with what they
    // make and use once it is solved (those waiting for inputs only if they will get them)
    const AIJH::AIConstruction& construction = aijh_.GetConstruction();
    const unsigned startupGFs = BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    std::vector<BuildingType> counted;
    for(const Recipe& recipe : civAI_.GetGraph().GetRecipes())
    {
        if(!managed_[recipe.building] || helpers::contains(counted, recipe.building))
            continue;
        counted.push_back(recipe.building);
        double factor =
          snapshot_.buildings[recipe.building].numSites + construction.GetNumPendingSites(recipe.building);
        for(const nobUsual* building : player_.GetBuildingRegister().GetBuildings(recipe.building))
        {
            std::vector<GoodType> missing;
            switch(GetDiagnosis(*building, recipe, missing))
            {
                case Diagnosis::StartingUp:
                {
                    const auto since = workingSince_.find(building->GetPos());
                    const double done = since == workingSince_.end() ?
                                          0. :
                                          std::min(1., static_cast<double>(currentGF_ - since->second) / startupGFs);
                    factor += 1. - done;
                    break;
                }
                case Diagnosis::NoInput:
                    if(!withStanding || !WillGetInputs(recipe, missing))
                        break;
                    [[fallthrough]];
                case Diagnosis::NoWorker:
                case Diagnosis::NoTool:
                case Diagnosis::FlagFull:
                    if(withStanding)
                        factor += std::max(0., 1. - (building->HasWorker() ? building->GetProductivity() / 100. : 0.));
                    break;
                case Diagnosis::Working:
                case Diagnosis::Exhausted:
                case Diagnosis::Unknown: break;
            }
        }
        if(factor <= 0)
            continue;
        // Tool makers work on order: what they make and use is counted with the tools (GetToolMakerCapacity)
        if(IsToolMaker(recipe))
            continue;
        const double steps = recipe.nominalStepsPerMinute * factor * length;
        for(const GoodType good : recipe.outputs)
            wareProduction_[good] += steps / recipe.outputs.size();
        for(unsigned i = 0; i < recipe.byproducts.size(); ++i)
        {
            const int tech = recipe.byproductTechs[i];
            if(tech < 0 || player_.GetResearch().IsResearched(static_cast<civ::TechIdx>(tech)))
                wareProduction_[recipe.byproducts[i]] += steps * recipe.byproductAmounts[i];
        }
        if(recipe.consumeEach)
        {
            for(const GoodType good : recipe.inputs)
                wareConsumption_[good] += steps;
        } else if(!recipe.inputs.empty())
            groupConsumption_.emplace_back(sorted(recipe.inputs), steps);
    }
}

void EconomyPlanner::ComputeStatuses()
{
    const EconomyStatistics& stats = player_.GetEconomyStatistics();
    const civ::TechTree& techTree = player_.GetGameWorld().GetTechTree();

    // Per ware once
    helpers::EnumArray<WareStatus, GoodType> wares;
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(good == GoodType::Nothing)
            continue;
        WareStatus& status = wares[good];
        status.stock = snapshot_.inWarehouses[good];
        status.balance = stats.GetBalance(good, BALANCE_MINUTES, currentGF_);
        status.consumptionPerMinute = stats.GetConsumptionPerMinute(good, BALANCE_MINUTES, currentGF_);
        status.inTransit = snapshot_.inTransit[good];
        // Start stock (wares added, not produced) not used up yet
        if(techTree.IsAIStartStockNoSurplus(good))
        {
            uint32_t used = 0;
            for(const auto use : helpers::enumRange<WareUse>())
                used += stats.GetTotalUsed(good, use);
            const uint32_t added = stats.GetTotalGained(good, WareSource::Added);
            status.startStockLeft = added > used ? added - used : 0u;
        }
    }
    std::vector<std::vector<GoodType>> openWares, plannedWares;
    for(const OpenDemand& demand : snapshot_.openDemand)
        openWares.push_back(sorted(demand.wares));
    for(const OpenDemand& demand : planned_)
        plannedWares.push_back(demand.wares);
    for(WareGroup& group : groups_)
    {
        WareStatus status;
        for(const GoodType good : group.wares)
        {
            status.stock += wares[good].stock;
            status.balance += wares[good].balance;
            status.consumptionPerMinute += wares[good].consumptionPerMinute;
            status.inTransit += wares[good].inTransit;
            status.startStockLeft += wares[good].startStockLeft;
            status.forecastProduction += wareProduction_[good];
            status.forecastConsumption += wareConsumption_[good];
        }
        // Entries the wares of the group can serve completely (an entry "any of" more wares is a group of its own)
        for(unsigned i = 0; i < openWares.size(); ++i)
        {
            if(isSubset(openWares[i], group.wares))
                status.openDemand += snapshot_.openDemand[i].amount;
        }
        for(unsigned i = 0; i < plannedWares.size(); ++i)
        {
            if(isSubset(plannedWares[i], group.wares))
                status.planned += planned_[i].amount;
        }
        for(const auto& [anyOf, amount] : groupConsumption_)
        {
            if(isSubset(anyOf, group.wares))
                status.forecastConsumption += amount;
        }
        // The tools of a kind the player keeps in stock are asked for too (so their maker comes before they are
        // missing)
        if(group.tools && helpers::contains(toolKindGroup_, static_cast<unsigned>(&group - groups_.data())))
            status.planned += TOOLS_IN_STOCK;
        status.free = static_cast<int>(status.stock) - static_cast<int>(status.openDemand + status.planned);
        FinishStatus(status);
        group.status = status;
    }
}

void EconomyPlanner::AddComingBuilding(const BuildingType bld)
{
    const Recipe* recipe = FindRecipe(bld);
    if(!recipe)
        return;
    for(const GoodType good : recipe->outputs)
        capacityCache_.erase(good);
    for(const GoodType good : recipe->byproducts)
        capacityCache_.erase(good);
    if(IsToolMaker(*recipe))
        return;
    const double steps = recipe->nominalStepsPerMinute * BALANCE_MINUTES;
    for(WareGroup& group : groups_)
    {
        bool changed = false;
        for(const GoodType good : group.wares)
        {
            if(helpers::contains(recipe->outputs, good))
            {
                group.status.forecastProduction += steps / recipe->outputs.size();
                changed = true;
            }
            if(recipe->consumeEach && helpers::contains(recipe->inputs, good))
            {
                group.status.forecastConsumption += steps;
                changed = true;
            }
        }
        if(!recipe->consumeEach && !recipe->inputs.empty() && isSubset(sorted(recipe->inputs), group.wares))
        {
            group.status.forecastConsumption += steps;
            changed = true;
        }
        if(changed)
            FinishStatus(group.status);
    }
}

void EconomyPlanner::UpdateSoldierStatus()
{
    // Soldiers count like a ware: the stock in the warehouses, made by recruiting, used when they leave for a military
    // building, asked for by the military buildings
    const EconomyStatistics& stats = player_.GetEconomyStatistics();
    const double coveredMinutes = stats.GetCoveredMinutes(BALANCE_MINUTES, currentGF_);
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
    status.free = static_cast<int>(status.stock) - static_cast<int>(status.openDemand);
    FinishStatus(status);
    soldiers_ = status;
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
    // Sites placed by the planner: forgotten when the building or site is gone
    for(auto it = placements_.begin(); it != placements_.end();)
    {
        const auto* building = world.GetSpecObj<noBaseBuilding>(it->first);
        const bool there = building && building->GetPlayer() == player_.GetPlayerId()
                           && building->GetBuildingType() == it->second.building;
        it = (there || construction.IsSitePending(it->first)) ? std::next(it) : placements_.erase(it);
    }
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
    // Also for a tool maker: the more tools are missing, the more it will use (not: the less its input suffices)
    return statusSuffices(GetWareStatus(input), consumer.nominalStepsPerMinute);
}

bool EconomyPlanner::IsToolMaker(const Recipe& recipe)
{
    return !recipe.outputs.empty() && std::all_of(recipe.outputs.begin(), recipe.outputs.end(), IsTool);
}

bool EconomyPlanner::InputsSuffice(const Recipe& recipe, const GoodType given) const
{
    if(recipe.inputs.empty())
        return true;
    if(recipe.consumeEach)
    {
        return std::all_of(recipe.inputs.begin(), recipe.inputs.end(),
                           [&](const GoodType input) { return input == given || InputSuffices(input, recipe); });
    }
    // Any of the inputs: the group of all of them
    if(helpers::contains(recipe.inputs, given))
        return true;
    const WareGroup* group = FindGroup(recipe.inputs);
    return group && statusSuffices(group->status, recipe.nominalStepsPerMinute);
}

unsigned EconomyPlanner::GetBuildingLimit() const
{
    const civ::TechTree& techTree = player_.GetGameWorld().GetTechTree();
    // Land of an HQ: the nodes in its radius
    constexpr unsigned startArea = 1 + 3 * HQ_RADIUS * (HQ_RADIUS + 1);
    const unsigned land = player_.GetStatisticCurrentValue(StatisticType::Country);
    const unsigned numStartAreas = std::max(1u, land / startArea);
    return techTree.GetAIBuildingLimitBase() + techTree.GetAIBuildingLimitPerStartArea() * numStartAreas;
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
    /// highest productivity of the existing producers, lowest construction costs
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
        if(lhs.productivity != rhs.productivity)
            return lhs.productivity > rhs.productivity;
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
    result.productivity = GetProductivity(recipe.building).first;
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
            }
            result.missingInputs.push_back(input);
            return {false, 0u};
        }
        return {true, ways.front().numNew};
    };
    for(const GoodType input : recipe.inputs)
    {
        if(input != given && GetWareStatus(input).state == SupplyState::Surplus)
            result.usesSurplus = true;
    }
    if(recipe.consumeEach)
    {
        for(const GoodType input : recipe.inputs)
        {
            const auto [possible, numNew] = inputCost(input);
            if(!possible)
                result.blocked = true;
            result.numNew += numNew;
        }
    } else if(!recipe.inputs.empty() && !InputsSuffice(recipe, given))
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
        {
            result.numNew += best;
            result.missingInputs.clear();
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
    missingInputs.clear();
    if(building.IsProductionDisabled())
        return Diagnosis::Working; // The player stopped it: not examined
    if(!building.HasWorker())
        return Diagnosis::NoWorker;
    const unsigned startupGFs = BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    const auto since = workingSince_.find(building.GetPos());
    if(since == workingSince_.end() || since->second + startupGFs > currentGF_)
        return Diagnosis::StartingUp;
    if(building.GetProductivity() >= HIGH_PRODUCTIVITY * 100)
        return Diagnosis::Working;
    if(!building.GetToolSlots().empty() && building.GetWorkSpeed() == 0)
        return Diagnosis::NoTool;
    const nofBuildingWorker* worker = building.GetWorker();
    if(worker && worker->GetState() == nofBuildingWorker::State::WaitForWareSpace)
        return Diagnosis::FlagFull;
    if(worker && worker->GetState() == nofBuildingWorker::State::WaitingForWaresOrProductionStopped)
    {
        const BldWorkDescription& workDesc = BLD_WORK_DESC[building.GetBuildingType()];
        bool anyThere = false;
        for(unsigned i = 0; i < workDesc.waresNeeded.size(); ++i)
        {
            if(workDesc.waresNeeded[i] == GoodType::Nothing)
                continue;
            if(building.GetNumWares(i) == 0)
                missingInputs.push_back(workDesc.waresNeeded[i]);
            else
                anyThere = true;
        }
        const bool canWork = workDesc.useOneWareEach ? missingInputs.empty() : anyThere;
        if(!canWork && !missingInputs.empty())
            return Diagnosis::NoInput;
        missingInputs.clear();
    }
    if(recipe.reach > 0
       && MapProfile::CountSourceNodes(player_.GetGameWorld(), building.GetPos(), recipe.reach, recipe.source) == 0)
        return Diagnosis::Exhausted;
    return Diagnosis::Unknown;
}

EconomyPlanner::Capacity EconomyPlanner::ProducerCapacity(const GoodType good, bool solveReasons)
{
    // The reasons are worked on once per step and ware
    solveReasons &= !helpers::contains(reasonsSolved_, good);
    const auto cached = capacityCache_.find(good);
    if(cached != capacityCache_.end() && !solveReasons)
        return cached->second;
    Capacity result;
    const EconomyGraph& graph = civAI_.GetGraph();
    const AIJH::AIConstruction& construction = aijh_.GetConstruction();
    const unsigned startupGFs = BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    std::vector<BuildingType> counted;
    // Reasons to work on, collected first: solving them may add orders
    std::vector<std::pair<BuildingType, std::vector<GoodType>>> missingInputs;
    bool missingWorker = false;
    for(const unsigned idx : graph.GetProducers(good))
    {
        const Recipe& recipe = graph.GetRecipes()[idx];
        if(!managed_[recipe.building] || helpers::contains(counted, recipe.building))
            continue;
        counted.push_back(recipe.building);
        const double perMinute = nominalOutput(recipe);
        // Coming: building sites and sites ordered (not in the game yet), also those of expanding orders
        const unsigned numSites =
          snapshot_.buildings[recipe.building].numSites + construction.GetNumPendingSites(recipe.building);
        result.coming += perMinute * numSites;
        result.numBuildings += numSites;
        for(const nobUsual* building : player_.GetBuildingRegister().GetBuildings(recipe.building))
        {
            ++result.numBuildings;
            std::vector<GoodType> missing;
            switch(GetDiagnosis(*building, recipe, missing))
            {
                case Diagnosis::Working: ++result.numBusy; break;
                case Diagnosis::StartingUp:
                {
                    // Its output comes into the balance bit by bit: the part not in it yet
                    const auto since = workingSince_.find(building->GetPos());
                    const double done = since == workingSince_.end() ?
                                          0. :
                                          std::min(1., static_cast<double>(currentGF_ - since->second) / startupGFs);
                    result.coming += perMinute * (1. - done);
                    break;
                }
                case Diagnosis::NoWorker:
                case Diagnosis::NoTool:
                case Diagnosis::NoInput:
                case Diagnosis::FlagFull:
                {
                    const double productivity = building->HasWorker() ? building->GetProductivity() / 100. : 0.;
                    result.reserve += perMinute * std::max(0., 1. - productivity);
                    ++result.numIdle;
                    if(building->HasWorker())
                        ++result.numIdleWorking;
                    else
                        missingWorker = true;
                    if(!missing.empty())
                        missingInputs.emplace_back(recipe.building, std::move(missing));
                    break;
                }
                case Diagnosis::Exhausted:
                    // Makes nothing anymore: its worker counts as working, its output is 0
                    ++result.numExhausted;
                    break;
                case Diagnosis::Unknown: break;
            }
        }
    }
    capacityCache_[good] = result;
    if(!solveReasons)
        return result;
    reasonsSolved_.push_back(good);
    // The reasons that are worked on: inputs (one order per input and building type standing still), helpers for the
    // workers. Tools come with the tool orders (OrderTools, ResolveTools), a full flag with transport and storage
    // Per input and building type: what the buildings standing still would use per minute by their work time
    std::vector<std::tuple<BuildingType, GoodType, double>> inputNeeds;
    for(const auto& [bld, wares] : missingInputs)
    {
        const Recipe* recipe = FindRecipe(bld);
        const bool each = !recipe || recipe->consumeEach;
        const std::vector<GoodType> toResolve = each ? wares : std::vector<GoodType>{ChooseWare(wares)};
        for(const GoodType input : toResolve)
        {
            if(input == GoodType::Nothing)
                continue;
            const double perMinute = recipe ? recipe->nominalStepsPerMinute : 0.;
            // A structured binding cannot be captured in C++17 (MSVC)
            const BuildingType building = bld;
            const auto it = std::find_if(inputNeeds.begin(), inputNeeds.end(), [&](const auto& entry) {
                return std::get<0>(entry) == building && std::get<1>(entry) == input;
            });
            if(it == inputNeeds.end())
                inputNeeds.emplace_back(building, input, perMinute);
            else
                std::get<2>(*it) += perMinute;
        }
    }
    for(const auto& [bld, input, perMinute] : inputNeeds)
    {
        Need need;
        need.cause = OrderCause::Idle;
        need.forBuilding = bld;
        // What the buildings standing still would use, beyond what the forecast of the input (without them) leaves
        need.perMinute = std::max(0., perMinute - baseForecast_[input] / BALANCE_MINUTES);
        need.maxOrders = 1;
        ResolveShortage(input, need);
    }
    if(missingWorker && GetFreeHelpers() <= 0)
        AddHelpersOrder(0);
    return result;
}

double EconomyPlanner::OutputPerProducer(const GoodType good, const Recipe& recipe) const
{
    // A tool maker makes what is ordered: all its work steps count
    if(IsToolMaker(recipe))
        return recipe.nominalStepsPerMinute;
    // Measured: what the producers that work made per minute, otherwise by the work time
    const EconomyStatistics& stats = player_.GetEconomyStatistics();
    const double minutes = stats.GetCoveredMinutes(BALANCE_MINUTES, currentGF_);
    const unsigned numWorking = GetNumWorkingProducers(good);
    const unsigned produced = stats.GetProduced(good, BALANCE_MINUTES, currentGF_);
    // Without those standing still or with nothing left in reach (they have a worker and finished their start-up, but
    // make nothing)
    const auto cached = capacityCache_.find(good);
    const unsigned numNotMaking =
      cached != capacityCache_.end() ? cached->second.numIdleWorking + cached->second.numExhausted : 0u;
    if(numWorking > numNotMaking && produced > 0)
        return produced / minutes / (numWorking - numNotMaking);
    return nominalOutput(recipe);
}

bool EconomyPlanner::CanWorkWhenBuilt(const BuildingType bld) const
{
    return !BuildingProperties::IsUsual(bld) || (IsWorkerAvailable(bld) && HasRequiredTools(bld));
}

bool EconomyPlanner::MakesWare(const BuildingType bld, const GoodType good) const
{
    const Recipe* recipe = FindRecipe(bld);
    return recipe && helpers::contains(recipe->outputs, good);
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
    order.kind = (order.cause == OrderCause::Expansion || order.cause == OrderCause::Deposits) ? OrderKind::Expanding :
                                                                                                 OrderKind::Needed;
    const auto isSame = [&](const Order& other) {
        return other.state != OrderState::Placed && other.building == order.building && other.forWare == order.forWare;
    };
    if(order.kind == OrderKind::Expanding)
    {
        // Not if the same order exists (needed ones stand above it)
        if(order.forWare != GoodType::Nothing && helpers::contains_if(orders_, isSame))
            return nullptr;
    } else if(order.forWare != GoodType::Nothing)
    {
        // A needed order replaces the same expanding one (it would stand lower)
        const auto expanding = std::find_if(orders_.begin(), orders_.end(), [&](const Order& other) {
            return isSame(other) && other.kind == OrderKind::Expanding;
        });
        if(expanding != orders_.end())
            orders_.erase(expanding);
    }
    order.id = nextId_++;
    order.sequence = nextSequence_++;
    order.reason = "neu, im nächsten Schritt";
    orders_.push_back(std::move(order));
    return &orders_.back();
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
        case OrderCause::Idle: return forecastOf(root.forWare);
        case OrderCause::Renewal:
        {
            const Recipe* harvester = FindRecipe(root.forBuilding);
            return harvester && !harvester->outputs.empty() ? forecastOf(harvester->outputs.front()) :
                                                              std::numeric_limits<double>::max() / 2;
        }
        // Without them no other building works (workers, tools, space for the wares): before every shortage
        case OrderCause::Helpers:
        case OrderCause::Storage:
        case OrderCause::Tools: return -1e12;
        // Orders for another one which is gone (it was placed meanwhile): as soon as possible
        case OrderCause::Input:
        case OrderCause::Material: return -1e11;
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
    double input = 0;
    if(recipe->consumeEach)
    {
        for(const GoodType good : recipe->inputs)
            input += GetWareStatus(good).forecast;
        input /= recipe->inputs.size();
    } else if(const WareGroup* group = FindGroup(recipe->inputs))
        input = group->status.forecast;
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
    for(const Order* order = Find(id); order; order = order->parent ? Find(order->parent) : nullptr)
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
        const auto it = workingSince_.find(order.placedAt);
        if(it == workingSince_.end())
            continue;
        order.startupEndGF = it->second + startupGFs;
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
        case OrderCause::Shortage: return groups_[order.group].status.state == SupplyState::Shortage;
        case OrderCause::Input:
        {
            const Order* parent = Find(order.parent);
            if(!parent || parent->state == OrderState::Placed)
                return false;
            const Recipe* recipe = FindRecipe(parent->building);
            if(!recipe || !helpers::contains(recipe->inputs, order.forWare))
                return false;
            // This input does not suffice (any of: none of them suffices)
            return recipe->consumeEach ? !InputSuffices(order.forWare, *recipe) : !InputsSuffice(*recipe);
        }
        case OrderCause::Material:
        {
            // Whether the material is there is checked when the order above is processed (it depends on the list)
            const Order* parent = Find(order.parent);
            return parent && parent->state != OrderState::Placed;
        }
        case OrderCause::Idle:
            // Buildings of the type still stand still because of the ware
            return helpers::contains_if(
              snapshot_.buildings[order.forBuilding].waitingForInput,
              [&](const WaitingForInput& waiting) { return helpers::contains(waiting.wares, order.forWare); });
        case OrderCause::Tools:
            // Tools its building makes are still short
            return helpers::contains_if(groups_, [&](const WareGroup& group) {
                return group.tools && group.demanded && group.status.state == SupplyState::Shortage
                       && helpers::contains_if(group.wares,
                                               [&](const GoodType good) { return MakesWare(order.building, good); });
            });
        case OrderCause::Soldiers: return soldiers_.state == SupplyState::Shortage;
        case OrderCause::Helpers: return GetFreeHelpers() <= 0;
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

    // 2. Problems: needed orders. Shortages of the groups consumers take (the best ware of the group), soldiers, tools
    for(unsigned i = 0; i < groups_.size(); ++i)
    {
        const WareGroup& group = groups_[i];
        if(!group.demanded || group.tools || group.status.state != SupplyState::Shortage)
            continue;
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
        need.perMinute = groups_[i].status.missingPerMinute;
        ResolveShortage(good, need);
    }
    ResolveSoldiers();
    ResolveTools();
    // Surpluses: an expanding consumer (a consumer whose product is short is chosen by the shortage)
    for(unsigned i = 0; i < groups_.size(); ++i)
    {
        if(groups_[i].demanded && !groups_[i].tools && groups_[i].status.state == SupplyState::Surplus)
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
    AddStorageOrder();
}

void EconomyPlanner::ResolveShortage(const GoodType good, const Need& need)
{
    if(helpers::contains(resolving_, good))
        return; // Already working on it further up (loop)
    const auto isOwn = [&](const Order& order) {
        if(need.cause == OrderCause::Tools)
            return order.cause == OrderCause::Tools && MakesWare(order.building, good);
        if(order.cause != need.cause || order.forWare != good)
            return false;
        switch(need.cause)
        {
            case OrderCause::Shortage: return order.group == need.group;
            // One maker makes all tools: its orders count for every tool it makes
            case OrderCause::Tools: return true;
            case OrderCause::Idle: return order.forBuilding == need.forBuilding;
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
        Order* blocked = need.parent ? Find(need.parent) : nullptr;
        if(blocked)
        {
            const Recipe* recipe = FindRecipe(blocked->building);
            if(recipe && helpers::contains(recipe->inputs, good) && !helpers::contains(blocked->inputsMissing, good))
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
                order.forBuilding = need.forBuilding;
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
            return;
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
    // Works on the reasons why producers of the ware stand still (orders for their inputs, helpers)
    resolving_.push_back(good);
    ProducerCapacity(good, true);
    resolving_.pop_back();

    // Orders of this need, and the other needed orders whose building makes the ware (also for another product of the
    // building: an armory ordered for shields makes swords too)
    std::vector<const Order*> own, others;
    for(const Order& order : orders_)
    {
        if(order.state == OrderState::Placed || order.kind != OrderKind::Needed)
            continue;
        if(isOwn(order))
            own.push_back(&order);
        else if(MakesWare(order.building, good))
            others.push_back(&order);
    }
    // An order for another one: the same building ordered for the ware for another order or for the statistic (also
    // placed already) is the solution for this one too. It stands above all of them (a farm and a well lacking boards:
    // one sawmill)
    if(need.parent != 0 && own.empty())
    {
        const auto same = std::find_if(orders_.begin(), orders_.end(), [&](const Order& other) {
            return other.kind == OrderKind::Needed && other.building == recipe.building && other.forWare == good
                   && other.id != need.parent;
        });
        if(same != orders_.end())
        {
            if(!helpers::contains(same->alsoFor, need.parent) && same->parent != need.parent)
                same->alsoFor.push_back(need.parent);
            return;
        }
    }
    // As many producers as the missing wares per minute need (the forecast counts the coming producers and those
    // standing still already): what one producer makes (measured, or by its work time), minus the orders of other
    // needs for the ware
    std::ostringstream coveredBy;
    coveredBy.setf(std::ios::fixed);
    coveredBy.precision(1);
    const double perProducer = OutputPerProducer(good, recipe);
    const unsigned total = (need.perMinute > 0 && perProducer > 0) ?
                             static_cast<unsigned>(std::ceil(need.perMinute / perProducer - 1e-9)) :
                             0u;
    unsigned wanted = total > others.size() ? total - static_cast<unsigned>(others.size()) : 0u;
    coveredBy << "gedeckt: fehlen " << need.perMinute << "/min, andere Aufträge " << others.size() << " (je "
              << perProducer << "/min)";
    if(need.maxOrders > 0)
        wanted = std::min(wanted, need.maxOrders);
    // Not more orders waiting than builders could build them (natural limit: more would only wait and plan their
    // construction costs)
    // construction costs). Counted over all needs: the orders waiting to be built and the builders free for them. One
    // order per need comes anyway (it shows what is missing)
    const unsigned numWaiting =
      static_cast<unsigned>(std::count_if(orders_.begin(), orders_.end(), [&](const Order& o) {
          return o.kind == OrderKind::Needed && o.state != OrderState::Placed && !IsChainBlocked(o.id);
      }));
    const unsigned builders = GetAvailableBuilders();
    const unsigned room =
      std::max(builders > numWaiting ? builders - numWaiting : 0u, (own.empty() && others.empty()) ? 1u : 0u);
    wanted = std::min(wanted, static_cast<unsigned>(own.size()) + room);
    // Its inputs do not suffice, or no worker or tool for it is there: one producer waits (its inputs, the tool and
    // helpers are ordered for it), more once they are there. Waiting orders would only plan their construction costs
    if(!InputsSuffice(recipe) || !CanWorkWhenBuilt(recipe.building))
    {
        const unsigned waiting = static_cast<unsigned>(
          std::count_if(others.begin(), others.end(), [&](const Order* o) { return o->building == recipe.building; }));
        if(waiting > 0 && wanted > 0)
            coveredBy.str("ein " + buildingName(recipe.building)
                          + " wartet schon (Eingang, Arbeiter oder Werkzeug fehlt)");
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
        order.forBuilding = need.forBuilding;
        order.parent = need.parent;
        AddOrder(std::move(order));
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
    if(soldiers_.state == SupplyState::Shortage && GetFreeHelpers() <= 0)
        AddHelpersOrder(0);
}

double EconomyPlanner::GetToolMakerCapacity(const BuildingType bld)
{
    const Recipe* recipe = FindRecipe(bld);
    if(!recipe)
        return 0;
    const unsigned startupGFs = BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    double factor = snapshot_.buildings[bld].numSites + aijh_.GetConstruction().GetNumPendingSites(bld);
    for(const nobUsual* building : player_.GetBuildingRegister().GetBuildings(bld))
    {
        std::vector<GoodType> missing;
        const double idle = std::max(0., 1. - (building->HasWorker() ? building->GetProductivity() / 100. : 0.));
        switch(GetDiagnosis(*building, *recipe, missing))
        {
            case Diagnosis::StartingUp:
            {
                const auto since = workingSince_.find(building->GetPos());
                factor += since == workingSince_.end() ?
                            1. :
                            1. - std::min(1., static_cast<double>(currentGF_ - since->second) / startupGFs);
                break;
            }
            case Diagnosis::NoInput:
                if(WillGetInputs(*recipe, missing))
                    factor += idle;
                break;
            // Free time of a maker that works, or that stands still for a reason worked on
            case Diagnosis::Working:
            case Diagnosis::Unknown:
            case Diagnosis::NoWorker:
            case Diagnosis::NoTool:
            case Diagnosis::FlagFull: factor += idle; break;
            case Diagnosis::Exhausted: break;
        }
    }
    return factor * recipe->nominalStepsPerMinute;
}

void EconomyPlanner::UpdateToolNeeds()
{
    // Asked for by workers and tool slots (any tool of the kind will do), and as wares (e.g. research costs): those
    // beyond the stock of exactly these tools. Wanted: what is asked for and TOOLS_IN_STOCK more of each kind
    helpers::EnumArray<unsigned, Tool> asked{};
    for(const OpenDemand& demand : snapshot_.openDemand)
    {
        if(demand.wares.empty())
            continue;
        const auto tool = getToolOf(demand.wares.front());
        if(!tool)
            continue;
        if(demand.purpose == WareUse::Tool)
            asked[*tool] += demand.amount;
        else
        {
            unsigned stock = 0;
            for(const GoodType good : demand.wares)
                stock += snapshot_.inWarehouses[good];
            if(demand.amount > stock)
                asked[*tool] += demand.amount - stock;
        }
    }
    // Tools the buildings coming will ask for (building sites, sites ordered, needed orders of the list)
    for(const OpenDemand& demand : planned_)
    {
        if(demand.purpose != WareUse::Tool || demand.wares.empty())
            continue;
        if(const auto tool = getToolOf(demand.wares.front()))
            asked[*tool] += demand.amount;
    }
    for(const auto tool : helpers::enumRange<Tool>())
    {
        const unsigned have = GetToolStock(TOOL_TO_GOOD[tool]) + player_.GetToolsOrdered(tool);
        const unsigned wanted = asked[tool] + TOOLS_IN_STOCK;
        toolsMissing_[tool] = wanted > have ? wanted - have : 0u;
    }
}

void EconomyPlanner::ResolveTools()
{
    // Tools are wares with a forecast (their groups: all tools of a kind, of a slot, a tool alone). What is short is
    // ordered (OrderTools) and made by the makers that are there. More makers by the forecast like every producer: the
    // tools missing of all groups a maker makes, minus what its makers can still do (coming ones, free time)
    struct MakerNeed
    {
        GoodType good = GoodType::Nothing;
        unsigned group = 0;
        double deficit = 0, lowest = 0;
    };
    std::map<BuildingType, MakerNeed> needs;
    const std::vector<Recipe>& recipes = civAI_.GetGraph().GetRecipes();
    for(unsigned i = 0; i < groups_.size(); ++i)
    {
        if(!groups_[i].tools || !groups_[i].demanded || groups_[i].status.state != SupplyState::Shortage)
            continue;
        const std::vector<GoodType> wares = groups_[i].wares;
        const double forecast = groups_[i].status.forecast;
        GoodType good = ChooseWare(wares);
        if(good == GoodType::Nothing)
            good = wares.front(); // No way to any of them: the first one shows why
        const std::vector<WayCheck> ways = RankProducers(good, false, 0);
        if(ways.empty())
            continue;
        MakerNeed& need = needs[recipes[ways.front().recipe].building];
        need.deficit -= forecast;
        if(need.good == GoodType::Nothing || forecast < need.lowest)
        {
            need.good = good;
            need.group = i;
            need.lowest = forecast;
        }
    }
    for(const auto& [bld, makerNeed] : needs)
    {
        Need need;
        need.cause = OrderCause::Tools;
        need.group = makerNeed.group;
        need.perMinute = std::max(0., makerNeed.deficit / BALANCE_MINUTES - GetToolMakerCapacity(bld));
        ResolveShortage(makerNeed.good, need);
    }
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
    // Producers of the ware stand still for a reason worked on: more of them would stand still too
    if(ProducerCapacity(good, false).numIdle > 0)
        return;
    // A tool maker works on order: no more of them while its makers have time left
    if(IsToolMaker(recipe) && GetToolMakerCapacity(bld) > 0)
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
        // No worker or tool for it: one order waits for them. Otherwise as many as can be equipped with the tools in
        // stock, the others come after them
        const unsigned maxWaiting = CanWorkWhenBuilt(renewer.building) ? GetNumEquippable(renewer.building) : 1u;
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
            if(order.state == OrderState::Placed || helpers::contains(other->inputsMissing, order.forWare))
                return false;
            if(other->state != OrderState::Blocked || other->retry.kind != RetryCondition::Kind::Material)
                return true;
            return !helpers::contains_if(BUILDING_COSTS[other->building], [&](const CostEntry& entry) {
                return helpers::contains(entry.wares, order.forWare);
            });
        });
    }
    unsigned searchBudget = SEARCHES_PER_STEP;
    keptInputs_.clear();
    militaryKept_ = BuildingType::Headquarters;
    std::vector<unsigned> ids;
    for(const Order* order : GetList())
        ids.push_back(order->id);
    bool militaryDone = false;
    for(const unsigned id : ids)
    {
        const Order* order = Find(id);
        // Military buildings are still placed by the old AI: before the first expanding order the material for its
        // next military building is kept free
        if(!militaryDone && order && order->kind == OrderKind::Expanding)
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
    if(order->parent != 0 && IsChainBlocked(order->parent))
    {
        Remove(id);
        return;
    }
    const bool needed = order->kind == OrderKind::Needed;
    const BuildingType bld = order->building;
    // The inputs this order lacks are kept for it: no order below it gets a building using them
    for(const GoodType input : order->inputsMissing)
        keptInputs_.emplace(input, id);
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
    if(order->state == OrderState::Blocked && !order->selfSolving && !IsRetryMet(order->retry, bld))
    {
        // A needed order works on its unmet rule in every step: it keeps the material that is there, and its solution
        // is ordered again if it is gone (e.g. no place for the sawmill ordered for it)
        const RetryCondition::Kind kind = order->retry.kind;
        if(needed
           && (kind == RetryCondition::Kind::Material || kind == RetryCondition::Kind::Builder
               || kind == RetryCondition::Kind::Helpers))
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
    // Orders created for its construction material are of no use once the material is there
    if(HasMaterial(bld))
    {
        std::vector<unsigned> materialChildren;
        for(const Order& other : orders_)
        {
            if(other.parent == id && other.cause == OrderCause::Material && other.state != OrderState::Placed
               && !HasPlacedChild(other.id))
                materialChildren.push_back(other.id);
        }
        for(const unsigned child : materialChildren)
            Remove(child);
        order = Find(id);
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
        // Inputs kept for an order above: this one must wait (any of: only if all of them are kept)
        std::vector<const Order*> keptFor;
        std::vector<GoodType> keptWares;
        for(const GoodType input : recipe->inputs)
        {
            if(input == order->usesWare)
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
        // An input whose producer was placed for this order counts as there: the building follows at once, beside
        // the site (not once the producer works). Its site must be in the game (the command takes up to 2 network
        // frames)
        bool waitsForSite = false;
        std::string waitsForSiteOf;
        const auto hasPlacedProducer = [&](const GoodType input) {
            for(const Order& other : orders_)
            {
                if(other.state != OrderState::Placed || other.forWare != input || other.kind != OrderKind::Needed
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
        std::vector<GoodType> missing;
        bool anyPlaced = false;
        for(const GoodType input : recipe->inputs)
        {
            if(input == order->usesWare || InputSuffices(input, *recipe))
                continue;
            if(hasPlacedProducer(input))
                anyPlaced = true;
            else
                missing.push_back(input);
        }
        const bool suffices =
          InputsSuffice(*recipe, order->usesWare) || (recipe->consumeEach ? missing.empty() : anyPlaced);
        if(!suffices && waitsForSite && needed)
        {
            // Checked again soon (independent of the reaction time of the AI)
            followUp_ = true;
            order->state = OrderState::Waiting;
            order->reason = "wartet auf die Baustelle von " + waitsForSiteOf;
            return;
        }
        order->inputsMissing = suffices ? std::vector<GoodType>() : missing;
        // Any of the inputs: none of them suffices (the group)
        const std::vector<GoodType>& missingNames = recipe->consumeEach ? missing : recipe->inputs;
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
                // Expanding: no orders for it, checked again every step (cheap, no place search)
                MoveToEnd(*order, "Eingang fehlt: " + wareNames(missingNames), RetryCondition{});
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
                noWay = "kein Weg zu " + wareNames(missing);
            if(!noWay.empty())
            {
                order->state = OrderState::Blocked;
                order->reason = noWay;
                order->retry = {RetryCondition::Kind::NoWay, GetSituation()};
                RemoveUnplacedChildren(id);
                return;
            }
            order->state = OrderState::Waiting;
            order->reason = "Eingang fehlt: " + wareNames(missingNames);
            const std::vector<GoodType> toResolve =
              recipe->consumeEach ? missing : std::vector<GoodType>{ChooseWare(missing)};
            for(const GoodType input : toResolve)
            {
                // Made for an order above already: a loop (the ware is needed to make itself)
                if(HasChildFor(id, input) || IsInChain(id, input, BuildingType::Headquarters))
                    continue;
                // What the building would use per minute by its work time, beyond what is made more than used now
                Need need;
                need.cause = OrderCause::Input;
                need.parent = id;
                need.perMinute =
                  std::max(0., recipe->nominalStepsPerMinute - GetWareStatus(input).forecast / BALANCE_MINUTES);
                need.maxOrders = 1;
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
        const bool clearing = order->clearing;
        if(!aijh_.PlaceBuilding(bld, pos))
            return;
        AIJH::ConstructionMaterial taken{};
        takeMaterial(freeMaterial_, bld, taken);
        // What it will make and use is part of the forecast from now on
        AddComingBuilding(bld);
        {
            const Recipe* placedRecipe = FindRecipe(bld);
            const int inReach = placedRecipe && placedRecipe->reach > 0 ?
                                  static_cast<int>(MapProfile::CountSourceNodes(
                                    player_.GetGameWorld(), pos, placedRecipe->reach, placedRecipe->source)) :
                                  -1;
            placements_[pos] = Placement{currentGF_, bld, order->cause, order->kind, inReach};
        }
        // Placed: its inputs are planned from now on (and its costs if it was not counted at the start of the step)
        const bool counted = helpers::contains(plannedOrderIds_, id);
        AddPlanned(bld, true, !counted, true, !counted);
        if(clearing && bld == BuildingType::Woodcutter)
        {
            aijh_.GetInterface().SetClearingWoodcutter(pos, true);
            clearingPending_.push_back(pos);
        }
        if(!needed)
        {
            // Expanding orders are done with the building site
            Remove(id);
            return;
        }
        order = Find(id);
        order->state = OrderState::Placed;
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
    if(!needed)
    {
        MoveToEnd(*order, "kein Bauplatz", noPlace);
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
           || order->cause == OrderCause::Material || order->cause == OrderCause::Idle
           || order->cause == OrderCause::Soldiers || order->cause == OrderCause::Tools))
    {
        failedWays_.push_back({order->forWare, bld, GetSituation()});
        Remove(id);
        return;
    }
    order->state = OrderState::Blocked;
    order->reason = "kein Bauplatz";
    order->retry = noPlace;
    RemoveUnplacedChildren(id);
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
        if(good != GoodType::Nothing && snapshot_.inWarehouses[good] > 0 && getToolOf(good) == kind)
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
            KeepMaterial(*order);
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
            std::vector<std::pair<GoodType, unsigned>> selfSolved, toOrder;
            for(const auto& [good, amount] : lacking)
            {
                if(order->selfSolving && helpers::contains(GetChain(good), bld))
                {
                    selfSolved.emplace_back(good, amount);
                    reservedMaterial_[good] += amount;
                } else
                    toOrder.emplace_back(good, amount);
            }
            const std::string cancelled = selfSolved.empty() ? std::string() : CancelSitesFor(selfSolved);
            order = Find(id);
            if(!selfSolved.empty())
                order->reason = "Vorrang (löst den Mangel selbst): " + order->reason;
            order->reason += " – " + why;
            if(!cancelled.empty())
                order->reason += "; abgebrochen: " + cancelled;
            for(const auto& [good, amount] : toOrder)
            {
                if(HasChildFor(id, good) || IsInChain(id, good, BuildingType::Headquarters))
                    continue;
                // The missing material within one balance length (the producers ordered for the shortage of the ware
                // count: its construction costs are planned)
                // Only if the economy does not make up for it in one balance length: the forecast of the material
                // (it counts the costs of this order) is negative
                const double forecast = GetWareStatus(good).forecast;
                if(forecast >= 0)
                    continue;
                Need need;
                need.cause = OrderCause::Material;
                need.parent = id;
                need.perMinute = -forecast / BALANCE_MINUTES;
                need.maxOrders = 1;
                ResolveShortage(good, need); // May add orders: `order` is not valid anymore
            }
            break;
        }
        case RetryCondition::Kind::Helpers: AddHelpersOrder(id); break;
        case RetryCondition::Kind::Builder:
            // A builder is recruited from a helper and his tool (kept in stock by the tool orders)
            if(GetFreeHelpers() <= 0)
                AddHelpersOrder(id);
            break;
        case RetryCondition::Kind::Limit:
        case RetryCondition::Kind::Now:
        case RetryCondition::Kind::Tool:
        case RetryCondition::Kind::Research:
        case RetryCondition::Kind::NoWay:
        case RetryCondition::Kind::NoPlace:
            // Tools: ordered (OrderTools), their makers come with ResolveTools. Research or territory needed
            break;
    }
}

std::string EconomyPlanner::DescribeMissingMaterial(const BuildingType bld, std::vector<GoodType>& missing) const
{
    std::string result;
    for(const CostEntry& entry : BUILDING_COSTS[bld])
    {
        int available = 0, stock = 0, forSites = 0, forOrdersAbove = 0;
        for(const GoodType good : entry.wares)
        {
            available += std::max(0, freeMaterial_[good]);
            stock += static_cast<int>(snapshot_.inWarehouses[good]);
            forSites += static_cast<int>(snapshot_.inWarehouses[good]) - stepFreeMaterial_[good];
            forOrdersAbove += std::max(0, stepFreeMaterial_[good]) - std::max(0, freeMaterial_[good]);
        }
        if(available >= static_cast<int>(entry.amount))
            continue;
        missing.push_back(entry.wares.front());
        std::ostringstream out;
        out << wareNames(entry.wares) << " fehlt: " << static_cast<unsigned>(entry.amount) << " nötig, " << available
            << " frei (Lager " << stock << ", Baustellen brauchen noch " << forSites << ", für Aufträge darüber "
            << forOrdersAbove << ")";
        result += (result.empty() ? "" : "; ") + out.str();
    }
    return result;
}

std::string EconomyPlanner::ExplainWare(const GoodType good, const unsigned depth)
{
    const WareStatus& status = GetWareStatus(good);
    std::ostringstream out;
    out << wareName(good) << ": frei " << status.free << ", Prognose " << static_cast<int>(std::floor(status.forecast));
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

bool EconomyPlanner::IsRetryMet(const RetryCondition& retry, const BuildingType bld) const
{
    const Situation situation = GetSituation();
    switch(retry.kind)
    {
        case RetryCondition::Kind::Now: return true;
        case RetryCondition::Kind::Material: return HasMaterial(bld);
        case RetryCondition::Kind::Builder: return aijh_.GetConstruction().HasEnoughBuildersAvailable();
        case RetryCondition::Kind::Helpers: return GetFreeHelpers() > 0;
        case RetryCondition::Kind::Limit: return GetNumBuildings(bld) < GetBuildingLimit();
        case RetryCondition::Kind::Tool: return IsWorkerAvailable(bld) && HasRequiredTools(bld);
        case RetryCondition::Kind::Research: return situation.researched != retry.situation.researched;
        case RetryCondition::Kind::NoWay: return false; // Checked every step anyway
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
               || other.cause == OrderCause::Idle || other.cause == OrderCause::Soldiers))
        {
            const Recipe* recipe = FindRecipe(order.building);
            if(recipe && helpers::contains(recipe->inputs, other.forWare))
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

MapPoint EconomyPlanner::FindScoredPlace(const Order& order, const Recipe* recipe, const MapPoint center,
                                         const MapPoint preferred, double& score) const
{
    const GameWorldBase& world = player_.GetGameWorld();
    const BuildingRegister& buildings = player_.GetBuildingRegister();
    const BuildingType bld = order.building;
    const auto addPositions = [&](std::vector<MapPoint>& positions, const BuildingType type) {
        for(const nobUsual* building : buildings.GetBuildings(type))
            positions.push_back(building->GetPos());
        for(const noBuildingSite* site : buildings.GetBuildingSites())
        {
            if(site->GetBuildingType() == type)
                positions.push_back(site->GetPos());
        }
        for(const Order& other : orders_)
        {
            if(other.state == OrderState::Placed && other.building == type
               && !helpers::contains(positions, other.placedAt))
                positions.push_back(other.placedAt);
        }
    };
    // Buildings of the same type, producers of its inputs and consumers of its products, warehouses, renewers
    std::vector<MapPoint> sameType, partners, warehouses, renewers;
    std::vector<BuildingType> partnerTypes;
    bool makesMaterial = false;
    unsigned renewerDistance = 0;
    const EconomyGraph& graph = civAI_.GetGraph();
    if(recipe)
    {
        for(const GoodType input : recipe->inputs)
        {
            for(const unsigned idx : graph.GetProducers(input))
                partnerTypes.push_back(graph.GetRecipes()[idx].building);
        }
        for(const GoodType output : recipe->outputs)
        {
            makesMaterial |= isMaterial_[output];
            for(const unsigned idx : consumers_[output])
                partnerTypes.push_back(graph.GetRecipes()[idx].building);
        }
        helpers::makeUnique(partnerTypes);
        for(const BuildingType type : partnerTypes)
        {
            if(type != bld)
                addPositions(partners, type);
        }
        // A clearing harvester and a renewer of its source: their reaches must not overlap
        if(order.clearing)
        {
            if(const Renewer* renewer = FindRenewer(recipe->source))
            {
                addPositions(renewers, renewer->building);
                renewerDistance = recipe->reach + renewer->reach;
            }
        }
    }
    const bool harvester = recipe && recipe->reach > 0;
    if(harvester || makesMaterial)
        addPositions(sameType, bld);
    if(makesMaterial)
    {
        for(const nobBaseWarehouse* warehouse : buildings.GetStorehouses())
            warehouses.push_back(warehouse->GetPos());
    }
    const unsigned sameTypeRadius = harvester ? 2 * recipe->reach : SAME_TYPE_RADIUS;
    const unsigned minSource = harvester ? GetMinSourceInReach(order, *recipe) : 0u;
    const auto closeness = [&](const std::vector<MapPoint>& positions, const MapPoint pt) {
        unsigned best = PARTNER_RADIUS;
        for(const MapPoint pos : positions)
            best = std::min(best, world.CalcDistance(pt, pos));
        return 1. - static_cast<double>(best) / PARTNER_RADIUS;
    };
    return aijh_.FindScoredPosition(
      bld, center, SEARCH_RADIUS,
      [&](const MapPoint pt, double& value) {
          if(IsFieldPlace(pt))
              return false;
          for(const MapPoint pos : renewers)
          {
              if(world.CalcDistance(pt, pos) <= renewerDistance)
                  return false;
          }
          value = 0;
          if(harvester)
          {
              const unsigned amount = MapProfile::CountSourceNodes(world, pt, recipe->reach, recipe->source);
              if(amount < minSource)
                  return false;
              value += SCORE_SOURCE * std::min(amount, SCORE_SOURCE_MAX);
          }
          for(const MapPoint pos : sameType)
          {
              if(world.CalcDistance(pt, pos) <= sameTypeRadius)
                  value += SCORE_SAME_TYPE;
          }
          if(preferred.isValid())
              value += SCORE_PREFERRED * closeness({preferred}, pt);
          if(!partners.empty())
              value += SCORE_PARTNER * closeness(partners, pt);
          if(!warehouses.empty())
              value += SCORE_WAREHOUSE * closeness(warehouses, pt);
          // Among equal places the one nearest to the center
          value -= 0.01 * world.CalcDistance(pt, center);
          return true;
      },
      score);
}

MapPoint EconomyPlanner::SearchPlace(Order& order, unsigned& searchBudget, bool& searchedAll)
{
    searchedAll = false;
    const Recipe* recipe = FindRecipe(order.building);
    const bool renewal = order.cause == OrderCause::Renewal;
    // Where its building should stand close to (the site of an input ordered for it)
    const MapPoint preferred = (renewal || order.besideRenewer) ? MapPoint::Invalid() : GetPreferredPoint(order);
    std::vector<MapPoint> centers;
    if(order.near.isValid())
        centers.push_back(order.near);
    else
    {
        if(preferred.isValid())
            centers.push_back(preferred);
        // Processing near the warehouses, buildings working on the map (and warehouses) also near the military
        // buildings (like the old AI)
        for(const nobBaseWarehouse* warehouse : player_.GetBuildingRegister().GetStorehouses())
            centers.push_back(warehouse->GetPos());
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
    std::vector<MapPoint> clearing = player_.GetClearingWoodcutters();
    clearing.insert(clearing.end(), clearingPending_.begin(), clearingPending_.end());
    // The place by what the building works on (not by the type of the building). By the score of the places:
    // processing buildings and harvesters of trees, stone piles, map objects and ore. With the rules of the old AI:
    // fish, animals, land, water, plants
    const SourceKind source = recipe ? recipe->source.kind : SourceKind::None;
    const bool scored =
      !renewal && !order.besideRenewer && order.cause != OrderCause::Storage && recipe
      && (source == SourceKind::None || source == SourceKind::Trees || source == SourceKind::StonePiles
          || source == SourceKind::MapObject || source == SourceKind::Ore);
    MapPoint best = MapPoint::Invalid();
    double bestScore = 0;
    // The place near the site of its input first, then the other centers in turn
    const unsigned first = preferred.isValid() && !order.near.isValid() ? 1u : 0u;
    while(searchBudget > 0)
    {
        if(order.centersTried >= centers.size())
        {
            order.centersTried = 0;
            searchedAll = !best.isValid();
            return best;
        }
        const unsigned idx = order.centersTried < first ?
                               order.centersTried :
                               first + (order.nextCenter++) % static_cast<unsigned>(centers.size() - first);
        const MapPoint center = centers[idx];
        ++order.centersTried;
        --searchBudget;
        aijh_.UpdateNodesAround(center, SEARCH_RADIUS);
        const unsigned borderReserve = AIJH::AIConstruction::GetBorderReserve(order.building);
        if(scored)
        {
            // The best place of the centers searched in this step
            double score = 0;
            const MapPoint pos = FindScoredPlace(order, recipe, center, preferred, score);
            if(pos.isValid() && (!best.isValid() || score > bestScore))
            {
                best = pos;
                bestScore = score;
            }
            continue;
        }
        MapPoint pos;
        if(order.besideRenewer)
        {
            const Renewer* renewer = recipe ? FindRenewer(recipe->source) : nullptr;
            pos = aijh_.FindHarvesterPositionBeside(order.building, center, renewer ? renewer->reach : 2u);
        } else if(renewal)
        {
            // Near its harvester, or (a harvester without a place) near the warehouses and military buildings. Never
            // where clearing woodcutters work (both reaches)
            if(order.building == BuildingType::Forester)
                pos = aijh_.FindForesterPosition(center, clearing, order.near.isValid() ? 0u : SEARCH_RADIUS);
            else
                pos = aijh_.FindPositionForBuildingAround(order.building, center);
        } else if(order.cause == OrderCause::Storage || !recipe)
        {
            // Storage: also next to another warehouse (the old AI keeps its storehouses apart)
            pos = aijh_.SimpleFindPosition(center, BUILDING_SIZE[order.building], SEARCH_RADIUS, borderReserve);
        } else
            pos = aijh_.FindPositionForBuildingAround(order.building, center);
        // A harvester only where enough of its source is in reach (not beside its renewer: it plants it). No building
        // on the fields of plantations
        if(pos.isValid() && recipe && recipe->reach > 0 && !order.besideRenewer
           && MapProfile::CountSourceNodes(player_.GetGameWorld(), pos, recipe->reach, recipe->source)
                < GetMinSourceInReach(order, *recipe))
            pos = MapPoint::Invalid();
        if(pos.isValid() && IsFieldPlace(pos))
            pos = MapPoint::Invalid();
        if(pos.isValid())
        {
            order.centersTried = 0;
            return pos;
        }
    }
    if(best.isValid())
        order.centersTried = 0;
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tools: only orders, at least TOOLS_IN_STOCK of each kind beyond what is asked for. The regulator stays at 0

void EconomyPlanner::OrderTools()
{
    // With tool orders (addon): the missing tools are ordered, the regulator stays at 0. Without: the regulator of a
    // kind is the number missing (at most 10), 0 if none is missing, so the makers make what is missing
    const bool ordering = player_.GetGameWorld().GetGGS().isEnabled(AddonId::TOOL_ORDERING);
    ToolSettings priorities{};
    helpers::EnumArray<int8_t, Tool> orders{};
    bool change = false;
    for(const auto tool : helpers::enumRange<Tool>())
    {
        if(ordering)
        {
            orders[tool] = static_cast<int8_t>(std::min(toolsMissing_[tool], 100u));
            change |= toolsMissing_[tool] > 0 || player_.GetToolPriority(tool) != 0;
        } else
        {
            priorities[tool] = static_cast<uint8_t>(std::min(toolsMissing_[tool], 10u));
            change |= player_.GetToolPriority(tool) != priorities[tool];
        }
    }
    if(change)
        aijh_.GetInterface().ChangeTools(priorities, ordering ? orders.data() : nullptr);
}

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
    const auto since = workingSince_.find(pt);
    if(since != workingSince_.end())
        out << ", Arbeiter seit GF " << since->second;
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
    const auto writeStatus = [&out](const std::string& name, const WareStatus& status) {
        out << " " << name << " (Prognose " << static_cast<int>(std::floor(status.forecast)) << " = frei "
            << status.free << " + Herstellung " << status.forecastProduction << " - Verbrauch "
            << status.forecastConsumption << "; Bestand " << status.stock << ", angefragt: offen " << status.openDemand
            << ", unterwegs " << status.inTransit << ", geplant " << status.planned << ");";
    };
    out << "  Mangel:";
    for(const WareGroup& group : groups_)
    {
        if(group.demanded && !group.tools && group.status.state == SupplyState::Shortage)
            writeStatus(wareNames(group.wares), group.status);
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
        if(group.demanded && !group.tools && group.status.state == SupplyState::Surplus)
            out << " " << wareNames(group.wares) << " (Prognose " << static_cast<int>(std::floor(group.status.forecast))
                << ");";
    }
    out << "\n  Werkzeugmangel:";
    for(const WareGroup& group : groups_)
    {
        if(group.demanded && group.tools && group.status.state == SupplyState::Shortage)
            out << " " << wareNames(group.wares) << " (Prognose " << static_cast<int>(std::floor(group.status.forecast))
                << ", Bestand " << group.status.stock << ", offen " << group.status.openDemand << ", geplant "
                << group.status.planned << ");";
    }
    out << "\n  Werkzeuge (bestellt / fehlen noch):";
    for(const auto tool : helpers::enumRange<Tool>())
    {
        const unsigned ordered = player_.GetToolsOrdered(tool);
        if(ordered > 0 || toolsMissing_[tool] > 0)
            out << " " << wareName(TOOL_TO_GOOD[tool]) << " " << ordered << "/" << toolsMissing_[tool] << ";";
    }
    out << "\n  Soldaten:";
    writeStatus(soldiers_.state == SupplyState::Shortage ? "Mangel" : "kein Mangel", soldiers_);
    for(const auto& [good, note] : soldierNotes_)
        out << " " << wareName(good) << " ohne Auftrag: " << note << ";";
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
        if(order->cause == OrderCause::Idle || (order->cause == OrderCause::Renewal && !order->parent))
            out << " für " << buildingName(order->forBuilding);
        if(order->cause == OrderCause::Shortage && groups_[order->group].wares.size() > 1u)
            out << " (Gruppe " << wareNames(groups_[order->group].wares) << ")";
        if(order->clearing)
            out << ", rodend";
        if(order->besideRenewer)
            out << ", neben " << pointName(order->near);
        out << ") " << stateNames[static_cast<unsigned>(order->state)];
        if(!order->reason.empty())
            out << ": " << order->reason;
        out << "\n";
    }
    if(!tornDown_.empty())
    {
        out << "  Zuletzt abgerissen (nichts mehr in Reichweite):";
        for(const std::string& entry : tornDown_)
            out << "\n    " << entry;
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
