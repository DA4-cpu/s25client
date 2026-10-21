// Copyright (C) 2005 - 2025 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PointOutput.h"
#include "TerrainBQOutput.h"
#include "files.h"
#include "helpers/Range.h"
#include "helpers/containerUtils.h"
#include "legacy/TerrainData.h"
#include "lua/GameDataLoader.h"
#include "gameData/CivContentDesc.h"
#include "gameData/EdgeDesc.h"
#include "gameData/TechnologyDesc.h"
#include "gameData/TerrainDesc.h"
#include "gameData/WorldDescription.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/LogAccessor.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <sstream>

namespace bfs = boost::filesystem;
namespace bnw = boost::nowide;

BOOST_AUTO_TEST_SUITE(GameData)

BOOST_AUTO_TEST_CASE(BQ_Output)
{
    std::stringstream s;
    s << TerrainBQ::Nothing;
    BOOST_TEST(s.str() == "Nothing");
    s.str("");
    s << TerrainBQ::Mine;
    BOOST_TEST(s.str() == "Mine");
}

template<class Container, class Predicate>
static std::vector<std::string> getNames(const Container& items, Predicate&& predicate)
{
    std::vector<std::string> result;
    for(const auto& item : items)
    {
        if(predicate(item))
            result.push_back(item.name);
    }
    return result;
}

BOOST_AUTO_TEST_CASE(IndexingToDescriptionVector)
{
    struct name_t
    {};
    using idx_t = DescIdx<name_t>;
    {
        // Default construct index is false-ish
        idx_t idx;
        BOOST_TEST(!idx);
        // With a value it is true-ish
        idx = idx_t(5);
        BOOST_TEST(!!idx);
    }

    DescriptionVector<std::string, idx_t::index_type> vec;
    vec.push_back("0");
    vec.push_back("1");
    vec.push_back("2");
    BOOST_TEST(vec[idx_t(0)] == "0");
    BOOST_TEST(vec[idx_t(1)] == "1");
    BOOST_TEST(vec[idx_t(2)] == "2");

    std::vector<idx_t> indices;
    for(const auto i : vec.indices())
        indices.push_back(i);
    BOOST_TEST((indices == std::vector<idx_t>{idx_t(0), idx_t(1), idx_t(2)}));
}

BOOST_AUTO_TEST_CASE(LoadGameData)
{
    WorldDescription worldDesc;
    loadGameData(worldDesc);
    BOOST_TEST(worldDesc.edges.size() == 3u * 5u - 1u);
    BOOST_TEST(worldDesc.terrain.size() >= 3u * NUM_TTS);
    for(unsigned l = 0; l < NUM_LTS; l++)
    {
        // indexOf(name) in these arrays should be the original RTTR index as in TerrainData
        const std::vector<std::string> tNames = getNames(
          worldDesc.terrain, [l, &worldDesc](const TerrainDesc& t) { return worldDesc.get(t.landscape).s2Id == l; });
        const std::vector<std::string> eNames = getNames(
          worldDesc.edges, [l, &worldDesc](const EdgeDesc& e) { return worldDesc.get(e.landscape).s2Id == l; });
        const auto lt = Landscape(l);
        for(const unsigned i : helpers::range(NUM_TTS))
        {
            const auto t = TerrainType(i);
            const TerrainDesc& desc = worldDesc.terrain.get(worldDesc.terrain.getIndex(tNames[i]));
            BOOST_TEST(desc.s2Id == TerrainData::GetTextureIdentifier(t));
            const EdgeType newEdge =
              !desc.edgeType ? ET_NONE : EdgeType((helpers::indexOf(eNames, worldDesc.get(desc.edgeType).name) + 1));
            BOOST_TEST(newEdge == TerrainData::GetEdgeType(lt, t));
            BOOST_TEST(desc.GetBQ() == TerrainData::GetBuildingQuality(t));
            BOOST_TEST(!desc.Is(ETerrain::Walkable)
                       == (TerrainData::GetBuildingQuality(t) == TerrainBQ::Nothing
                           || TerrainData::GetBuildingQuality(t) == TerrainBQ::Danger));
            BOOST_TEST(desc.Is(ETerrain::Mineable) == (TerrainData::GetBuildingQuality(t) == TerrainBQ::Mine));
            BOOST_TEST(desc.Is(ETerrain::Buildable) == (TerrainData::GetBuildingQuality(t) == TerrainBQ::Castle));
            BOOST_TEST(desc.Is(ETerrain::Shippable) == TerrainData::IsUsableByShip(t));
            BOOST_TEST((desc.kind == TerrainKind::Snow) == TerrainData::IsSnow(lt, t));

            BOOST_TEST((desc.IsUsableByAnimals() || desc.kind == TerrainKind::Snow)
                       == (TerrainData::IsUsableByAnimals(t) || TerrainData::IsSnow(lt, t)));
            BOOST_TEST(desc.IsVital() == TerrainData::IsVital(t));
            BOOST_TEST(desc.minimapColor == TerrainData::GetColor(lt, t));
        }
    }
    // TerrainData::PrintEdgePrios();
}

