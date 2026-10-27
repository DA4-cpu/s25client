// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwMapGenerator.h"
#include "Loader.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlProgress.h"
#include "controls/ctrlText.h"
#include "helpers/containerUtils.h"
#include "helpers/format.hpp"
#include "lua/GameDataLoader.h"
#include "gameData/MaxPlayers.h"
#include "gameData/WorldDescription.h"
#include "gameData/const_gui_ids.h"
#include "s25util/StringConversion.h"
#include "s25util/colors.h"
#include <random>
#include <string>

using namespace rttr::mapGenerator;

namespace {
// Range of the "climate zone size" slider. Kept in sync with the clamping done in
// MapSettings::MakeValid(); the slider itself (via ctrlProgress) only ever produces values in
// [0, climateZoneSizeMax - climateZoneSizeMin], which are offset by climateZoneSizeMin below.
constexpr unsigned short climateZoneSizeMin = 10;
constexpr unsigned short climateZoneSizeMax = 150;

// Layout (namespace constants: MSVC does not allow local constexpr in lambdas without capture)
constexpr int col1 = 20;
constexpr int col2 = 280;
constexpr int ctrlOffset = 120;
constexpr int rowHeight = 30;
constexpr Extent ctrlSize(130, 20);
constexpr Extent editSize(60, 20);
constexpr Extent buttonSize(100, 20);

/// Read a number from an edit field, limited to maxValue
template<typename T>
T GetNumber(const ctrlEdit& edit, T maxValue)
{
    const std::string text = edit.GetText();
    unsigned long long value = 0;
    for(const char c : text)
    {
        if(c < '0' || c > '9')
            break;
        value = value * 10 + static_cast<unsigned>(c - '0');
        if(value > maxValue)
            return maxValue;
    }
    return static_cast<T>(value);
}
} // namespace

