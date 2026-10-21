// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "SerializedGameData.h"
#include "addons/const_addons.h"
#include "buildings/noBaseBuilding.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHQ.h"
#include "civ/Identifiers.h"
#include "civ/Knowledge.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "factories/BuildingFactory.h"
#include "helpers/EnumRange.h"
#include "helpers/OptionalIO.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "gameData/ShieldConsts.h"
#include "gameData/WorldDescription.h"
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <memory>

BOOST_AUTO_TEST_SUITE(ResearchSuite)

using civ::KnowledgeType;

BOOST_AUTO_TEST_CASE(IdentifiersRoundTrip)
{
    for(unsigned i = 0; i < NUM_BUILTIN_BUILDINGS; ++i)
    {
        const auto bld = BuildingType(i);
        BOOST_TEST_REQUIRE(!civ::getIdentifier(bld).empty());
        if(bld == BuildingType::Headquarters || bld == BuildingType::Nothing9)
            BOOST_TEST(!civ::buildingFromIdentifier(civ::getIdentifier(bld)));
        else
            BOOST_TEST((civ::buildingFromIdentifier(civ::getIdentifier(bld)) == bld));
    }
    for(unsigned i = 0; i < NUM_BUILTIN_GOODS; ++i)
    {
        const auto good = GoodType(i);
        BOOST_TEST_REQUIRE(!civ::getIdentifier(good).empty());
        const auto parsed = civ::goodFromIdentifier(civ::getIdentifier(good));
        if(good == GoodType::WaterEmpty)
            BOOST_TEST(!parsed);
        else
            BOOST_TEST((parsed == ConvertShields(good)));
    }
    for(unsigned i = 0; i < NUM_BUILTIN_JOBS; ++i)
    {
        const auto job = Job(i);
        BOOST_TEST((civ::jobFromIdentifier(civ::getIdentifier(job)) == job));
    }
    BOOST_TEST((civ::goodFromIdentifier("Shield") == GoodType::ShieldRomans));
    BOOST_TEST(!civ::buildingFromIdentifier("NotABuilding"));
    BOOST_TEST((civ::buildingFromIdentifier("Headquarters", true) == BuildingType::Headquarters));
    BOOST_TEST(!civ::buildingFromIdentifier("Nothing9", true));
    BOOST_TEST(!civ::goodFromIdentifier("NotAGood"));
    BOOST_TEST(!civ::jobFromIdentifier("NotAJob"));
    // Unused custom slots
    BOOST_TEST(civ::getIdentifier(GoodType::Custom0) == "Custom0");
    BOOST_TEST(civ::getIdentifier(getCustomBuilding(3)) == "Custom3");
    BOOST_TEST(!civ::goodFromIdentifier("Custom0"));
}