BOOST_AUTO_TEST_CASE(DetectError)
{
    rttr::test::TmpFolder tmp;
    rttr::test::ConfigOverride _("RTTR", tmp);
    auto worldPath = RTTRCONFIG.ExpandPath(s25::folders::gamedata) / "world";
    create_directories(worldPath);
    {
        bnw::ofstream file(worldPath / "default.lua");
        BOOST_TEST_REQUIRE(!!(file << "empty = true"));
    }
    WorldDescription desc;
    BOOST_REQUIRE_NO_THROW(loadGameData(desc));

    {
        bnw::ofstream file(worldPath / "default.lua");
        BOOST_TEST_REQUIRE(!!(file << "syntax{-error"));
    }
    GameDataLoader loader(desc, tmp);
    rttr::test::LogAccessor logAcc;
    BOOST_TEST(!loader.Load());
    BOOST_CHECK_THROW(loadGameData(desc), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(LoadTechnologies)
{
    rttr::test::TmpFolder tmp;
    {
        bnw::ofstream file(tmp / "default.lua");
        file << R"(
rttr:AddTechnology{ name = "a", title = "A", era = 0, cost = { Boards = 2, Stones = 1 },
                    knowledge = { LWP = 3, MWP = 1 }, unlocks = { buildings = { "Farm" } } }
rttr:AddTechnology{ name = "b", title = "B", description = "Desc", era = 2, requires = { "a" }, time = 30,
                    exclusive = true, addon = "WINE", aiPriority = 5 }
rttr:SetBuildingKnowledge{ Headquarters = { LWP = 5, IWP = 5, MWP = 5 }, Catapult = { MWP = -5 } }
rttr:SetBuildingKnowledge{ Woodcutter = { IWP = 2 } }
)";
    }
    WorldDescription desc;
    GameDataLoader loader(desc, tmp);
    BOOST_TEST_REQUIRE(loader.Load());
    BOOST_TEST_REQUIRE(desc.technologies.size() == 2u);
    const TechnologyDesc& a = *desc.technologies.tryGet("a");
    BOOST_TEST(a.title == "A");
    BOOST_TEST(a.cost.size() == 2u);
    BOOST_TEST(a.cost.at("Boards") == 2u);
    BOOST_TEST((a.unlockedBuildings == std::vector<std::string>{"Farm"}));
    BOOST_TEST(!a.exclusive);
    BOOST_TEST((a.knowledge == KnowledgeValues{3, 0, 1}));
    const TechnologyDesc& b = *desc.technologies.tryGet("b");
    BOOST_TEST(b.description == "Desc");
    BOOST_TEST(b.era == 2u);
    BOOST_TEST((b.requirements == std::vector<std::string>{"a"}));
    BOOST_TEST(b.time == 30u);
    BOOST_TEST(b.exclusive);
    BOOST_TEST(b.requiredAddon == "WINE");
    BOOST_TEST(b.aiPriority == 5);
    BOOST_TEST((b.knowledge == KnowledgeValues{0, 0, 0}));

    BOOST_TEST_REQUIRE(desc.buildingKnowledge.size() == 3u);
    BOOST_TEST((desc.buildingKnowledge.at("Headquarters") == KnowledgeValues{5, 5, 5}));
    BOOST_TEST((desc.buildingKnowledge.at("Catapult") == KnowledgeValues{0, 0, -5}));
    BOOST_TEST((desc.buildingKnowledge.at("Woodcutter") == KnowledgeValues{0, 2, 0}));
}

BOOST_AUTO_TEST_CASE(DetectInvalidTechnologies)
{
    const auto expectLoadFailure = [](const std::string& content) {
        rttr::test::TmpFolder tmp;
        {
            bnw::ofstream file(tmp / "default.lua");
            file << content;
        }
        WorldDescription desc;
        GameDataLoader loader(desc, tmp);
        rttr::test::LogAccessor logAcc;
        BOOST_TEST(!loader.Load());
    };
    // Prerequisite defined after use (would allow cycles)
    expectLoadFailure(R"(rttr:AddTechnology{ name = "b", requires = { "a" } }
rttr:AddTechnology{ name = "a" })");
    // Duplicate name
    expectLoadFailure(R"(rttr:AddTechnology{ name = "a" }
rttr:AddTechnology{ name = "a" })");
    // Unknown key (typo)
    expectLoadFailure(R"(rttr:AddTechnology{ name = "a", cots = { Boards = 1 } })");
    // Zero cost
    expectLoadFailure(R"(rttr:AddTechnology{ name = "a", cost = { Boards = 0 } })");
    // Invalid era
    expectLoadFailure(R"(rttr:AddTechnology{ name = "a", era = 7 })");
    // Unknown kind of knowledge
    expectLoadFailure(R"(rttr:AddTechnology{ name = "a", knowledge = { XWP = 1 } })");
    // Negative knowledge costs
    expectLoadFailure(R"(rttr:AddTechnology{ name = "a", knowledge = { LWP = -1 } })");
    // Building knowledge must be a table per building
    expectLoadFailure(R"(rttr:SetBuildingKnowledge{ Woodcutter = 2 })");
    expectLoadFailure(R"(rttr:SetBuildingKnowledge{ Woodcutter = { Iwp = 2 } })");
    // Defined twice
    expectLoadFailure(R"(rttr:SetBuildingKnowledge{ Woodcutter = { IWP = 2 } }
rttr:SetBuildingKnowledge{ Woodcutter = { IWP = 3 } })");
}

BOOST_AUTO_TEST_CASE(LoadWaresAndWorkshops)
{
    rttr::test::TmpFolder tmp;
    {
        bnw::ofstream file(tmp / "default.lua");
        file << R"(
rttr:AddWare{ name = "Raw", title = "Rohstoff", kind = "RawKind", properties = { "A", "B" }, look = "Stones",
              transport = "Stones", knowledge = { LWP = 1 } }
rttr:AddWare{ name = "Product", look = "Boards", transport = "Boards" }
rttr:SetWareTags{ Boards = { kind = "Bretter", properties = { "Nadelholz" } } }
rttr:AddWorkshop{ name = "Shop", title = "Werkstatt", description = "Desc", size = "medium",
                  cost = { Boards = 2, Stones = 1 }, look = "Sawmill",
                  worker = { title = "Arbeiter", tool = "none", look = "Carpenter" },
                  inputs = { "Raw", { "Bretter", "Nadelholz" } }, consume = "any", output = "Product",
                  inheritProperties = true, storage = 4, workTime = 20, waitTime = 5 }
rttr:AddWorkshop{ name = "Shop2", size = "small", look = "Woodcutter", output = { "Product" }, workTime = 1,
                  worker = { tool = "Hammer", look = "Helper" } }
)";
    }
    WorldDescription desc;
    GameDataLoader loader(desc, tmp);
    BOOST_TEST_REQUIRE(loader.Load());
    BOOST_TEST_REQUIRE(desc.wares.size() == 2u);
    const WareDesc& raw = desc.wares[0];
    BOOST_TEST(raw.name == "Raw");
    BOOST_TEST(raw.title == "Rohstoff");
    BOOST_TEST(raw.kind == "RawKind");
    BOOST_TEST((raw.properties == std::vector<std::string>{"A", "B"}));
    BOOST_TEST(raw.look == "Stones");
    BOOST_TEST(raw.transport == "Stones");
    BOOST_TEST((raw.knowledge == KnowledgeValues{1, 0, 0}));
    BOOST_TEST(desc.wares[1].title == "Product"); // Default: name
    BOOST_TEST_REQUIRE(desc.wareTags.count("Boards") == 1u);
    BOOST_TEST(desc.wareTags.at("Boards").kind == "Bretter");
    BOOST_TEST((desc.wareTags.at("Boards").properties == std::vector<std::string>{"Nadelholz"}));

    BOOST_TEST_REQUIRE(desc.workshops.size() == 2u);
    const WorkshopDesc& shop = desc.workshops[0];
    BOOST_TEST(shop.name == "Shop");
    BOOST_TEST(shop.title == "Werkstatt");
    BOOST_TEST(shop.description == "Desc");
    BOOST_TEST(shop.size == "medium");
    BOOST_TEST(shop.cost.at("Boards") == 2u);
    BOOST_TEST(shop.look == "Sawmill");
    BOOST_TEST(shop.worker.title == "Arbeiter");
    BOOST_TEST(shop.worker.tool == "none");
    BOOST_TEST(shop.worker.look == "Carpenter");
    BOOST_TEST_REQUIRE(shop.inputs.size() == 2u);
    BOOST_TEST((shop.inputs[0] == TagQuery{"Raw"}));
    BOOST_TEST((shop.inputs[1] == TagQuery{"Bretter", "Nadelholz"}));
    BOOST_TEST(!shop.consumeEach);
    BOOST_TEST((shop.output == TagQuery{"Product"}));
    BOOST_TEST(shop.inheritProperties);
    BOOST_TEST(shop.storage == 4u);
    BOOST_TEST(shop.workTime == 20u);
    BOOST_TEST(shop.waitTime == 5u);
    const WorkshopDesc& shop2 = desc.workshops[1];
    BOOST_TEST(shop2.inputs.empty());
    BOOST_TEST(shop2.consumeEach);
    BOOST_TEST(shop2.worker.title == "Shop2");
    BOOST_TEST(shop2.storage == 6u);
}

