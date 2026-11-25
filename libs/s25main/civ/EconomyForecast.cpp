// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/EconomyForecast.h"
#include "BuildingRegister.h"
#include "EconomyStatistics.h"
#include "GamePlayer.h"
#include "RTTR_Assert.h"
#include "Ware.h"
#include "ai/civ/MapProfile.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/PlayerResearch.h"
#include "civ/TechTree.h"
#include "figures/nofBuildingWorker.h"
#include "figures/nofCustomPlanter.h"
#include "figures/nofFarmer.h"
#include "figures/nofFarmhand.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "world/GameWorld.h"
#include "nodeObjs/noDeposit.h"
#include "nodeObjs/noFlag.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/JobConsts.h"
#include "gameData/ToolConsts.h"
#include <algorithm>
#include <cmath>

namespace civ {

namespace {
    std::vector<GoodType> sorted(std::vector<GoodType> wares)
    {
        helpers::erase(wares, GoodType::Nothing);
        std::sort(wares.begin(), wares.end());
        wares.erase(std::unique(wares.begin(), wares.end()), wares.end());
        return wares;
    }

    helpers::OptionalEnum<Job> getJob(const BuildingType bld)
    {
        if(isCustomBuilding(bld))
            return getCustomJob(getCustomBuildingIdx(bld));
        if(const auto& job = BLD_WORK_DESC[bld].job)
            return *job;
        return std::nullopt;
    }