BOOST_AUTO_TEST_CASE(KnowledgeNamesAndFormatting)
{
    for(const auto type : helpers::enumRange<KnowledgeType>())
    {
        // Same order as in the lua loader
        BOOST_TEST(std::string(civ::getShortName(type)) == KNOWLEDGE_LUA_NAMES[static_cast<unsigned>(type)]);
        BOOST_TEST((civ::knowledgeTypeFromShortName(civ::getShortName(type)) == type));
    }
    BOOST_TEST(!civ::knowledgeTypeFromShortName("XWP"));
    BOOST_TEST(civ::formatKnowledge(0) == "0");
    BOOST_TEST(civ::formatKnowledge(70) == "7");
    BOOST_TEST(civ::formatKnowledge(75) == "7,5");
    BOOST_TEST(civ::formatKnowledge(-50) == "-5");
    BOOST_TEST(civ::formatKnowledge(-5) == "-0,5");
    BOOST_TEST(civ::formatKnowledge(1234) == "123,4");
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
    // Every technology costs knowledge points
    for(civ::TechIdx i = 0; i < techTree.size(); ++i)
    {
        BOOST_TEST_INFO(techTree.get(i).name);
        BOOST_TEST(techTree.get(i).HasKnowledgeCost());
    }
    // Knowledge points of buildings (values from the design document)
    const auto knowledge = [&techTree](BuildingType bld, KnowledgeType type) {
        return techTree.GetBuildingKnowledge(bld)[type];
    };
    for(const auto type : helpers::enumRange<KnowledgeType>())
    {
        BOOST_TEST(knowledge(BuildingType::Headquarters, type) == 5);
        BOOST_TEST(knowledge(BuildingType::Storehouse, type) == 1);
        BOOST_TEST(knowledge(BuildingType::HarborBuilding, type) == 1);
    }
    BOOST_TEST(knowledge(BuildingType::Woodcutter, KnowledgeType::Industry) == 2);
    BOOST_TEST(knowledge(BuildingType::Woodcutter, KnowledgeType::Agriculture) == 0);
    BOOST_TEST(knowledge(BuildingType::Forester, KnowledgeType::Agriculture) == 1);
    BOOST_TEST(knowledge(BuildingType::Forester, KnowledgeType::Industry) == 1);
    BOOST_TEST(knowledge(BuildingType::Farm, KnowledgeType::Agriculture) == 3);
    BOOST_TEST(knowledge(BuildingType::Farm, KnowledgeType::Industry) == 1);
    BOOST_TEST(knowledge(BuildingType::Armory, KnowledgeType::Industry) == 2);
    BOOST_TEST(knowledge(BuildingType::Armory, KnowledgeType::Military) == 1);
    BOOST_TEST(knowledge(BuildingType::Fortress, KnowledgeType::Military) == 5);
    BOOST_TEST(knowledge(BuildingType::Catapult, KnowledgeType::Military) == -5);
    for(const auto mine :
        {BuildingType::GraniteMine, BuildingType::CoalMine, BuildingType::IronMine, BuildingType::GoldMine})
        BOOST_TEST(knowledge(mine, KnowledgeType::Industry) == 1);
    // Every built-in building except the placeholder has some knowledge points
    for(unsigned i = 0; i < NUM_BUILTIN_BUILDINGS; ++i)
    {
        const auto bld = BuildingType(i);
        if(bld == BuildingType::Nothing9)
            continue;
        const civ::KnowledgePoints& points = techTree.GetBuildingKnowledge(bld);
        BOOST_TEST_INFO(civ::getIdentifier(bld));
        BOOST_TEST(std::any_of(points.begin(), points.end(), [](int p) { return p != 0; }));
    }
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
    civ::TechIdx agriculture = 0, milling = 1, slowTech = 2, culture = 3, wineTech = 4, writing = 5, drill = 6;

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
        auto writingDesc = makeTech("writing", {}, {"Brewery"});
        writingDesc.knowledge = {8, 0, 0};
        desc.technologies.add(writingDesc);
        auto drillDesc = makeTech("drill", {}, {"Armory"});
        drillDesc.knowledge = {0, 0, 3};
        desc.technologies.add(drillDesc);
        desc.buildingKnowledge = {
          {"Headquarters", {5, 5, 5}}, {"Fishery", {2, 0, 0}}, {"Woodcutter", {0, 2, 0}}, {"Catapult", {0, 0, -5}}};
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
    }

    nobBaseWarehouse& getHQ(unsigned playerId) { return *world.GetPlayer(playerId).GetHQ(); }
    void addWares(unsigned playerId, GoodType good, unsigned amount)
    {
        getHQ(playerId).AddToInventory(GoodCounts::make(good, amount), true);
    }
    civ::PlayerResearch& research(unsigned playerId) { return world.GetPlayer(playerId).GetResearch(); }
    /// Place a finished building of player 0 at the given offset to the HQ
    MapPoint addBuilding(BuildingType bld, Position offset)
    {
        const MapPoint pos = world.MakeMapPoint(hqPos + offset);
        BuildingFactory::CreateBuilding(world, bld, pos, 0, Nation::Romans);
        BOOST_TEST_REQUIRE(world.GetSpecObj<noBaseBuilding>(pos));
        return pos;
    }
    civ::KnowledgePoints total(unsigned playerId) { return civ::calcTotalKnowledge(world.GetPlayer(playerId)); }
    civ::KnowledgePoints available(unsigned playerId) { return civ::calcAvailableKnowledge(world.GetPlayer(playerId)); }
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
    // drill is the only one which can be paid right now (no boards in stock, enough MWP)
    BOOST_TEST((civ::chooseResearchForAI(world, 0) == drill));
    research(0).SetResearched(drill);
    // agriculture (2) vs culture (1) vs writing (8 LWP)
    BOOST_TEST((civ::chooseResearchForAI(world, 0) == culture));
    research(1).SetResearched(culture); // Taken by other player
    BOOST_TEST((civ::chooseResearchForAI(world, 0) == agriculture));
}