BOOST_AUTO_TEST_CASE(DetectInvalidWaresAndWorkshops)
{
    const auto expectLoadFailure = [](const std::string& content) {
        rttr::test::TmpFolder tmp;
        {
            bnw::ofstream file(tmp / "default.lua");
            file << content;
        }
        WorldDescription desc;
        GameDataLoader loader(desc, tmp);
        rttr::test::LogAccessor logAcc;
        BOOST_TEST(!loader.Load());
    };
    const std::string worker = R"(worker = { tool = "none", look = "Helper" })";
    // Invalid characters in the name
    expectLoadFailure(R"(rttr:AddWare{ name = "Kno chen", look = "Stones", transport = "Stones" })");
    expectLoadFailure(R"(rttr:AddWare{ name = "Bäume", look = "Stones", transport = "Stones" })");
    // Missing look or transport
    expectLoadFailure(R"(rttr:AddWare{ name = "Bones", transport = "Stones" })");
    expectLoadFailure(R"(rttr:AddWare{ name = "Bones", look = "Stones" })");
    // Typo
    expectLoadFailure(R"(rttr:AddWare{ name = "Bones", look = "Stones", transport = "Stones", kinf = "X" })");
    // Tags must be tables
    expectLoadFailure(R"(rttr:SetWareTags{ Boards = "Bretter" })");
    // Workshops
    expectLoadFailure(
      R"(rttr:AddWorkshop{ name = "W", size = "huge", look = "Sawmill", output = "Boards", workTime = 1, )" + worker
      + " }");
    expectLoadFailure(R"(rttr:AddWorkshop{ name = "W", size = "small", look = "Sawmill", workTime = 1, )" + worker
                      + " }");
    expectLoadFailure(R"(rttr:AddWorkshop{ name = "W", size = "small", look = "Sawmill", output = "Boards", )" + worker
                      + " }");
    expectLoadFailure(
      R"(rttr:AddWorkshop{ name = "W", size = "small", look = "Sawmill", output = "Boards", workTime = 1 })");
    expectLoadFailure(
      R"(rttr:AddWorkshop{ name = "W", size = "small", look = "Sawmill", output = "Boards", workTime = 1,
                      inputs = { "A", "B", "C", "D" }, )"
      + worker + " }");
    expectLoadFailure(
      R"(rttr:AddWorkshop{ name = "W", size = "small", look = "Sawmill", output = "Boards", workTime = 1,
                      consume = "all", )"
      + worker + " }");
    expectLoadFailure(
      R"(rttr:AddWorkshop{ name = "W", size = "small", look = "Sawmill", output = { 1 }, workTime = 1, )" + worker
      + " }");
}

