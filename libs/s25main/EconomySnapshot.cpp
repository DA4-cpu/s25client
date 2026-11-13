// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "EconomySnapshot.h"
#include "BuildingRegister.h"
#include "GamePlayer.h"
#include "LeatherLoader.h"
#include "Ware.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHarborBuilding.h"
#include "buildings/nobMilitary.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "figures/nofBuildingWorker.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "world/GameWorld.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/JobConsts.h"
#include <algorithm>

unsigned BuildingTypeState::GetNumWaitingForInput() const
{
    unsigned result = 0;
    for(const WaitingForInput& entry : waitingForInput)
        result += entry.numBuildings;
    return result;
}

unsigned BuildingTypeState::GetProductivity() const
{
    const unsigned numWithWorker = numBuildings - withoutWorker;
    return numWithWorker ? productivitySum / numWithWorker : 0u;
}

unsigned EconomySnapshot::GetOpenDemand(const GoodType good) const
{
    unsigned result = 0;
    for(const OpenDemand& entry : openDemand)
    {
        if(helpers::contains(entry.wares, good))
            result += entry.amount;
    }
    return result;
}

namespace {
void addDemand(EconomySnapshot& snapshot, std::vector<GoodType> wares, const unsigned amount, const WareUse purpose)
{
    if(amount > 0 && !wares.empty())
        snapshot.openDemand.push_back({std::move(wares), amount, purpose});
}
} // namespace

void addInputSpaces(std::vector<OpenDemand>& demand, const BuildingType bld)
{
    const BldWorkDescription& workDesc = BLD_WORK_DESC[bld];
    if(workDesc.waresNeeded.empty() || workDesc.numSpacesPerWare == 0)
        return;
    if(workDesc.useOneWareEach)
    {
        for(const GoodType good : workDesc.waresNeeded)
        {
            if(good != GoodType::Nothing)
                demand.push_back({{good}, workDesc.numSpacesPerWare, WareUse::Production});
        }
    } else
    {
        std::vector<GoodType> anyOf;
        for(const GoodType good : workDesc.waresNeeded)
        {
            if(good != GoodType::Nothing)
                anyOf.push_back(good);
        }
        if(!anyOf.empty())
            demand.push_back({std::move(anyOf), workDesc.numSpacesPerWare, WareUse::Production});
    }
}

namespace {
void addWaiting(BuildingTypeState& state, std::vector<GoodType> wares)
{
    for(WaitingForInput& entry : state.waitingForInput)
    {
        if(entry.wares == wares)
        {
            ++entry.numBuildings;
            return;
        }
    }
    state.waitingForInput.push_back({std::move(wares), 1});
}

/// Tool a warehouse needs to recruit a worker of the job. Nothing: none
GoodType getRecruitTool(const GameWorld& world, const Job job)
{
    const bool civMode = civ::isEnabled(world);
    if(civMode && civ::getActiveContent().IsRecruitedWithoutTool(job))
        return GoodType::Nothing;
    if(isCustomJob(job))
    {
        const civ::CustomWorker* worker = civMode ? civ::getActiveContent().GetWorker(job) : nullptr;
        return worker ? worker->tool : GoodType::Nothing;
    }
    return JOB_CONSTS[job].tool.value_or(GoodType::Nothing);
}

void addUsualBuilding(EconomySnapshot& snapshot, const nobUsual& building)
{
    const BuildingType bld = building.GetBuildingType();
    BuildingTypeState& state = snapshot.buildings[bld];
    ++state.numBuildings;
    const bool stopped = building.IsProductionDisabled();
    if(stopped)
        ++state.stopped;

    // Inputs not yet ordered (a warehouse would have sent them if it had any)
    const BldWorkDescription& workDesc = BLD_WORK_DESC[bld];
    if(!stopped && !building.IsLibrary())
    {
        std::vector<GoodType> anyOf;
        unsigned anyOfMissing = 0;
        for(unsigned i = 0; i < workDesc.waresNeeded.size(); ++i)
        {
            const unsigned have = building.GetNumWares(i) + building.GetNumOrderedWares(i);
            const unsigned missing = workDesc.numSpacesPerWare > have ? workDesc.numSpacesPerWare - have : 0u;
            if(missing == 0)
                continue;
            if(workDesc.useOneWareEach)
                addDemand(snapshot, {workDesc.waresNeeded[i]}, missing, WareUse::Production);
            else
            {
                anyOf.push_back(workDesc.waresNeeded[i]);
                anyOfMissing = std::max(anyOfMissing, missing);
            }
        }
        addDemand(snapshot, anyOf, anyOfMissing, WareUse::Production);
    }

    // Tool reform: tools for empty slots and replacements that are due
    const auto& toolSlots = building.GetToolSlots();
    if(!toolSlots.empty() && !stopped)
    {
        const auto& slotDescs = civ::getActiveContent().GetBuildingTools(bld).slots;
        for(unsigned i = 0; i < toolSlots.size() && i < slotDescs.size(); ++i)
        {
            if(building.NeedsToolOrder(toolSlots[i]))
                addDemand(snapshot, slotDescs[i].wares, 1, WareUse::Tool);
        }
    }

    if(!building.HasWorker())
    {
        ++state.withoutWorker;
        return;
    }
    state.productivitySum += building.GetProductivity();
    if(!toolSlots.empty() && building.GetWorkSpeed() == 0)
        ++state.withoutTool;
    else if(!stopped && building.GetWorker()
            && building.GetWorker()->GetState() == nofBuildingWorker::State::WaitingForWaresOrProductionStopped)
    {
        // Waiting for wares: which ones are missing
        std::vector<GoodType> missing;
        bool anyThere = false;
        for(unsigned i = 0; i < workDesc.waresNeeded.size(); ++i)
        {
            if(building.GetNumWares(i) == 0)
                missing.push_back(workDesc.waresNeeded[i]);
            else
                anyThere = true;
        }
        const bool canWork = workDesc.useOneWareEach ? missing.empty() : anyThere;
        if(!canWork && !missing.empty())
            addWaiting(state, std::move(missing));
    }
}
} // namespace

