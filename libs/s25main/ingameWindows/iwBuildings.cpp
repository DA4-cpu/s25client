// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwBuildings.h"
#include "AddonHelperFunctions.h"
#include "Cheats.h"
#include "GameInterface.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "LeatherLoader.h"
#include "Loader.h"
#include "WindowManager.h"
#include "WineLoader.h"
#include "addons/const_addons.h"
#include "buildings/nobBaseWarehouse.h"
#include "buildings/nobHarborBuilding.h"
#include "buildings/nobMilitary.h"
#include "buildings/nobUsual.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlImageButton.h"
#include "files.h"
#include "iwBaseWarehouse.h"
#include "iwBuilding.h"
#include "iwHarborBuilding.h"
#include "iwHelp.h"
#include "iwMilitaryBuilding.h"
#include "iwTempleBuilding.h"
#include "ogl/FontStyle.h"
#include "ogl/glFont.h"
#include "world/GameWorldBase.h"
#include "world/GameWorldView.h"
#include "world/GameWorldViewer.h"
#include "gameTypes/BuildingCount.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/const_gui_ids.h"

void iwBuildings::setBuildingOrder()
{
    // Order of the buildings in which they will be shown
    bts = {
      BuildingType::Barracks,       BuildingType::Guardhouse, BuildingType::Watchtower,     BuildingType::Fortress,
      BuildingType::GraniteMine,    BuildingType::CoalMine,   BuildingType::IronMine,       BuildingType::GoldMine,
      BuildingType::LookoutTower,   BuildingType::Catapult,   BuildingType::Woodcutter,     BuildingType::Fishery,
      BuildingType::Quarry,         BuildingType::Forester,   BuildingType::Slaughterhouse, BuildingType::Hunter,
      BuildingType::Brewery,        BuildingType::Armory,     BuildingType::Metalworks,     BuildingType::Ironsmelter,
      BuildingType::PigFarm,
      BuildingType::Storehouse, // entry 21
      BuildingType::Mill,           BuildingType::Bakery,     BuildingType::Sawmill,        BuildingType::Mint,
      BuildingType::Well,           BuildingType::Shipyard,   BuildingType::Farm,           BuildingType::DonkeyBreeder,
      BuildingType::Charburner,
      BuildingType::HarborBuilding,                                                       // entry 31
      BuildingType::Vineyard,       BuildingType::Winery,     BuildingType::Temple,       // entry 34
      BuildingType::Skinner,        BuildingType::Tannery,    BuildingType::LeatherWorks, // entry 37
    };

    helpers::erase_if(bts, makeIsUnusedBuilding(gwv.GetWorld().GetGGS()));
}

// Abstand des ersten Icons vom linken oberen Fensterrand
const Extent bldContentOffset(30, 40);
// Abstand der einzelnen Symbole untereinander
const Extent iconSpacing(40, 48);
// Abstand der Schriften unter den Icons
const unsigned short font_distance_y = 20;

namespace {
enum
{
    ID_Help,
    ID_CbDebugPlayer,
    ID_BuildingsStart,
};
} // namespace

