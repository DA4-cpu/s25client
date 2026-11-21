// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/WareDistribution.h"
#include "BuildingRegister.h"
#include "EconomySnapshot.h"
#include "EventManager.h"
#include "GamePlayer.h"
#include "SerializedGameData.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "figures/nofBuildingWorker.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "helpers/serializeEnums.h"
#include "mygettext/mygettext.h"
#include "postSystem/PostMsg.h"
#include "random/Random.h"
#include "world/GameWorld.h"
#include "gameData/BuildingProperties.h"
#include <algorithm>

namespace civ {

DistributionData::DistributionData(const GameWorldBase& world) : graph(world)
{
    for(const civai::Recipe& recipe : graph.GetRecipes())
    {
        BuildingProducts& entry = products[recipe.building];
        if(!entry.products.empty() || !entry.byproducts.empty())
            continue;
        entry.products = recipe.outputs;
        entry.byproducts = recipe.byproducts;
        entry.byproductTechs = recipe.byproductTechs;
    }
}

} // namespace civ

const civ::DistributionData& GameWorldBase::GetDistributionData() const
{
    // The game data does not change during the game: built once
    if(!distributionData_)
        distributionData_ = std::make_unique<civ::DistributionData>(*this);
    return *distributionData_;
}

namespace civ {

void WareDistribution::SetSetting(const GoodType good, const WareSetting& setting)
{
    WareSetting& own = settings_[good];
    own = setting;
    own.manualValue = std::min(own.manualValue, DISTRIBUTION_SHOWN);
}

void WareDistribution::ChangeOrders(const GoodType good, const int change)
{
    const int result = std::clamp(static_cast<int>(orders_[good]) + change, 0, static_cast<int>(MAX_WARE_ORDERS));
    orders_[good] = static_cast<uint16_t>(result);
}

bool WareDistribution::OrderProcessed(const GoodType good)
{
    if(orders_[good] > 0)
        --orders_[good];
    return std::all_of(orders_.begin(), orders_.end(), [](const uint16_t orders) { return orders == 0; });
}

void WareDistribution::Update(const GameWorldBase& world, const unsigned playerId, const unsigned gf)
{
    const GamePlayer& player = world.GetPlayer(playerId);
    const DistributionData& data = world.GetDistributionData();
    EconomyForecast forecast(data.graph);
    const EconomySnapshot snapshot = takeEconomySnapshot(player);
    forecast.Compute(player, gf, snapshot, {});

    // Made by a finished building of the player
    helpers::EnumArray<bool, GoodType> made{};
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!BuildingProperties::IsUsual(bld) || player.GetBuildingRegister().GetBuildings(bld).empty())
            continue;
        for(const GoodType good : data.products[bld].products)
            made[good] = true;
    }
    // The number of a ware: its own, or the lower one of a group with it a consumer asks for exactly (what none of the
    // wares of the group has left is missing for each of them)
    const auto numberOf = [this](const ForecastStatus& status) {
        return usesForecast_ ? status.forecast : status.count;
    };
    helpers::EnumArray<Amount, GoodType> numbers{};
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(good != GoodType::Nothing)
            numbers[good] = numberOf(forecast.GetWare(good));
    }
    for(const std::vector<GoodType>& wares : forecast.GetEntrySets())
    {
        if(wares.size() < 2u)
            continue;
        const Amount groupNumber = numberOf(forecast.GetGroup(wares));
        for(const GoodType good : wares)
            numbers[good] = std::min(numbers[good], groupNumber);
    }
    // Surplus: the number is above SURPLUS_FACTOR x consumption per minute, at least MIN_STOCK (the start stock counts,
    // Nutzer 2026-11-21)
    std::vector<GoodType> valued;
    for(const auto good : helpers::enumRange<GoodType>())
    {
        values_[good] = 0;
        numbers_[good] = numbers[good];
        if(good == GoodType::Nothing || !isInDistribution(good))
        {
            states_[good] = WareDistState::NoBuilding;
            unwanted_[good] = false;
            continue;
        }
        const WareSetting& setting = settings_[good];
        const Amount threshold = std::max<Amount>(static_cast<Amount>(MIN_STOCK) * AMOUNT_UNIT,
                                                  SURPLUS_FACTOR * forecast.GetWare(good).consumptionPerMinute);
        const bool surplus = numbers[good] > threshold;
        unwanted_[good] = setting.autoPause ? surplus : setting.paused;
        if(!made[good])
            states_[good] = WareDistState::NoBuilding;
        else if(unwanted_[good])
            states_[good] = setting.autoPause ? WareDistState::Surplus : WareDistState::Paused;
        else
        {
            states_[good] = WareDistState::Valued;
            valued.push_back(good);
        }
    }
    if(valued.empty())
        return;
    // The lowest number (largest shortage) gets the highest value, the highest number 0. Values set by the player stay
    Amount lowest = numbers[valued.front()], highest = lowest;
    for(const GoodType good : valued)
    {
        lowest = std::min(lowest, numbers[good]);
        highest = std::max(highest, numbers[good]);
    }
    for(const GoodType good : valued)
    {
        const WareSetting& setting = settings_[good];
        if(!setting.autoValue)
            values_[good] = static_cast<uint32_t>(setting.manualValue) * (DISTRIBUTION_SCALE / DISTRIBUTION_SHOWN);
        else if(highest == lowest)
            values_[good] = DISTRIBUTION_SCALE;
        else
        {
            values_[good] = static_cast<uint32_t>(static_cast<Amount>(DISTRIBUTION_SCALE) * (highest - numbers[good])
                                                  / (highest - lowest));
        }
    }
}

