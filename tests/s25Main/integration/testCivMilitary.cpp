// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "SerializedGameData.h"
#include "addons/const_addons.h"
#include "ai/aijh/AIConstruction.h"
#include "buildings/nobMilitary.h"
#include "civ/Content.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "factories/BuildingFactory.h"
#include "figures/nofPassiveSoldier.h"
#include "lua/GameDataLoader.h"
#include "worldFixtures/MockLocalGameState.h"
#include "worldFixtures/WorldWithGCExecution.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/format.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <memory>
#include <vector>

// Civilization mode: military buildings defined in lua (rttr:AddWorkshop with military = { ... })
BOOST_AUTO_TEST_SUITE(CivMilitarySuite)

namespace {
bool loadLua(const std::string& content, WorldDescription& desc)
{
    rttr::test::TmpFolder tmp;
    {
        boost::nowide::ofstream file(tmp / "default.lua");
        file << content;
    }
    GameDataLoader loader(desc, tmp);
    rttr::test::LogAccessor logAcc;
    return loader.Load();
}

/// The palisade (workshop 0): 3 soldiers, radius 9, vision 14, 1 coin, 1 armor. %s: more fields
const std::string palisadeLua =
  R"(rttr:AddWorkshop{ name = "Palisade", title = "PalisadeTitle", size = "small", cost = { Boards = 3 },
    look = "Barracks", knowledge = { MWP = 1 }, military = { soldiers = 3, radius = 9, vision = 14, coins = 1, armor = 1 }%s })";
const BuildingType PALISADE = getCustomBuilding(0);

WorldDescription makePalisadeDesc()
{
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua((boost::format(palisadeLua) % "").str(), desc));
    return desc;
}

bool sameValues(const civ::MilitaryValues& values, unsigned soldiers, unsigned radius, unsigned vision, unsigned coins,
                unsigned armor, unsigned look)
{
    return values.soldiers == soldiers && values.radius == radius && values.vision == vision && values.coins == coins
           && values.armor == armor && values.look == look;
}

struct MilitaryFixture : public WorldWithGCExecution<1, 64, 64>
{
    void init(const WorldDescription& desc)
    {
        ggs.setSelection(AddonId::CIVILIZATION, 1);
        world.SetTechTree(std::make_unique<civ::TechTree>(desc));
        civ::startGame(world);
    }
    GamePlayer& player() { return world.GetPlayer(curPlayer); }
    /// Let a soldier move in (the building is then occupied and holds its land)
    void addSoldier(nobMilitary& bld)
    {
        const MapPoint pos = bld.GetPos();
        auto& soldier = world.AddFigure(pos, std::make_unique<nofPassiveSoldier>(pos, curPlayer, &bld, &bld, 0));
        player().IncreaseInventoryJob(soldier.GetJobType(), 1);
        soldier.WalkToGoal();
        BOOST_TEST_REQUIRE(!soldier.GetGoal());
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(MilitaryFromLua)
{
    const WorldDescription desc = makePalisadeDesc();
    const civ::Content content(desc);
    BOOST_TEST(content.IsCustomMilitary(PALISADE));
    BOOST_TEST(!content.IsCustomWarehouse(PALISADE));
    BOOST_TEST(!content.IsCustomMilitary(BuildingType::Barracks));
    BOOST_TEST(sameValues(content.GetMilitaryValues(PALISADE), 3, 9, 14, 1, 1, 0));
    // The built-in ones: original tables, vision = radius + 5
    BOOST_TEST(sameValues(content.GetMilitaryValues(BuildingType::Barracks), 2, 10, 15, 1, 1, 0));
    BOOST_TEST(sameValues(content.GetMilitaryValues(BuildingType::Guardhouse), 3, 12, 17, 2, 2, 1));
    BOOST_TEST(sameValues(content.GetMilitaryValues(BuildingType::Watchtower), 6, 14, 19, 4, 4, 2));
    BOOST_TEST(sameValues(content.GetMilitaryValues(BuildingType::Fortress), 9, 16, 21, 6, 6, 3));
    BOOST_TEST(sameValues(content.GetMilitaryValues(BuildingType::Woodcutter), 0, 0, 0, 0, 0, 0));
    // No worker, no product
    const civ::CustomBuilding& palisade = *content.GetBuilding(PALISADE);
    BOOST_TEST(!palisade.workDesc.job);
    BOOST_TEST(palisade.workDesc.inputs.empty());
    BOOST_TEST(!palisade.HasProduct());
    BOOST_TEST((palisade.size == BuildingQuality::Hut));
    BOOST_TEST((palisade.look == BuildingType::Barracks));
    // Its knowledge points count like those of every building
    const civ::TechTree techTree(desc);
    BOOST_TEST(techTree.GetBuildingKnowledge(PALISADE)[civ::KnowledgeType::Military] == 1);

    // Vision and armor are optional: radius + 5 and the coins
    WorldDescription defaultsDesc;
    BOOST_TEST_REQUIRE(
      loadLua(R"(rttr:AddWorkshop{ name = "Tower", size = "large", cost = { Stones = 5 }, look = "Fortress",
    military = { soldiers = 12, radius = 18, coins = 0 } })",
              defaultsDesc));
    BOOST_TEST(sameValues(civ::Content(defaultsDesc).GetMilitaryValues(PALISADE), 12, 18, 23, 0, 0, 3));
}