iwMapGenerator::iwMapGenerator(MapSettings& settings, uint32_t lastSeed)
    : IngameWindow(CGI_MAP_GENERATOR, IngameWindow::posLastOrCenter, Extent(540, 430), _("Map Generator"),
                   LOADER.GetImageN("resource", 41), true, CloseBehavior::Custom),
      mapSettings(settings)
{
    WorldDescription desc;
    GameDataLoader gdLoader(desc);
    if(!gdLoader.Load())
    {
        Close();
        return;
    }

    // The generator always targets the combined "world" landscape, which draws on the terrains of all landscapes
    // via the climate (humidity, temperature). Greenland/wasteland/winterworld stay for existing maps.
    const DescIdx<LandscapeDesc> worldLandscape = desc.landscapes.getIndex("world");
    if(!worldLandscape)
    {
        Close();
        return;
    }
    mapSettings.type = worldLandscape;


    const auto addLabel = [this](unsigned id, int x, int y, const std::string& text) {
        AddText(id, DrawPoint(x, y + 3), text, COLOR_YELLOW, FontStyle{}, NormalFont);
    };
    const auto addNumberEdit = [this](unsigned id, int x, int y, unsigned short maxLength) {
        ctrlEdit* edit = AddEdit(id, DrawPoint(x, y), editSize, TextureColor::Grey, NormalFont, maxLength);
        edit->SetNumberOnly(true);
        return edit;
    };

    // ---- Left column: shape of the map ----
    int y = 30;
    ctrlComboBox* combo =
      AddComboBox(ID_cbNumPlayers, DrawPoint(col1, y), Extent(ctrlOffset + ctrlSize.x, 20), TextureColor::Grey,
                  NormalFont, 100);
    for(unsigned n = 2; n <= MAX_PLAYERS; n++)
        combo->AddItem(helpers::format(_("%1% players"), n));

    y += rowHeight;
    addLabel(ID_txtMapSize, col1, y, _("Size"));
    auto* cbSizeX =
      AddComboBox(ID_cbMapSizeX, DrawPoint(col1 + ctrlOffset, y), Extent(55, 20), TextureColor::Grey, NormalFont, 200);
    AddText(ID_txtMapSizeX, DrawPoint(col1 + ctrlOffset + 60, y + 10), "x", COLOR_YELLOW, FontStyle::VCENTER,
            NormalFont);
    auto* cbSizeY = AddComboBox(ID_cbMapSizeY, DrawPoint(col1 + ctrlOffset + 75, y), Extent(55, 20),
                                TextureColor::Grey, NormalFont, 200);
    for(unsigned size = 32; size <= 512; size += 32)
    {
        const auto strSize = s25util::toStringClassic(size);
        cbSizeX->AddItem(strSize);
        cbSizeY->AddItem(strSize);
    }

    y += rowHeight;
    addLabel(ID_txtSeed, col1, y, _("Zufallswert"));
    AddEdit(ID_edtSeed, DrawPoint(col1 + ctrlOffset, y), Extent(80, 20), TextureColor::Grey, NormalFont, 10)
      ->SetNumberOnly(true);
    AddTextButton(ID_btRandomSeed, DrawPoint(col1 + ctrlOffset + 85, y), Extent(45, 20), TextureColor::Grey,
                  _("Neu"), NormalFont, _("Leer = bei jeder Karte ein neuer Zufallswert"));

    y += rowHeight;
    addLabel(ID_txtLandMass, col1, y, _("Landmasse"));
    combo = AddComboBox(ID_cbLandMass, DrawPoint(col1 + ctrlOffset, y), ctrlSize, TextureColor::Grey, NormalFont, 100);
    combo->AddItem(_("Inseln"));
    combo->AddItem(_("Normal"));
    combo->AddItem(_("Kontinente"));

    y += rowHeight;
    addLabel(ID_txtWater, col1, y, _("Wasser (in %)"));
    addNumberEdit(ID_edtWater, col1 + ctrlOffset, y, 2);

    y += rowHeight;
    addLabel(ID_txtMountains, col1, y, _("Gebirge (in %)"));
    addNumberEdit(ID_edtMountains, col1 + ctrlOffset, y, 2);

    y += rowHeight;
    addLabel(ID_txtRoughness, col1, y, _("Oberfläche"));
    combo =
      AddComboBox(ID_cbRoughness, DrawPoint(col1 + ctrlOffset, y), ctrlSize, TextureColor::Grey, NormalFont, 100);
    combo->AddItem(_("Glatt"));
    combo->AddItem(_("Normal"));
    combo->AddItem(_("Rau"));

    y += rowHeight;
    addLabel(ID_txtUnevenness, col1, y, _("Unebenheit (in %)"));
    addNumberEdit(ID_edtUnevenness, col1 + ctrlOffset, y, 3);

    y += rowHeight;
    addLabel(ID_txtRivers, col1, y, _("Rivers:"));
    AddProgress(ID_pgRivers, DrawPoint(col1 + ctrlOffset, y), ctrlSize, TextureColor::Grey, 139, 138, 100);

    y += rowHeight;
    addLabel(ID_txtMountainDist, col1, y, _("HQ-Abstand Berg"));
    combo =
      AddComboBox(ID_cbMountainDist, DrawPoint(col1 + ctrlOffset, y), ctrlSize, TextureColor::Grey, NormalFont, 100);
    combo->AddItem(_("Close"));
    combo->AddItem(_("Normal"));
    combo->AddItem(_("Far"));
    combo->AddItem(_("Very far"));

    y += rowHeight;
    AddText(ID_txtLastSeed, DrawPoint(col1, y + 3),
            lastSeed ? helpers::format(_("Letzte Karte: Zufallswert %1%"), lastSeed) : std::string(), COLOR_GREEN,
            FontStyle{}, NormalFont);

    // ---- Right column: climate and resources ----
    y = 30;
    addLabel(ID_txtClimateTemperature, col2, y, _("Temperature"));
    combo = AddComboBox(ID_cbClimateTemperature, DrawPoint(col2 + ctrlOffset, y), ctrlSize, TextureColor::Grey,
                        NormalFont, 100);
    combo->AddItem(_("Kalt"));
    combo->AddItem(_("Kühl"));
    combo->AddItem(_("Gemäßigt"));
    combo->AddItem(_("Warm"));
    combo->AddItem(_("Heiß"));

    y += rowHeight;
    addLabel(ID_txtClimateHumidity, col2, y, _("Humidity"));
    combo = AddComboBox(ID_cbClimateHumidity, DrawPoint(col2 + ctrlOffset, y), ctrlSize, TextureColor::Grey,
                        NormalFont, 100);
    combo->AddItem(_("Feucht"));
    combo->AddItem(_("Gemäßigt"));
    combo->AddItem(_("Trocken"));

    y += rowHeight;
    addLabel(ID_txtClimateZoneSize, col2, y, _("Klimagebiete"));
    AddProgress(ID_pgClimateZoneSize, DrawPoint(col2 + ctrlOffset, y), ctrlSize, TextureColor::Grey, 139, 138,
                climateZoneSizeMax - climateZoneSizeMin);

    y += rowHeight;
    addLabel(ID_txtTrees, col2, y, _("Wald (in %)"));
    addNumberEdit(ID_edtTrees, col2 + ctrlOffset, y, 3);

    y += rowHeight;
    addLabel(ID_txtStonePiles, col2, y, _("Steinhaufen (in %)"));
    addNumberEdit(ID_edtStonePiles, col2 + ctrlOffset, y, 3);

    y += rowHeight;
    addLabel(ID_txtResourceAmount, col2, y, _("Ressourcen (in %)"));
    addNumberEdit(ID_edtResourceAmount, col2 + ctrlOffset, y, 3);

    y += rowHeight;
    addLabel(ID_txtGold, col2, y, _("Gold:"));
    AddProgress(ID_pgGoldRatio, DrawPoint(col2 + ctrlOffset, y), ctrlSize, TextureColor::Grey, 139, 138, 100);
    y += rowHeight;
    addLabel(ID_txtIron, col2, y, _("Iron:"));
    AddProgress(ID_pgIronRatio, DrawPoint(col2 + ctrlOffset, y), ctrlSize, TextureColor::Grey, 139, 138, 100);
    y += rowHeight;
    addLabel(ID_txtCoal, col2, y, _("Coal:"));
    AddProgress(ID_pgCoalRatio, DrawPoint(col2 + ctrlOffset, y), ctrlSize, TextureColor::Grey, 139, 138, 100);
    y += rowHeight;
    addLabel(ID_txtGranite, col2, y, _("Granite:"));
    AddProgress(ID_pgGraniteRatio, DrawPoint(col2 + ctrlOffset, y), ctrlSize, TextureColor::Grey, 139, 138, 100);

    y = 30 + 11 * rowHeight + 5;
    AddTextButton(ID_btBack, DrawPoint(col1 + 130, y), buttonSize, TextureColor::Red1, _("Back"), NormalFont);
    AddTextButton(ID_btApply, DrawPoint(col2 + 10, y), buttonSize, TextureColor::Green2, _("Apply"), NormalFont);

    Reset();
}

