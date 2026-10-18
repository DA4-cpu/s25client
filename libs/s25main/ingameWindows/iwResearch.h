// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "IngameWindow.h"
#include "civ/TechTree.h"
#include <optional>
#include <string>

class GameCommandFactory;
class GameWorldViewer;

/// Civilization mode: Shows the tech tree and lets the player choose the technology to research
class iwResearch : public IngameWindow
{
public:
    iwResearch(const GameWorldViewer& gwv, GameCommandFactory& gcFactory);

private:
    const GameWorldViewer& gwv;
    GameCommandFactory& gcFactory;
    /// Percentage of the costs of the current research which are already paid (displayed by a ctrlPercent)
    unsigned short currentProgress_;
    /// Signature of the research state of all players to detect when the table needs to be refreshed
    std::string lastStateSignature_;

    void Msg_PaintBefore() override;
    void Msg_ButtonClick(unsigned ctrl_id) override;
    void Msg_TableSelectItem(unsigned ctrl_id, const std::optional<unsigned>& selection) override;

    /// Recreate all rows of the table (keeps selection)
    void RefreshTable();
    /// Update the area of the current research
    void RefreshCurrentResearch();
    /// Update the description and buttons for the selected technology
    void RefreshSelection();
    std::optional<civ::TechIdx> GetSelectedTech() const;
    std::string GetStateSignature() const;
};
