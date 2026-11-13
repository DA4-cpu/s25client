// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "EconomySnapshot.h"
#include "EconomyStatistics.h"
#include "GamePlayer.h"
#include "SerializedGameData.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "buildings/nobMilitary.h"
#include "buildings/nobUsual.h"
#include "factories/BuildingFactory.h"
#include "figures/nofBuildingWorker.h"
#include "helpers/EnumRange.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "gameTypes/GameTypesOutput.h"
#include <boost/test/unit_test.hpp>

// Exact economy statistic of the players: every ware entering or leaving the stock, per minute
BOOST_AUTO_TEST_SUITE(EconomyStatisticsSuite)

namespace {
const unsigned GFS_PER_MINUTE = EconomyStatistics::GetGFsPerMinute();

/// The stock of every ware must be exactly what was gained minus what was used since the start
void checkStockMatchesCounts(const GamePlayer& player)
{
    const EconomyStatistics& stats = player.GetEconomyStatistics();
    for(const auto good : helpers::enumRange<GoodType>())
    {
        long long counted = 0;
        for(const auto source : helpers::enumRange<WareSource>())
            counted += stats.GetTotalGained(good, source);
        for(const auto use : helpers::enumRange<WareUse>())
            counted -= stats.GetTotalUsed(good, use);
        BOOST_TEST_INFO("ware " << static_cast<unsigned>(good));
        BOOST_TEST(counted == static_cast<long long>(player.GetInventory().goods[good]));
    }
}

struct EconomyFixture : public WorldWithGCExecution1P
{
    nobBaseWarehouse& hq() { return *world.GetPlayer(0).GetHQ(); }
    GamePlayer& player() { return world.GetPlayer(0); }
    const EconomyStatistics& stats() { return player().GetEconomyStatistics(); }
    unsigned gf() { return em.GetCurrentGF(); }

