// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwDistribution.h"
#include "AddonHelperFunctions.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "LeatherLoader.h"
#include "Loader.h"
#include "WindowManager.h"
#include "WineLoader.h"
#include "addons/const_addons.h"
#include "civ/Content.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlProgress.h"
#include "controls/ctrlTab.h"
#include "iwHelp.h"
#include "network/GameClient.h"
#include "ogl/FontStyle.h"
#include "ogl/glArchivItem_Bitmap.h"
#include "world/GameWorldBase.h"
#include "world/GameWorldViewer.h"
#include "gameData/BuildingConsts.h"
#include "gameData/GoodConsts.h"
#include "gameData/const_gui_ids.h"
#include <utility>

/// Dertermines width of the progress bars: distance to the window borders
const unsigned PROGRESS_BORDER_DISTANCE = 20;
const unsigned TAB_ICON_SIZE = 36;

iwDistribution::iwDistribution(const GameWorldViewer& gwv, GameCommandFactory& gcFactory)
    : TransmitSettingsIgwAdapter(CGI_DISTRIBUTION, IngameWindow::posLastOrCenter, Extent(290, 312),
                                 _("Distribution of goods"), LOADER.GetImageN("resource", 41)),
      gwv(gwv), gcFactory(gcFactory)
{
    CreateGroups();
    // Civilization mode: Tab "Wissen" for the regulator of knowledge media (only if there are media)
    hasKnowledgeTab = civ::isEnabled(gwv.GetWorld()) && !civ::getActiveContent().GetKnowledgeMedia().empty();
    const unsigned numTabs = groups.size() + (hasKnowledgeTab ? 1u : 0u);

    Resize(Extent(numTabs * TAB_ICON_SIZE + TAB_ICON_SIZE / 2 + 20, 312));

    // Tab Control
    ctrlTab* tab = AddTabCtrl(0, DrawPoint(10, 20), numTabs * TAB_ICON_SIZE + TAB_ICON_SIZE / 2);
    DrawPoint txtPos(GetSize().x / 2, 60);
    DrawPoint progPos(PROGRESS_BORDER_DISTANCE - tab->GetPos().x, txtPos.y);
    const Extent progSize(GetSize().x - 2 * PROGRESS_BORDER_DISTANCE, 20);

    for(unsigned groupId = 0; groupId < groups.size(); groupId++)
    {
        const DistributionGroup& group = groups[groupId];
        ctrlGroup* tabGrp = tab->AddTab(group.img, group.name, groupId);
        txtPos.y = progPos.y = 60;
        unsigned curId = 0;
        for(const auto& entry : group.entries)
        {
            unsigned txtId = group.entries.size() + curId;
            tabGrp->AddText(txtId, txtPos, std::get<0>(entry), COLOR_YELLOW, FontStyle::CENTER | FontStyle::BOTTOM,
                            SmallFont);
            tabGrp->AddProgress(curId++, progPos, progSize, TextureColor::Grey, 139, 138, 10);
            txtPos.y = progPos.y += progSize.y * 2;
        }
    }

    if(hasKnowledgeTab)
    {
        // Icon: ware symbol of the first knowledge medium (placeholder graphics)
        glArchivItem_Bitmap* img = nullptr;
        if(LOADER.HasMapGraphics())
            img = dynamic_cast<glArchivItem_Bitmap*>(
              LOADER.GetWareTex(civ::getActiveContent().GetKnowledgeMedia().front()));
        if(!img)
            img = LOADER.GetImageN("io", 225);
        ctrlGroup* tabGrp = tab->AddTab(img, _("Wissen: Art der Wissensmedien"), GetKnowledgeTabId());
        txtPos.y = progPos.y = 60;
        const std::array<std::string, 3> names = {_("Landwirtschaft (LWP)"), _("Industrie (IWP)"), _("Militär (MWP)")};
        for(unsigned i = 0; i < names.size(); ++i)
        {
            tabGrp->AddText(names.size() + i, txtPos, names[i], COLOR_YELLOW, FontStyle::CENTER | FontStyle::BOTTOM,
                            SmallFont);
            tabGrp->AddProgress(i, progPos, progSize, TextureColor::Grey, 139, 138, 10);
            txtPos.y = progPos.y += progSize.y * 2;
        }
        tabGrp->AddText(2 * names.size(), DrawPoint(txtPos.x, txtPos.y + 10),
                        _("Welches Wissensmedium Werkstätten herstellen"), COLOR_YELLOW,
                        FontStyle::CENTER | FontStyle::BOTTOM, SmallFont);
    }

    // Gruppe auswählen
    tab->SetSelection(0);

    const Extent btSize(32, 32);
    // "Help" button
    AddImageButton(2, DrawPoint(15, GetFullSize().y - 15 - btSize.y), btSize, TextureColor::Grey,
                   LOADER.GetImageN("io", 225), _("Help"));
    // "Default" button
    AddImageButton(10, GetFullSize() - DrawPoint::all(15) - btSize, btSize, TextureColor::Grey,
                   LOADER.GetImageN("io", 191), _("Default"));

    iwDistribution::UpdateSettings();
}

