// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GamePlayer.h"
#include "PointOutput.h"
#include "RttrForeachPt.h"
#include "ai/AIInterface.h"
#include "ai/AIPlayer.h"
#include "ai/aijh/AIConstruction.h"
#include "ai/aijh/AIPlayerJH.h"
#include "buildings/noBuilding.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobMilitary.h"
#include "factories/AIFactory.h"
#include "factories/BuildingFactory.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "network/GameMessage_Chat.h"
#include "notifications/NodeNote.h"
#include "worldFixtures/TestEventManager.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "worldFixtures/terrainHelpers.h"
#include "nodeObjs/noFlag.h"
#include "nodeObjs/noTree.h"
#include "gameTypes/GameTypesOutput.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/MilitaryConsts.h"
#include "gameData/TerrainDesc.h"
#include "rttr/test/random.hpp"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <memory>
#include <set>

namespace {
// We need border land
using BiggerWorldWithGCExecution = WorldWithGCExecution<1, 24, 22>;
using EmptyWorldFixture1P = WorldFixture<CreateEmptyWorld, 1>;
using EmptyWorldFixture2P = WorldFixture<CreateEmptyWorld, 2>;

template<class T_Col>
inline bool containsBldType(const T_Col& collection, BuildingType type)
{
    return helpers::contains_if(collection,
                                [type](const noBaseBuilding* bld) { return bld->GetBuildingType() == type; });
}

inline bool playerHasBld(const GamePlayer& player, BuildingType type)
{
    const BuildingRegister& blds = player.GetBuildingRegister();
    if(containsBldType(blds.GetBuildingSites(), type))
        return true;
    if(BuildingProperties::IsMilitary(type))
        return containsBldType(blds.GetMilitaryBuildings(), type);
    if(BuildingProperties::IsWareHouse(type)) // Includes harbors
        return containsBldType(blds.GetStorehouses(), type);
    return !blds.GetBuildings(type).empty();
}

struct MockAI final : public AIPlayer
{
    MockAI(unsigned char playerId, const GameWorldBase& gwb, const AI::Level level) : AIPlayer(playerId, gwb, level) {}
    // LCOV_EXCL_START
    void RunGF(unsigned /*gf*/, bool /*gfisnwf*/) override {}
    void OnChatMessage(unsigned /*sendPlayerId*/, ChatDestination, const std::string& /*msg*/) override {}
    // LCOV_EXCL_STOP
};
} // namespace

// Note game command execution is emulated to be like the ones send via network:
// Run "Network Frame" then execute GCs from last NWF
// Also use "HARD" AI for faster execution
BOOST_AUTO_TEST_SUITE(AI)

BOOST_FIXTURE_TEST_CASE(PlayerHasBld_IsCorrect, EmptyWorldFixture1P)
{
    const GamePlayer& player = world.GetPlayer(0);
    BOOST_TEST(playerHasBld(player, BuildingType::Headquarters));
    MapPoint pos = player.GetHQPos();
    for(const auto bld : {BuildingType::Woodcutter, BuildingType::Barracks, BuildingType::Storehouse})
    {
        pos = world.MakeMapPoint(pos + Position(2, 0));
        BOOST_TEST_INFO(bld);
        BOOST_TEST(!playerHasBld(player, bld));
        BuildingFactory::CreateBuilding(world, bld, pos, player.GetPlayerId(), Nation::Romans);
        BOOST_TEST_INFO(bld);
        BOOST_TEST(playerHasBld(player, bld));
    }
}

BOOST_FIXTURE_TEST_CASE(AIChat, EmptyWorldFixture2P)
{
    MockAI ai(1, world, AI::Level::Easy);
    ai.getAIInterface().Chat("Hello players!");
    ai.getAIInterface().Chat("2nd Message!", ChatDestination::Allies);
    const auto msgs = ai.getAIInterface().FetchChatMessages();
    BOOST_TEST_REQUIRE(msgs.size() == 2u);
    BOOST_TEST(msgs[0]->player == 1u);
    BOOST_TEST(msgs[0]->destination == ChatDestination::All);
    BOOST_TEST(msgs[0]->text == "Hello players!");
    BOOST_TEST(msgs[1]->player == 1u);
    BOOST_TEST(msgs[1]->destination == ChatDestination::Allies);
    BOOST_TEST(msgs[1]->text == "2nd Message!");
    // Messages cleared by first call
    BOOST_TEST(ai.getAIInterface().FetchChatMessages().empty());
    // Can readd
    const auto dest = rttr::test::randomEnum<ChatDestination>();
    ai.getAIInterface().Chat("Hello again!", dest);
    // Iterate just like in ExecuteNWF function
    for(auto& msg : ai.getAIInterface().FetchChatMessages())
    {
        BOOST_TEST(msg->player == 1u);
        BOOST_TEST(msg->destination == dest);
        BOOST_TEST(msg->text == "Hello again!");
    }
}