    /// GFs of [from, to) in the window starting at windowStart
    unsigned gfsIn(const unsigned from, const unsigned to, const unsigned windowStart)
    {
        const unsigned start = std::max(from, windowStart);
        return to > start ? to - start : 0u;
    }
    /// What `steps` work steps take of the input (its amount per step)
    Amount amountOf(const Amount steps, const civai::RecipeInput& input)
    {
        return steps * static_cast<Amount>(input.amount) / 100;
    }
} // namespace

bool needsFields(const BuildingType bld)
{
    if(bld == BuildingType::Farm)
        return true;
    if(!isCustomBuilding(bld))
        return false;
    const CustomBuilding* building = getActiveContent().GetBuilding(bld);
    return building && building->IsPlanter();
}

unsigned countFieldNodes(const GameWorld& world, const BuildingType bld, const MapPoint pt, const bool placed)
{
    const MapPoint flagPt = world.GetNeighbour(pt, Direction::SouthEast);
    const auto nextToBuilding = [&](const MapPoint node) {
        return node == pt || node == flagPt || world.CalcDistance(node, pt) <= 1
               || world.CalcDistance(node, flagPt) <= 1;
    };
    unsigned radius = 0;
    std::function<bool(MapPoint)> isField;
    const CustomBuilding* custom = isCustomBuilding(bld) ? getActiveContent().GetBuilding(bld) : nullptr;
    if(bld == BuildingType::Farm)
    {
        radius = nofFarmhand::GetWorkRadius(BuildingType::Farm);
        isField = [&](const MapPoint node) {
            if(world.GetNO(node)->GetType() == NodalObjectType::Grainfield)
                return true;
            return nofFarmer::GetNewFieldPointQuality(world, node) != nofFarmhand::PointQuality::NotPossible;
        };
    } else if(custom && custom->IsPlanter())
    {
        radius = custom->plantRadius;
        isField = [&](const MapPoint node) {
            const auto* deposit = world.GetSpecObj<noDeposit>(node);
            return (deposit && static_cast<int>(deposit->GetObjectIdx()) == custom->plantObject)
                   || nofCustomPlanter::CanPlantAt(world, node, *custom);
        };
    } else
        return 0;
    unsigned result = 0;
    world.CheckPointsInRadius(
      pt, radius,
      [&](const MapPoint node, unsigned) {
          if(node == pt || node == flagPt || (!placed && nextToBuilding(node)))
              return false;
          if(isField(node))
              ++result;
          return false;
      },
      false);
    return result;
}

SourceInfo countSources(const GameWorld& world, const MapPoint pt, const civai::Recipe& recipe, const bool placed)
{
    SourceInfo info;
    info.inReach = recipe.reach > 0 ? civai::MapProfile::CountSourceNodes(world, pt, recipe.reach, recipe.source) : 0u;
    info.nearby = recipe.reach >= 2 ? civai::MapProfile::CountSourceNodes(world, pt, recipe.reach / 2, recipe.source) :
                                      info.inReach;
    info.fields = needsFields(recipe.building) ? countFieldNodes(world, recipe.building, pt, placed) : 0u;
    return info;
}

Diagnosis diagnose(const GamePlayer& player, const nobUsual& building, const civai::Recipe& recipe,
                   std::vector<GoodType>& missingInputs, const std::function<SourceInfo()>& getSources)
{
    using civai::SourceKind;
    // Every building, whatever its productivity: the first problem found
    missingInputs.clear();
    if(building.IsProductionStopped())
        return Diagnosis::Stopped; // The player or the distribution stopped it: not examined
    // No road at its flag besides the one to the building
    bool road = false;
    for(const auto dir : helpers::EnumRange<Direction>{})
        road |= dir != Direction::NorthWest && building.GetFlag()->GetRoute(dir) != nullptr;
    if(!road)
        return Diagnosis::NoRoad;
    if(!building.HasWorker())
        return player.IsJobWanted(building) ? Diagnosis::NoWorker : Diagnosis::WorkerComing;
    // Tool reform: it does not work without tools (base speed 0) and has none giving work speed. An empty slot of a
    // building that works without that tool (slowly) is no reason to wait (Nutzer, 2026-11-18): reported below
    const auto& slots = building.GetToolSlots();
    if(!slots.empty() && building.GetWorkSpeed() == 0)
        return Diagnosis::NoTool;
    const nofBuildingWorker* worker = building.GetWorker();
    if(worker && worker->GetState() == nofBuildingWorker::State::WaitingForWaresOrProductionStopped)
    {
        const BldWorkDescription& workDesc = BLD_WORK_DESC[building.GetBuildingType()];
        // Inputs without the wares of a step (built-in mines: without any ware)
        bool anyThere = false;
        for(unsigned i = 0; i < workDesc.inputs.size(); ++i)
        {
            const unsigned have = building.GetNumWares(i);
            if(have != 0)
                anyThere = true;
            if(workDesc.useOneWareEach ? have < building.GetNumNeededForStep(i) : have == 0)
            {
                for(const GoodType good : workDesc.inputs[i].wares)
                {
                    if(good != GoodType::Nothing)
                        missingInputs.push_back(good);
                }
            }
        }
        const bool canWork = workDesc.useOneWareEach ? missingInputs.empty() : anyThere;
        if(!canWork && !missingInputs.empty())
            return Diagnosis::NoInput;
        missingInputs.clear();
    }
    if(worker && worker->GetState() == nofBuildingWorker::State::WaitForWareSpace)
        return Diagnosis::FlagFull;
    const bool countable = recipe.reach > 0
                           && (recipe.source.kind == SourceKind::Trees || recipe.source.kind == SourceKind::StonePiles
                               || recipe.source.kind == SourceKind::MapObject || recipe.source.kind == SourceKind::Ore
                               || recipe.source.kind == SourceKind::Fish || recipe.source.kind == SourceKind::Water);
    const bool fields = needsFields(recipe.building);
    if(countable || fields)
    {
        const SourceInfo info = getSources();
        if(countable && info.inReach == 0)
            return Diagnosis::Exhausted;
        if(fields && info.fields < MIN_FIELD_NODES)
            return Diagnosis::FewFields;
        // Harvesters walking to their source (miners and wells do not walk)
        if(countable && recipe.reach >= 2 && recipe.source.kind != SourceKind::Ore
           && recipe.source.kind != SourceKind::Water && info.nearby == 0)
            return Diagnosis::SourceFar;
    }
    if(helpers::contains_if(slots, [](const nobUsual::ToolSlot& slot) { return slot.tool == GoodType::Nothing; }))
        return Diagnosis::MissingTool;
    // A tool in use worse than the best one of its slot
    const auto& slotDescs = getActiveContent().GetBuildingTools(building.GetBuildingType()).slots;
    for(unsigned i = 0; i < slots.size() && i < slotDescs.size(); ++i)
    {
        unsigned best = 0;
        for(const GoodType good : slotDescs[i].wares)
            best = std::max(best, getToolQuality(good));
        if(getToolQuality(slots[i].tool) < best)
            return Diagnosis::BadTool;
    }
    return Diagnosis::Working;
}

helpers::OptionalEnum<Tool> getToolKindOf(const GoodType good)
{
    for(const auto tool : helpers::enumRange<Tool>())
    {
        if(TOOL_TO_GOOD[tool] == good)
            return tool;
    }
    if(const auto kind = getActiveContent().GetToolTypeOf(good))
        return *kind;
    return std::nullopt;
}

unsigned getToolQuality(const GoodType good)
{
    const ToolInfo* info = getActiveContent().GetTool(good);
    if(!info)
        return 0;
    const int bonus = std::max(0, static_cast<int>(info->speed) + info->GetBestOutputBonus());
    return static_cast<unsigned>(bonus) * 1000u + std::min(info->durability, 999u);
}

GoodType getRecruitTool(const EconomySnapshot& snapshot, const BuildingType bld)
{
    const Content& content = getActiveContent();
    const auto job = getJob(bld);
    if(!job || snapshot.inWarehousesPeople[*job] > snapshot.jobsWanted[*job] || content.IsRecruitedWithoutTool(*job))
        return GoodType::Nothing;
    // Workers of workshops need no tool
    if(isCustomJob(*job))
        return GoodType::Nothing;
    return JOB_CONSTS[*job].tool.value_or(GoodType::Nothing);
}

void addToolDemand(const EconomySnapshot& snapshot, std::vector<OpenDemand>& demand, const BuildingType bld)
{
    if(!BuildingProperties::IsUsual(bld))
        return;
    if(const GoodType tool = getRecruitTool(snapshot, bld); tool != GoodType::Nothing)
        demand.push_back({{tool}, 1, WareUse::Tool});
    // Tool reform: one tool per slot (also the slots of a building working without tools: it asks for them)
    for(const BuildingTools::Slot& slot : getActiveContent().GetBuildingTools(bld).slots)
    {
        if(!slot.wares.empty())
            demand.push_back({slot.wares, 1, WareUse::Tool});
    }
}

EconomyForecast::EconomyForecast(const civai::EconomyGraph& graph) : graph_(graph)
{
    for(const civai::Recipe& recipe : graph_.GetRecipes())
    {
        const auto steps = static_cast<Amount>(std::llround(recipe.nominalStepsPerMinute * AMOUNT_UNIT));
        nominalSteps_.push_back(steps);
        // Products per minute (all together): every product is chosen for an equal part of the work steps, each step
        // saves the output ratio for its product, a product costs its work steps (ProducedPerBuilding)
        Amount products = steps * static_cast<Amount>(recipe.outputRatio) / 100;
        if(!recipe.outputs.empty())
        {
            double perStep = 0;
            for(const unsigned productSteps : recipe.outputSteps)
                perStep += static_cast<double>(recipe.outputRatio) / std::max(1u, productSteps);
            products = static_cast<Amount>(
              std::llround(static_cast<double>(steps) * perStep / static_cast<double>(recipe.outputs.size())));
        }
        nominalProducts_.push_back(products);
    }
}

const civai::Recipe* EconomyForecast::FindRecipe(const BuildingType bld) const
{
    for(const civai::Recipe& recipe : graph_.GetRecipes())
    {
        if(recipe.building == bld)
            return &recipe;
    }
    return nullptr;
}

Amount EconomyForecast::GetNominalSteps(const BuildingType bld) const
{
    const auto& recipes = graph_.GetRecipes();
    for(unsigned i = 0; i < recipes.size(); ++i)
    {
        if(recipes[i].building == bld)
            return nominalSteps_[i];
    }
    return 0;
}

void EconomyForecast::Compute(const GamePlayer& player, const unsigned gf, const EconomySnapshot& snapshot,
                              const Extra& extra)
{
    gf_ = gf;
    player_ = &player;
    const EconomyStatistics& stats = player.GetEconomyStatistics();
    coveredGFs_ = std::max(1u, stats.GetCoveredGFs(BALANCE_MINUTES, gf_));
    // Less than a minute at the start counts as one minute: a burst in the first seconds is no rate per minute
    measuredGFs_ = std::max(EconomyStatistics::GetGFsPerMinute(), coveredGFs_);
    windowStart_ = gf_ + 1 > coveredGFs_ ? gf_ + 1 - coveredGFs_ : 0u;
    ComputeMeasures(player);
    ComputeFlows(player, snapshot, extra);
    ComputeStatuses(player, snapshot, extra);
}

void EconomyForecast::RecomputeStatuses(const GamePlayer& player, const EconomySnapshot& snapshot, const Extra& extra)
{
    RTTR_Assert(player_ == &player);
    ComputeStatuses(player, snapshot, extra);
}

void EconomyForecast::ComputeMeasures(const GamePlayer& player)
{
    measures_ = {};
    const EconomyStatistics& stats = player.GetEconomyStatistics();
    const unsigned now = gf_ + 1;
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!BuildingProperties::IsUsual(bld))
            continue;
        TypeMeasure& measure = measures_[bld];
        for(const nobUsual* building : player.GetBuildingRegister().GetBuildings(bld))
        {
            measure.buildingGFs += gfsIn(building->GetFinishedGF(), now, windowStart_);
            if(building->GetWorkerSinceGF() == nobUsual::NO_GF)
                continue;
            measure.worked = true;
            measure.workingGFsPercent += static_cast<uint64_t>(gfsIn(building->GetWorkerSinceGF(), now, windowStart_))
                                         * building->GetProductivity();
        }
    }
    // Buildings gone: their minutes count until they are older than the balance length
    for(const GamePlayer::GoneBuilding& gone : player.GetGoneBuildings())
        measures_[gone.type].buildingGFs += gfsIn(gone.finished, std::min(gone.gone, now), windowStart_);
    // Stopped (by the player or the distribution of all wares, Nutzer 2026-11-22): these minutes say nothing about what
    // a building makes. Counted every DISTRIBUTION_INTERVAL GFs: at most the minutes they stood finished
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        TypeMeasure& measure = measures_[bld];
        measure.stoppedGFs = std::min(measure.buildingGFs, stats.GetStoppedBy(bld, BALANCE_MINUTES, gf_));
    }
    // A type whose buildings made anything in the window has worked (also if they are gone now)
    for(const civai::Recipe& recipe : graph_.GetRecipes())
    {
        TypeMeasure& measure = measures_[recipe.building];
        for(const GoodType good : recipe.outputs)
        {
            if(!measure.worked && stats.GetProducedBy(recipe.building, good, BALANCE_MINUTES, gf_) > 0)
                measure.worked = true;
        }
    }
}