void iwDistribution::TransmitSettings()
{
    if(GAMECLIENT.IsReplayModeOn())
        return;
    if(hasKnowledgeTab && knowledgeChanged)
    {
        ctrlGroup* tab = GetCtrl<ctrlTab>(0)->GetGroup(GetKnowledgeTabId());
        std::array<uint8_t, 3> values{};
        for(unsigned i = 0; i < values.size(); ++i)
            values[i] = static_cast<uint8_t>(tab->GetCtrl<ctrlProgress>(i)->GetPosition());
        if(gcFactory.SetKnowledgeDistribution(values))
            knowledgeChanged = false;
    }
    if(settings_changed)
    {
        // Read values from the progress ctrls to the struct
        Distributions newDistribution;
        // Set default values for all distributions because groups with only one entry are not shown and will not be set
        std::transform(distributionMap.begin(), distributionMap.end(), newDistribution.begin(),
                       [](DistributionMapping const& mapping) { return std::get<2>(mapping); });

        for(unsigned i = 0; i < groups.size(); ++i)
        {
            ctrlGroup* tab = GetCtrl<ctrlTab>(0)->GetGroup(i);
            const DistributionGroup& group = groups[i];
            // Werte der Gruppen auslesen
            for(unsigned j = 0; j < group.entries.size(); ++j)
            {
                auto value = static_cast<uint8_t>(tab->GetCtrl<ctrlProgress>(j)->GetPosition());
                newDistribution[std::get<1>(group.entries[j])] = value;
            }
        }

        // und übermitteln
        if(gcFactory.ChangeDistribution(newDistribution))
        {
            GAMECLIENT.visual_settings.distribution = newDistribution;
            settings_changed = false;
        }
    }
}

void iwDistribution::Msg_Group_ProgressChange(const unsigned /*group_id*/, const unsigned /*ctrl_id*/,
                                              const unsigned short /*position*/)
{
    // Note: The tab control reports its own id as group id, so check which tab is shown (only that one can be changed)
    if(hasKnowledgeTab && GetCtrl<ctrlTab>(0)->GetCurrentTab() == GetKnowledgeTabId())
        knowledgeChanged = true;
    else
        settings_changed = true;
}

void iwDistribution::SetKnowledgeValues(const std::array<uint8_t, 3>& values)
{
    if(!hasKnowledgeTab)
        return;
    ctrlGroup* tab = GetCtrl<ctrlTab>(0)->GetGroup(GetKnowledgeTabId());
    for(unsigned i = 0; i < values.size(); ++i)
        tab->GetCtrl<ctrlProgress>(i)->SetPosition(values[i]);
}

void iwDistribution::UpdateSettings(const Distributions& distribution)
{
    if(GAMECLIENT.IsReplayModeOn())
        gwv.GetPlayer().FillVisualSettings(GAMECLIENT.visual_settings);

    for(unsigned g = 0; g < groups.size(); ++g)
    {
        // Look for correct group
        const DistributionGroup& group = groups[g];
        ctrlGroup* tab = GetCtrl<ctrlTab>(0)->GetGroup(g);
        // And correct entry
        for(unsigned i = 0; i < group.entries.size(); ++i)
            tab->GetCtrl<ctrlProgress>(i)->SetPosition(distribution[std::get<1>(group.entries[i])]);
    }
}