void iwMapGenerator::Msg_ButtonClick(const unsigned ctrl_id)
{
    switch(ctrl_id)
    {
        case ID_btBack: Close(); break;
        case ID_btApply:
            Apply();
            Close();
            break;
        case ID_btRandomSeed:
        {
            // Not game relevant, so a non deterministic random number is fine
            uint32_t seed = 0;
            while(seed == 0)
                seed = std::random_device{}() % 1000000000u;
            GetCtrl<ctrlEdit>(ID_edtSeed)->SetText(s25util::toStringClassic(seed));
            break;
        }
    }
}

void iwMapGenerator::Apply()
{
    mapSettings.numPlayers = GetCtrl<ctrlComboBox>(ID_cbNumPlayers)->GetSelection().value() + 2;
    mapSettings.ratioGold = GetCtrl<ctrlProgress>(ID_pgGoldRatio)->GetPosition();
    mapSettings.ratioIron = GetCtrl<ctrlProgress>(ID_pgIronRatio)->GetPosition();
    mapSettings.ratioCoal = GetCtrl<ctrlProgress>(ID_pgCoalRatio)->GetPosition();
    mapSettings.ratioGranite = GetCtrl<ctrlProgress>(ID_pgGraniteRatio)->GetPosition();
    mapSettings.rivers = GetCtrl<ctrlProgress>(ID_pgRivers)->GetPosition();
    mapSettings.seed = GetNumber<uint32_t>(*GetCtrl<ctrlEdit>(ID_edtSeed), 4000000000u);
    mapSettings.water = GetNumber<unsigned short>(*GetCtrl<ctrlEdit>(ID_edtWater), 90);
    mapSettings.mountains = GetNumber<unsigned short>(*GetCtrl<ctrlEdit>(ID_edtMountains), 90);
    mapSettings.trees = GetNumber<unsigned short>(*GetCtrl<ctrlEdit>(ID_edtTrees), 100);
    mapSettings.stonePiles = GetNumber<unsigned short>(*GetCtrl<ctrlEdit>(ID_edtStonePiles), 100);
    mapSettings.resourceAmount = GetNumber<unsigned short>(*GetCtrl<ctrlEdit>(ID_edtResourceAmount), 300);
    mapSettings.unevenness = GetNumber<unsigned short>(*GetCtrl<ctrlEdit>(ID_edtUnevenness), 100);

    switch(GetCtrl<ctrlComboBox>(ID_cbMountainDist)->GetSelection().value())
    {
        case 0: mapSettings.mountainDistance = MountainDistance::Close; break;
        case 1: mapSettings.mountainDistance = MountainDistance::Normal; break;
        case 2: mapSettings.mountainDistance = MountainDistance::Far; break;
        case 3: mapSettings.mountainDistance = MountainDistance::VeryFar; break;
    }
    switch(GetCtrl<ctrlComboBox>(ID_cbLandMass)->GetSelection().value())
    {
        case 0: mapSettings.landMass = LandMass::Islands; break;
        case 1: mapSettings.landMass = LandMass::Normal; break;
        case 2: mapSettings.landMass = LandMass::Continents; break;
    }
    switch(GetCtrl<ctrlComboBox>(ID_cbRoughness)->GetSelection().value())
    {
        case 0: mapSettings.roughness = Roughness::Smooth; break;
        case 1: mapSettings.roughness = Roughness::Normal; break;
        case 2: mapSettings.roughness = Roughness::Rough; break;
    }
    mapSettings.size.x =
      s25util::fromStringClassic<unsigned>(GetCtrl<ctrlComboBox>(ID_cbMapSizeX)->GetSelectedText().value_or("128"));
    mapSettings.size.y =
      s25util::fromStringClassic<unsigned>(GetCtrl<ctrlComboBox>(ID_cbMapSizeY)->GetSelectedText().value_or("128"));
    switch(GetCtrl<ctrlComboBox>(ID_cbClimateTemperature)->GetSelection().value())
    {
        case 0: mapSettings.climateTemperature = ClimateTemperature::Cold; break;
        case 1: mapSettings.climateTemperature = ClimateTemperature::Cool; break;
        case 2: mapSettings.climateTemperature = ClimateTemperature::Temperate; break;
        case 3: mapSettings.climateTemperature = ClimateTemperature::Warm; break;
        case 4: mapSettings.climateTemperature = ClimateTemperature::Hot; break;
    }
    switch(GetCtrl<ctrlComboBox>(ID_cbClimateHumidity)->GetSelection().value())
    {
        case 0: mapSettings.climateHumidity = ClimateHumidity::Humid; break;
        case 1: mapSettings.climateHumidity = ClimateHumidity::Temperate; break;
        case 2: mapSettings.climateHumidity = ClimateHumidity::Dry; break;
    }
    mapSettings.climateZoneSize = static_cast<unsigned short>(
      GetCtrl<ctrlProgress>(ID_pgClimateZoneSize)->GetPosition() + climateZoneSizeMin);
}

