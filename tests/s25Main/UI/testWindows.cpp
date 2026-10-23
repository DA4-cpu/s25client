// Copyright (C) 2005 - 2025 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GameCommands.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "WindowManager.h"
#include "addons/const_addons.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlCheck.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlImage.h"
#include "controls/ctrlMultiline.h"
#include "controls/ctrlProgress.h"
#include "controls/ctrlTab.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "desktops/Desktop.h"
#include "factories/GameCommandFactory.h"
#include "helpers/containerUtils.h"
#include "ingameWindows/iwAddons.h"
#include "ingameWindows/iwDistribution.h"
#include "ingameWindows/iwResearch.h"
#include "ingameWindows/iwSkipGFs.h"
#include "ingameWindows/iwVictory.h"
#include "uiHelper/uiHelpers.hpp"
#include "worldFixtures/CreateEmptyWorld.h"
#include "worldFixtures/WorldFixture.h"
#include "world/GameWorld.h"
#include "world/GameWorldView.h"
#include "world/GameWorldViewer.h"
#include "gameData/WorldDescription.h"
#include <turtle/mock.hpp>
#include <boost/test/unit_test.hpp>
#include <mygettext/mygettext.h>

//-V:MOCK_METHOD:813
//-V:MOCK_EXPECT:807

using SmallWorldFixture = WorldFixture<CreateEmptyWorld, 1, 10, 10>;
using TwoPlayerWorldFixture = WorldFixture<CreateEmptyWorld, 2>;

BOOST_FIXTURE_TEST_SUITE(Windows, uiHelper::Fixture)

BOOST_AUTO_TEST_CASE(Victory)
{
    std::vector<std::string> winnerNames;
    winnerNames.push_back("FooName");
    winnerNames.push_back("BarNameBaz");
    const iwVictory wnd(winnerNames);
    // 2 buttons
    BOOST_TEST_REQUIRE(wnd.GetCtrls<ctrlButton>().size() == 2u);
    // Find a text field containing all winner names
    const auto txts = wnd.GetCtrls<ctrlMultiline>();
    bool found = false;
    for(const ctrlMultiline* txt : txts)
    {
        if(txt->GetNumLines() != winnerNames.size())
            continue; // LCOV_EXCL_LINE
        bool curFound = true;
        for(unsigned i = 0; i < winnerNames.size(); i++)
        {
            curFound &= txt->GetLine(i) == winnerNames[i];
        }
        found |= curFound;
    }
    BOOST_TEST_REQUIRE(found);
}

BOOST_AUTO_TEST_CASE(AddonWindow)
{
    GlobalGameSettings ggs;
    const iwAddons wndAllChangeable(ggs, nullptr, AddonChangeAllowed::All);
    const iwAddons wndAllReadOnly(ggs, nullptr, AddonChangeAllowed::None);
    const auto addonsChangeableGui = wndAllChangeable.GetCtrls<ctrlGroup>();
    BOOST_TEST_REQUIRE(addonsChangeableGui.size() == ggs.getNumAddons() + 1); // First element is the option group
    const auto addonsReadonlyGui = wndAllReadOnly.GetCtrls<ctrlGroup>();
    BOOST_TEST_REQUIRE(addonsReadonlyGui.size() == ggs.getNumAddons() + 1);
    for(unsigned i = 1; i <= ggs.getNumAddons(); ++i)
    {
        const ctrlGroup* changeableGroup = addonsChangeableGui[i];
        const ctrlGroup* readonlyGroup = addonsReadonlyGui[i];
        // No lock icon
        BOOST_TEST(changeableGroup->GetCtrls<ctrlImage>().empty());
        // Lock icon
        BOOST_TEST_REQUIRE(!readonlyGroup->GetCtrls<ctrlImage>().empty());
        // Verify it is the lock icon with tooltip
        BOOST_TEST_REQUIRE(readonlyGroup->GetCtrls<ctrlImage>()[0]->GetTooltip() == _("Locked"));
        for(const auto* checkbox : changeableGroup->GetCtrls<ctrlCheck>())
            BOOST_TEST_REQUIRE(!checkbox->isReadOnly());
        for(const auto* checkbox : readonlyGroup->GetCtrls<ctrlCheck>())
            BOOST_TEST_REQUIRE(checkbox->isReadOnly());
        for(const auto* cb : changeableGroup->GetCtrls<ctrlComboBox>())
            BOOST_TEST_REQUIRE(!cb->isReadOnly());
        for(const auto* cb : readonlyGroup->GetCtrls<ctrlComboBox>())
            BOOST_TEST_REQUIRE(cb->isReadOnly());
    }
}