Amount EconomyForecast::ProducedPerBuilding(const BuildingType bld, const GoodType good) const
{
    // Per minute the buildings of the type stood finished and not stopped (Nutzer, 2026-11-22)
    const TypeMeasure& measure = measures_[bld];
    const unsigned gfsPerMinute = EconomyStatistics::GetGFsPerMinute();
    const unsigned runningGFs = measure.buildingGFs - measure.stoppedGFs;
    if(player_ && measure.worked && runningGFs >= gfsPerMinute)
    {
        const Amount made = player_->GetEconomyStatistics().GetProducedBy(bld, good, BALANCE_MINUTES, gf_);
        return made * AMOUNT_UNIT * gfsPerMinute / runningGFs;
    }
    // No building of the type worked yet, or they stood stopped (almost) all the time: by the work time
    const auto& recipes = graph_.GetRecipes();
    for(unsigned i = 0; i < recipes.size(); ++i)
    {
        const civai::Recipe& recipe = recipes[i];
        if(recipe.building != bld)
            continue;
        for(unsigned k = 0; k < recipe.outputs.size(); ++k)
        {
            // An equal part of the work steps, the output ratio saved for it, divided by its work steps
            if(recipe.outputs[k] == good)
            {
                return nominalSteps_[i] * static_cast<Amount>(recipe.outputRatio)
                       / (static_cast<Amount>(recipe.outputs.size())
                          * static_cast<Amount>(std::max(1u, recipe.outputSteps[k])));
            }
        }
        for(unsigned j = 0; j < recipe.byproducts.size(); ++j)
        {
            if(recipe.byproducts[j] == good)
                return nominalProducts_[i] * recipe.byproductAmounts[j];
        }
        return 0;
    }
    return 0;
}