void iwMapGenerator::Reset()
{
    GetCtrl<ctrlComboBox>(ID_cbNumPlayers)
      ->SetSelection(std::min(mapSettings.numPlayers, MAX_PLAYERS) - 2); // List starts at 2 players

    GetCtrl<ctrlProgress>(ID_pgGoldRatio)->SetPosition(mapSettings.ratioGold);
    GetCtrl<ctrlProgress>(ID_pgIronRatio)->SetPosition(mapSettings.ratioIron);
    GetCtrl<ctrlProgress>(ID_pgCoalRatio)->SetPosition(mapSettings.ratioCoal);
    GetCtrl<ctrlProgress>(ID_pgGraniteRatio)->SetPosition(mapSettings.ratioGranite);
    GetCtrl<ctrlProgress>(ID_pgRivers)->SetPosition(mapSettings.rivers);

    GetCtrl<ctrlEdit>(ID_edtSeed)->SetText(mapSettings.seed ? s25util::toStringClassic(mapSettings.seed) : "");
    GetCtrl<ctrlEdit>(ID_edtWater)->SetText(mapSettings.water);
    GetCtrl<ctrlEdit>(ID_edtMountains)->SetText(mapSettings.mountains);
    GetCtrl<ctrlEdit>(ID_edtTrees)->SetText(mapSettings.trees);
    GetCtrl<ctrlEdit>(ID_edtStonePiles)->SetText(mapSettings.stonePiles);
    GetCtrl<ctrlEdit>(ID_edtResourceAmount)->SetText(mapSettings.resourceAmount);
    GetCtrl<ctrlEdit>(ID_edtUnevenness)->SetText(mapSettings.unevenness);

    auto* combo = GetCtrl<ctrlComboBox>(ID_cbMountainDist);
    switch(mapSettings.mountainDistance)
    {
        case MountainDistance::Close: combo->SetSelection(0); break;
        case MountainDistance::Normal: combo->SetSelection(1); break;
        case MountainDistance::Far: combo->SetSelection(2); break;
        case MountainDistance::VeryFar: combo->SetSelection(3); break;
    }

    combo = GetCtrl<ctrlComboBox>(ID_cbLandMass);
    switch(mapSettings.landMass)
    {
        case LandMass::Islands: combo->SetSelection(0); break;
        case LandMass::Normal: combo->SetSelection(1); break;
        case LandMass::Continents: combo->SetSelection(2); break;
    }

    combo = GetCtrl<ctrlComboBox>(ID_cbRoughness);
    switch(mapSettings.roughness)
    {
        case Roughness::Smooth: combo->SetSelection(0); break;
        case Roughness::Normal: combo->SetSelection(1); break;
        case Roughness::Rough: combo->SetSelection(2); break;
    }

    const auto strSizeX = s25util::toStringClassic(mapSettings.size.x);
    const auto strSizeY = s25util::toStringClassic(mapSettings.size.y);
    auto* cbSizeX = GetCtrl<ctrlComboBox>(ID_cbMapSizeX);
    auto* cbSizeY = GetCtrl<ctrlComboBox>(ID_cbMapSizeY);
    const auto numSizes = cbSizeX->GetNumItems();
    RTTR_Assert(numSizes == cbSizeY->GetNumItems());
    for(unsigned i = 0; i < numSizes; ++i)
    {
        if(cbSizeX->GetText(i) == strSizeX)
            cbSizeX->SetSelection(i);
        if(cbSizeY->GetText(i) == strSizeY)
            cbSizeY->SetSelection(i);
    }

    combo = GetCtrl<ctrlComboBox>(ID_cbClimateTemperature);
    switch(mapSettings.climateTemperature)
    {
        case ClimateTemperature::Cold: combo->SetSelection(0); break;
        case ClimateTemperature::Cool: combo->SetSelection(1); break;
        case ClimateTemperature::Temperate: combo->SetSelection(2); break;
        case ClimateTemperature::Warm: combo->SetSelection(3); break;
        case ClimateTemperature::Hot: combo->SetSelection(4); break;
    }

    combo = GetCtrl<ctrlComboBox>(ID_cbClimateHumidity);
    switch(mapSettings.climateHumidity)
    {
        case ClimateHumidity::Humid: combo->SetSelection(0); break;
        case ClimateHumidity::Temperate: combo->SetSelection(1); break;
        case ClimateHumidity::Dry: combo->SetSelection(2); break;
    }

    GetCtrl<ctrlProgress>(ID_pgClimateZoneSize)
      ->SetPosition(static_cast<unsigned short>(mapSettings.climateZoneSize - climateZoneSizeMin));
}