BOOST_FIXTURE_TEST_CASE(KeepBQUpdated, BiggerWorldWithGCExecution)
{
    addStartResources();
    // Place some trees to reduce BQ at some points
    RTTR_FOREACH_PT(MapPoint, world.GetSize())
    {
        if(pt.x % 4 == 0 && pt.y % 2 == 0 && world.GetNode(pt).bq == BuildingQuality::Castle
           && world.CalcDistance(pt, hqPos) > 6)
            world.SetNO(pt, new noTree(pt, 0, 3));
    }
    world.InitAfterLoad();

    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    const AIJH::AIPlayerJH& aijh = static_cast<AIJH::AIPlayerJH&>(*ai);

    const auto assertBqEqualOnWholeMap = [this, &aijh](const unsigned lineNr) {
        BOOST_TEST_CONTEXT("Line #" << lineNr)
        RTTR_FOREACH_PT(MapPoint, world.GetSize())
        {
            BOOST_TEST_INFO(pt);
            BOOST_TEST(this->world.GetBQ(pt, curPlayer) == aijh.GetAINode(pt).bq);
        }
    };
    const auto assertBqEqualAround = [this, &aijh](const unsigned lineNr, MapPoint pt, unsigned radius) {
        BOOST_TEST_CONTEXT("Line #" << lineNr)
        world.CheckPointsInRadius(
          pt, radius,
          [&](const MapPoint curPt, unsigned) {
              BOOST_TEST_INFO(curPt);
              BOOST_TEST(this->world.GetBQ(curPt, curPlayer) == aijh.GetAINode(curPt).bq);
              return false;
          },
          true);
    };

    // 100GFs for initialization
    for(unsigned gf = 0; gf < 100; ++gf)
    {
        em.ExecuteNextGF();
        ai->RunGF(em.GetCurrentGF(), true);
    }
    assertBqEqualOnWholeMap(__LINE__);

    // Set and destroy flag everywhere possible
    std::vector<MapPoint> possibleFlagNodes;
    RTTR_FOREACH_PT(MapPoint, world.GetSize())
    {
        if(world.GetBQ(pt, curPlayer) != BuildingQuality::Nothing && !world.IsFlagAround(pt))
            possibleFlagNodes.push_back(pt);
    }
    for(const MapPoint flagPos : possibleFlagNodes)
    {
        this->SetFlag(flagPos);
        BOOST_TEST_REQUIRE(world.GetSpecObj<noFlag>(flagPos));
        em.ExecuteNextGF();
        ai->RunGF(em.GetCurrentGF(), true);
        assertBqEqualAround(__LINE__, flagPos, 3);

        this->DestroyFlag(flagPos);
        BOOST_TEST_REQUIRE(!world.GetSpecObj<noFlag>(flagPos));
        em.ExecuteNextGF();
        ai->RunGF(em.GetCurrentGF(), true);
        assertBqEqualAround(__LINE__, flagPos, 3);
    }

    // Build road
    const MapPoint flagPos = world.MakeMapPoint(world.GetNeighbour(hqPos, Direction::SouthEast) + Position(4, 0));
    this->BuildRoad(world.GetNeighbour(hqPos, Direction::SouthEast), false, std::vector<Direction>(4, Direction::East));
    BOOST_TEST_REQUIRE(world.GetSpecObj<noFlag>(flagPos)->GetRoute(Direction::West));
    em.ExecuteNextGF();
    ai->RunGF(em.GetCurrentGF(), true);
    assertBqEqualAround(__LINE__, flagPos, 6);

    // Destroy road and flag
    this->DestroyFlag(flagPos);
    em.ExecuteNextGF();
    ai->RunGF(em.GetCurrentGF(), true);
    assertBqEqualAround(__LINE__, flagPos, 6);

    // Build building
    const MapPoint bldPos = world.MakeMapPoint(hqPos + Position(5, 0));
    this->SetBuildingSite(bldPos, BuildingType::Barracks);
    BOOST_TEST_REQUIRE(world.GetSpecObj<noBuildingSite>(bldPos));
    em.ExecuteNextGF();
    ai->RunGF(em.GetCurrentGF(), true);
    assertBqEqualAround(__LINE__, bldPos, 6);

    this->BuildRoad(world.GetNeighbour(bldPos, Direction::SouthEast), false,
                    std::vector<Direction>(5, Direction::West));
    em.ExecuteNextGF();
    ai->RunGF(em.GetCurrentGF(), true);
    RTTR_EXEC_TILL(2000, world.GetSpecObj<noBuilding>(bldPos));
    em.ExecuteNextGF();
    ai->RunGF(em.GetCurrentGF(), false);
    assertBqEqualOnWholeMap(__LINE__);

    // Gain land
    const nobMilitary* bld = world.GetSpecObj<nobMilitary>(bldPos);
    for(unsigned i = 0; i < 500; i++)
    {
        em.ExecuteNextGF();
        ai->RunGF(em.GetCurrentGF(), false);
        if(bld->GetNumTroops() > 0u)
            break;
    }
    BOOST_TEST_REQUIRE(bld->GetNumTroops() > 0u);
    assertBqEqualOnWholeMap(__LINE__);

    // Move the boundary by one node
    std::set<MapPoint, MapPointLess> outerBoundaryNodes;
    std::vector<MapPoint> borderNodes;
    RTTR_FOREACH_PT(MapPoint, world.GetSize())
    {
        if(world.IsBorderNode(pt, curPlayer + 1))
        {
            borderNodes.push_back(pt);
            world.CheckPointsInRadius(
              pt, 1,
              [&outerBoundaryNodes, &world = this->world, curPlayer = this->curPlayer](const MapPoint curPt, unsigned) {
                  if(world.GetNode(curPt).owner != curPlayer + 1)
                      outerBoundaryNodes.insert(curPt);
                  return false;
              },
              false);
        }
    }
    // Once to outside
    for(const MapPoint pt : outerBoundaryNodes)
        world.SetOwner(pt, curPlayer + 1);
    world.RecalcBorderStones(Position(0, 0), Extent(world.GetSize()));
    for(const MapPoint pt : outerBoundaryNodes)
        world.GetNotifications().publish(NodeNote(NodeNote::Owner, pt));
    em.ExecuteNextGF();
    ai->RunGF(em.GetCurrentGF(), false);
    assertBqEqualOnWholeMap(__LINE__);

    // And back
    for(const MapPoint pt : outerBoundaryNodes)
        world.SetOwner(pt, 0);
    world.RecalcBorderStones(Position(0, 0), Extent(world.GetSize()));
    for(const MapPoint pt : outerBoundaryNodes)
        world.GetNotifications().publish(NodeNote(NodeNote::Owner, pt));
    em.ExecuteNextGF();
    ai->RunGF(em.GetCurrentGF(), false);
    assertBqEqualOnWholeMap(__LINE__);

    // And once to inside
    for(const MapPoint pt : borderNodes)
        world.SetOwner(pt, 0);
    world.RecalcBorderStones(Position(0, 0), Extent(world.GetSize()));
    for(const MapPoint pt : borderNodes)
        world.GetNotifications().publish(NodeNote(NodeNote::Owner, pt));
    em.ExecuteNextGF();
    ai->RunGF(em.GetCurrentGF(), false);
    assertBqEqualOnWholeMap(__LINE__);
}