BOOST_AUTO_TEST_CASE(DetectRecursion)
{
    rttr::test::TmpFolder tmp;
    {
        bnw::ofstream file(tmp / "default.lua");
        file << "include(\"foo.lua\")\n";
        bnw::ofstream file2(tmp / "foo.lua");
        file2 << "include(\"default.lua\")\n";
    }
    WorldDescription desc;
    GameDataLoader loader(desc, tmp);
    rttr::test::LogAccessor logAcc;
    BOOST_TEST(!loader.Load());
    RTTR_REQUIRE_LOG_CONTAINS("Maximum include depth", false);
}

BOOST_AUTO_TEST_CASE(DetectInvalidFilenames)
{
    rttr::test::TmpFolder tmp;
    {
        bnw::ofstream file(tmp / "default.lua");
        file << "include(\"foo(=.lua\")\n";
    }
    WorldDescription desc;
    GameDataLoader loader(desc, tmp);
    rttr::test::LogAccessor logAcc;
    BOOST_TEST(!loader.Load());
    RTTR_REQUIRE_LOG_CONTAINS("disallowed chars", false);
}

BOOST_AUTO_TEST_CASE(DetectNonexistingFile)
{
    rttr::test::TmpFolder tmp;
    {
        bnw::ofstream file(tmp / "default.lua");
        file << "include(\"foo.lua\")\n";
    }
    WorldDescription desc;
    GameDataLoader loader(desc, tmp);
    rttr::test::LogAccessor logAcc;
    BOOST_TEST(!loader.Load());
    RTTR_REQUIRE_LOG_CONTAINS("File not found", false);
}

