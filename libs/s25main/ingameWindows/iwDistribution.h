// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TransmitSettingsIgwAdapter.h"
#include "gameTypes/SettingsTypes.h"
#include <array>
#include <cstdint>
#include <vector>

class GameCommandFactory;
class GameWorldViewer;

class iwDistribution final : public TransmitSettingsIgwAdapter
{
    struct DistributionGroup
    {
        DistributionGroup(std::string name, glArchivItem_Bitmap* img) : name(std::move(name)), img(img) {}
        std::string name;
        glArchivItem_Bitmap* img;
        std::vector<std::tuple<std::string, unsigned>> entries;
    };

public:
    iwDistribution(const GameWorldViewer& gwv, GameCommandFactory& gcFactory);

private:
    const GameWorldViewer& gwv;
    GameCommandFactory& gcFactory;

    /// Updatet die Steuerelemente mit den übergebenen Einstellungen
    void UpdateSettings(const Distributions& distribution);
    void UpdateSettings() override;
    /// Sendet veränderte Einstellungen (an den Client), falls sie verändert wurden
    void TransmitSettings() override;

    void Msg_Group_ProgressChange(unsigned group_id, unsigned ctrl_id, unsigned short position) override;
    void Msg_ButtonClick(unsigned ctrl_id) override;

    /// Groups for the settings
    std::vector<DistributionGroup> groups;
    /// Initialize the groups structure
    void CreateGroups();

    /// Civilization mode: Tab "Wissen" (regulator for the knowledge type of media) after the groups
    bool hasKnowledgeTab = false;
    bool knowledgeChanged = false;
    unsigned GetKnowledgeTabId() const { return static_cast<unsigned>(groups.size()); }
    void SetKnowledgeValues(const std::array<uint8_t, 3>& values);
};