BOOST_FIXTURE_TEST_CASE(BuildWoodIndustry, BiggerWorldWithGCExecution)
{
    // The AI opens building sites only with builders and material in stock (AIConstruction)
    addStartResources();
    // Place a few trees. Not right next to the HQ: A forester is only built where there is little forest yet
    // (AIPlayerJH::FindPositionForBuildingAround)
    for(const MapPoint& pt : world.GetPointsInRadius(world.MakeMapPoint(hqPos + Position(6, 0)), 2))
    {
        if(!world.GetNode(pt).obj)
            world.SetNO(pt, new noTree(pt, 0, 3));
    }
    world.InitAfterLoad();

    const GamePlayer& player = world.GetPlayer(curPlayer);
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    // Build a woodcutter, sawmill and forester at some point
    for(unsigned gf = 0; gf < 8000;)
    {
        std::vector<gc::GameCommandPtr> aiGcs = ai->FetchGameCommands();
        for(unsigned i = 0; i < 5; i++, gf++)
        {
            em.ExecuteNextGF();
            ai->RunGF(em.GetCurrentGF(), i == 0);
        }
        for(gc::GameCommandPtr& gc : aiGcs)
        {
            gc->Execute(world, curPlayer);
        }
        if(playerHasBld(player, BuildingType::Sawmill) && playerHasBld(player, BuildingType::Woodcutter)
           && playerHasBld(player, BuildingType::Forester))
            break;
    }
    BOOST_TEST(playerHasBld(player, BuildingType::Sawmill));
    BOOST_TEST(playerHasBld(player, BuildingType::Woodcutter));
    BOOST_TEST(playerHasBld(player, BuildingType::Forester));
}

