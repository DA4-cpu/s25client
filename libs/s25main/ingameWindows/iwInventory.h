// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "iwWares.h"
#include <vector>

class GamePlayer;

class iwInventory : public iwWares
{
public:
    explicit iwInventory(const GamePlayer& player);

private:
    void Msg_ComboSelectItem(unsigned ctrl_id, unsigned selection) override;
    void Msg_PaintBefore() override;

    /// Switch the currently displayed player. Used by the debug feature that allows browsing other players'
    /// (including AI players') stock.
    void ShowPlayer(unsigned playerId);

    /// ID of the player that originally opened this window. Used to jump back to "your own" stock once cheat mode
    /// is turned off again.
    const unsigned homePlayerId_;
    /// Player IDs corresponding to the entries of the debug player combo box, in the same order.
    std::vector<unsigned> selectablePlayerIds_;
};