Amount EconomyForecast::UsedPerBuilding(const BuildingType bld, const GoodType good) const
{
    // Per minute the buildings of the type stood finished and not stopped (Nutzer, 2026-11-22)
    const TypeMeasure& measure = measures_[bld];
    const unsigned gfsPerMinute = EconomyStatistics::GetGFsPerMinute();
    const unsigned runningGFs = measure.buildingGFs - measure.stoppedGFs;
    if(player_ && measure.worked && runningGFs >= gfsPerMinute)
    {
        const Amount used = player_->GetEconomyStatistics().GetUsedBy(bld, good, BALANCE_MINUTES, gf_);
        return used * AMOUNT_UNIT * gfsPerMinute / runningGFs;
    }
    // By the work time: the amount of each input per work step, an input with several wares shared out among them
    const auto& recipes = graph_.GetRecipes();
    for(unsigned i = 0; i < recipes.size(); ++i)
    {
        const civai::Recipe& recipe = recipes[i];
        if(recipe.building != bld)
            continue;
        Amount used = 0;
        for(const civai::RecipeInput& input : recipe.inputs)
        {
            if(helpers::contains(input.wares, good))
                used += amountOf(nominalSteps_[i], input) / static_cast<Amount>(input.wares.size());
        }
        return used;
    }
    return 0;
}