void WareDistribution::Serialize(SerializedGameData& sgd) const
{
    sgd.PushBool(usesForecast_);
    for(const auto good : helpers::enumRange<GoodType>())
    {
        const WareSetting& setting = settings_[good];
        sgd.PushBool(setting.autoValue);
        sgd.PushUnsignedShort(setting.manualValue);
        sgd.PushBool(setting.autoPause);
        sgd.PushBool(setting.paused);
        sgd.PushUnsignedShort(orders_[good]);
        sgd.PushUnsignedInt(values_[good]);
        sgd.PushSignedInt(static_cast<int32_t>(std::clamp<Amount>(numbers_[good], INT32_MIN, INT32_MAX)));
        helpers::pushEnum<uint8_t>(sgd, states_[good]);
        sgd.PushBool(unwanted_[good]);
    }
}

void WareDistribution::Deserialize(SerializedGameData& sgd)
{
    usesForecast_ = sgd.PopBool();
    for(const auto good : helpers::enumRange<GoodType>())
    {
        WareSetting& setting = settings_[good];
        setting.autoValue = sgd.PopBool();
        setting.manualValue = std::min(sgd.PopUnsignedShort(), DISTRIBUTION_SHOWN);
        setting.autoPause = sgd.PopBool();
        setting.paused = sgd.PopBool();
        orders_[good] = sgd.PopUnsignedShort();
        values_[good] = sgd.PopUnsignedInt();
        numbers_[good] = sgd.PopSignedInt();
        states_[good] = helpers::popEnum<WareDistState>(sgd);
        unwanted_[good] = sgd.PopBool();
    }
}

bool isInDistribution(const GoodType good)
{
    return good != GoodType::Nothing && !helpers::contains(getActiveContent().GetKnowledgeMedia(), good);
}

bool isDistributionActive(const GameWorldBase& world)
{
    return isEnabled(world);
}

const BuildingProducts& getBuildingProducts(const GameWorldBase& world, const BuildingType bld)
{
    return world.GetDistributionData().products[bld];
}

bool isPausedByDistribution(const GameWorldBase& world, const nobUsual& building)
{
    if(!isDistributionActive(world))
        return false;
    const BuildingProducts& products = getBuildingProducts(world, building.GetBuildingType());
    if(products.products.empty())
        return false;
    const GamePlayer& owner = world.GetPlayer(building.GetPlayer());
    const WareDistribution& distribution = owner.GetWareDistribution();
    for(const GoodType good : products.products)
    {
        if(!isInDistribution(good) || !distribution.IsUnwanted(good) || distribution.GetOrders(good) > 0)
            return false;
    }
    // A byproduct still wanted keeps it working (both must be surplus, Nutzer 2026-11-21)
    for(unsigned i = 0; i < products.byproducts.size(); ++i)
    {
        const int tech = products.byproductTechs[i];
        if(tech >= 0 && !owner.GetResearch().IsResearched(static_cast<TechIdx>(tech)))
            continue;
        if(!isInDistribution(products.byproducts[i]) || !distribution.IsUnwanted(products.byproducts[i]))
            return false;
    }
    return true;
}

namespace {
    std::vector<std::pair<nobUsual*, bool>> getPauseStates(const GameWorld& world, const unsigned playerId)
    {
        std::vector<std::pair<nobUsual*, bool>> result;
        const BuildingRegister& buildings = world.GetPlayer(playerId).GetBuildingRegister();
        for(const auto bld : helpers::enumRange<BuildingType>())
        {
            if(!BuildingProperties::IsUsual(bld))
                continue;
            for(nobUsual* building : buildings.GetBuildings(bld))
                result.emplace_back(building, building->IsProductionStopped());
        }
        return result;
    }

    void notifyChanged(const std::vector<std::pair<nobUsual*, bool>>& before)
    {
        for(const auto& [building, wasStopped] : before)
        {
            nofBuildingWorker* worker = building->GetWorkerToNotify();
            if(!worker || building->IsProductionStopped() == wasStopped)
                continue;
            if(wasStopped)
                worker->GotWareOrProductionAllowed();
            else
                worker->ProductionStopped();
        }
    }
} // namespace