namespace {
void forceExpansion(const GamePlayer& player, GameWorld& world)
{
    // No space for saw mill due to altitude diff of 3 in range 2 -> Huts only
    for(unsigned y = 0; y < world.GetHeight(); y += 2)
    {
        for(unsigned x = 0; x < world.GetWidth(); x += 2)
            world.ChangeAltitude(MapPoint(x, y), 13);
    }
    RTTR_FOREACH_PT(MapPoint, world.GetSize())
    {
        BOOST_TEST_REQUIRE(world.GetBQ(pt, player.GetPlayerId()) <= BuildingQuality::Hut);
    }
}

void runUntilMilitaryBuildingSiteFound(TestEventManager& em, unsigned curPlayer, GameWorld& world,
                                       const std::list<noBuildingSite*>& bldSites)
{
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    for(unsigned gf = 0; gf < 2000;)
    {
        std::vector<gc::GameCommandPtr> aiGcs = ai->FetchGameCommands();
        for(unsigned i = 0; i < 5; i++, gf++)
        {
            em.ExecuteNextGF();
            ai->RunGF(em.GetCurrentGF(), i == 0);
        }
        for(gc::GameCommandPtr& gc : aiGcs)
        {
            gc->Execute(world, curPlayer);
        }
        if(containsBldType(bldSites, BuildingType::Barracks) || containsBldType(bldSites, BuildingType::Guardhouse))
            break;
    }
}
} // namespace

BOOST_FIXTURE_TEST_CASE(ExpandWhenNoSpace, BiggerWorldWithGCExecution)
{
    addStartResources();
    const auto& player = world.GetPlayer(curPlayer);
    const auto& bldSites = player.GetBuildingRegister().GetBuildingSites();

    forceExpansion(player, world);
    runUntilMilitaryBuildingSiteFound(em, curPlayer, world, bldSites);

    BOOST_TEST_REQUIRE(
      (containsBldType(bldSites, BuildingType::Barracks) || containsBldType(bldSites, BuildingType::Guardhouse)));
}

BOOST_FIXTURE_TEST_CASE(DoNotBuildMilitaryBuildingsWithinComputerBarrier, BiggerWorldWithGCExecution)
{
    addStartResources();
    const auto& player = world.GetPlayer(curPlayer);
    const auto& bldSites = player.GetBuildingRegister().GetBuildingSites();

    const auto& barrierPt = player.GetHQPos();
    constexpr auto radius = HQ_RADIUS;

    world.SetComputerBarrier(barrierPt, radius);
    forceExpansion(player, world);
    runUntilMilitaryBuildingSiteFound(em, curPlayer, world, bldSites);

    BOOST_TEST_REQUIRE(
      !(containsBldType(bldSites, BuildingType::Barracks) || containsBldType(bldSites, BuildingType::Guardhouse)));
}

BOOST_FIXTURE_TEST_CASE(DoBuildMilitaryBuildingsOutsideComputerBarrier, BiggerWorldWithGCExecution)
{
    addStartResources();
    const auto& player = world.GetPlayer(curPlayer);
    const auto& bldSites = player.GetBuildingRegister().GetBuildingSites();

    auto barrierPt = player.GetHQPos();
    // move barrier 2 tiles west of HQ, now military buildings should be buildable to the east
    barrierPt.x -= 2;
    constexpr auto radius = HQ_RADIUS;

    world.SetComputerBarrier(barrierPt, radius);
    forceExpansion(player, world);
    runUntilMilitaryBuildingSiteFound(em, curPlayer, world, bldSites);

    BOOST_TEST_REQUIRE(
      (containsBldType(bldSites, BuildingType::Barracks) || containsBldType(bldSites, BuildingType::Guardhouse)));

    for(noBuildingSite* bldSite : bldSites)
        BOOST_TEST_REQUIRE(!world.CheckPointsInRadius(
          barrierPt, radius,
          [bldSite](const MapPoint& pt, unsigned) {
              const auto type = bldSite->GetBuildingType();
              return (type == BuildingType::Barracks || type == BuildingType::Guardhouse) && pt == bldSite->GetPos();
          },
          true));
}

