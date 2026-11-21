// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "IngameWindow.h"
#include "gameTypes/GoodTypes.h"
#include <string>
#include <vector>

class GameCommandFactory;
class GameWorldViewer;

/// Civilization mode: window of the distribution of all wares (Gesamtverteilung). Like the tool window, for all wares:
/// one row per ware under headings by kind. Per row: the value 0..1000 (set by itself, or by the player: moving the bar
/// makes it manual), value automatic or manual, orders, stop automatic (surplus) or by the player. On top: values from
/// the forecast or from the number of wares
class iwWareDistribution final : public IngameWindow
{
public:
    iwWareDistribution(const GameWorldViewer& gwv, GameCommandFactory& gcFactory);

private:
    struct Row
    {
        /// Heading: no ware
        bool heading = false;
        std::string title;
        GoodType good = GoodType::Nothing;
    };

    const GameWorldViewer& gwv;
    GameCommandFactory& gcFactory;
    std::vector<Row> rows_;
    unsigned page_ = 0;
    unsigned lastUpdate_ = 0;
    bool isReplay_;

    unsigned GetNumPages() const;
    /// Show the rows of the current page with the values of the player
    void UpdateRows();
    /// Ware of the row shown at `line` of the page (Nothing: heading or empty)
    GoodType GetWareAt(unsigned line) const;
    /// Send the setting of the ware
    void SendSetting(GoodType good, bool autoValue, uint16_t manualValue, bool autoPause, bool paused);

    void Msg_ButtonClick(unsigned ctrl_id) override;
    void Msg_ProgressChange(unsigned ctrl_id, unsigned short position) override;
    void Msg_PaintBefore() override;
};
