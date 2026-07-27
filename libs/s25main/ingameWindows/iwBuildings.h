// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "IngameWindow.h"
#include "gameTypes/BuildingType.h"
#include <list>
#include <vector>

class GameCommandFactory;
class GamePlayer;
class GameWorldView;

/// Fenster, welches die Anzahl aller Gebäude und der Baustellen auflistet
class iwBuildings : public IngameWindow
{
    GameWorldView& gwv;
    GameCommandFactory& gcFactory;

public:
    iwBuildings(GameWorldView& gwv, GameCommandFactory& gcFactory);

private:
    /// Anzahlen der Gebäude zeichnen
    void Msg_PaintAfter() override;

    void Msg_ButtonClick(unsigned ctrl_id) override;
    void Msg_ComboSelectItem(unsigned ctrl_id, unsigned selection) override;
    void Msg_PaintBefore() override;

    template<class T_Window, class T_Building>
    void GoToFirstMatching(BuildingType bldType, const std::list<T_Building*>& blds);

    void setBuildingOrder();
    /// The player whose building counts are currently displayed (debug feature: can differ from the local player)
    const GamePlayer& GetViewedPlayer() const;

    std::vector<BuildingType> bts;

    /// ID of the local (home) player, i.e. whose buildings are shown when not browsing other players in debug mode
    const unsigned homePlayerId_;
    /// Currently displayed player, see GetViewedPlayer()
    unsigned viewedPlayerId_;
    /// Nation the building icons were last drawn for (refreshed if it changes, e.g. when browsing another player)
    Nation lastDrawnNation_;
    /// Player IDs corresponding to the entries of the debug player combo box, in the same order
    std::vector<unsigned> selectablePlayerIds_;
};