BOOST_FIXTURE_TEST_CASE(DoNotOpenMoreBuildingSitesThanBuilders, BiggerWorldWithGCExecution)
{
    // Exactly 2 builders, no hammers -> the AI may never have more than 2 building sites at once:
    // A new site is only started if a builder is left for it (HasEnoughBuildersAvailable)
    constexpr unsigned numBuilders = 2;
    auto* hq = world.GetSpecObj<nobBaseWarehouse>(hqPos);
    {
        GoodsAndPeopleCounts inv;
        inv[Job::Builder] = numBuilders;
        inv[Job::Helper] = 30;
        inv[GoodType::Boards] = 60;
        inv[GoodType::Stones] = 60;
        hq->AddToInventory(inv, true);
    }
    world.InitAfterLoad();

    const GamePlayer& player = world.GetPlayer(curPlayer);
    const auto& bldSites = player.GetBuildingRegister().GetBuildingSites();
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    unsigned maxSites = 0;
    for(unsigned gf = 0; gf < 6000;)
    {
        // Same as over the network: Commands of the last NWF are executed at the next NWF
        std::vector<gc::GameCommandPtr> aiGcs = ai->FetchGameCommands();
        for(unsigned i = 0; i < 5; i++, gf++)
        {
            em.ExecuteNextGF();
            ai->RunGF(em.GetCurrentGF(), i == 0);
        }
        for(gc::GameCommandPtr& gc : aiGcs)
            gc->Execute(world, curPlayer);
        maxSites = std::max<unsigned>(maxSites, bldSites.size());
        BOOST_TEST_INFO("GF " << gf);
        BOOST_TEST_REQUIRE(bldSites.size() <= numBuilders);
    }
    // The AI did build something
    BOOST_TEST(maxSites >= 1u);
}

namespace {
/// Run the AI like over the network (commands of the last NWF are executed at the next NWF), calling `check` after each
/// NWF
template<class T_Check>
void runAIWithNWFs(TestEventManager& em, GameWorld& world, AIPlayer& ai, unsigned playerId, unsigned numGFs,
                   const T_Check& check)
{
    for(unsigned gf = 0; gf < numGFs;)
    {
        std::vector<gc::GameCommandPtr> aiGcs = ai.FetchGameCommands();
        for(unsigned i = 0; i < 5; i++, gf++)
        {
            em.ExecuteNextGF();
            ai.RunGF(em.GetCurrentGF(), i == 0);
        }
        for(gc::GameCommandPtr& gc : aiGcs)
            gc->Execute(world, playerId);
        check(gf);
    }
}

unsigned countMilitarySites(const GamePlayer& player)
{
    unsigned result = 0;
    for(const noBuildingSite* site : player.GetBuildingRegister().GetBuildingSites())
    {
        if(BuildingProperties::IsMilitary(site->GetBuildingType()))
            ++result;
    }
    return result;
}
} // namespace

BOOST_FIXTURE_TEST_CASE(MilitarySitesAtOnceAreLimited, BiggerWorldWithGCExecution)
{
    const unsigned maxSites = AIJH::AIConstruction::GetMaxMilitarySites();
    addStartResources();
    auto* hq = world.GetSpecObj<nobBaseWarehouse>(hqPos);
    hq->AddToInventory(GoodCounts::make(GoodType::Boards, 100), true);
    hq->AddToInventory(GoodCounts::make(GoodType::Stones, 100), true);
    world.InitAfterLoad();
    const GamePlayer& player = world.GetPlayer(curPlayer);
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    unsigned maxMilitarySites = 0;
    runAIWithNWFs(em, world, *ai, curPlayer, 8000, [&](unsigned gf) {
        const unsigned numSites = countMilitarySites(player);
        maxMilitarySites = std::max(maxMilitarySites, numSites);
        BOOST_TEST_INFO("GF " << gf);
        BOOST_TEST_REQUIRE(numSites <= maxSites);
    });
    // It still expands
    BOOST_TEST(maxMilitarySites >= 1u);
}

BOOST_FIXTURE_TEST_CASE(MilitarySitesNeedSoldiers, BiggerWorldWithGCExecution)
{
    // Each military site needs this many soldiers in the warehouses (incl. the new one)
    const unsigned minSoldiers = AIJH::AIConstruction::GetMinSoldiersPerMilitarySite();
    if(minSoldiers == 0)
    {
        BOOST_TEST_MESSAGE("MIN_SOLDIERS_PER_MILITARY_SITE is 0: no soldiers needed, nothing to test");
        return;
    }
    // Enough for 1 military site, not for 2. The HQ keeps no reserve (by default 1 soldier per rank), which the AI
    // could not send
    auto* hq = world.GetSpecObj<nobBaseWarehouse>(hqPos);
    hq->SetRealReserve(0, 0);
    {
        GoodsAndPeopleCounts inv;
        inv[Job::Builder] = 5;
        inv[Job::Helper] = 30;
        inv[Job::Private] = 2 * minSoldiers - 1;
        inv[GoodType::Boards] = 100;
        inv[GoodType::Stones] = 100;
        hq->AddToInventory(inv, true);
    }
    world.InitAfterLoad();
    const GamePlayer& player = world.GetPlayer(curPlayer);
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    unsigned maxMilitarySites = 0;
    runAIWithNWFs(em, world, *ai, curPlayer, 6000, [&](unsigned gf) {
        const unsigned numSites = countMilitarySites(player);
        maxMilitarySites = std::max(maxMilitarySites, numSites);
        BOOST_TEST_INFO("GF " << gf);
        BOOST_TEST_REQUIRE(numSites <= 1u);
    });
    BOOST_TEST(maxMilitarySites == 1u);
}

