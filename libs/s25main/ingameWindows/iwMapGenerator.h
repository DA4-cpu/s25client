// Copyright (C) 2005 - 2022 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "IngameWindow.h"
#include "mapGenerator/MapSettings.h"

/// Window to configure parameters used for random map generation
class iwMapGenerator : public IngameWindow
{
public:
    enum
    {
        ID_btBack,
        ID_btApply,
        ID_cbNumPlayers,
        ID_txtMapSize,
        ID_txtMapSizeX,
        ID_cbMapSizeX,
        ID_cbMapSizeY,
        ID_txtSeed,
        ID_edtSeed,
        ID_btRandomSeed,
        ID_txtLastSeed,
        ID_txtLandMass,
        ID_cbLandMass,
        ID_txtWater,
        ID_edtWater,
        ID_txtMountains,
        ID_edtMountains,
        ID_txtRoughness,
        ID_cbRoughness,
        ID_txtRivers,
        ID_pgRivers,
        ID_txtMountainDist,
        ID_cbMountainDist,
        ID_txtClimateTemperature,
        ID_cbClimateTemperature,
        ID_txtClimateHumidity,
        ID_cbClimateHumidity,
        ID_txtClimateZoneSize,
        ID_pgClimateZoneSize,
        ID_txtTrees,
        ID_edtTrees,
        ID_txtStonePiles,
        ID_edtStonePiles,
        ID_txtResourceAmount,
        ID_edtResourceAmount,
        ID_txtGold,
        ID_pgGoldRatio,
        ID_txtIron,
        ID_pgIronRatio,
        ID_txtCoal,
        ID_pgCoalRatio,
        ID_txtGranite,
        ID_pgGraniteRatio
    };

    /**
     * Creates a new ingame window to configure the random map generator.
     * @param settings reference to the settings to be manipulated
     * @param lastSeed seed of the last generated map (shown for reproducing it), 0 = none
     */
    iwMapGenerator(rttr::mapGenerator::MapSettings& settings, uint32_t lastSeed = 0);

    void Msg_ButtonClick(unsigned ctrl_id) override;

private:
    rttr::mapGenerator::MapSettings& mapSettings;

    /// Reset the UI to the values of @ref mapSettings
    void Reset();
    /// Updates @ref mapSettings with the values currently configured in the UI.
    void Apply();
};