BOOST_FIXTURE_TEST_CASE(KnowledgeFromBuildings, ResearchFixture)
{
    // HQ: 5 of each kind (calculated in tenths)
    for(const auto type : helpers::enumRange<KnowledgeType>())
    {
        BOOST_TEST(total(0)[type] == 50);
        BOOST_TEST(available(0)[type] == 50);
    }
    const MapPoint fisheryPos = addBuilding(BuildingType::Fishery, Position(4, 0));
    addBuilding(BuildingType::Woodcutter, Position(6, 0));
    BOOST_TEST(total(0)[KnowledgeType::Agriculture] == 70);
    BOOST_TEST(total(0)[KnowledgeType::Industry] == 70);
    BOOST_TEST(total(0)[KnowledgeType::Military] == 50);
    // Negative values are possible
    addBuilding(BuildingType::Catapult, Position(8, 0));
    BOOST_TEST(total(0)[KnowledgeType::Military] == 0);
    // Buildings without values don't count, neither do building sites
    addBuilding(BuildingType::Sawmill, Position(2, 4));
    this->SetBuildingSite(world.MakeMapPoint(hqPos + Position(4, 4)), BuildingType::Woodcutter);
    BOOST_TEST_REQUIRE(world.GetSpecObj<noBaseBuilding>(world.MakeMapPoint(hqPos + Position(4, 4))));
    BOOST_TEST(total(0)[KnowledgeType::Industry] == 70);
    // Other players are not affected
    BOOST_TEST(total(1)[KnowledgeType::Agriculture] == 50);
    // Destroyed buildings lose their points
    world.DestroyNO(fisheryPos);
    BOOST_TEST(total(0)[KnowledgeType::Agriculture] == 50);
}

BOOST_FIXTURE_TEST_CASE(ResearchWaitsForKnowledge, ResearchFixture)
{
    // Writing needs 8 LWP, the HQ gives 5
    this->SetResearch(writing);
    BOOST_TEST_REQUIRE((research(0).GetCurrent() == writing));
    BOOST_TEST(!civ::hasEnoughKnowledge(world, 0, writing));
    BOOST_TEST(!civ::isAffordable(world, 0, writing));
    BOOST_TEST(civ::getMissingKnowledge(world, 0, writing)[KnowledgeType::Agriculture] == 30);
    BOOST_TEST(civ::getMissingKnowledge(world, 0, writing)[KnowledgeType::Industry] == 0);
    for(unsigned i = 0; i < 5; i++)
        civ::update(world);
    BOOST_TEST(!research(0).IsResearched(writing));
    BOOST_TEST((research(0).GetCurrent() == writing));
    BOOST_TEST(research(0).GetSpentKnowledge()[KnowledgeType::Agriculture] == 0);

    // 1 fishery -> 7 LWP: Still not enough
    addBuilding(BuildingType::Fishery, Position(4, 0));
    civ::update(world);
    BOOST_TEST(!research(0).IsResearched(writing));
    BOOST_TEST(civ::getMissingKnowledge(world, 0, writing)[KnowledgeType::Agriculture] == 10);
    // 2 fisheries -> 9 LWP
    addBuilding(BuildingType::Fishery, Position(6, 0));
    BOOST_TEST(civ::hasEnoughKnowledge(world, 0, writing));
    BOOST_TEST(civ::isAffordable(world, 0, writing));
    civ::update(world);
    BOOST_TEST(research(0).IsResearched(writing));
    BOOST_TEST(world.GetPlayer(0).IsBuildingEnabled(BuildingType::Brewery));
    // Spent on completion: 9 - 8 = 1 available
    BOOST_TEST(research(0).GetSpentKnowledge()[KnowledgeType::Agriculture] == 80);
    BOOST_TEST(total(0)[KnowledgeType::Agriculture] == 90);
    BOOST_TEST(available(0)[KnowledgeType::Agriculture] == 10);
    BOOST_TEST(available(0)[KnowledgeType::Military] == 50);
    // Other player unaffected
    BOOST_TEST(research(1).GetSpentKnowledge()[KnowledgeType::Agriculture] == 0);
}