Amount EconomyForecast::OutputPerWorkingMinute(const BuildingType bld) const
{
    // Wares (all products together) a building makes per minute while its worker is there
    const civai::Recipe* recipe = FindRecipe(bld);
    if(!recipe)
        return 0;
    const TypeMeasure& measure = measures_[bld];
    const unsigned gfsPerMinute = EconomyStatistics::GetGFsPerMinute();
    // At least half a minute worked
    if(player_ && measure.workingGFsPercent * 2 >= static_cast<uint64_t>(gfsPerMinute) * 100u)
    {
        Amount made = 0;
        for(const GoodType good : recipe->outputs)
            made += player_->GetEconomyStatistics().GetProducedBy(bld, good, BALANCE_MINUTES, gf_);
        if(made > 0)
            return made * AMOUNT_UNIT * gfsPerMinute * 100 / static_cast<Amount>(measure.workingGFsPercent);
    }
    const auto& recipes = graph_.GetRecipes();
    return nominalProducts_[static_cast<size_t>(recipe - recipes.data())];
}

void EconomyForecast::ComputeFlows(const GamePlayer& player, const EconomySnapshot& snapshot, const Extra& extra)
{
    const EconomyStatistics& stats = player.GetEconomyStatistics();
    const Amount length = BALANCE_MINUTES;
    const Amount gfsPerMinute = EconomyStatistics::GetGFsPerMinute();
    // Measured: what was made and used in the last balance length, extrapolated to the balance length
    const auto perLength = [&](const Amount count) {
        return count * AMOUNT_UNIT * length * gfsPerMinute / measuredGFs_;
    };
    for(const auto good : getActiveContent().GetGoods())
    {
        if(good == GoodType::Nothing)
            continue;
        production_[good] = perLength(stats.GetProduced(good, BALANCE_MINUTES, gf_));
        // Used as input of buildings. What is used for construction, tools, soldiers, research ... is asked for
        // before it is used: it is part of "requested" (counting it here too would count it twice)
        consumption_[good] = perLength(stats.GetUsed(good, WareUse::Production, BALANCE_MINUTES, gf_));
    }
    groupConsumption_.clear();
    // Coming: building sites, sites ordered, and buildings finished less than a balance length ago (the part of their
    // work not in the statistic yet), by the measured value per building of their type. Standing: buildings with a
    // problem (input, tool, worker, flag, road, fields ...) make only what is measured, but what they would use by
    // their work time counts always
    const unsigned windowGFs = BALANCE_MINUTES * EconomyStatistics::GetGFsPerMinute();
    const GameWorld& world = player.GetGameWorld();
    const auto& recipes = graph_.GetRecipes();
    std::vector<BuildingType> counted;
    for(unsigned idx = 0; idx < recipes.size(); ++idx)
    {
        const civai::Recipe& recipe = recipes[idx];
        if((extra.counts && !extra.counts(recipe.building)) || helpers::contains(counted, recipe.building))
            continue;
        counted.push_back(recipe.building);
        // In hundredths of buildings
        Amount coming =
          (snapshot.buildings[recipe.building].numSites + extra.pendingSites[recipe.building]) * AMOUNT_UNIT;
        Amount standing = 0;
        for(const nobUsual* building : player.GetBuildingRegister().GetBuildings(recipe.building))
        {
            const unsigned age = gf_ > building->GetFinishedGF() ? gf_ - building->GetFinishedGF() : 0u;
            if(age < windowGFs)
            {
                coming += AMOUNT_UNIT - AMOUNT_UNIT * age / windowGFs;
                continue;
            }
            std::vector<GoodType> missing;
            const Diagnosis diagnosis =
              extra.diagnose ? extra.diagnose(*building, recipe) : diagnose(player, *building, recipe, missing, [&]() {
                  return countSources(world, building->GetPos(), recipe, true);
              });
            if(diagnosis == Diagnosis::Working || diagnosis == Diagnosis::Stopped || diagnosis == Diagnosis::Exhausted)
                continue;
            standing += building->HasWorker() ?
                          AMOUNT_UNIT - std::min<Amount>(AMOUNT_UNIT, building->GetProductivity()) :
                          AMOUNT_UNIT;
        }
        // Stopped buildings (by the player or the distribution of all wares, also those gone since) count with their
        // full work (Nutzer, 2026-11-22): in the minutes they stood stopped what one makes per minute, extrapolated
        // like the measured production. Not what they would use: they take their inputs once they work again
        const Amount stoppedGFs = measures_[recipe.building].stoppedGFs;
        if(stoppedGFs > 0)
        {
            const auto addStopped = [&](const GoodType good) {
                production_[good] += stoppedGFs * ProducedPerBuilding(recipe.building, good) * length / measuredGFs_;
            };
            for(const GoodType good : recipe.outputs)
                addStopped(good);
            for(unsigned i = 0; i < recipe.byproducts.size(); ++i)
            {
                const int tech = recipe.byproductTechs[i];
                if(tech < 0 || player.GetResearch().IsResearched(static_cast<TechIdx>(tech)))
                    addStopped(recipe.byproducts[i]);
            }
        }
        if(coming > 0)
        {
            for(const GoodType good : recipe.outputs)
                production_[good] += coming * ProducedPerBuilding(recipe.building, good) * length / AMOUNT_UNIT;
            for(unsigned i = 0; i < recipe.byproducts.size(); ++i)
            {
                const int tech = recipe.byproductTechs[i];
                if(tech < 0 || player.GetResearch().IsResearched(static_cast<TechIdx>(tech)))
                {
                    production_[recipe.byproducts[i]] +=
                      coming * ProducedPerBuilding(recipe.building, recipe.byproducts[i]) * length / AMOUNT_UNIT;
                }
            }
            for(const civai::RecipeInput& input : recipe.inputs)
            {
                if(input.wares.size() == 1u)
                {
                    const GoodType good = input.wares.front();
                    consumption_[good] += coming * UsedPerBuilding(recipe.building, good) * length / AMOUNT_UNIT;
                } else
                {
                    // Any of the wares of the input
                    Amount used = 0;
                    for(const GoodType good : input.wares)
                        used += UsedPerBuilding(recipe.building, good);
                    groupConsumption_.emplace_back(sorted(input.wares), coming * used * length / AMOUNT_UNIT);
                }
            }
        }
        if(standing > 0)
        {
            const Amount steps = nominalSteps_[idx] * standing * length / AMOUNT_UNIT;
            for(const civai::RecipeInput& input : recipe.inputs)
            {
                if(input.wares.size() == 1u)
                    consumption_[input.wares.front()] += amountOf(steps, input);
                else
                    groupConsumption_.emplace_back(sorted(input.wares), amountOf(steps, input));
            }
        }
    }
}

