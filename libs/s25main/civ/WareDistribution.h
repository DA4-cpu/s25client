// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "civ/EconomyForecast.h"
#include "helpers/EnumArray.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include <cstdint>
#include <vector>

class GameWorld;
class GameWorldBase;
class SerializedGameData;
class nobUsual;

namespace civ {

/// Distribution of all wares (Gesamtverteilung, DESIGN.md): one value per ware, set by itself from the forecast or the
/// number of wares of the player. The values steer every building that makes the wares: a building with several
/// products chooses by them, a building stops when all its products and byproducts are not wanted.
/// Every player computes it every DISTRIBUTION_INTERVAL GFs, staggered: player p at GF % interval == p * interval / n
constexpr unsigned DISTRIBUTION_INTERVAL = 200;
/// Values are computed in 0..DISTRIBUTION_SCALE and shown as 0..DISTRIBUTION_SHOWN
constexpr uint32_t DISTRIBUTION_SCALE = 1000000;
constexpr uint16_t DISTRIBUTION_SHOWN = 1000;
/// Most wares a player can order of one ware
constexpr unsigned MAX_WARE_ORDERS = 999;

/// State of a ware in the distribution
enum class WareDistState : uint8_t
{
    /// No finished building of the player makes it
    NoBuilding,
    /// Valued: part of the comparison
    Valued,
    /// Surplus (automatic stop): its buildings stop making it
    Surplus,
    /// Stopped by the player (manual stop)
    Paused
};
constexpr auto maxEnumValue(WareDistState)
{
    return WareDistState::Paused;
}

/// What the player sets for a ware
struct WareSetting
{
    /// Value set by itself (true) or by the player (manualValue, 0..DISTRIBUTION_SHOWN)
    bool autoValue = true;
    uint16_t manualValue = DISTRIBUTION_SHOWN / 2;
    /// Stop by surplus (true) or by the player (paused)
    bool autoPause = true;
    bool paused = false;

    bool operator==(const WareSetting& rhs) const
    {
        return autoValue == rhs.autoValue && manualValue == rhs.manualValue && autoPause == rhs.autoPause
               && paused == rhs.paused;
    }
};

/// Demand only the AI knows, its "geplant" (DESIGN.md, Gesamtverteilung): construction costs and tools of the orders of
/// its list not placed yet, in the order of the list. The AI sends it (gc::SetPlannedDemand), it counts as planned in
/// the forecast. Everything else of "geplant" the game computes itself (tools of the building sites, tools in stock)
struct PlannedDemand
{
    /// Any of the wares (sorted)
    std::vector<GoodType> wares;
    uint16_t amount = 0;

    bool operator==(const PlannedDemand& rhs) const { return wares == rhs.wares && amount == rhs.amount; }
};

/// Distribution of all wares of one player (part of the game state)
class WareDistribution
{
public:
    /// Values from the forecast (true) or from the number of wares (false), chosen by the player
    bool UsesForecast() const { return usesForecast_; }
    void SetUsesForecast(bool usesForecast) { usesForecast_ = usesForecast; }
    const WareSetting& GetSetting(GoodType good) const { return settings_[good]; }
    void SetSetting(GoodType good, const WareSetting& setting);
    /// Wares ordered by the player (made first, also when the ware is not wanted)
    unsigned GetOrders(GoodType good) const { return orders_[good]; }
    void ChangeOrders(GoodType good, int change);
    /// One ordered ware was started. Returns true if no order is left at all
    bool OrderProcessed(GoodType good);
    /// Planned demand of the AI (empty for a human player)
    const std::vector<PlannedDemand>& GetPlanned() const { return planned_; }
    void SetPlanned(std::vector<PlannedDemand> planned) { planned_ = std::move(planned); }

    /// Computed: value 0..DISTRIBUTION_SCALE (0 for wares not valued), number (forecast or number of wares, in
    /// hundredths), state
    uint32_t GetValue(GoodType good) const { return values_[good]; }
    Amount GetNumber(GoodType good) const { return numbers_[good]; }
    WareDistState GetState(GoodType good) const { return states_[good]; }
    /// Not wanted: surplus (automatic stop) or stopped by the player. Its buildings stop making it
    bool IsUnwanted(GoodType good) const { return unwanted_[good]; }

    /// Compute the values of the player (rule of the game)
    void Update(const GameWorldBase& world, unsigned playerId, unsigned gf);

    void Serialize(SerializedGameData& sgd) const;
    void Deserialize(SerializedGameData& sgd);

private:
    bool usesForecast_ = true;
    helpers::EnumArray<WareSetting, GoodType> settings_;
    helpers::EnumArray<uint16_t, GoodType> orders_{};
    helpers::EnumArray<uint32_t, GoodType> values_{};
    helpers::EnumArray<Amount, GoodType> numbers_{};
    helpers::EnumArray<WareDistState, GoodType> states_{};
    helpers::EnumArray<bool, GoodType> unwanted_{};
    std::vector<PlannedDemand> planned_;
};

/// Is the ware part of the distribution? All wares but the knowledge media (regulator "Wissen")
bool isInDistribution(GoodType good);
/// Is the distribution of all wares active (civilization mode)?
bool isDistributionActive(const GameWorldBase& world);

/// Products of a building type (several: it chooses by the distribution) and its byproducts with the technology each
/// needs (-1: none), from the production graph of the game data
struct BuildingProducts
{
    std::vector<GoodType> products;
    std::vector<GoodType> byproducts;
    std::vector<int> byproductTechs;
};
const BuildingProducts& getBuildingProducts(const GameWorldBase& world, BuildingType bld);

/// Production graph of the game data and the products of the building types (GameWorldBase::GetDistributionData)
struct DistributionData
{
    explicit DistributionData(const GameWorldBase& world);
    civai::EconomyGraph graph;
    helpers::EnumArray<BuildingProducts, BuildingType> products;
};

/// Compute the distributions of the players whose turn it is (Game::RunGF)
void runDistributions(GameWorld& world, unsigned gf);
/// Compute the distribution of the player now and wake or stop its buildings
void updateDistribution(GameWorld& world, unsigned playerId);
/// Wake the buildings of the player making the ware (an order was placed)
void wakeBuildingsFor(GameWorld& world, unsigned playerId, GoodType good);
/// Is the building stopped by the distribution? All its products and byproducts are not wanted and none is ordered
bool isPausedByDistribution(const GameWorldBase& world, const nobUsual& building);
/// A building with one product started a work step: an order of its product is processed
void startedWorkStep(GameWorld& world, const nobUsual& building);
/// Choose the product of a building with several products: the ordered ones first (the order is processed),
/// otherwise weighted by the values of the wanted products, converted for the building (highest 100 %, lowest 0 %).
/// `objId`: object id for the random number
GoodType chooseProduct(GameWorld& world, unsigned playerId, const std::vector<GoodType>& products, unsigned objId);

} // namespace civ
