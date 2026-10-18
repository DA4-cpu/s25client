// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "SerializedGameData.h"
#include "addons/const_addons.h"
#include "buildings/noBaseBuilding.h"
#include "buildings/nobBaseWarehouse.h"
#include "civ/Identifiers.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "buildings/nobHQ.h"
#include "helpers/EnumRange.h"
#include "helpers/OptionalIO.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "gameData/ShieldConsts.h"
#include "gameData/WorldDescription.h"
#include <boost/test/unit_test.hpp>
#include <memory>

BOOST_AUTO_TEST_SUITE(ResearchSuite)

BOOST_AUTO_TEST_CASE(IdentifiersRoundTrip)
{
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        BOOST_TEST_REQUIRE(civ::getIdentifier(bld));
        if(bld == BuildingType::Headquarters || bld == BuildingType::Nothing9)
            BOOST_TEST(!civ::buildingFromIdentifier(civ::getIdentifier(bld)));
        else
            BOOST_TEST((civ::buildingFromIdentifier(civ::getIdentifier(bld)) == bld));
    }
    for(const auto good : helpers::enumRange<GoodType>())
    {
        BOOST_TEST_REQUIRE(civ::getIdentifier(good));
        const auto parsed = civ::goodFromIdentifier(civ::getIdentifier(good));
        if(good == GoodType::WaterEmpty)
            BOOST_TEST(!parsed);
        else
            BOOST_TEST((parsed == ConvertShields(good)));
    }
    BOOST_TEST((civ::goodFromIdentifier("Shield") == GoodType::ShieldRomans));
    BOOST_TEST(!civ::buildingFromIdentifier("NotABuilding"));
    BOOST_TEST(!civ::goodFromIdentifier("NotAGood"));
}

BOOST_AUTO_TEST_CASE(DefaultTechTreeIsValid)
{
    const civ::TechTree techTree = civ::loadDefaultTechTree();
    BOOST_TEST_REQUIRE(!techTree.empty());
    for(civ::TechIdx i = 0; i < techTree.size(); ++i)
    {
        const civ::Technology& tech = techTree.get(i);
        BOOST_TEST(!tech.title.empty());
        BOOST_TEST(tech.era <= 6u);
        for(const civ::TechIdx req : tech.requirements)
            BOOST_TEST(req < i); // Defined before -> no cycles
    }
    // Basic economy is available from the start
    for(const auto bld : {BuildingType::Woodcutter, BuildingType::Forester, BuildingType::Quarry,
                          BuildingType::Sawmill, BuildingType::Fishery, BuildingType::Hunter, BuildingType::Well,
                          BuildingType::Barracks, BuildingType::Guardhouse, BuildingType::LookoutTower})
        BOOST_TEST(!techTree.IsLockedAtStart(bld));
    // Every other building can be researched
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(bld == BuildingType::Headquarters || bld == BuildingType::Nothing9 || !techTree.IsLockedAtStart(bld))
            continue;
        BOOST_TEST(!techTree.GetTechsUnlocking(bld).empty());
    }
    const auto agriculture = techTree.find("agriculture");
    BOOST_TEST_REQUIRE(agriculture);
    BOOST_TEST(techTree.IsLockedAtStart(BuildingType::Farm));
    BOOST_TEST((techTree.GetTechsUnlocking(BuildingType::Farm) == std::vector<civ::TechIdx>{*agriculture}));
}

namespace {
TechnologyDesc makeTech(const std::string& name, std::map<std::string, unsigned> cost,
                        std::vector<std::string> buildings, std::vector<std::string> requirements = {},
                        unsigned time = 0, bool exclusive = false)
{
    TechnologyDesc desc;
    desc.name = name;
    desc.title = name;
    desc.cost = std::move(cost);
    desc.unlockedBuildings = std::move(buildings);
    desc.requirements = std::move(requirements);
    desc.time = time;
    desc.exclusive = exclusive;
    return desc;
}

struct ResearchFixture : public WorldWithGCExecution2P
{
    civ::TechIdx agriculture = 0, milling = 1, slowTech = 2, culture = 3, wineTech = 4;

    ResearchFixture()
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        WorldDescription desc;
        desc.technologies.add(makeTech("agriculture", {{"Boards", 2}}, {"Farm"}));
        desc.technologies.add(makeTech("milling", {{"Boards", 1}, {"Stones", 3}}, {"Mill", "Bakery"}, {"agriculture"}));
        desc.technologies.add(makeTech("slow", {}, {"Mint"}, {}, 10));
        desc.technologies.add(makeTech("culture", {{"Boards", 1}}, {"Fortress"}, {}, 0, true));
        auto wine = makeTech("wine", {{"Boards", 1}}, {"Vineyard"});
        wine.requiredAddon = "WINE";
        desc.technologies.add(wine);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
    }