void EconomyForecast::ComputeStatuses(const GamePlayer& player, const EconomySnapshot& snapshot, const Extra& extra)
{
    const EconomyStatistics& stats = player.GetEconomyStatistics();
    const TechTree& techTree = player.GetGameWorld().GetTechTree();
    const Amount gfsPerMinute = EconomyStatistics::GetGFsPerMinute();

    // "Reserviert": the wares on their way, without those on their way to a consumer (their request is not open any
    // more, otherwise they would count twice)
    helpers::EnumArray<unsigned, GoodType> onTheWay{};
    for(const Ware* ware : player.GetWares())
    {
        if(ware->type != GoodType::Nothing)
            ++onTheWay[ware->type];
    }
    for(const auto good : getActiveContent().GetGoods())
    {
        ForecastStatus status;
        if(good != GoodType::Nothing)
        {
            status.stock = snapshot.inWarehouses[good];
            status.inTransit = snapshot.inTransit[good];
            status.onTheWay =
              static_cast<Amount>(onTheWay[good] > status.inTransit ? onTheWay[good] - status.inTransit : 0u)
              * AMOUNT_UNIT;
            status.balance = stats.GetBalance(good, BALANCE_MINUTES, gf_);
            status.consumptionPerMinute = static_cast<Amount>(stats.GetConsumed(good, BALANCE_MINUTES, gf_))
                                          * AMOUNT_UNIT * gfsPerMinute / coveredGFs_;
            if(techTree.IsAIStartStockNoSurplus(good))
            {
                uint32_t used = 0;
                for(const auto use : helpers::enumRange<WareUse>())
                    used += stats.GetTotalUsed(good, use);
                const uint32_t added = stats.GetTotalGained(good, WareSource::Added);
                status.startStockLeft = added > used ? added - used : 0u;
            }
            status.production = production_[good];
        }
        wares_[good] = status;
    }

    // Every request is an entry: some wares (one of them will do) and an amount. Open: what the game asks for now.
    // Consumption: what the buildings will use in the balance length (alone or "any of"). Planned: what the building
    // sites and the AI will ask for, and the tools kept in stock.
    // The entries are shared out among the wares, so that no ware is counted twice (Nutzer, 2026-11-17). What a ware
    // has: its stock, the wares on their way and what will be made in the balance length. The entries with fewest wares
    // first (a ware asked for alone can come from nowhere else); equal: open before consumption before planned, planned
    // in the order given. In an entry the wares fewer other entries take first, equal: the one with more left. What
    // none of its wares has left is missing for exactly the wares of the entry
    enum class Part : uint8_t
    {
        Open,
        Consumption,
        Planned
    };
    struct Entry
    {
        std::vector<GoodType> wares;
        Amount amount;
        Part part;
        bool byOrder;
    };
    std::vector<Entry> entries;
    const auto addEntry = [&entries](std::vector<GoodType> wares, const Amount amount, const Part part,
                                     const bool byOrder) {
        wares = sorted(std::move(wares));
        if(wares.empty() || amount <= 0)
            return;
        entries.push_back({std::move(wares), amount, part, byOrder});
    };
    // Construction costs: with the share given (the AI evaluates its shortages with less, the game counts them fully)
    const auto constructionPart = [&extra](const Amount amount) {
        return amount * static_cast<Amount>(extra.constructionShare) / static_cast<Amount>(CONSTRUCTION_SHARE_FULL);
    };
    for(const OpenDemand& demand : snapshot.openDemand)
    {
        const Amount amount = static_cast<Amount>(demand.amount) * AMOUNT_UNIT;
        addEntry(demand.wares, demand.purpose == WareUse::Construction ? constructionPart(amount) : amount, Part::Open,
                 false);
    }
    for(const auto good : getActiveContent().GetGoods())
    {
        if(good != GoodType::Nothing)
            addEntry({good}, consumption_[good], Part::Consumption, false);
    }
    for(const auto& [anyOf, amount] : groupConsumption_)
        addEntry(anyOf, amount, Part::Consumption, false);
    // Building sites: the tools they will ask for (their construction costs the game asks for: open)
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        for(unsigned i = snapshot.buildings[bld].numSites; i > 0; --i)
        {
            std::vector<OpenDemand> demand;
            addToolDemand(snapshot, demand, bld);
            for(const OpenDemand& entry : demand)
                addEntry(entry.wares, static_cast<Amount>(entry.amount) * AMOUNT_UNIT, Part::Planned, false);
        }
    }
    for(const PlannedEntry& entry : extra.planned)
        addEntry(entry.wares, entry.construction ? constructionPart(entry.amount) : entry.amount, Part::Planned,
                 entry.byOrder);
    // The tools of a kind kept in stock are asked for too (so they are made before they are missing)
    helpers::EnumArray<std::vector<GoodType>, Tool> toolKinds;
    for(const auto good : getActiveContent().GetGoods())
    {
        if(good == GoodType::Nothing)
            continue;
        if(const auto kind = getToolKindOf(good))
            toolKinds[*kind].push_back(good);
    }
    for(const auto tool : helpers::enumRange<Tool>())
    {
        if(!toolKinds[tool].empty())
            addEntry(toolKinds[tool], static_cast<Amount>(TOOLS_IN_STOCK) * AMOUNT_UNIT, Part::Planned, false);
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& lhs, const Entry& rhs) {
        if(lhs.wares.size() != rhs.wares.size())
            return lhs.wares.size() < rhs.wares.size();
        return lhs.part < rhs.part;
    });
    // How many different entries (by their wares) take each ware
    helpers::EnumArray<unsigned, GoodType> numTaking{};
    entrySets_.clear();
    for(const Entry& entry : entries)
    {
        if(helpers::contains(entrySets_, entry.wares))
            continue;
        entrySets_.push_back(entry.wares);
        for(const GoodType good : entry.wares)
            ++numTaking[good];
    }
    // Share out a list of entries among the wares (`left`: what each ware has), returns per ware what it got
    const auto shareOut = [&numTaking](const std::vector<const Entry*>& list, helpers::EnumArray<Amount, GoodType> left,
                                       const auto& give, const auto& lack) {
        std::vector<GoodType> order;
        for(const Entry* entry : list)
        {
            order = entry->wares;
            std::stable_sort(order.begin(), order.end(), [&](const GoodType lhs, const GoodType rhs) {
                if(numTaking[lhs] != numTaking[rhs])
                    return numTaking[lhs] < numTaking[rhs];
                return left[lhs] > left[rhs];
            });
            Amount need = entry->amount;
            for(const GoodType good : order)
            {
                if(need <= 0)
                    break;
                const Amount taken = std::min(need, std::max<Amount>(0, left[good]));
                if(taken <= 0)
                    continue;
                left[good] -= taken;
                give(*entry, good, taken);
                need -= taken;
            }
            if(need > 0)
                lack(*entry, need);
        }
    };
    const auto addShare = [](Shares& shares, const Entry& entry, const Amount amount) {
        switch(entry.part)
        {
            case Part::Open: shares.open += amount; break;
            case Part::Consumption: shares.consumption += amount; break;
            case Part::Planned:
                shares.planned += amount;
                if(entry.byOrder)
                    shares.plannedByOrders += amount;
                break;
        }
    };
    // Forecast: all entries, what a ware has = stock + on the way + production
    given_ = {};
    missing_.clear();
    available_ = {};
    availableCount_ = {};
    for(const auto good : getActiveContent().GetGoods())
    {
        if(good == GoodType::Nothing)
            continue;
        availableCount_[good] = static_cast<Amount>(wares_[good].stock) * AMOUNT_UNIT + wares_[good].onTheWay;
        available_[good] = availableCount_[good] + production_[good];
    }
    std::vector<const Entry*> all, open;
    for(const Entry& entry : entries)
    {
        all.push_back(&entry);
        if(entry.part == Part::Open)
            open.push_back(&entry);
    }
    shareOut(
      all, available_,
      [&](const Entry& entry, const GoodType good, const Amount amount) { addShare(given_[good], entry, amount); },
      [&](const Entry& entry, const Amount amount) { addShare(missing_[entry.wares], entry, amount); });
    // Number of wares: only the open requests, what a ware has = stock + on the way
    givenCount_ = {};
    missingCount_.clear();
    shareOut(
      open, availableCount_,
      [&](const Entry&, const GoodType good, const Amount amount) { givenCount_[good] += amount; },
      [&](const Entry& entry, const Amount amount) { missingCount_[entry.wares] += amount; });

    for(const auto good : getActiveContent().GetGoods())
    {
        if(good != GoodType::Nothing)
            wares_[good] = GetGroup({good});
    }
}