BOOST_FIXTURE_TEST_CASE(JumpWindow, SmallWorldFixture)
{
    uiHelper::Fixture f;
    // Test if it is constructible only, accesses GameClient for buttons
    GameWorldViewer gwv(0, world);
    GameWorldView view(gwv, Position(0, 0), Extent(100, 100));
    iwSkipGFs wnd(view);
    // At least 4 buttons for "jump by x" and at least 1 extra for "jump to"
    const auto bts = wnd.GetCtrls<ctrlTextButton>();
    BOOST_TEST(bts.size() > 4);
    const auto numIncBts = helpers::count_if(bts, [](const ctrlTextButton* bt) { return bt->GetText().at(0) == '+'; });
    BOOST_TEST(numIncBts >= 4);
}

namespace {
struct NoGCFactory : GameCommandFactory
{
    unsigned numGCs = 0;
    bool AddGC(gc::GameCommandPtr) override
    {
        ++numGCs;
        return true;
    }
};
} // namespace

namespace {
struct RecordingGCFactory : GameCommandFactory
{
    std::vector<gc::GameCommandPtr> gcs;
    bool AddGC(gc::GameCommandPtr gc) override
    {
        gcs.push_back(gc);
        return true;
    }
};
/// Number of tabs and the id of the last tab of the distribution window
std::pair<unsigned, unsigned> getTabs(ctrlTab& tab)
{
    unsigned numTabs = 0;
    for(unsigned nr = 0; nr < 20; ++nr)
    {
        const unsigned before = tab.GetCurrentTab();
        tab.SetSelection(static_cast<unsigned short>(nr));
        if(nr > 0 && tab.GetCurrentTab() == before)
            break;
        ++numTabs;
    }
    tab.SetSelection(static_cast<unsigned short>(numTabs - 1));
    return {numTabs, tab.GetCurrentTab()};
}
} // namespace

BOOST_FIXTURE_TEST_CASE(DistributionWindowHasKnowledgeTab, TwoPlayerWorldFixture)
{
    uiHelper::Fixture f;
    GameWorldViewer gwv(0, world);
    unsigned numTabsWithoutCiv;
    {
        RecordingGCFactory gcFactory;
        iwDistribution wnd(gwv, gcFactory);
        numTabsWithoutCiv = getTabs(*wnd.GetCtrl<ctrlTab>(0)).first;
    }

    ggs.setSelection(AddonId::CIVILIZATION, 1);
    WorldDescription desc;
    WareDesc medium;
    medium.name = "Medium";
    medium.look = medium.transport = "Stones";
    medium.knowledge = {1, 0, 0};
    desc.wares.push_back(medium);
    desc.knowledgeDistribution = KnowledgeValues{7, 3, 0};
    world.SetTechTree(std::make_unique<civ::TechTree>(desc));
    civ::startGame(world);

    RecordingGCFactory gcFactory;
    iwDistribution wnd(gwv, gcFactory);
    auto& tab = *wnd.GetCtrl<ctrlTab>(0);
    const std::pair<unsigned, unsigned> tabs = getTabs(tab);
    const unsigned numTabs = tabs.first, knowledgeTabId = tabs.second;
    BOOST_TEST(numTabs == numTabsWithoutCiv + 1u);
    // The last tab shows the regulator with the defaults from lua
    ctrlGroup* group = tab.GetGroup(knowledgeTabId);
    BOOST_TEST_REQUIRE(group);
    const auto progresses = group->GetCtrls<ctrlProgress>();
    BOOST_TEST_REQUIRE(progresses.size() == 3u);
    BOOST_TEST(group->GetCtrl<ctrlProgress>(0)->GetPosition() == 7u);
    BOOST_TEST(group->GetCtrl<ctrlProgress>(1)->GetPosition() == 3u);
    BOOST_TEST(group->GetCtrl<ctrlProgress>(2)->GetPosition() == 0u);

    // Change a slider like the user does (the tab control reports its own id as group id) -> 1 command on transmit
    group->GetCtrl<ctrlProgress>(2)->SetPosition(9);
    static_cast<Window&>(wnd).Msg_Group_ProgressChange(0, 2, 9);
    static_cast<Window&>(wnd).Msg_Timer(0);
    BOOST_TEST_REQUIRE(gcFactory.gcs.size() == 1u);
    BOOST_TEST(dynamic_cast<gc::SetKnowledgeDistribution*>(gcFactory.gcs[0].get()));
    gcFactory.gcs[0]->Execute(world, 0);
    const auto distribution = world.GetPlayer(0).GetResearch().GetKnowledgeDistribution(world.GetTechTree());
    BOOST_TEST(distribution[civ::KnowledgeType::Agriculture] == 7u);
    BOOST_TEST(distribution[civ::KnowledgeType::Military] == 9u);
    // Nothing changed anymore -> nothing sent
    static_cast<Window&>(wnd).Msg_Timer(0);
    BOOST_TEST(gcFactory.gcs.size() == 1u);
}