BOOST_FIXTURE_TEST_CASE(NoBuildingSitesWithoutMaterial, BiggerWorldWithGCExecution)
{
    // Builders and soldiers, but no boards and stones -> nothing is started
    auto* hq = world.GetSpecObj<nobBaseWarehouse>(hqPos);
    {
        GoodsAndPeopleCounts inv;
        inv[Job::Builder] = 5;
        inv[Job::Helper] = 30;
        inv[Job::Private] = 20;
        hq->AddToInventory(inv, true);
    }
    world.InitAfterLoad();
    const GamePlayer& player = world.GetPlayer(curPlayer);
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    runAIWithNWFs(em, world, *ai, curPlayer, 3000, [&](unsigned gf) {
        BOOST_TEST_INFO("GF " << gf);
        BOOST_TEST_REQUIRE(player.GetBuildingRegister().GetBuildingSites().empty());
    });
}

BOOST_FIXTURE_TEST_CASE(MaterialIsReservedForImportantBuildings, BiggerWorldWithGCExecution)
{
    auto* hq = world.GetSpecObj<nobBaseWarehouse>(hqPos);
    const BuildingCost smelterCost = BUILTIN_BUILDING_COSTS[static_cast<unsigned>(BuildingType::Ironsmelter)];
    {
        GoodsAndPeopleCounts inv;
        inv[GoodType::Boards] = smelterCost.boards;
        inv[GoodType::Stones] = smelterCost.stones;
        hq->AddToInventory(inv, true);
    }
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    AIJH::AIConstruction& construction = static_cast<AIJH::AIPlayerJH&>(*ai).GetConstruction();
    BOOST_TEST(AIJH::AIConstruction::IsImportantBuilding(BuildingType::Ironsmelter));
    BOOST_TEST(AIJH::AIConstruction::IsImportantBuilding(BuildingType::IronMine));
    BOOST_TEST(!AIJH::AIConstruction::IsImportantBuilding(BuildingType::GraniteMine));
    BOOST_TEST(!AIJH::AIConstruction::IsImportantBuilding(BuildingType::Woodcutter));
    // Exactly enough for the smelter
    BOOST_TEST(construction.GetFreeConstructionMaterial()[GoodType::Boards] == static_cast<int>(smelterCost.boards));
    BOOST_TEST(construction.HasMaterialFor(BuildingType::Ironsmelter));
    BOOST_TEST(construction.HasMaterialFor(BuildingType::Woodcutter));
    // The smelter waits for material -> it is kept free: other buildings can't have it, important ones can
    construction.ReserveMaterialFor(BuildingType::Ironsmelter);
    BOOST_TEST(!construction.HasMaterialFor(BuildingType::Woodcutter));
    BOOST_TEST(construction.HasMaterialFor(BuildingType::Ironsmelter));

    // A building site needs its material -> less is free
    const MapPoint sitePos = world.MakeMapPoint(hqPos + Position(4, 0));
    world.SetBuildingSite(BuildingType::Woodcutter, sitePos, curPlayer);
    BOOST_TEST_REQUIRE(world.GetSpecObj<noBuildingSite>(sitePos));
    const BuildingCost woodcutterCost = BUILTIN_BUILDING_COSTS[static_cast<unsigned>(BuildingType::Woodcutter)];
    BOOST_TEST(construction.GetFreeConstructionMaterial()[GoodType::Boards]
               == static_cast<int>(smelterCost.boards) - static_cast<int>(woodcutterCost.boards));
    BOOST_TEST(!construction.HasMaterialFor(BuildingType::Ironsmelter));
}