    nobBaseWarehouse& getHQ(unsigned playerId) { return *world.GetPlayer(playerId).GetHQ(); }
    void addWares(unsigned playerId, GoodType good, unsigned amount)
    {
        getHQ(playerId).AddToInventory(GoodCounts::make(good, amount), true);
    }
    civ::PlayerResearch& research(unsigned playerId) { return world.GetPlayer(playerId).GetResearch(); }
};
} // namespace

BOOST_FIXTURE_TEST_CASE(BuildingsLockedAtStart, ResearchFixture)
{
    const GamePlayer& player = world.GetPlayer(0);
    BOOST_TEST(!player.IsBuildingEnabled(BuildingType::Farm));
    BOOST_TEST(!player.IsBuildingEnabled(BuildingType::Mill));
    BOOST_TEST(!player.IsBuildingEnabled(BuildingType::Fortress));
    BOOST_TEST(player.IsBuildingEnabled(BuildingType::Woodcutter));
    BOOST_TEST(player.IsBuildingEnabled(BuildingType::Watchtower)); // Not part of this tech tree
    // Locked buildings can't be placed
    const MapPoint bldPos = world.MakeMapPoint(hqPos + Position(4, 0));
    this->SetBuildingSite(bldPos, BuildingType::Mill);
    BOOST_TEST(!world.GetSpecObj<noBaseBuilding>(bldPos));
    // But after researching them
    research(0).SetResearched(agriculture);
    research(0).SetResearched(milling);
    civ::applyBuildingAvailability(world.GetPlayer(0));
    BOOST_TEST(player.IsBuildingEnabled(BuildingType::Mill));
    this->SetBuildingSite(bldPos, BuildingType::Mill);
    BOOST_TEST(world.GetSpecObj<noBaseBuilding>(bldPos));
}

BOOST_FIXTURE_TEST_CASE(ResearchRequiresPrerequisitesAndUnlocksBuildings, ResearchFixture)
{
    // Not possible yet
    this->SetResearch(milling);
    BOOST_TEST(!research(0).GetCurrent());
    BOOST_TEST((civ::getTechState(world, 0, milling) == civ::TechState::MissingRequirements));

    addWares(0, GoodType::Boards, 5);
    const unsigned boardsBefore = world.GetPlayer(0).GetInventory()[GoodType::Boards];
    this->SetResearch(agriculture);
    BOOST_TEST_REQUIRE((research(0).GetCurrent() == agriculture));
    BOOST_TEST((civ::getTechState(world, 0, agriculture) == civ::TechState::InProgress));

    civ::update(world);
    BOOST_TEST(research(0).IsResearched(agriculture));
    BOOST_TEST(!research(0).GetCurrent());
    BOOST_TEST(world.GetPlayer(0).IsBuildingEnabled(BuildingType::Farm));
    BOOST_TEST(world.GetPlayer(0).GetInventory()[GoodType::Boards] == boardsBefore - 2u);
    BOOST_TEST(getHQ(0).GetNumRealWares(GoodType::Boards) == 3u);
    // Other player is unaffected
    BOOST_TEST(!world.GetPlayer(1).IsBuildingEnabled(BuildingType::Farm));
    BOOST_TEST((civ::getTechState(world, 0, milling) == civ::TechState::Available));
    // Can't research twice
    this->SetResearch(agriculture);
    BOOST_TEST(!research(0).GetCurrent());
}