BOOST_FIXTURE_TEST_CASE(ResearchWindowShowsOtherPlayers, TwoPlayerWorldFixture)
{
    uiHelper::Fixture f;
    ggs.setSelection(AddonId::CIVILIZATION, 1);
    WorldDescription desc;
    TechnologyDesc alpha;
    alpha.name = "alpha";
    alpha.title = "Alpha";
    desc.technologies.add(alpha);
    TechnologyDesc beta;
    beta.name = "beta";
    beta.title = "Beta";
    desc.technologies.add(beta);
    world.SetTechTree(std::make_unique<civ::TechTree>(desc));
    civ::startGame(world);
    world.GetPlayer(1).GetResearch().SetCurrent(1u);

    GameWorldViewer gwv(0, world);
    NoGCFactory gcFactory;
    iwResearch wnd(gwv, gcFactory);
    const auto hasText = [&wnd](const std::string& text) {
        return helpers::contains_if(wnd.GetCtrls<ctrlText>(),
                                    [&text](const ctrlText* txt) { return txt->GetText() == text; });
    };
    const auto cbs = wnd.GetCtrls<ctrlComboBox>();
    BOOST_TEST_REQUIRE(cbs.size() == 1u);
    ctrlComboBox* cb = cbs.front();
    BOOST_TEST(cb->GetNumItems() == 2u);
    BOOST_TEST(!cb->IsVisible()); // No cheat mode without a game interface
    BOOST_TEST(!hasText("Beta"));

    // Switch to player 1: Shows its research, but it can't be changed
    static_cast<Window&>(wnd).Msg_ComboSelectItem(cb->GetID(), 1);
    BOOST_TEST(hasText("Beta"));
    for(const ctrlTextButton* bt : wnd.GetCtrls<ctrlTextButton>())
        BOOST_TEST(!bt->GetEnabled());
    // Back to the own player
    static_cast<Window&>(wnd).Msg_ComboSelectItem(cb->GetID(), 0);
    BOOST_TEST(!hasText("Beta"));
    BOOST_TEST(gcFactory.numGCs == 0u);
}

namespace {
MOCK_BASE_CLASS(TestWindow, Window)
{
public:
    TestWindow(Window * parent, unsigned id, const DrawPoint& position) : Window(parent, id, position) {}
    MOCK_METHOD(Msg_PaintBefore, 0)
    MOCK_METHOD(Msg_PaintAfter, 0)
    MOCK_METHOD(Draw_, 0, void())
};
} // namespace

BOOST_AUTO_TEST_CASE(DrawOrder)
{
    Desktop* dsk = WINDOWMANAGER.GetCurrentDesktop();
    std::vector<TestWindow*> wnds;
    wnds.reserve(6);
    // Top level controls
    for(int i = 0; i < 3; i++)
    {
        wnds.push_back(
          dsk->AddCtrl(std::make_unique<TestWindow>(dsk, static_cast<unsigned>(wnds.size()), DrawPoint(0, 0))));
    }
    // Some groups with own controls
    for(int i = 0; i < 3; i++)
    {
        ctrlGroup* grp = dsk->AddGroup(100 + i);
        for(int i = 0; i < 3; i++)
        {
            wnds.push_back(
              grp->AddCtrl(std::make_unique<TestWindow>(dsk, static_cast<unsigned>(wnds.size()), DrawPoint(0, 0))));
        }
    }
    mock::sequence s;
    // Note: Actually order of calls to controls is undefined but in practice matches the IDs
    for(TestWindow* wnd : wnds)
        MOCK_EXPECT(wnd->Msg_PaintBefore).once().in(s);
    for(TestWindow* wnd : wnds)
        MOCK_EXPECT(wnd->Draw_).once().in(s);
    for(TestWindow* wnd : wnds)
        MOCK_EXPECT(wnd->Msg_PaintAfter).once().in(s);
    WINDOWMANAGER.Draw();
    mock::verify();
}

BOOST_AUTO_TEST_SUITE_END()
