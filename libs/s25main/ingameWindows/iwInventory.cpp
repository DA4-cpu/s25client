// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwInventory.h"
#include "Cheats.h"
#include "GameInterface.h"
#include "GamePlayer.h"
#include "Loader.h"
#include "controls/ctrlComboBox.h"
#include "world/GameWorld.h"
#include "gameData/const_gui_ids.h"

namespace {
enum
{
    // From iwWares
    ID_Paginate = 0,
    ID_Help = 12,
    // New: debug-only player selector
    ID_CbDebugPlayer
};
} // namespace

iwInventory::iwInventory(const GamePlayer& player)
    : iwWares(CGI_INVENTORY, IngameWindow::posLastOrCenter, 34, _("Stock"), false, SmallFont, player.GetInventory(),
              player),
      homePlayerId_(player.GetPlayerId())
{
    const GameWorld& world = player.GetGameWorld();

    // Debug feature: lets you browse the stock of any player (including AI players). Hidden unless cheat/debug mode
    // is active, see Msg_PaintBefore.
    ctrlComboBox* cb = AddComboBox(ID_CbDebugPlayer, DrawPoint(16, GetFullSize().y - 81), Extent(135, 20),
                                   TextureColor::Grey, SmallFont, 100);
    cb->SetVisible(false);

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

void iwInventory::Msg_ComboSelectItem(const unsigned ctrl_id, const unsigned selection)
{
    if(ctrl_id == ID_CbDebugPlayer && selection < selectablePlayerIds_.size())
        ShowPlayer(selectablePlayerIds_[selection]);
}

void iwInventory::ShowPlayer(const unsigned playerId)
{
    const GameWorld& world = player->GetGameWorld();
    player = &world.GetPlayer(playerId);
    inventory = &player->GetInventory();
}

void iwInventory::Msg_PaintBefore()
{
    iwWares::Msg_PaintBefore();

    // The player selector is a debug feature and only usable while cheat mode is on (consistent with all other
    // debug/cheat visualizations, e.g. the enemy productivity overlay).
    const GameInterface* gi = player->GetGameWorld().GetGameInterface();
    const bool debugModeOn = gi && gi->GI_GetCheats().isCheatModeOn();

    auto* cb = GetCtrl<ctrlComboBox>(ID_CbDebugPlayer);
    if(cb && cb->IsVisible() != debugModeOn)
    {
        cb->SetVisible(debugModeOn);

        // Cheat mode got turned off while we were looking at another player's stock -> jump back to our own
        if(!debugModeOn && player->GetPlayerId() != homePlayerId_)
        {
            ShowPlayer(homePlayerId_);
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
}