    MapPoint placeConnectedBuilding(BuildingType bld, Position offset)
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + offset);
        BuildingFactory::CreateBuilding(world, bld, pos, 0, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<noBuilding>(pos));
        this->BuildRoadForBlds(pos, hqPos);
        return pos;
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(CountsPerMinute)
{
    EconomyStatistics stats;
    stats.AddGained(GoodType::Wood, 3, WareSource::Production, 10);
    stats.AddUsed(GoodType::Wood, 1, WareUse::Production, 20);
    BOOST_TEST(stats.GetProduced(GoodType::Wood, 1, 30) == 3u);
    BOOST_TEST(stats.GetConsumed(GoodType::Wood, 1, 30) == 1u);
    BOOST_TEST(stats.GetBalance(GoodType::Wood, 5, 30) == 2);

    // The next minute
    stats.AddGained(GoodType::Wood, 2, WareSource::Production, GFS_PER_MINUTE + 5);
    BOOST_TEST(stats.GetProduced(GoodType::Wood, 1, GFS_PER_MINUTE + 5) == 2u);
    BOOST_TEST(stats.GetProduced(GoodType::Wood, 2, GFS_PER_MINUTE + 5) == 5u);
    // Two minutes later nothing happened: the last minute is empty, the last 3 minutes contain minute 1
    BOOST_TEST(stats.GetProduced(GoodType::Wood, 1, 3 * GFS_PER_MINUTE) == 0u);
    BOOST_TEST(stats.GetProduced(GoodType::Wood, 3, 3 * GFS_PER_MINUTE) == 2u);
    BOOST_TEST(!stats.GetMinute(0, 3 * GFS_PER_MINUTE));
    BOOST_TEST_REQUIRE(stats.GetMinute(2, 3 * GFS_PER_MINUTE));
    BOOST_TEST(stats.GetMinute(2, 3 * GFS_PER_MINUTE)->gained[GoodType::Wood][WareSource::Production] == 2u);

    // Gains other than production are counted, but not in the balance
    stats.AddGained(GoodType::Wood, 10, WareSource::Added, 3 * GFS_PER_MINUTE);
    BOOST_TEST(stats.GetGained(GoodType::Wood, WareSource::Added, 1, 3 * GFS_PER_MINUTE) == 10u);
    BOOST_TEST(stats.GetBalance(GoodType::Wood, 1, 3 * GFS_PER_MINUTE) == 0);
    // Every use counts as consumption
    stats.AddUsed(GoodType::Wood, 4, WareUse::Lost, 3 * GFS_PER_MINUTE);
    stats.AddUsed(GoodType::Wood, 2, WareUse::Construction, 3 * GFS_PER_MINUTE);
    BOOST_TEST(stats.GetConsumed(GoodType::Wood, 1, 3 * GFS_PER_MINUTE) == 6u);
    BOOST_TEST(stats.GetBalance(GoodType::Wood, 1, 3 * GFS_PER_MINUTE) == -6);
    BOOST_TEST(stats.GetBalance(GoodType::Wood, 5, 3 * GFS_PER_MINUTE) == 5 - 7);

    BOOST_TEST(stats.GetTotalGained(GoodType::Wood, WareSource::Production) == 5u);
    BOOST_TEST(stats.GetTotalUsed(GoodType::Wood, WareUse::Lost) == 4u);

    stats.AddHelpersCreated(2, 3 * GFS_PER_MINUTE);
    stats.AddHelpersUsed(1, 3 * GFS_PER_MINUTE);
    BOOST_TEST(stats.GetHelpersCreated(5, 3 * GFS_PER_MINUTE) == 2u);
    BOOST_TEST(stats.GetHelpersUsed(5, 3 * GFS_PER_MINUTE) == 1u);
}

BOOST_AUTO_TEST_CASE(ConsumptionPerMinuteOverTheCoveredTime)
{
    EconomyStatistics stats;
    // At the start fewer minutes passed than asked for
    stats.AddUsed(GoodType::Fish, 6, WareUse::Production, 0);
    const unsigned gf = 2 * GFS_PER_MINUTE - 1;
    BOOST_TEST(stats.GetCoveredMinutes(5, gf) == 2., boost::test_tools::tolerance(1e-9));
    BOOST_TEST(stats.GetConsumptionPerMinute(GoodType::Fish, 5, gf) == 3., boost::test_tools::tolerance(1e-9));
    // Later the whole window: the current minute counts as far as it passed
    const unsigned laterGf = 10 * GFS_PER_MINUTE - 1;
    BOOST_TEST(stats.GetCoveredMinutes(5, laterGf) == 5., boost::test_tools::tolerance(1e-9));
}

BOOST_AUTO_TEST_CASE(HistoryIsCutAfterFourHours)
{
    constexpr unsigned numMinutes = EconomyStatistics::HISTORY_MINUTES;
    BOOST_TEST(numMinutes == 240u);
    EconomyStatistics stats;
    stats.AddUsed(GoodType::Boards, 4, WareUse::Construction, 0);
    const unsigned lastStoredGf = (numMinutes - 1) * GFS_PER_MINUTE;
    BOOST_TEST(stats.GetUsed(GoodType::Boards, WareUse::Construction, numMinutes, lastStoredGf) == 4u);
    // One minute later the first minute is gone
    stats.AddUsed(GoodType::Boards, 1, WareUse::Construction, numMinutes * GFS_PER_MINUTE);
    BOOST_TEST(stats.GetUsed(GoodType::Boards, WareUse::Construction, numMinutes, numMinutes * GFS_PER_MINUTE) == 1u);
    BOOST_TEST(stats.GetTotalUsed(GoodType::Boards, WareUse::Construction) == 5u);
    // After a long time without anything: empty
    const unsigned muchLater = 3 * numMinutes * GFS_PER_MINUTE;
    BOOST_TEST(stats.GetUsed(GoodType::Boards, WareUse::Construction, numMinutes, muchLater) == 0u);
    stats.AddGained(GoodType::Boards, 7, WareSource::Production, muchLater);
    BOOST_TEST(stats.GetProduced(GoodType::Boards, numMinutes, muchLater) == 7u);
    BOOST_TEST(stats.GetUsed(GoodType::Boards, WareUse::Construction, numMinutes, muchLater) == 0u);
}

BOOST_FIXTURE_TEST_CASE(EveryChangeOfTheStockIsCounted, EconomyFixture)
{
    // Start wares are "added"
    hq().AddToInventory(GoodCounts::make(GoodType::Wood, 4), true);
    BOOST_TEST(stats().GetTotalGained(GoodType::Wood, WareSource::Added) == 4u);
    checkStockMatchesCounts(player());

    // A building site without boards (and without builder yet): open demand for construction
    const MapPoint sitePos = world.MakeMapPoint(hqPos + Position(-4, 2));
    this->SetBuildingSite(sitePos, BuildingType::Woodcutter);
    this->BuildRoadForBlds(sitePos, hqPos);
    BOOST_TEST_REQUIRE(world.GetSpecObj<noBuildingSite>(sitePos));
    EconomySnapshot siteSnapshot = takeEconomySnapshot(player());
    BOOST_TEST(siteSnapshot.buildings[BuildingType::Woodcutter].numSites == 1u);
    bool found = false;
    for(const OpenDemand& demand : siteSnapshot.openDemand)
    {
        if(demand.purpose == WareUse::Construction)
        {
            found = true;
            BOOST_TEST((demand.wares == std::vector<GoodType>{GoodType::Boards}));
        }
    }
    BOOST_TEST(found);
    // A sawmill makes boards
    hq().AddToInventory(PeopleCounts::make(Job::Carpenter, 1), true);
    hq().AddToInventory(PeopleCounts::make(Job::Helper, 10), true);
    const MapPoint sawmillPos = placeConnectedBuilding(BuildingType::Sawmill, Position(4, 2));
    const auto& sawmill = *world.GetSpecObj<nobUsual>(sawmillPos);
    // It makes boards of the wood: produced boards, wood used for production
    RTTR_EXEC_TILL(6000, stats().GetTotalGained(GoodType::Boards, WareSource::Production) == 4u && sawmill.GetWorker()
                           && sawmill.GetWorker()->GetState()
                                == nofBuildingWorker::State::WaitingForWaresOrProductionStopped);
    BOOST_TEST(stats().GetTotalUsed(GoodType::Wood, WareUse::Production) == 4u);
    BOOST_TEST(stats().GetProduced(GoodType::Boards, 10, gf()) == 4u);
    BOOST_TEST(stats().GetBalance(GoodType::Wood, 10, gf()) == -4);
    checkStockMatchesCounts(player());

    // Now it waits for wood, which no warehouse has
    EconomySnapshot snapshot = takeEconomySnapshot(player());
    const BuildingTypeState& state = snapshot.buildings[BuildingType::Sawmill];
    BOOST_TEST(state.numBuildings == 1u);
    BOOST_TEST(state.withoutWorker == 0u);
    BOOST_TEST_REQUIRE(state.waitingForInput.size() == 1u);
    BOOST_TEST((state.waitingForInput[0].wares == std::vector<GoodType>{GoodType::Wood}));
    BOOST_TEST(state.GetNumWaitingForInput() == 1u);
    BOOST_TEST(snapshot.GetOpenDemand(GoodType::Wood) == 6u);

    // With a builder the boards made by the sawmill go to the site: used for construction
    hq().AddToInventory(PeopleCounts::make(Job::Builder, 1), true);
    RTTR_EXEC_TILL(4000, stats().GetTotalUsed(GoodType::Boards, WareUse::Construction) == 2u);
    checkStockMatchesCounts(player());

    // Soldiers: sword, shield and beer
    hq().AddToInventory(GoodCounts::make(GoodType::Sword, 2), true);
    hq().AddToInventory(GoodCounts::make(GoodType::ShieldRomans, 2), true);
    hq().AddToInventory(GoodCounts::make(GoodType::Beer, 2), true);
    RTTR_EXEC_TILL(2000, stats().GetTotalUsed(GoodType::Beer, WareUse::Soldiers) == 2u);
    BOOST_TEST(stats().GetTotalUsed(GoodType::Sword, WareUse::Soldiers) == 2u);
    BOOST_TEST(stats().GetHelpersUsed(EconomyStatistics::HISTORY_MINUTES, gf()) >= 2u);
    checkStockMatchesCounts(player());

    // The warehouse creates helpers
    BOOST_TEST(stats().GetHelpersCreated(EconomyStatistics::HISTORY_MINUTES, gf()) > 0u);

    // Destroyed with the building: lost
    hq().AddToInventory(GoodCounts::make(GoodType::Wood, 2), true);
    RTTR_EXEC_TILL(1000, sawmill.GetNumWares(0) > 0u);
    const unsigned woodInside = sawmill.GetNumWares(0);
    this->DestroyBuilding(sawmillPos);
    BOOST_TEST(stats().GetTotalUsed(GoodType::Wood, WareUse::Lost) >= woodInside);
    checkStockMatchesCounts(player());
}

BOOST_FIXTURE_TEST_CASE(CoinsOnlyWhereAllowed, EconomyFixture)
{
    hq().AddToInventory(PeopleCounts::make(Job::Private, 2), true);
    hq().AddToInventory(PeopleCounts::make(Job::Helper, 5), true);
    const MapPoint pos = placeConnectedBuilding(BuildingType::Guardhouse, Position(4, 2));
    const auto& building = *world.GetSpecObj<nobMilitary>(pos);
    RTTR_EXEC_TILL(2000, building.GetNumTroops() > 0u && !building.IsNewBuilt());
    // No coins in stock: the building asks for coins
    EconomySnapshot snapshot = takeEconomySnapshot(player());
    BOOST_TEST(snapshot.GetOpenDemand(GoodType::Coins) == building.GetMaxCoinCt());
    // Coins not allowed: no demand
    this->SetCoinsAllowed(pos, false);
    snapshot = takeEconomySnapshot(player());
    BOOST_TEST(snapshot.GetOpenDemand(GoodType::Coins) == 0u);
}

BOOST_FIXTURE_TEST_CASE(StatisticIsSaved, EconomyFixture)
{
    hq().AddToInventory(GoodCounts::make(GoodType::Wood, 4), true);
    hq().AddToInventory(PeopleCounts::make(Job::Carpenter, 1), true);
    placeConnectedBuilding(BuildingType::Sawmill, Position(4, 2));
    RTTR_EXEC_TILL(6000, stats().GetTotalGained(GoodType::Boards, WareSource::Production) == 4u);
    const unsigned savedGf = gf();
    const unsigned producedBefore = stats().GetProduced(GoodType::Boards, 10, savedGf);
    const int balanceBefore = stats().GetBalance(GoodType::Wood, 10, savedGf);

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);

    BOOST_TEST(stats().GetTotalGained(GoodType::Boards, WareSource::Production) == 4u);
    BOOST_TEST(stats().GetTotalGained(GoodType::Wood, WareSource::Added) == 4u);
    BOOST_TEST(stats().GetProduced(GoodType::Boards, 10, savedGf) == producedBefore);
    BOOST_TEST(stats().GetBalance(GoodType::Wood, 10, savedGf) == balanceBefore);
    checkStockMatchesCounts(player());
}

BOOST_AUTO_TEST_SUITE_END()