BOOST_AUTO_TEST_CASE(InvalidMilitaryIsRefused)
{
    // Not allowed together with military
    for(const std::string& extra :
        {std::string(R"(, worker = { look = "Helper" })"), std::string(R"(, outputs = { "Boards" })"),
         std::string(R"(, inputs = { "Wood" })"), std::string(", workTime = 100"), std::string(", waitTime = 100"),
         std::string(", storage = 2"), std::string(", aiNearBorder = true"), std::string(", outputRatio = 1"),
         std::string(R"(, tools = { baseSpeed = 25 })"),
         std::string(R"(, warehouse = { wares = { "Boards" }, capacity = 5 })"),
         std::string(R"(, collect = { object = "X", radius = 3 })"), std::string(R"(, library = { capacity = 5 })")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(extra);
        BOOST_TEST(!loadLua((boost::format(palisadeLua) % extra).str(), invalidDesc));
    }
    // Missing or invalid values
    for(const std::string& military :
        {std::string("{ radius = 9, coins = 1 }"), std::string("{ soldiers = 3, coins = 1 }"),
         std::string("{ soldiers = 3, radius = 9 }"), std::string("{ soldiers = 0, radius = 9, coins = 1 }"),
         std::string("{ soldiers = 3, radius = 0, coins = 1 }"),
         std::string("{ soldiers = 1.5, radius = 9, coins = 1 }"),
         std::string("{ soldiers = 256, radius = 9, coins = 1 }"),
         std::string("{ soldiers = 3, radius = 9, coins = -1 }"),
         std::string("{ soldiers = 3, radius = 9, coins = 1, vision = 0 }"),
         std::string("{ soldiers = 3, radius = 9, coins = 1, range = 4 }")})
    {
        WorldDescription invalidDesc;
        BOOST_TEST_INFO(military);
        BOOST_TEST(!loadLua(R"(rttr:AddWorkshop{ name = "Palisade", size = "small", look = "Barracks", military = )"
                              + military + " }",
                            invalidDesc));
    }
    // No mine
    WorldDescription mineDesc;
    BOOST_TEST(!loadLua(R"(rttr:AddWorkshop{ name = "Palisade", size = "mine", look = "Barracks",
    military = { soldiers = 3, radius = 9, coins = 1 } })",
                        mineDesc));
    // The look must be a built-in military building (graphics, flags)
    WorldDescription lookDesc;
    BOOST_TEST_REQUIRE(
      loadLua(R"(rttr:AddWorkshop{ name = "Palisade", size = "small", cost = { Boards = 3 }, look = "Woodcutter",
    military = { soldiers = 3, radius = 9, coins = 1 } })",
              lookDesc));
    BOOST_CHECK_THROW(civ::Content{lookDesc}, GameDataError);
    // No byproducts
    WorldDescription byproductDesc;
    BOOST_TEST_REQUIRE(loadLua((boost::format(palisadeLua) % "").str() + R"(
rttr:AddByproduct{ building = "Palisade", ware = "Boards" })",
                               byproductDesc));
    BOOST_CHECK_THROW(civ::TechTree{byproductDesc}, GameDataError);
}