BOOST_AUTO_TEST_CASE(DetectWrongExtension)
{
    rttr::test::TmpFolder tmp;
    {
        bnw::ofstream file(tmp / "default.lua");
        file << "include(\"foo.txt\")\n";
        bnw::ofstream file2(tmp / "foo.txt");
    }
    WorldDescription desc;
    GameDataLoader loader(desc, tmp);
    rttr::test::LogAccessor logAcc;
    BOOST_TEST(!loader.Load());
    RTTR_REQUIRE_LOG_CONTAINS("File must have .lua as the extension", false);
}

BOOST_AUTO_TEST_CASE(DetectFolderEscape)
{
    rttr::test::TmpFolder tmp;
    bfs::path basePath(tmp / "gameData");
    create_directories(basePath);
    {
        bnw::ofstream file(basePath / "default.lua");
        file << "include(\"../foo.lua\")\n";
        bnw::ofstream file2(tmp / "foo.lua");
    }
    WorldDescription desc;
    GameDataLoader loader(desc, basePath);
    rttr::test::LogAccessor logAcc;
    BOOST_TEST(!loader.Load());
    RTTR_REQUIRE_LOG_CONTAINS("outside the lua data directory", false);
}

#ifndef BOOST_WINDOWS_API
BOOST_AUTO_TEST_CASE(DetectAbsolute)
{
    rttr::test::TmpFolder tmp;
    bfs::path basePath(tmp / "gameData");
    create_directories(basePath);
    {
        bnw::ofstream file(basePath / "default.lua");
        file << "include(\"/tmp/foo.lua\")\n";
    }
    WorldDescription desc;
    GameDataLoader loader(desc, basePath);
    rttr::test::LogAccessor logAcc;
    BOOST_TEST(!loader.Load());
    RTTR_REQUIRE_LOG_CONTAINS("relative", false);
}
#endif

