// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwWareDistribution.h"
#include "GamePlayer.h"
#include "Loader.h"
#include "WindowManager.h"
#include "civ/Content.h"
#include "civ/EconomyForecast.h"
#include "civ/WareDistribution.h"
#include "controls/ctrlBaseText.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlProgress.h"
#include "controls/ctrlText.h"
#include "controls/ctrlTextButton.h"
#include "controls/ctrlTextDeepening.h"
#include "drivers/VideoDriverWrapper.h"
#include "factories/GameCommandFactory.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "helpers/toString.h"
#include "iwHelp.h"
#include "network/GameClient.h"
#include "world/GameWorldBase.h"
#include "world/GameWorldViewer.h"
#include "gameData/BuildingProperties.h"
#include "gameData/GoodConsts.h"
#include "gameData/ToolConsts.h"
#include "gameData/const_gui_ids.h"
#include "s25util/colors.h"
#include <algorithm>
#include <map>

namespace {
constexpr unsigned ROWS_PER_PAGE = 12;
constexpr unsigned ROW_HEIGHT = 28;
constexpr unsigned FIRST_ROW_Y = 56;
/// Control ids: per line of the page ROW_IDS ids
constexpr unsigned ROW_BASE = 100;
constexpr unsigned ROW_IDS = 10;
enum RowCtrl : unsigned
{
    NameText,
    ValueBar,
    ValueMode,
    OrdersText,
    OrderPlus,
    OrderMinus,
    PauseMode
};
enum WindowCtrl : unsigned
{
    ModeButton = 1,
    HelpButton,
    PageText,
    PrevPage,
    NextPage,
    HeaderText
};
unsigned rowId(const unsigned line, const RowCtrl ctrl)
{
    return ROW_BASE + line * ROW_IDS + ctrl;
}

std::string wareTitle(const GoodType good)
{
    return _(WARE_NAMES[good]);
}

/// Heading of a ware: its tool kind, otherwise its kind (lua), otherwise "Sonstige"
std::string headingOf(const GoodType good)
{
    if(const auto kind = civ::getToolKindOf(good))
        return _(WARE_NAMES[TOOL_TO_GOOD[*kind]]);
    const civ::Content& content = civ::getActiveContent();
    const std::vector<std::string>& tags = content.GetTags(good);
    const std::vector<std::string>& properties = content.GetProperties(good);
    for(unsigned i = 1; i < tags.size(); ++i)
    {
        if(!helpers::contains(properties, tags[i]))
            return tags[i];
    }
    return _("Sonstige");
}
} // namespace

iwWareDistribution::iwWareDistribution(const GameWorldViewer& gwv, GameCommandFactory& gcFactory)
    : IngameWindow(CGI_TOOLS, IngameWindow::posLastOrCenter, Extent(520, FIRST_ROW_Y + ROWS_PER_PAGE * ROW_HEIGHT + 60),
                   _("Gesamtverteilung"), LOADER.GetImageN("io", 5)),
      gwv(gwv), gcFactory(gcFactory), isReplay_(GAMECLIENT.IsReplayModeOn())
{
    // Every ware a building of the game data makes, under headings
    std::map<std::string, std::vector<GoodType>> byHeading;
    std::vector<GoodType> seen;
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!BuildingProperties::IsUsual(bld))
            continue;
        for(const GoodType good : civ::getBuildingProducts(gwv.GetWorld(), bld).products)
        {
            if(!civ::isInDistribution(good) || helpers::contains(seen, good))
                continue;
            seen.push_back(good);
            byHeading[headingOf(good)].push_back(good);
        }
    }
    for(auto& [heading, wares] : byHeading)
    {
        rows_.push_back({true, heading, GoodType::Nothing});
        std::sort(wares.begin(), wares.end(),
                  [](const GoodType lhs, const GoodType rhs) { return wareTitle(lhs) < wareTitle(rhs); });
        for(const GoodType good : wares)
            rows_.push_back({false, wareTitle(good), good});
    }

    AddTextButton(ModeButton, DrawPoint(20, 24), Extent(260, 22), TextureColor::Grey, "", NormalFont,
                  _("Werte aus der Prognose oder aus den Warenzahlen (Lagerbestand + unterwegs - offen)"));
    AddText(HeaderText, DrawPoint(300, 28), _("Wert / Bestellt / Anhalten"), COLOR_YELLOW, FontStyle::LEFT, NormalFont);
    for(unsigned line = 0; line < ROWS_PER_PAGE; ++line)
    {
        const unsigned y = FIRST_ROW_Y + line * ROW_HEIGHT;
        AddText(rowId(line, NameText), DrawPoint(20, y + 6), "", COLOR_YELLOW, FontStyle::LEFT, NormalFont);
        ctrlProgress* bar = AddProgress(rowId(line, ValueBar), DrawPoint(150, y), Extent(170, 24), TextureColor::Grey,
                                        139, 138, civ::DISTRIBUTION_SHOWN, _("Wert (verschieben: manuell)"));
        if(isReplay_)
            bar->ActivateControls(false);
        AddTextButton(rowId(line, ValueMode), DrawPoint(324, y + 1), Extent(28, 22), TextureColor::Grey, "", NormalFont,
                      _("Wert automatisch (A) oder manuell (M)"));
        auto* orders = static_cast<ctrlTextDeepening*>(AddTextDeepening(rowId(line, OrdersText), DrawPoint(356, y + 3),
                                                                        Extent(34, 18), TextureColor::Grey, "",
                                                                        NormalFont, COLOR_YELLOW));
        static_cast<void>(orders);
        AddImageButton(rowId(line, OrderPlus), DrawPoint(391, y - 1), Extent(20, 13), TextureColor::Grey,
                       LOADER.GetImageN("io", 33), "+1");
        AddImageButton(rowId(line, OrderMinus), DrawPoint(391, y + 12), Extent(20, 13), TextureColor::Grey,
                       LOADER.GetImageN("io", 34), "-1");
        AddTextButton(rowId(line, PauseMode), DrawPoint(414, y + 1), Extent(90, 22), TextureColor::Grey, "", NormalFont,
                      _("Anhalten: automatisch bei Überschuss, oder vom Spieler (läuft / angehalten)"));
    }
    const unsigned bottom = FIRST_ROW_Y + ROWS_PER_PAGE * ROW_HEIGHT + 10;
    AddImageButton(HelpButton, DrawPoint(20, bottom), Extent(30, 32), TextureColor::Grey, LOADER.GetImageN("io", 225),
                   _("Help"));
    AddTextButton(PrevPage, DrawPoint(330, bottom + 5), Extent(30, 22), TextureColor::Grey, "<", NormalFont);
    AddText(PageText, DrawPoint(400, bottom + 9), "", COLOR_YELLOW, FontStyle::CENTER, NormalFont);
    AddTextButton(NextPage, DrawPoint(440, bottom + 5), Extent(30, 22), TextureColor::Grey, ">", NormalFont);
    UpdateRows();
}