namespace {
/// Enough space to expand several times
using LargeWorldWithGCExecution = WorldWithGCExecution<1, 40, 40>;

/// Buildings the AI may build near a land border (all others keep that space free for military buildings)
bool mayBeNearBorder(BuildingType type)
{
    if(BuildingProperties::IsMilitary(type))
        return true;
    const std::vector<BuildingType> allowed{
      BuildingType::Woodcutter,  BuildingType::Quarry,         BuildingType::Fishery,
      BuildingType::GoldMine,    BuildingType::CoalMine,       BuildingType::IronMine,
      BuildingType::GraniteMine, BuildingType::HarborBuilding, BuildingType::Catapult};
    return helpers::contains(allowed, type);
}

/// Computed here independently of the AI: Is there a border node of the player within the radius which has land (not
/// his territory, not only water) within the radius behind it?
bool isNearLandBorder(const GameWorldBase& world, unsigned char playerId, const MapPoint pt, unsigned radius)
{
    const unsigned char owner = playerId + 1;
    const auto isLandBorder = [&](const MapPoint borderPt, unsigned) {
        if(world.GetNode(borderPt).owner != owner)
            return false;
        bool isBorder = false;
        for(const MapPoint nb : world.GetNeighbours(borderPt))
            isBorder |= world.GetNode(nb).owner != owner;
        if(!isBorder)
            return false;
        return world.CheckPointsInRadius(
          borderPt, radius,
          [&](const MapPoint behindPt, unsigned) {
              return world.GetNode(behindPt).owner != owner && !world.IsWaterPoint(behindPt);
          },
          false);
    };
    return world.CheckPointsInRadius(pt, radius, isLandBorder, true);
}

/// Set all 6 triangles around the point to the terrain
void setTerrainAround(GameWorld& world, const MapPoint pt, DescIdx<TerrainDesc> terrain)
{
    MapNode& curNode = world.GetNodeWriteable(pt);
    curNode.t1 = curNode.t2 = terrain;
    world.GetNodeWriteable(world.GetNeighbour(pt, Direction::West)).t2 = terrain;
    MapNode& nwNode = world.GetNodeWriteable(world.GetNeighbour(pt, Direction::NorthWest));
    nwNode.t1 = nwNode.t2 = terrain;
    world.GetNodeWriteable(world.GetNeighbour(pt, Direction::NorthEast)).t1 = terrain;
}

/// Only water outside the territory of the player
void floodOutsideOfTerritory(GameWorld& world, unsigned char playerId)
{
    const DescIdx<TerrainDesc> tWater = GetWaterTerrain(world.GetDescription());
    std::vector<MapPoint> outside;
    RTTR_FOREACH_PT(MapPoint, world.GetSize())
    {
        if(world.GetNode(pt).owner != playerId + 1)
            outside.push_back(pt);
    }
    for(const MapPoint& pt : outside)
        setTerrainAround(world, pt, tWater);
}

/// First border node east of the point
MapPoint findBorderEastOf(const AIInterface& aii, const GameWorldBase& world, MapPoint pt)
{
    for(unsigned i = 0; i < world.GetWidth() && !aii.IsBorder(pt); ++i)
        pt = world.GetNeighbour(pt, Direction::East);
    BOOST_TEST_REQUIRE(aii.IsBorder(pt));
    return pt;
}
} // namespace

BOOST_FIXTURE_TEST_CASE(LandBorderNeedsLandBehindIt, BiggerWorldWithGCExecution)
{
    std::vector<gc::GameCommandPtr> gcs;
    const AIInterface aii(world, gcs, curPlayer);
    const MapPoint borderPt = findBorderEastOf(aii, world, hqPos);
    const MapPoint insidePt = world.GetNeighbour(world.GetNeighbour(borderPt, Direction::West), Direction::West);
    BOOST_TEST(aii.IsLandBorder(borderPt, 4));
    BOOST_TEST(!aii.IsLandBorder(insidePt, 4)); // No border
    BOOST_TEST(aii.IsNearLandBorder(insidePt, 4));
    BOOST_TEST(aii.IsNearLandBorder(borderPt, 4));
    BOOST_TEST(!aii.IsNearLandBorder(hqPos, 4));
    BOOST_TEST(isNearLandBorder(world, curPlayer, insidePt, 4));
    BOOST_TEST(!isNearLandBorder(world, curPlayer, hqPos, 4));
    // Exactly 4 nodes away still counts, 5 not
    MapPoint pt = borderPt;
    for(unsigned i = 0; i < 4; ++i)
        pt = world.GetNeighbour(pt, Direction::West);
    BOOST_TEST(aii.IsNearLandBorder(pt, 4));
    BOOST_TEST(!aii.IsNearLandBorder(world.GetNeighbour(pt, Direction::West), 4));

    // Only water behind the border (e.g. sea in front of the coast) -> no land border
    floodOutsideOfTerritory(world, curPlayer);
    BOOST_TEST(!aii.IsLandBorder(borderPt, 4));
    BOOST_TEST(!aii.IsNearLandBorder(insidePt, 4));
    BOOST_TEST(!isNearLandBorder(world, curPlayer, insidePt, 4));

    // Land 2 nodes behind the border (e.g. on the other side of a river) -> land border again
    const MapPoint landPt = world.GetNeighbour(world.GetNeighbour(borderPt, Direction::East), Direction::East);
    BOOST_TEST_REQUIRE(!aii.IsOwnTerritory(landPt));
    setTerrainAround(world, landPt, GetLandTerrain(world.GetDescription()));
    BOOST_TEST(aii.IsLandBorder(borderPt, 4));
    BOOST_TEST(aii.IsNearLandBorder(insidePt, 4));
    BOOST_TEST(isNearLandBorder(world, curPlayer, insidePt, 4));
}