BOOST_AUTO_TEST_CASE(TextureCoords)
{
    rttr::test::TmpFolder tmp;
    {
        bnw::ofstream file(tmp / "default.lua");
        file << "rttr:AddLandscape{\
            name = \"testland\",\
            mapGfx = \"<RTTR_GAME>/DATA/MAP_0_Z.LST\",\
            roads = {\
                normal = { texture = \"<RTTR_GAME>/foo\"},\
                upgraded = { texture = \"<RTTR_GAME>/foo\"},\
                boat = { texture = \"<RTTR_GAME>/foo\"},\
                mountain = { texture = \"<RTTR_GAME>/foo\"}\
            }}";
        file << "rttr:AddTerrainEdge{ name = \"edge\", landscape = \"testland\", texture = \"<RTTR_GAME>/foo\" }";
        file << "rttr:AddTerrain{\
            name = \"terrain1\",\
            landscape = \"testland\", edgeType = \"edge\", texture = \"<RTTR_GAME>/foo\", color = 0, \
            pos = { 10, 20, 32, 31 }, texType = \"overlapped\" }";
        file << "rttr:AddTerrain{\
            name = \"terrain2\",\
            landscape = \"testland\", edgeType = \"edge\", texture = \"<RTTR_GAME>/foo\", color = 0, \
            pos = { 10, 20, 32, 31 }, texType = \"stacked\" }";
        file << "rttr:AddTerrain{\
            name = \"terrain3\",\
            landscape = \"testland\", edgeType = \"edge\", texture = \"<RTTR_GAME>/foo\", color = 0, \
            pos = { 10, 20, 32, 31 }, texType = \"rotated\" }";
    }
    WorldDescription desc;
    GameDataLoader loader(desc, tmp);
    BOOST_TEST_REQUIRE(loader.Load());
    // Border points are inset by half a pixel for OpenGL (sample middle of pixel!)
    // Overlapped uses the full rectangle
    const TerrainDesc::Triangle rsuO = desc.terrain.tryGet("terrain1")->GetRSUTriangle();
    BOOST_TEST(rsuO.tip == PointF(10 + 32 / 2.f, 20 + 0.5f));
    BOOST_TEST(rsuO.left == PointF(10 + 0.5f, 20 + 31 - 0.5f));
    BOOST_TEST(rsuO.right == PointF(10 + 32 - 0.5f, 20 + 31 - 0.5f));
    const TerrainDesc::Triangle usdO = desc.terrain.tryGet("terrain1")->GetUSDTriangle();
    BOOST_TEST(usdO.tip == PointF(10 + 32 / 2.f, 20 + 31 - 0.5f));
    BOOST_TEST(usdO.left == PointF(10 + 0.5f, 20 + 0.5f));
    BOOST_TEST(usdO.right == PointF(10 + 32 - 0.5f, 20 + 0.5f));

    // Stacked has RSU over USD
    const TerrainDesc::Triangle rsuS = desc.terrain.tryGet("terrain2")->GetRSUTriangle();
    BOOST_TEST(rsuS.tip == rsuO.tip);
    BOOST_TEST(rsuS.left == PointF(10 + 0.5f, 20 + 31 / 2.f));
    BOOST_TEST(rsuS.right == PointF(10 + 32 - 0.5f, 20 + 31 / 2.f));
    const TerrainDesc::Triangle usdS = desc.terrain.tryGet("terrain2")->GetUSDTriangle();
    BOOST_TEST(usdS.tip == usdO.tip);
    BOOST_TEST(usdS.left == PointF(10 + 0.5f, 20 + 31 / 2.f));
    BOOST_TEST(usdS.right == PointF(10 + 32 - 0.5f, 20 + 31 / 2.f));

    // Rotated is stacked sideways (note that order stays the same)
    const TerrainDesc::Triangle rsuR = desc.terrain.tryGet("terrain3")->GetRSUTriangle();
    BOOST_TEST(rsuR.tip == rsuS.left);
    BOOST_TEST(rsuR.left == rsuS.right);
    BOOST_TEST(rsuR.right == rsuS.tip);
    const TerrainDesc::Triangle usdR = desc.terrain.tryGet("terrain3")->GetUSDTriangle();
    BOOST_TEST(usdR.tip == usdS.right);
    BOOST_TEST(usdR.left == usdS.tip);
    BOOST_TEST(usdR.right == usdS.left);
}

BOOST_AUTO_TEST_SUITE_END()
