// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "IngameWindow.h"
#include "civ/Knowledge.h"
#include "civ/TechTree.h"
#include <optional>
#include <string>
#include <vector>

class GameCommandFactory;
class GameWorldViewer;
class GamePlayer;

/// Civilization mode: Shows the tech tree and lets the player choose the technology to research
class iwResearch : public IngameWindow
{
public:
    iwResearch(const GameWorldViewer& gwv, GameCommandFactory& gcFactory);

private:
    const GameWorldViewer& gwv;
    GameCommandFactory& gcFactory;
    /// Player who opened the window (only this one can be given commands)
    const unsigned homePlayerId_;
    /// Player whose research is shown. Other players can be selected in cheat/debug mode
    unsigned viewedPlayerId_;
    /// Player IDs corresponding to the entries of the debug player combo box, in the same order
    std::vector<unsigned> selectablePlayerIds_;
    /// Percentage of the costs of the current research which are already paid (displayed by a ctrlPercent)
    unsigned short currentProgress_;
    /// Signature of the research state of all players to detect when the table needs to be refreshed
    std::string lastStateSignature_;
    /// Total and available knowledge (tenths) as last displayed
    civ::KnowledgePoints lastTotalKnowledge_{}, lastAvailableKnowledge_{};
    bool knowledgeShown_ = false;

    void Msg_PaintBefore() override;
    void Msg_ButtonClick(unsigned ctrl_id) override;
    void Msg_TableSelectItem(unsigned ctrl_id, const std::optional<unsigned>& selection) override;
    void Msg_ComboSelectItem(unsigned ctrl_id, unsigned selection) override;

    const GamePlayer& GetViewedPlayer() const;
    bool IsViewingOwnPlayer() const { return viewedPlayerId_ == homePlayerId_; }
    /// Show the research of another player (debug feature)
    void ShowPlayer(unsigned playerId);

    /// Recreate all rows of the table (keeps selection)
    void RefreshTable();
    /// Update the area of the current research
    void RefreshCurrentResearch();
    /// Update the display of the knowledge points. Returns true if they changed since the last call
    bool RefreshKnowledge();
    /// Update the description and buttons for the selected technology
    void RefreshSelection();
    std::optional<civ::TechIdx> GetSelectedTech() const;
    std::string GetStateSignature() const;
};
