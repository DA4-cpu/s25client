// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwBuildingSite.h"
#include "Loader.h"
#include "WindowManager.h"
#include "buildings/noBuildingSite.h"
#include "iwDemolishBuilding.h"
#include "iwHelp.h"
#include "ogl/FontStyle.h"
#include "ogl/glArchivItem_Bitmap.h"
#include "world/GameWorldView.h"
#include "world/MapBase.h"
#include "gameData/BuildingConsts.h"
#include "gameData/const_gui_ids.h"
#include <algorithm>

namespace {
/// Construction materials in a row of the window at most
constexpr unsigned WARES_PER_ROW = 8;
/// Additional height for more than 2 rows of construction materials
unsigned getExtraHeight(const noBuildingSite& buildingsite)
{
    unsigned numRows = 0;
    for(const CostEntry& entry : BUILDING_COSTS[buildingsite.GetBuildingType()])
        numRows += (entry.amount + WARES_PER_ROW - 1) / WARES_PER_ROW;
    return numRows > 2 ? (numRows - 2) * 29 : 0;
}
} // namespace

iwBuildingSite::iwBuildingSite(GameWorldView& gwv, const noBuildingSite* const buildingsite)
    : IngameWindow(CGI_BUILDING + MapBase::CreateGUIID(buildingsite->GetPos()), IngameWindow::posAtMouse,
                   Extent(226, 194 + getExtraHeight(*buildingsite)), _(BUILDING_NAMES[buildingsite->GetBuildingType()]),
                   LOADER.GetImageN("resource", 41)),
      gwv(gwv), buildingsite(buildingsite)
{
    const int extra = static_cast<int>(getExtraHeight(*buildingsite));
    // Bild des Gebäudes
    AddImage(0, DrawPoint(113, 130 + extra), &buildingsite->GetBuildingImage());
    // Gebäudename
    AddText(1, DrawPoint(113, 44), _("Order of building site"), COLOR_YELLOW, FontStyle::CENTER, NormalFont);

    // Hilfe
    AddImageButton(2, DrawPoint(16, 147 + extra), Extent(30, 32), TextureColor::Grey, LOADER.GetImageN("io", 225),
                   _("Help"));
    // Gebäude abbrennen
    AddImageButton(3, DrawPoint(50, 147 + extra), Extent(34, 32), TextureColor::Grey, LOADER.GetImageN("io", 23),
                   _("Demolish house"));

    // "Gehe Zu Ort"
    AddImageButton(4, DrawPoint(179, 147 + extra), Extent(30, 32), TextureColor::Grey, LOADER.GetImageN("io", 107),
                   _("Go to place"));
}

void iwBuildingSite::Msg_ButtonClick(const unsigned ctrl_id)
{
    switch(ctrl_id)
    {
        case 2: // Hilfe
        {
            WINDOWMANAGER.ReplaceWindow(
              std::make_unique<iwHelp>(_(BUILDING_HELP_STRINGS[buildingsite->GetBuildingType()])));
        }
        break;
        case 3: // Gebäude abbrennen
        {
            // Abreißen?
            Close();
            WINDOWMANAGER.Show(std::make_unique<iwDemolishBuilding>(gwv, buildingsite));
        }
        break;
        case 4: // "Gehe Zu Ort"
        {
            gwv.MoveToMapPt(buildingsite->GetPos());
        }
        break;
    }
}

void iwBuildingSite::Msg_PaintAfter()
{
    IngameWindow::Msg_PaintAfter();
    // Baukosten zeichnen: one row per material (more for many wares), used ones with a hammer
    DrawPoint curPos = GetDrawPos() + DrawPoint(GetSize().x / 2, 60);
    const BuildingCosts& costs = BUILDING_COSTS[buildingsite->GetBuildingType()];
    const auto& materials = buildingsite->GetMaterials();
    for(unsigned entry = 0; entry < costs.size() && entry < materials.size(); ++entry)
    {
        const noBuildingSite::Material& material = materials[entry];
        // Icons: used (with hammer), lying here, missing (dark)
        std::vector<GoodType> icons(material.used, costs[entry].wares.front());
        icons.insert(icons.end(), material.delivered.begin(), material.delivered.end());
        const unsigned numDelivered = static_cast<unsigned>(icons.size());
        while(icons.size() < costs[entry].amount)
            icons.push_back(costs[entry].wares.front());

        for(unsigned rowStart = 0; rowStart < icons.size(); rowStart += WARES_PER_ROW)
        {
            const unsigned numInRow = std::min<unsigned>(WARES_PER_ROW, static_cast<unsigned>(icons.size()) - rowStart);
            // "Schwarzer Rahmen"
            DrawPoint waresPos = curPos - DrawPoint(24 * numInRow / 2, 0);
            DrawRectangle(Rect(waresPos, Extent(24 * numInRow, 24)), 0x80000000);
            waresPos += DrawPoint(12, 12);

            // Die Waren
            for(unsigned z = rowStart; z < rowStart + numInRow; ++z)
            {
                LOADER.GetWareTex(icons[z])->DrawFull(waresPos, (z < numDelivered ? 0xFFFFFFFF : 0xFF404040));

                // Hammer wenn Ware verbaut
                if(z < material.used)
                    LOADER.GetWareTex(GoodType::Hammer)->DrawFull(waresPos);
                waresPos.x += 24;
            }
            curPos.y += 29;
        }
    }
}