iwBuildings::iwBuildings(GameWorldView& gwv, GameCommandFactory& gcFactory)
    : IngameWindow(CGI_BUILDINGS, IngameWindow::posLastOrCenter, Extent(185, 480), _("Buildings"),
                   LOADER.GetImageN("resource", 41)),
      gwv(gwv), gcFactory(gcFactory), homePlayerId_(gwv.GetViewer().GetPlayerId()), viewedPlayerId_(homePlayerId_)
{
    setBuildingOrder();
    // Reserve one extra icon-row's worth of vertical space (compared to the original "+ 1") for the debug player
    // selector added below, see ID_CbDebugPlayer.
    Resize(iconSpacing * Extent(4, helpers::divCeil(bts.size(), 4) + 2) + bldContentOffset);

    lastDrawnNation_ = GetViewedPlayer().nation;
    // Symbole für die einzelnen Gebäude erstellen
    for(unsigned y = 0; y < bts.size() / 4 + (bts.size() % 4 > 0 ? 1 : 0); ++y)
    {
        for(unsigned x = 0; x < 4; ++x)
        {
            if(y * 4 + x >= bts.size()) //-V547
                break;

            Extent btSize = Extent(32, 32);
            DrawPoint btPos = bldContentOffset - btSize / 2 + iconSpacing * DrawPoint(x, y);
            AddImageButton(ID_BuildingsStart + y * 4 + x, btPos, btSize, TextureColor::Grey,
                           LOADER.GetNationIcon(lastDrawnNation_, bts[y * 4 + x]), _(BUILDING_NAMES[bts[y * 4 + x]]));
        }
    }

    // "Help" button
    Extent btSize = Extent(30, 32);
    const DrawPoint helpPos = GetFullSize() - DrawPoint(14, 20) - btSize;
    AddImageButton(ID_Help, helpPos, btSize, TextureColor::Grey, LOADER.GetImageN("io", 225), _("Help"));

    // Debug feature: lets you browse the building statistics of any player (including AI players). Hidden unless
    // cheat/debug mode is active, see Msg_PaintBefore. Placed in the extra row reserved above, with a small gap
    // above the Help button so the two never overlap regardless of window size.
    const Extent cbSize(GetFullSize().x - (bldContentOffset.x - 14) * 2, 20);
    const DrawPoint cbPos(bldContentOffset.x - 14, helpPos.y - 8 - cbSize.y);
    ctrlComboBox* cb = AddComboBox(ID_CbDebugPlayer, cbPos, cbSize, TextureColor::Grey, NormalFont, 100);
    cb->SetVisible(false);

    const GameWorldBase& world = gwv.GetWorld();
    unsigned homeSelection = 0;
    for(unsigned i = 0; i < world.GetNumPlayers(); ++i)
    {
        const GamePlayer& p = world.GetPlayer(i);
        if(!p.isUsed())
            continue;

        if(i == homePlayerId_)
            homeSelection = static_cast<unsigned>(selectablePlayerIds_.size());

        cb->AddItem(p.isHuman() ? p.name : (p.name + " (" + _("AI") + ")"));
        selectablePlayerIds_.push_back(i);
    }
    cb->SetSelection(homeSelection);
}

const GamePlayer& iwBuildings::GetViewedPlayer() const
{
    return gwv.GetWorld().GetPlayer(viewedPlayerId_);
}

/// Anzahlen der Gebäude zeichnen
void iwBuildings::Msg_PaintAfter()
{
    static boost::format fmt("%1%/%2%");
    IngameWindow::Msg_PaintAfter();
    // Anzahlen herausfinden
    BuildingCount bc = GetViewedPlayer().GetBuildingRegister().GetBuildingNums();

    // Anzahlen unter die Gebäude schreiben
    DrawPoint rowPos = GetDrawPos() + bldContentOffset + DrawPoint(0, font_distance_y);
    for(unsigned y = 0; y < helpers::divCeil(bts.size(), 4); ++y)
    {
        DrawPoint curPos = rowPos;
        for(unsigned x = 0; x < 4; x++)
        {
            if(y * 4 + x >= bts.size()) //-V547
                break;

            fmt % bc.buildings[bts[y * 4 + x]] % bc.buildingSites[bts[y * 4 + x]];
            NormalFont->Draw(curPos, fmt.str(), FontStyle::CENTER, COLOR_YELLOW);
            curPos.x += iconSpacing.x;
        }
        rowPos.y += iconSpacing.y;
    }
}