BOOST_AUTO_TEST_CASE(MilitaryBuildingsAreSorted)
{
    // Without lua: the 4 built-in ones in their order
    const std::vector<BuildingType> builtin = {BuildingType::Barracks, BuildingType::Guardhouse,
                                               BuildingType::Watchtower, BuildingType::Fortress};
    BOOST_TEST((civ::Content().GetMilitaryBuildings() == builtin));
    BOOST_TEST(civ::Content().GetMilitarySearchSquares() == 3u);
    // By soldiers, then radius, then order of definition (built-in first): the palisade (3 soldiers, radius 9) before
    // the guardhouse (3, 12), the camp (2, 10) after the barracks (2, 10)
    WorldDescription desc;
    BOOST_TEST_REQUIRE(loadLua((boost::format(palisadeLua) % "").str() + R"(
rttr:AddWorkshop{ name = "Camp", size = "small", cost = { Boards = 2 }, look = "Barracks", military = { soldiers = 2, radius = 10, coins = 0 } }
rttr:AddWorkshop{ name = "Citadel", size = "large", cost = { Stones = 9 }, look = "Fortress", military = { soldiers = 20, radius = 40, coins = 8 } })",
                               desc));
    const civ::Content content(desc);
    const std::vector<BuildingType>& sorted = content.GetMilitaryBuildings();
    BOOST_TEST_REQUIRE(sorted.size() == 7u);
    BOOST_TEST((sorted[0] == BuildingType::Barracks));
    BOOST_TEST((sorted[1] == getCustomBuilding(1)));
    BOOST_TEST((sorted[2] == PALISADE));
    BOOST_TEST((sorted[3] == BuildingType::Guardhouse));
    BOOST_TEST((sorted[4] == BuildingType::Watchtower));
    BOOST_TEST((sorted[5] == BuildingType::Fortress));
    BOOST_TEST((sorted[6] == getCustomBuilding(2)));
    // Radius 40: buildings up to 2 * 40 + 2 = 82 nodes apart can meet -> 5 military squares (20 nodes each)
    BOOST_TEST(content.GetMilitarySearchSquares() == 5u);
}

BOOST_FIXTURE_TEST_CASE(OldAITakesRolesFromTheOrder, MilitaryFixture)
{
    // Only the built-in ones: the original roles
    BOOST_TEST((AIJH::AIConstruction::GetGuardhouseRole() == BuildingType::Guardhouse));
    BOOST_TEST((AIJH::AIConstruction::GetWatchtowerRole() == BuildingType::Watchtower));
    BOOST_TEST(AIJH::AIConstruction::IsSmallMilitary(BuildingType::Barracks));
    BOOST_TEST(AIJH::AIConstruction::IsSmallMilitary(BuildingType::Guardhouse));
    BOOST_TEST(!AIJH::AIConstruction::IsSmallMilitary(BuildingType::Watchtower));
    BOOST_TEST(AIJH::AIConstruction::IsBigMilitary(BuildingType::Watchtower));
    BOOST_TEST(AIJH::AIConstruction::IsBigMilitary(BuildingType::Fortress));
    BOOST_TEST(!AIJH::AIConstruction::IsBigMilitary(BuildingType::Guardhouse));
    BOOST_TEST(!AIJH::AIConstruction::IsSmallMilitary(BuildingType::Woodcutter));

    // With the palisade (Barracks, Palisade, Guardhouse, Watchtower, Fortress): it has the role of the guardhouse
    init(makePalisadeDesc());
    BOOST_TEST((AIJH::AIConstruction::GetGuardhouseRole() == PALISADE));
    BOOST_TEST((AIJH::AIConstruction::GetWatchtowerRole() == BuildingType::Watchtower));
    BOOST_TEST(AIJH::AIConstruction::IsSmallMilitary(PALISADE));
    BOOST_TEST(!AIJH::AIConstruction::IsSmallMilitary(BuildingType::Guardhouse));
    BOOST_TEST(!AIJH::AIConstruction::IsBigMilitary(BuildingType::Guardhouse));
    BOOST_TEST(AIJH::AIConstruction::IsBigMilitary(BuildingType::Watchtower));
}