BOOST_FIXTURE_TEST_CASE(NegativeKnowledgeKeepsTechnologies, ResearchFixture)
{
    const MapPoint fishery1 = addBuilding(BuildingType::Fishery, Position(4, 0));
    const MapPoint fishery2 = addBuilding(BuildingType::Fishery, Position(6, 0));
    this->SetResearch(writing);
    civ::update(world);
    BOOST_TEST_REQUIRE(research(0).IsResearched(writing));

    // Losing the fisheries: 5 total, 8 spent
    world.DestroyNO(fishery1);
    world.DestroyNO(fishery2);
    BOOST_TEST(total(0)[KnowledgeType::Agriculture] == 50);
    BOOST_TEST(available(0)[KnowledgeType::Agriculture] == -30);
    // Technology and its buildings stay
    BOOST_TEST(research(0).IsResearched(writing));
    BOOST_TEST(world.GetPlayer(0).IsBuildingEnabled(BuildingType::Brewery));
    // Technologies with other kinds of knowledge can still be completed
    this->SetResearch(drill);
    civ::update(world);
    BOOST_TEST(research(0).IsResearched(drill));
    BOOST_TEST(available(0)[KnowledgeType::Military] == 20);
}

BOOST_FIXTURE_TEST_CASE(NegativeBuildingKnowledgeBlocksResearch, ResearchFixture)
{
    // Catapult: -5 MWP -> 0 available, drill needs 3
    const MapPoint catapultPos = addBuilding(BuildingType::Catapult, Position(8, 0));
    BOOST_TEST(available(0)[KnowledgeType::Military] == 0);
    this->SetResearch(drill);
    civ::update(world);
    BOOST_TEST(!research(0).IsResearched(drill));
    world.DestroyNO(catapultPos);
    civ::update(world);
    BOOST_TEST(research(0).IsResearched(drill));
}

BOOST_FIXTURE_TEST_CASE(AIPrefersTechnologiesWithEnoughKnowledge, ResearchFixture)
{
    ggs.setSelection(AddonId::WINE, 0);
    for(const civ::TechIdx tech : {agriculture, slowTech, culture})
        research(0).SetResearched(tech);
    research(0).SetResearched(milling);
    // Left: writing (8 LWP, only 5 available) and drill (3 MWP)
    BOOST_TEST((civ::chooseResearchForAI(world, 0) == drill));
    research(0).SetResearched(drill);
    // Writing is the only one left, even if not affordable yet
    BOOST_TEST((civ::chooseResearchForAI(world, 0) == writing));
}

BOOST_FIXTURE_TEST_CASE(SerializeResearch, ResearchFixture)
{
    research(0).SetResearched(agriculture);
    addWares(0, GoodType::Stones, 2);
    this->SetResearch(milling);
    civ::update(world);
    research(1).SetResearched(culture);
    civ::completeResearch(world, 1, drill);
    BOOST_TEST_REQUIRE(research(0).GetProgress(milling));
    BOOST_TEST_REQUIRE(research(1).GetSpentKnowledge()[KnowledgeType::Military] == 30);

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
    BOOST_TEST(research(1).IsResearched(drill));
    BOOST_TEST(research(1).GetSpentKnowledge()[KnowledgeType::Military] == 30);
    BOOST_TEST(research(0).GetSpentKnowledge()[KnowledgeType::Military] == 0);
    BOOST_TEST(available(1)[KnowledgeType::Military] == 20);
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