BOOST_FIXTURE_TEST_CASE(ResearchWaitsForWaresAndKeepsProgress, ResearchFixture)
{
    research(0).SetResearched(agriculture);
    addWares(0, GoodType::Boards, 1);
    addWares(0, GoodType::Stones, 1);
    this->SetResearch(milling);
    civ::update(world);
    BOOST_TEST(!research(0).IsResearched(milling));
    const civ::PlayerResearch::Progress* progress = research(0).GetProgress(milling);
    BOOST_TEST_REQUIRE(progress);
    // Costs are sorted by good type
    BOOST_TEST_REQUIRE(world.GetTechTree().get(milling).cost.size() == 2u);
    BOOST_TEST((progress->paid == std::vector<unsigned>{1, 1}));
    BOOST_TEST(getHQ(0).GetNumRealWares(GoodType::Stones) == 0u);

    // Switch research -> progress stays
    this->SetResearch(slowTech);
    BOOST_TEST((research(0).GetCurrent() == slowTech));
    BOOST_TEST(research(0).GetProgress(milling));
    this->SetResearch(std::nullopt);
    BOOST_TEST(!research(0).GetCurrent());

    // Continue and provide the missing wares
    this->SetResearch(milling);
    addWares(0, GoodType::Stones, 5);
    civ::update(world);
    BOOST_TEST(research(0).IsResearched(milling));
    BOOST_TEST(!research(0).GetProgress(milling));
    BOOST_TEST(getHQ(0).GetNumRealWares(GoodType::Stones) == 3u);
    BOOST_TEST(world.GetPlayer(0).IsBuildingEnabled(BuildingType::Mill));
    BOOST_TEST(world.GetPlayer(0).IsBuildingEnabled(BuildingType::Bakery));
}

BOOST_FIXTURE_TEST_CASE(MinimumResearchTime, ResearchFixture)
{
    const unsigned requiredGFs = world.GetTechTree().get(slowTech).minResearchGFs;
    BOOST_TEST_REQUIRE(requiredGFs > civ::RESEARCH_UPDATE_INTERVAL);
    this->SetResearch(slowTech);
    unsigned numUpdates = 0;
    while(!research(0).IsResearched(slowTech) && numUpdates < 1000)
    {
        civ::update(world);
        ++numUpdates;
    }
    BOOST_TEST(research(0).IsResearched(slowTech));
    BOOST_TEST(numUpdates * civ::RESEARCH_UPDATE_INTERVAL >= requiredGFs);
    BOOST_TEST((numUpdates - 1) * civ::RESEARCH_UPDATE_INTERVAL < requiredGFs);
}

BOOST_FIXTURE_TEST_CASE(ExclusiveTechnology, ResearchFixture)
{
    // Player 1 already paid partially, player 0 can pay everything
    addWares(0, GoodType::Boards, 1);
    this->curPlayer = 1;
    this->SetResearch(culture);
    this->curPlayer = 0;
    this->SetResearch(culture);
    BOOST_TEST_REQUIRE((research(1).GetCurrent() == culture));
    // Player 1 has no boards -> progress without payment
    civ::update(world);
    BOOST_TEST(research(0).IsResearched(culture));
    BOOST_TEST(!research(1).IsResearched(culture));
    BOOST_TEST(!research(1).GetCurrent());
    BOOST_TEST(!research(1).GetProgress(culture));
    BOOST_TEST((civ::getExclusiveOwner(world, culture) == 0u));
    BOOST_TEST((civ::getTechState(world, 1, culture) == civ::TechState::TakenByOther));
    BOOST_TEST(!civ::canResearch(world, 1, culture));
    BOOST_TEST(world.GetPlayer(0).IsBuildingEnabled(BuildingType::Fortress));
    BOOST_TEST(!world.GetPlayer(1).IsBuildingEnabled(BuildingType::Fortress));
    this->curPlayer = 1;
    this->SetResearch(culture);
    BOOST_TEST(!research(1).GetCurrent());
}

BOOST_FIXTURE_TEST_CASE(ExclusiveTechnologyRefundsPaidWares, ResearchFixture)
{
    // Make culture expensive for this test
    WorldDescription desc;
    desc.technologies.add(makeTech("culture", {{"Boards", 3}}, {"Fortress"}, {}, 0, true));
    world.SetTechTree(std::make_unique<civ::TechTree>(desc));
    const civ::TechIdx expensiveCulture = 0;
    addWares(1, GoodType::Boards, 2);
    this->curPlayer = 1;
    this->SetResearch(expensiveCulture);
    civ::update(world);
    BOOST_TEST_REQUIRE(research(1).GetProgress(expensiveCulture));
    BOOST_TEST(getHQ(1).GetNumRealWares(GoodType::Boards) == 0u);

    // Player 0 gets it first
    civ::completeResearch(world, 0, expensiveCulture);
    BOOST_TEST(!research(1).GetProgress(expensiveCulture));
    BOOST_TEST(!research(1).GetCurrent());
    BOOST_TEST(getHQ(1).GetNumRealWares(GoodType::Boards) == 2u);
}

BOOST_FIXTURE_TEST_CASE(AddonRequiredTechnology, ResearchFixture)
{
    ggs.setSelection(AddonId::WINE, 0);
    BOOST_TEST((civ::getTechState(world, 0, wineTech) == civ::TechState::Unavailable));
    this->SetResearch(wineTech);
    BOOST_TEST(!research(0).GetCurrent());
    ggs.setSelection(AddonId::WINE, 1);
    BOOST_TEST((civ::getTechState(world, 0, wineTech) == civ::TechState::Available));
}