BOOST_FIXTURE_TEST_CASE(OnlyAllowedBuildingsNearLandBorder, BiggerWorldWithGCExecution)
{
    for(const auto type : helpers::enumRange<BuildingType>())
    {
        BOOST_TEST_INFO(type);
        BOOST_TEST(AIJH::AIConstruction::IsAllowedNearBorder(type) == mayBeNearBorder(type));
        BOOST_TEST(AIJH::AIConstruction::GetBorderReserve(type) == (mayBeNearBorder(type) ? 0u : 4u));
    }

    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    auto& aijh = static_cast<AIJH::AIPlayerJH&>(*ai);
    const MapPoint borderPt = findBorderEastOf(aijh.GetInterface(), world, hqPos);
    const MapPoint around = world.GetNeighbour(world.GetNeighbour(borderPt, Direction::West), Direction::West);
    // A well keeps more than 4 nodes away from the border, a catapult may stand there
    const MapPoint wellPos = aijh.FindPositionForBuildingAround(BuildingType::Well, around);
    BOOST_TEST_REQUIRE(wellPos.isValid());
    BOOST_TEST(!isNearLandBorder(world, curPlayer, wellPos, 4));
    const MapPoint catapultPos = aijh.FindPositionForBuildingAround(BuildingType::Catapult, around);
    BOOST_TEST_REQUIRE(catapultPos.isValid());
    BOOST_TEST(isNearLandBorder(world, curPlayer, catapultPos, 4));
    // Also when it is placed directly next to a warehouse (sawmill)
    const MapPoint sawmillPos = aijh.FindPositionForBuildingAround(BuildingType::Sawmill, around);
    BOOST_TEST_REQUIRE(sawmillPos.isValid());
    BOOST_TEST(!isNearLandBorder(world, curPlayer, sawmillPos, 4));
}

BOOST_FIXTURE_TEST_CASE(BuildingsMayBeNearBorderWithOnlyWaterBehind, BiggerWorldWithGCExecution)
{
    floodOutsideOfTerritory(world, curPlayer);
    world.InitAfterLoad();
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    auto& aijh = static_cast<AIJH::AIPlayerJH&>(*ai);
    const MapPoint borderPt = findBorderEastOf(aijh.GetInterface(), world, hqPos);
    const MapPoint around = world.GetNeighbour(world.GetNeighbour(borderPt, Direction::West), Direction::West);
    const MapPoint wellPos = aijh.FindPositionForBuildingAround(BuildingType::Well, around);
    BOOST_TEST_REQUIRE(wellPos.isValid());
    // Close to the (water) border
    BOOST_TEST(world.CalcDistance(wellPos, borderPt) <= 4u);
}

BOOST_FIXTURE_TEST_CASE(AIKeepsSpaceAtBorderFreeForMilitary, LargeWorldWithGCExecution)
{
    addStartResources();
    auto* hq = world.GetSpecObj<nobBaseWarehouse>(hqPos);
    hq->AddToInventory(GoodCounts::make(GoodType::Boards, 100), true);
    hq->AddToInventory(GoodCounts::make(GoodType::Stones, 100), true);
    world.InitAfterLoad();
    const GamePlayer& player = world.GetPlayer(curPlayer);
    auto ai = AIFactory::Create(AI::Info(AI::Type::Default, AI::Level::Hard), curPlayer, world);
    std::vector<MapPoint> seenSites;
    unsigned numChecked = 0, numMilitary = 0;
    runAIWithNWFs(em, world, *ai, curPlayer, 12000, [&](unsigned gf) {
        for(const noBuildingSite* site : player.GetBuildingRegister().GetBuildingSites())
        {
            if(helpers::contains(seenSites, site->GetPos()))
                continue;
            seenSites.push_back(site->GetPos());
            const BuildingType type = site->GetBuildingType();
            if(BuildingProperties::IsMilitary(type))
                ++numMilitary;
            if(mayBeNearBorder(type))
                continue;
            ++numChecked;
            // The border only moves away (no enemies), so it was not closer when the site was ordered
            BOOST_TEST_INFO("GF " << gf << ": " << type << " at " << site->GetPos());
            BOOST_TEST(!isNearLandBorder(world, curPlayer, site->GetPos(), 4));
        }
    });
    // The AI did build other buildings and expanded
    BOOST_TEST(numChecked >= 3u);
    BOOST_TEST(numMilitary >= 1u);
}

BOOST_AUTO_TEST_SUITE_END()