BOOST_FIXTURE_TEST_CASE(LuaMilitaryBuildingHoldsItsLand, MilitaryFixture)
{
    init(makePalisadeDesc());
    BOOST_TEST(BuildingProperties::IsMilitary(PALISADE));
    BOOST_TEST(!BuildingProperties::IsUsual(PALISADE));
    BOOST_TEST(!BuildingProperties::IsWareHouse(PALISADE));
    BOOST_TEST(std::string(BUILDING_NAMES[PALISADE]) == "PalisadeTitle");

    // 24 nodes east of the HQ (radius 10, vision 15): its own land and sight are apart from the ones of the HQ
    const MapPoint pos = world.MakeMapPoint(hqPos + Position(24, 0));
    auto* palisade =
      dynamic_cast<nobMilitary*>(BuildingFactory::CreateBuilding(world, PALISADE, pos, curPlayer, Nation::Romans));
    BOOST_TEST_REQUIRE(palisade);
    BOOST_TEST((palisade->GetBuildingType() == PALISADE));
    BOOST_TEST(palisade->GetMaxTroopsCt() == 3u);
    BOOST_TEST(palisade->GetMilitaryRadius() == 9u);
    BOOST_TEST(palisade->GetVisualRange() == 14u);
    BOOST_TEST(palisade->GetMaxCoinCt() == 1u);
    BOOST_TEST(palisade->GetMaxArmorCt() == 1u);
    BOOST_TEST(player().GetBuildingRegister().GetMilitaryBuildings().size() == 1u);
    BOOST_TEST(player().GetBuildingRegister().GetBuildingNums().buildings[PALISADE] == 1u);
    // Not occupied yet: no land
    const MapPoint land9 = world.MakeMapPoint(hqPos + Position(24 + 9, 0));
    const MapPoint land10 = world.MakeMapPoint(hqPos + Position(24 + 10, 0));
    BOOST_TEST(world.GetNode(land9).owner == 0u);

    addSoldier(*palisade);
    BOOST_TEST(!palisade->IsNewBuilt());
    BOOST_TEST(palisade->GetNumTroops() == 1u);
    // Land up to radius 9
    BOOST_TEST(world.GetNode(pos).owner == curPlayer + 1u);
    BOOST_TEST(world.GetNode(land9).owner == curPlayer + 1u);
    BOOST_TEST(world.GetNode(land10).owner == 0u);
    // Sight up to 14
    const MapPoint sight14 = world.MakeMapPoint(hqPos + Position(24 + 14, 0));
    const MapPoint sight16 = world.MakeMapPoint(hqPos + Position(24 + 16, 0));
    BOOST_TEST((world.CalcVisiblityWithAllies(sight14, curPlayer) == Visibility::Visible));
    BOOST_TEST((world.CalcVisiblityWithAllies(sight16, curPlayer) != Visibility::Visible));

    // Saved and loaded: still the palisade with its values and its soldier
    SerializedGameData sgd;
    sgd.MakeSnapshot(*game);
    MockLocalGameState lgs;
    em.Clear();
    world.Unload();
    sgd.ReadSnapshot(*game, lgs);
    palisade = world.GetSpecObj<nobMilitary>(pos);
    BOOST_TEST_REQUIRE(palisade);
    BOOST_TEST((palisade->GetBuildingType() == PALISADE));
    BOOST_TEST(palisade->GetMaxTroopsCt() == 3u);
    BOOST_TEST(palisade->GetMilitaryRadius() == 9u);
    BOOST_TEST(palisade->GetNumTroops() == 1u);
    BOOST_TEST(world.GetNode(land9).owner == curPlayer + 1u);

    // Destroyed: the land is gone
    world.DestroyNO(pos);
    BOOST_TEST(world.GetNode(land9).owner == 0u);
    BOOST_TEST(player().GetBuildingRegister().GetMilitaryBuildings().empty());
}

BOOST_AUTO_TEST_SUITE_END()