BOOST_FIXTURE_TEST_CASE(NothingHappensWithoutAddon, ResearchFixture)
{
    ggs.setSelection(AddonId::CIVILIZATION, 0);
    addWares(0, GoodType::Boards, 5);
    this->SetResearch(agriculture);
    BOOST_TEST(!research(0).GetCurrent());
    research(0).SetCurrent(agriculture);
    civ::update(world);
    BOOST_TEST(!research(0).IsResearched(agriculture));
}

BOOST_FIXTURE_TEST_CASE(AIChoosesResearch, ResearchFixture)
{
    ggs.setSelection(AddonId::WINE, 0);
    // All have priority 0 -> cheapest first ("slow" has no costs)
    BOOST_TEST((civ::chooseResearchForAI(world, 0) == slowTech));
    research(0).SetResearched(slowTech);
    // agriculture (2) vs culture (1)
    BOOST_TEST((civ::chooseResearchForAI(world, 0) == culture));
    research(1).SetResearched(culture); // Taken by other player
    BOOST_TEST((civ::chooseResearchForAI(world, 0) == agriculture));
}

BOOST_FIXTURE_TEST_CASE(SerializeResearch, ResearchFixture)
{
    research(0).SetResearched(agriculture);
    addWares(0, GoodType::Stones, 2);
    this->SetResearch(milling);
    civ::update(world);
    research(1).SetResearched(culture);
    BOOST_TEST_REQUIRE(research(0).GetProgress(milling));

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    civ::startGame(world);

    BOOST_TEST(research(0).IsResearched(agriculture));
    BOOST_TEST(!research(0).IsResearched(milling));
    BOOST_TEST((research(0).GetCurrent() == milling));
    const civ::PlayerResearch::Progress* progress = research(0).GetProgress(milling);
    BOOST_TEST_REQUIRE(progress);
    BOOST_TEST((progress->paid == std::vector<unsigned>{0, 2}));
    BOOST_TEST(research(1).IsResearched(culture));
    // Building availability restored
    BOOST_TEST(world.GetPlayer(0).IsBuildingEnabled(BuildingType::Farm));
    BOOST_TEST(!world.GetPlayer(0).IsBuildingEnabled(BuildingType::Mill));
    BOOST_TEST(world.GetPlayer(1).IsBuildingEnabled(BuildingType::Fortress));
    BOOST_TEST(!world.GetPlayer(1).IsBuildingEnabled(BuildingType::Farm));

    // Serialize again and compare
    SerializedGameData sgd2;
    sgd2.MakeSnapshot(*game);
    BOOST_CHECK_EQUAL_COLLECTIONS(sgd.GetData(), sgd.GetData() + sgd.GetLength(), sgd2.GetData(),
                                  sgd2.GetData() + sgd2.GetLength());
}

BOOST_FIXTURE_TEST_CASE(LoadWithChangedTechTree, ResearchFixture)
{
    research(0).SetResearched(agriculture);
    research(0).SetResearched(culture);
    addWares(0, GoodType::Stones, 2);
    this->SetResearch(milling);
    civ::update(world);

    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);

    // New tree: culture removed, milling costs changed (stones only), new tech inserted at the start
    WorldDescription desc;
    desc.technologies.add(makeTech("new", {{"Wood", 1}}, {"Shipyard"}));
    desc.technologies.add(makeTech("agriculture", {{"Boards", 2}}, {"Farm"}));
    desc.technologies.add(makeTech("milling", {{"Stones", 1}}, {"Mill"}, {"agriculture"}));
    world.SetTechTree(std::make_unique<civ::TechTree>(desc));

    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);

    const civ::TechTree& tree = world.GetTechTree();
    BOOST_TEST(research(0).IsResearched(*tree.find("agriculture")));
    BOOST_TEST(!research(0).IsResearched(*tree.find("new")));
    BOOST_TEST((research(0).GetCurrent() == tree.find("milling")));
    const auto* progress = research(0).GetProgress(*tree.find("milling"));
    BOOST_TEST_REQUIRE(progress);
    // Paid 2 stones before, only 1 required now
    BOOST_TEST((progress->paid == std::vector<unsigned>{1}));
    BOOST_TEST(research(0).GetNumResearched() == 1u);
}

BOOST_AUTO_TEST_SUITE_END()