ForecastStatus EconomyForecast::GetGroup(const std::vector<GoodType>& wares) const
{
    ForecastStatus status;
    Shares shares;
    Amount countGiven = 0, countAvailable = 0;
    for(const GoodType good : wares)
    {
        const ForecastStatus& ware = wares_[good];
        status.stock += ware.stock;
        status.onTheWay += ware.onTheWay;
        status.inTransit += ware.inTransit;
        status.balance += ware.balance;
        status.consumptionPerMinute += ware.consumptionPerMinute;
        status.startStockLeft += ware.startStockLeft;
        status.production += production_[good];
        shares.open += given_[good].open;
        shares.consumption += given_[good].consumption;
        shares.planned += given_[good].planned;
        shares.plannedByOrders += given_[good].plannedByOrders;
        countGiven += givenCount_[good];
        countAvailable += availableCount_[good];
    }
    const auto lacking = missing_.find(wares);
    if(lacking != missing_.end())
    {
        const Shares& lack = lacking->second;
        shares.open += lack.open;
        shares.consumption += lack.consumption;
        shares.planned += lack.planned;
        shares.plannedByOrders += lack.plannedByOrders;
        status.missing = lack.open + lack.consumption + lack.planned;
    }
    status.openDemand = shares.open;
    status.planned = shares.planned;
    status.plannedByOrders = shares.plannedByOrders;
    status.consumption = shares.consumption;
    status.free =
      static_cast<Amount>(status.stock) * AMOUNT_UNIT + status.onTheWay - status.openDemand - status.planned;
    status.forecast = status.free + status.production - status.consumption;
    const auto lackingCount = missingCount_.find(wares);
    status.countMissing = lackingCount != missingCount_.end() ? lackingCount->second : 0;
    status.count = countAvailable - countGiven - status.countMissing;
    return status;
}

} // namespace civ