void iwDistribution::UpdateSettings()
{
    UpdateSettings(GAMECLIENT.visual_settings.distribution);
    if(hasKnowledgeTab && !knowledgeChanged)
    {
        const auto distribution = gwv.GetPlayer().GetResearch().GetKnowledgeDistribution(gwv.GetWorld().GetTechTree());
        SetKnowledgeValues({distribution[civ::KnowledgeType::Agriculture], distribution[civ::KnowledgeType::Industry],
                            distribution[civ::KnowledgeType::Military]});
    }
}

void iwDistribution::Msg_ButtonClick(const unsigned ctrl_id)
{
    if(GAMECLIENT.IsReplayModeOn())
        return;
    switch(ctrl_id)
    {
        default: return;

        case 2:
        {
            WINDOWMANAGER.ReplaceWindow(
              std::make_unique<iwHelp>(_("The priority of goods for the individual buildings can be set here. "
                                         "The higher the value, the quicker the required goods are delivered "
                                         "to the building concerned.")));
        }
        break;
        // Default button
        case 10:
        {
            UpdateSettings(GAMECLIENT.default_settings.distribution);
            settings_changed = true;
            if(hasKnowledgeTab)
            {
                const auto& defaults = civ::getActiveContent().GetDefaultKnowledgeDistribution();
                SetKnowledgeValues({defaults[civ::KnowledgeType::Agriculture], defaults[civ::KnowledgeType::Industry],
                                    defaults[civ::KnowledgeType::Military]});
                knowledgeChanged = true;
            }
        }
        break;
    }
}

void iwDistribution::CreateGroups()
{
    if(!groups.empty())
        return;

    GoodType lastGood = GoodType::Nothing;
    unsigned pos = 0;
    for(const DistributionMapping& mapping : distributionMap)
    {
        // New group?
        if(lastGood != std::get<0>(mapping))
        {
            lastGood = std::get<0>(mapping);
            // Fish = all foodstuff
            std::string name = lastGood == GoodType::Fish ? gettext_noop("Foodstuff") : WARE_NAMES[lastGood];
            glArchivItem_Bitmap* img = nullptr;
            switch(lastGood)
            {
                case GoodType::Fish: img = LOADER.GetImageN("io", 80); break;
                case GoodType::Grain: img = LOADER.GetImageN("io", 90); break;
                case GoodType::Iron: img = LOADER.GetImageN("io", 81); break;
                case GoodType::Coal: img = LOADER.GetImageN("io", 91); break;
                case GoodType::Wood: img = LOADER.GetImageN("io", 89); break;
                case GoodType::Boards: img = LOADER.GetImageN("io", 82); break;
                case GoodType::Water: img = LOADER.GetImageN("io", 92); break;
                case GoodType::Ham:
                    img = LOADER.GetImageN("leather_bobs",
                                           leatheraddon::bobIndex[leatheraddon::BobType::DistributionOfPigsIcon]);
                    break;
                default: break;
            }
            if(!img)
                throw std::runtime_error("Unexpected good in distribution");

            groups.push_back(DistributionGroup(_(name), img));
        }
        // HQ = Construction
        std::string name = std::get<1>(mapping) == BuildingType::Headquarters ? gettext_noop("Construction") :
                                                                                BUILDING_NAMES[std::get<1>(mapping)];
        groups.back().entries.push_back(std::tuple(_(name), pos));
        pos++;
    }

    auto isUnused = [&](std::tuple<std::string, unsigned> const& bts) {
        const BuildingType buildingType = std::get<1>(distributionMap[std::get<1>(bts)]);
        return makeIsUnusedBuilding(gwv.GetWorld().GetGGS())(buildingType);
    };
    for(auto& group : groups)
        helpers::erase_if(group.entries, isUnused);

    // Remove groups with only one entry (pigs when leather addon is disabled)
    helpers::erase_if(groups, [](DistributionGroup& group) { return group.entries.size() == 1; });
}