unsigned iwWareDistribution::GetNumPages() const
{
    return std::max<unsigned>(1u, (static_cast<unsigned>(rows_.size()) + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE);
}

GoodType iwWareDistribution::GetWareAt(const unsigned line) const
{
    const unsigned idx = page_ * ROWS_PER_PAGE + line;
    if(idx >= rows_.size() || rows_[idx].heading)
        return GoodType::Nothing;
    return rows_[idx].good;
}

void iwWareDistribution::UpdateRows()
{
    const civ::WareDistribution& distribution = gwv.GetPlayer().GetWareDistribution();
    GetCtrl<ctrlTextButton>(ModeButton)
      ->SetText(distribution.UsesForecast() ? _("Werte aus: Prognose") : _("Werte aus: Warenzahlen"));
    GetCtrl<ctrlText>(PageText)->SetText(helpers::toString(page_ + 1) + "/" + helpers::toString(GetNumPages()));
    for(unsigned line = 0; line < ROWS_PER_PAGE; ++line)
    {
        const unsigned idx = page_ * ROWS_PER_PAGE + line;
        const bool exists = idx < rows_.size();
        const bool heading = exists && rows_[idx].heading;
        const GoodType good = GetWareAt(line);
        auto* name = GetCtrl<ctrlText>(rowId(line, NameText));
        name->SetVisible(exists);
        if(exists)
        {
            name->SetText(rows_[idx].title);
            name->SetTextColor(heading ? COLOR_WHITE : COLOR_YELLOW);
        }
        for(const RowCtrl ctrl : {ValueBar, ValueMode, OrdersText, OrderPlus, OrderMinus, PauseMode})
            GetCtrl<Window>(rowId(line, ctrl))->SetVisible(good != GoodType::Nothing);
        if(good == GoodType::Nothing)
            continue;
        const civ::WareSetting& setting = distribution.GetSetting(good);
        const unsigned value = setting.autoValue ?
                                 distribution.GetValue(good) / (civ::DISTRIBUTION_SCALE / civ::DISTRIBUTION_SHOWN) :
                                 setting.manualValue;
        GetCtrl<ctrlProgress>(rowId(line, ValueBar))->SetPosition(static_cast<unsigned short>(value));
        GetCtrl<ctrlTextButton>(rowId(line, ValueMode))->SetText(setting.autoValue ? "A" : "M");
        GetCtrl<ctrlTextDeepening>(rowId(line, OrdersText))->SetText(helpers::toString(distribution.GetOrders(good)));
        std::string pause;
        if(!setting.autoPause)
            pause = setting.paused ? _("angehalten") : _("läuft");
        else
        {
            switch(distribution.GetState(good))
            {
                case civ::WareDistState::Surplus: pause = _("A: Überschuss"); break;
                case civ::WareDistState::NoBuilding: pause = _("A: kein Geb."); break;
                default: pause = _("A: läuft"); break;
            }
        }
        GetCtrl<ctrlTextButton>(rowId(line, PauseMode))->SetText(pause);
        // The number behind the value: shown as tooltip of the name
        const civ::Amount number = distribution.GetNumber(good);
        GetCtrl<ctrlText>(rowId(line, NameText))
          ->SetText(rows_[idx].title + " (" + helpers::toString(number / civ::AMOUNT_UNIT) + ")");
    }
}

void iwWareDistribution::SendSetting(const GoodType good, const bool autoValue, const uint16_t manualValue,
                                     const bool autoPause, const bool paused)
{
    if(isReplay_)
        return;
    civ::WareSetting setting;
    setting.autoValue = autoValue;
    setting.manualValue = manualValue;
    setting.autoPause = autoPause;
    setting.paused = paused;
    gcFactory.SetWareDistribution(gwv.GetPlayer().GetWareDistribution().UsesForecast(), {{good, setting}});
}

void iwWareDistribution::Msg_ProgressChange(const unsigned ctrl_id, const unsigned short position)
{
    if(ctrl_id < ROW_BASE)
        return;
    const unsigned line = (ctrl_id - ROW_BASE) / ROW_IDS;
    const GoodType good = GetWareAt(line);
    if(good == GoodType::Nothing)
        return;
    // Moving the bar sets the value by the player
    const civ::WareSetting& setting = gwv.GetPlayer().GetWareDistribution().GetSetting(good);
    SendSetting(good, false, position, setting.autoPause, setting.paused);
}

void iwWareDistribution::Msg_ButtonClick(const unsigned ctrl_id)
{
    switch(ctrl_id)
    {
        case ModeButton:
            if(!isReplay_)
                gcFactory.SetWareDistribution(!gwv.GetPlayer().GetWareDistribution().UsesForecast(), {});
            return;
        case HelpButton:
            WINDOWMANAGER.ReplaceWindow(std::make_unique<iwHelp>(_(
              "Gesamtverteilung: Die Werte stellen sich selbst ein. Die Ware mit der niedrigsten Zahl (größter "
              "Mangel) bekommt 1000, die mit der höchsten 0. Gebäude mit mehreren Produkten wählen danach, Bestelltes "
              "zuerst. Gebäude halten an, wenn alle ihre Waren Überschuss sind (mehr als das Zehnfache des Verbrauchs "
              "pro Minute, mindestens 8). A/M: Wert automatisch oder manuell. Der rechte Knopf: Anhalten automatisch "
              "oder vom Spieler.")));
            return;
        case PrevPage:
            if(page_ > 0)
                --page_;
            UpdateRows();
            return;
        case NextPage:
            if(page_ + 1 < GetNumPages())
                ++page_;
            UpdateRows();
            return;
        default: break;
    }
    if(ctrl_id < ROW_BASE || isReplay_)
        return;
    const unsigned line = (ctrl_id - ROW_BASE) / ROW_IDS;
    const auto ctrl = static_cast<RowCtrl>((ctrl_id - ROW_BASE) % ROW_IDS);
    const GoodType good = GetWareAt(line);
    if(good == GoodType::Nothing)
        return;
    const civ::WareDistribution& distribution = gwv.GetPlayer().GetWareDistribution();
    const civ::WareSetting& setting = distribution.GetSetting(good);
    switch(ctrl)
    {
        case ValueMode:
        {
            // To manual: the value shown now stays
            const auto shown =
              static_cast<uint16_t>(distribution.GetValue(good) / (civ::DISTRIBUTION_SCALE / civ::DISTRIBUTION_SHOWN));
            SendSetting(good, !setting.autoValue, setting.autoValue ? shown : setting.manualValue, setting.autoPause,
                        setting.paused);
            break;
        }
        case PauseMode:
            // Automatic -> by the player, running -> by the player, stopped -> automatic
            if(setting.autoPause)
                SendSetting(good, setting.autoValue, setting.manualValue, false, false);
            else if(!setting.paused)
                SendSetting(good, setting.autoValue, setting.manualValue, false, true);
            else
                SendSetting(good, setting.autoValue, setting.manualValue, true, false);
            break;
        case OrderPlus: gcFactory.ChangeWareOrders(good, 1); break;
        case OrderMinus:
            if(distribution.GetOrders(good) > 0)
                gcFactory.ChangeWareOrders(good, -1);
            break;
        default: break;
    }
}

void iwWareDistribution::Msg_PaintBefore()
{
    IngameWindow::Msg_PaintBefore();
    // The values change by themselves: shown anew twice a second
    const unsigned now = VIDEODRIVER.GetTickCount();
    if(now - lastUpdate_ >= 500)
    {
        lastUpdate_ = now;
        UpdateRows();
    }
}