EconomySnapshot takeEconomySnapshot(const GamePlayer& player)
{
    EconomySnapshot snapshot;
    const GameWorld& world = player.GetGameWorld();
    const BuildingRegister& buildings = player.GetBuildingRegister();

    // Building sites: material not yet ordered
    for(const noBuildingSite* site : buildings.GetBuildingSites())
    {
        const BuildingType bld = site->GetBuildingType();
        ++snapshot.buildings[bld].numSites;
        const BuildingCosts& costs = BUILDING_COSTS[bld];
        const auto& materials = site->GetMaterials();
        for(unsigned i = 0; i < costs.size() && i < materials.size(); ++i)
        {
            const noBuildingSite::Material& material = materials[i];
            const unsigned have = material.used + material.delivered.size() + material.ordered.size();
            if(costs[i].amount > have)
                addDemand(snapshot, costs[i].wares, costs[i].amount - have, WareUse::Construction);
        }
        if(BuildingProperties::IsUsual(bld))
            addInputSpaces(snapshot.siteInputs, bld);
    }

    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!BuildingProperties::IsUsual(bld))
            continue;
        for(const nobUsual* building : buildings.GetBuildings(bld))
            addUsualBuilding(snapshot, *building);
    }

    // Workers no warehouse could send yet: the tool to recruit them
    snapshot.jobsWanted = player.GetNumJobsWanted();
    for(const auto job : helpers::enumRange<Job>())
    {
        if(snapshot.jobsWanted[job] == 0)
            continue;
        const GoodType tool = getRecruitTool(world, job);
        if(tool != GoodType::Nothing)
            addDemand(snapshot, {tool}, snapshot.jobsWanted[job], WareUse::Tool);
    }

    // Military buildings: coins (only where they are allowed), armor, soldiers
    const bool armorActive = leatheraddon::isAddonActive(world);
    for(const nobMilitary* building : buildings.GetMilitaryBuildings())
    {
        const unsigned present =
          building->GetNumTroops() + building->GetNumOrderedTroops() + building->GetNumTroopsOnMission();
        const unsigned required = building->CalcRequiredNumTroops();
        if(required > present)
            snapshot.openSoldiers += required - present;
        if(building->IsNewBuilt())
            continue;
        const unsigned coins = building->GetNumCoins() + building->GetNumOrderedCoins();
        if(!building->IsGoldDisabled() && building->GetMaxCoinCt() > coins)
            addDemand(snapshot, {GoodType::Coins}, building->GetMaxCoinCt() - coins, WareUse::Military);
        const unsigned armor = building->GetNumArmor() + building->GetNumOrderedArmor();
        if(armorActive && building->IsArmorAllowed() && building->GetMaxArmorCt() > armor)
            addDemand(snapshot, {GoodType::Armor}, building->GetMaxArmorCt() - armor, WareUse::Military);
    }

    // Expeditions being prepared in harbors
    for(const nobHarborBuilding* harbor : buildings.GetHarbors())
    {
        addDemand(snapshot, {GoodType::Boards}, harbor->GetExpeditionBoardsMissing(), WareUse::Construction);
        addDemand(snapshot, {GoodType::Stones}, harbor->GetExpeditionStonesMissing(), WareUse::Construction);
    }

    // Current research (civilization mode): costs not paid yet
    if(civ::isEnabled(world))
    {
        const civ::PlayerResearch& research = player.GetResearch();
        if(const auto current = research.GetCurrent())
        {
            const civ::Technology& tech = world.GetTechTree().get(*current);
            const civ::PlayerResearch::Progress* progress = research.GetProgress(*current);
            for(unsigned i = 0; i < tech.cost.size(); ++i)
            {
                const unsigned paid = progress && i < progress->paid.size() ? progress->paid[i] : 0u;
                if(tech.cost[i].amount > paid)
                    addDemand(snapshot, {tech.cost[i].good}, tech.cost[i].amount - paid, WareUse::Research);
            }
        }
    }

    // Wares on their way to a consumer
    for(const Ware* ware : player.GetWares())
    {
        const noBaseBuilding* goal = ware->GetGoal();
        if(goal && !BuildingProperties::IsWareHouse(goal->GetBuildingType()) && ware->type != GoodType::Nothing)
            ++snapshot.inTransit[ware->type];
    }

    for(const nobBaseWarehouse* warehouse : buildings.GetStorehouses())
    {
        for(const auto good : helpers::enumRange<GoodType>())
            snapshot.inWarehouses[good] += warehouse->GetNumRealWares(good);
        for(const auto job : helpers::enumRange<Job>())
            snapshot.inWarehousesPeople[job] += warehouse->GetNumRealFigures(job);
    }
    return snapshot;
}