void iwBuildings::Msg_ButtonClick(const unsigned ctrl_id)
{
    if(ctrl_id == ID_Help) // Help button
    {
        WINDOWMANAGER.ReplaceWindow(
          std::make_unique<iwHelp>(_("The building statistics window gives you an insight into "
                                     "the number of buildings you have, by type. The number on "
                                     "the left is the total number of this type of building "
                                     "completed, the number on the right shows how many are "
                                     "currently under construction.")));
        return;
    }

    if(ctrl_id < ID_BuildingsStart)
        return; // not a building icon (defensive; Help and the combo box are handled above / don't reach here)

    // no buildings of type complete? -> do nothing
    const GamePlayer& viewedPlayer = GetViewedPlayer();
    const BuildingRegister& buildingRegister = viewedPlayer.GetBuildingRegister();

    BuildingType bldType = bts[ctrl_id - ID_BuildingsStart];
    if(BuildingProperties::IsMilitary(bldType))
        GoToFirstMatching<iwMilitaryBuilding>(bldType, buildingRegister.GetMilitaryBuildings());
    else if(bldType == BuildingType::HarborBuilding)
        GoToFirstMatching<iwHarborBuilding>(bldType, buildingRegister.GetHarbors());
    else if(BuildingProperties::IsWareHouse(bldType))
        GoToFirstMatching<iwBaseWarehouse>(bldType, buildingRegister.GetStorehouses());
    else if(bldType == BuildingType::Temple)
        GoToFirstMatching<iwTempleBuilding>(bldType, buildingRegister.GetBuildings(bldType));
    else
        GoToFirstMatching<iwBuilding>(bldType, buildingRegister.GetBuildings(bldType));
}

void iwBuildings::Msg_ComboSelectItem(const unsigned ctrl_id, const unsigned selection)
{
    if(ctrl_id == ID_CbDebugPlayer && selection < selectablePlayerIds_.size())
        viewedPlayerId_ = selectablePlayerIds_[selection];
}

void iwBuildings::Msg_PaintBefore()
{
    IngameWindow::Msg_PaintBefore();

    // The player selector is a debug feature and only usable while cheat mode is on (consistent with all other
    // debug/cheat visualizations, e.g. the enemy productivity overlay).
    const GameInterface* gi = gwv.GetWorld().GetGameInterface();
    const bool debugModeOn = gi && gi->GI_GetCheats().isCheatModeOn();

    auto* cb = GetCtrl<ctrlComboBox>(ID_CbDebugPlayer);
    if(cb && cb->IsVisible() != debugModeOn)
    {
        cb->SetVisible(debugModeOn);

        // Cheat mode got turned off while we were looking at another player's buildings -> jump back to our own
        if(!debugModeOn && viewedPlayerId_ != homePlayerId_)
        {
            viewedPlayerId_ = homePlayerId_;
            for(unsigned i = 0; i < selectablePlayerIds_.size(); ++i)
            {
                if(selectablePlayerIds_[i] == homePlayerId_)
                {
                    cb->SetSelection(i);
                    break;
                }
            }
        }
    }

    // Building icons are nation-specific. Refresh them if we're now looking at a player of a different nation
    // (only relevant while browsing other players' stats in debug mode).
    const Nation viewedNation = GetViewedPlayer().nation;
    if(viewedNation != lastDrawnNation_)
    {
        lastDrawnNation_ = viewedNation;
        for(unsigned i = 0; i < bts.size(); ++i)
        {
            auto* icon = GetCtrl<ctrlImageButton>(ID_BuildingsStart + i);
            if(icon)
                icon->SetImage(LOADER.GetNationIcon(lastDrawnNation_, bts[i]));
        }
    }
}

template<class T_Window, class T_Building>
void iwBuildings::GoToFirstMatching(BuildingType bldType, const std::list<T_Building*>& blds)
{
    for(T_Building* bld : blds)
    {
        if(bld->GetBuildingType() == bldType)
        {
            gwv.MoveToMapPt(bld->GetPos());
            auto nextscrn = std::make_unique<T_Window>(gwv, gcFactory, static_cast<T_Building*>(bld));
            nextscrn->SetPos(GetPos());
            WINDOWMANAGER.ReplaceWindow(std::move(nextscrn));
            return;
        }
    }
}