void updateDistribution(GameWorld& world, const unsigned playerId)
{
    const auto before = getPauseStates(world, playerId);
    world.GetPlayer(playerId).GetWareDistribution().Update(world, playerId, world.GetEvMgr().GetCurrentGF());
    notifyChanged(before);
}

void runDistributions(GameWorld& world, const unsigned gf)
{
    if(!isDistributionActive(world))
        return;
    const unsigned numPlayers = world.GetNumPlayers();
    for(unsigned i = 0; i < numPlayers; ++i)
    {
        if(world.GetPlayer(i).isUsed() && gf % DISTRIBUTION_INTERVAL == i * DISTRIBUTION_INTERVAL / numPlayers)
            updateDistribution(world, i);
    }
}

void wakeBuildingsFor(GameWorld& world, const unsigned playerId, const GoodType good)
{
    const BuildingRegister& buildings = world.GetPlayer(playerId).GetBuildingRegister();
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!BuildingProperties::IsUsual(bld) || !helpers::contains(getBuildingProducts(world, bld).products, good))
            continue;
        for(nobUsual* building : buildings.GetBuildings(bld))
        {
            if(building->GetWorkerToNotify() && !building->IsProductionStopped())
                building->GetWorkerToNotify()->GotWareOrProductionAllowed();
        }
    }
}

namespace {
    void orderProcessed(GameWorld& world, const unsigned playerId, const GoodType good)
    {
        if(world.GetPlayer(playerId).GetWareDistribution().OrderProcessed(good))
        {
            world.GetPlayer(playerId).SendPostMessage(std::make_unique<PostMsg>(
              world.GetEvMgr().GetCurrentGF(), _("Completed the ordered amount of wares."), PostCategory::Economy));
        }
    }
} // namespace

void startedWorkStep(GameWorld& world, const nobUsual& building)
{
    if(!isDistributionActive(world))
        return;
    const BuildingProducts& products = getBuildingProducts(world, building.GetBuildingType());
    // Several products: the order is processed when the product is chosen (chooseProduct)
    if(products.products.size() != 1u)
        return;
    const GoodType good = products.products.front();
    if(world.GetPlayer(building.GetPlayer()).GetWareDistribution().GetOrders(good) > 0)
        orderProcessed(world, building.GetPlayer(), good);
}

GoodType chooseProduct(GameWorld& world, const unsigned playerId, const std::vector<GoodType>& products,
                       const unsigned objId)
{
    RTTR_Assert(!products.empty());
    WareDistribution& distribution = world.GetPlayer(playerId).GetWareDistribution();
    // Weights for the building: its highest value 1000 (100 %), its lowest 0 (0 %). All equal: equal weights.
    // `minWeight` 1: ordered products, each one possible
    const auto pick = [&](const std::vector<GoodType>& candidates, const uint32_t minWeight) {
        uint32_t lowest = DISTRIBUTION_SCALE, highest = 0;
        for(const GoodType good : candidates)
        {
            lowest = std::min(lowest, distribution.GetValue(good));
            highest = std::max(highest, distribution.GetValue(good));
        }
        std::vector<uint32_t> weights;
        uint32_t total = 0;
        for(const GoodType good : candidates)
        {
            uint32_t weight = 1;
            if(highest > lowest)
            {
                weight =
                  std::max(minWeight, static_cast<uint32_t>(static_cast<uint64_t>(distribution.GetValue(good) - lowest)
                                                            * 1000u / (highest - lowest)));
            }
            weights.push_back(weight);
            total += weight;
        }
        if(total == 0)
            return candidates.front();
        uint32_t value = static_cast<uint32_t>(RANDOM.Rand(RANDOM_CONTEXT2(objId), static_cast<int>(total)));
        for(unsigned i = 0; i < candidates.size(); ++i)
        {
            if(value < weights[i])
                return candidates[i];
            value -= weights[i];
        }
        return candidates.back();
    };
    // Orders first (shared by all buildings: whoever starts first fulfills it)
    std::vector<GoodType> candidates;
    for(const GoodType good : products)
    {
        if(distribution.GetOrders(good) > 0)
            candidates.push_back(good);
    }
    if(!candidates.empty())
    {
        const GoodType good = pick(candidates, 1u);
        orderProcessed(world, playerId, good);
        return good;
    }
    // The wanted ones. None (a byproduct keeps the building working): all, equally
    for(const GoodType good : products)
    {
        if(!distribution.IsUnwanted(good))
            candidates.push_back(good);
    }
    if(candidates.empty())
        return products[RANDOM.Rand(RANDOM_CONTEXT2(objId), static_cast<int>(products.size()))];
    return pick(candidates, 0u);
}

} // namespace civ
