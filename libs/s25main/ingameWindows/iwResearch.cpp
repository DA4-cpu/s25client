// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iwResearch.h"
#include "GamePlayer.h"
#include "Loader.h"
#include "civ/Knowledge.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlMultiline.h"
#include "controls/ctrlPercent.h"
#include "controls/ctrlTable.h"
#include "controls/ctrlText.h"
#include "factories/GameCommandFactory.h"
#include "helpers/EnumRange.h"
#include "helpers/format.hpp"
#include "helpers/toString.h"
#include "world/GameWorldBase.h"
#include "world/GameWorldViewer.h"
#include "gameData/BuildingConsts.h"
#include "gameData/GoodConsts.h"
#include "gameData/const_gui_ids.h"
#include "s25util/StringConversion.h"
#include "s25util/colors.h"
#include <algorithm>
#include <array>

namespace {
enum
{
    ID_txtCurrentLabel,
    ID_txtCurrent,
    ID_pctCurrent,
    ID_txtCurrentCosts,
    ID_tblTechs,
    ID_mlDescription,
    ID_btResearch,
    ID_btStop,
    ID_txtKnowledgeLabel,
    ID_txtKnowledgeFirst, // One text per kind of knowledge
};

/// Column containing the (hidden) index of the technology
constexpr unsigned COL_TECH_IDX = 4;

const std::array<const char*, 7> ERA_NAMES = {gettext_noop("Jungsteinzeit"), gettext_noop("Antike"),
                                              gettext_noop("Klassik"),       gettext_noop("Mittelalter"),
                                              gettext_noop("Frühe Neuzeit"), gettext_noop("Industriezeitalter"),
                                              gettext_noop("Moderne")};

std::string getEraName(unsigned era)
{
    return era < ERA_NAMES.size() ? _(ERA_NAMES[era]) : helpers::toString(era);
}

/// Knowledge costs like "4 LWP, 2 IWP" (empty if there are none). If `missing` is given, append the missing points
std::string getKnowledgeCostText(const civ::Technology& tech, const civ::KnowledgePoints* missing = nullptr)
{
    std::string result;
    for(const auto type : helpers::enumRange<civ::KnowledgeType>())
    {
        if(tech.knowledgeCost[type] <= 0)
            continue;
        if(!result.empty())
            result += ", ";
        result += helpers::format("%1% %2%", tech.knowledgeCost[type], civ::getShortName(type));
        if(missing && (*missing)[type] > 0)
            result += helpers::format(_(" (es fehlen %1%)"), civ::formatKnowledge((*missing)[type]));
    }
    return result;
}

bool hasMissingKnowledge(const civ::KnowledgePoints& missing)
{
    for(const int points : missing)
    {
        if(points > 0)
            return true;
    }
    return false;
}

/// Ware costs (with paid amounts if progress is given) or empty if there are none
std::string getWareCostText(const civ::Technology& tech, const civ::PlayerResearch::Progress* progress)
{
    std::string result;
    for(unsigned i = 0; i < tech.cost.size(); ++i)
    {
        if(!result.empty())
            result += ", ";
        if(progress)
            result += helpers::format("%1%/%2% %3%", progress->paid.at(i), tech.cost[i].amount,
                                      _(WARE_NAMES[tech.cost[i].good]));
        else
            result += helpers::format("%1% %2%", tech.cost[i].amount, _(WARE_NAMES[tech.cost[i].good]));
    }
    return result;
}

/// All costs for the table: knowledge and wares
std::string getCostText(const civ::Technology& tech, const civ::PlayerResearch::Progress* progress)
{
    const std::string knowledge = getKnowledgeCostText(tech);
    const std::string wares = getWareCostText(tech, progress);
    if(knowledge.empty() && wares.empty())
        return _("keine");
    if(knowledge.empty() || wares.empty())
        return knowledge + wares;
    return knowledge + ", " + wares;
}

/// True if all wares are paid and the minimum time has passed (only knowledge may be missing)
bool isWaitingOnlyForKnowledge(const civ::Technology& tech, const civ::PlayerResearch::Progress* progress)
{
    if(!progress || progress->researchedGFs < tech.minResearchGFs)
        return false;
    for(unsigned i = 0; i < tech.cost.size(); ++i)
    {
        if(progress->paid.at(i) < tech.cost[i].amount)
            return false;
    }
    return true;
}

std::string getStateText(const GameWorldBase& world, unsigned playerId, civ::TechIdx tech)
{
    switch(civ::getTechState(world, playerId, tech))
    {
        case civ::TechState::Unavailable: return _("Nicht verfügbar");
        case civ::TechState::Researched: return _("Erforscht");
        case civ::TechState::InProgress: return _("Wird erforscht");
        case civ::TechState::Available: return _("Verfügbar");
        case civ::TechState::MissingRequirements: return _("Gesperrt");
        case civ::TechState::TakenByOther:
        {
            const auto owner = civ::getExclusiveOwner(world, tech);
            return helpers::format(_("Vergeben an %1%"), owner ? world.GetPlayer(*owner).name : std::string());
        }
    }
    return "";
}
} // namespace

iwResearch::iwResearch(const GameWorldViewer& gwv, GameCommandFactory& gcFactory)
    : IngameWindow(CGI_RESEARCH, IngameWindow::posLastOrCenter, Extent(640, 542), _("Forschung"),
                   LOADER.GetImageN("resource", 41)),
      gwv(gwv), gcFactory(gcFactory), currentProgress_(0)
{
    AddText(ID_txtKnowledgeLabel, DrawPoint(20, 30), _("Wissen (verfügbar/gesamt):"), COLOR_YELLOW, FontStyle{},
            NormalFont);
    for(const auto type : helpers::enumRange<civ::KnowledgeType>())
    {
        const auto idx = static_cast<unsigned>(type);
        AddText(ID_txtKnowledgeFirst + idx, DrawPoint(250 + idx * 120, 30), "", COLOR_WHITE, FontStyle{}, NormalFont);
    }
    AddText(ID_txtCurrentLabel, DrawPoint(20, 52), _("Aktuelle Forschung:"), COLOR_YELLOW, FontStyle{}, NormalFont);
    AddText(ID_txtCurrent, DrawPoint(180, 52), "", COLOR_WHITE, FontStyle{}, NormalFont);
    AddPercent(ID_pctCurrent, DrawPoint(20, 72), Extent(600, 20), TextureColor::Grey, COLOR_YELLOW, SmallFont,
               &currentProgress_);
    AddText(ID_txtCurrentCosts, DrawPoint(20, 98), "", COLOR_YELLOW, FontStyle{}, SmallFont)->setMaxWidth(600);

    using SRT = ctrlTable::SortType;
    AddTable(ID_tblTechs, DrawPoint(20, 117), Extent(600, 230), TextureColor::Grey, NormalFont,
             ctrlTable::Columns{{_("Zeitalter"), 110, SRT::String},
                                {_("Technologie"), 190, SRT::String},
                                {_("Status"), 140, SRT::String},
                                {_("Kosten"), 260, SRT::String},
                                {}});

    AddMultiline(ID_mlDescription, DrawPoint(20, 357), Extent(600, 135), TextureColor::Grey, NormalFont);

    AddTextButton(ID_btResearch, DrawPoint(20, 502), Extent(290, 22), TextureColor::Green2, _("Erforschen"),
                  NormalFont);
    AddTextButton(ID_btStop, DrawPoint(330, 502), Extent(290, 22), TextureColor::Red1, _("Forschung anhalten"),
                  NormalFont);

    RefreshKnowledge();
    RefreshTable();
    RefreshCurrentResearch();
    RefreshSelection();
}

bool iwResearch::RefreshKnowledge()
{
    const GamePlayer& player = gwv.GetPlayer();
    const civ::KnowledgePoints total = civ::calcTotalKnowledge(player);
    const civ::KnowledgePoints available = civ::calcAvailableKnowledge(player);
    const auto isSame = [](const civ::KnowledgePoints& lhs, const civ::KnowledgePoints& rhs) {
        return std::equal(lhs.begin(), lhs.end(), rhs.begin());
    };
    if(knowledgeShown_ && isSame(total, lastTotalKnowledge_) && isSame(available, lastAvailableKnowledge_))
        return false;
    knowledgeShown_ = true;
    lastTotalKnowledge_ = total;
    lastAvailableKnowledge_ = available;
    for(const auto type : helpers::enumRange<civ::KnowledgeType>())
    {
        auto* txt = GetCtrl<ctrlText>(ID_txtKnowledgeFirst + static_cast<unsigned>(type));
        txt->SetText(helpers::format("%1% %2% / %3%", civ::getShortName(type), civ::formatKnowledge(available[type]),
                                     civ::formatKnowledge(total[type])));
        txt->SetTextColor(available[type] < 0 ? COLOR_RED : COLOR_WHITE);
    }
    return true;
}

std::string iwResearch::GetStateSignature() const
{
    const GameWorldBase& world = gwv.GetWorld();
    std::string signature;
    for(unsigned i = 0; i < world.GetNumPlayers(); ++i)
    {
        const civ::PlayerResearch& research = world.GetPlayer(i).GetResearch();
        signature += helpers::toString(research.GetNumResearched()) + ":";
        signature += research.GetCurrent() ? helpers::toString(*research.GetCurrent()) : std::string("-");
        signature += ";";
    }
    return signature;
}

void iwResearch::RefreshTable()
{
    const GameWorldBase& world = gwv.GetWorld();
    const civ::TechTree& techTree = world.GetTechTree();
    const unsigned playerId = gwv.GetPlayerId();
    const civ::PlayerResearch& research = gwv.GetPlayer().GetResearch();
    const std::optional<civ::TechIdx> selectedTech = GetSelectedTech();

    auto* table = GetCtrl<ctrlTable>(ID_tblTechs);
    table->DeleteAllItems();
    for(civ::TechIdx tech = 0; tech < techTree.size(); ++tech)
    {
        // Technologies of disabled addons are not part of this game
        if(civ::getTechState(world, playerId, tech) == civ::TechState::Unavailable)
            continue;
        const civ::Technology& technology = techTree.get(tech);
        // Prefix with the era number so sorting by the era column keeps the eras in order
        table->AddRow({helpers::format("%1% %2%", technology.era, getEraName(technology.era)), _(technology.title),
                       getStateText(world, playerId, tech), getCostText(technology, research.GetProgress(tech)),
                       helpers::toString(tech)});
    }
    if(selectedTech)
    {
        for(unsigned row = 0; row < table->GetNumRows(); ++row)
        {
            if(table->GetItemText(row, COL_TECH_IDX) == helpers::toString(*selectedTech))
            {
                table->SetSelection(row);
                break;
            }
        }
    }
    lastStateSignature_ = GetStateSignature();
}

void iwResearch::RefreshCurrentResearch()
{
    const GameWorldBase& world = gwv.GetWorld();
    const civ::PlayerResearch& research = gwv.GetPlayer().GetResearch();
    auto* txtCurrent = GetCtrl<ctrlText>(ID_txtCurrent);
    auto* txtCosts = GetCtrl<ctrlText>(ID_txtCurrentCosts);
    if(!research.GetCurrent())
    {
        txtCurrent->SetText(_("keine"));
        txtCosts->SetText(_("Wähle unten eine verfügbare Technologie aus und klicke auf \"Erforschen\"."));
        txtCosts->SetTextColor(COLOR_YELLOW);
        currentProgress_ = 0;
        GetCtrl<ctrlButton>(ID_btStop)->SetEnabled(false);
        return;
    }
    const civ::Technology& tech = world.GetTechTree().get(*research.GetCurrent());
    const civ::PlayerResearch::Progress* progress = research.GetProgress(*research.GetCurrent());
    txtCurrent->SetText(_(tech.title));
    const civ::KnowledgePoints missing = civ::getMissingKnowledge(world, gwv.GetPlayerId(), *research.GetCurrent());
    std::string costText;
    const std::string wareText = getWareCostText(tech, progress);
    if(!wareText.empty())
        costText = helpers::format(_("Waren: %1%"), wareText);
    const std::string knowledgeText = getKnowledgeCostText(tech, &missing);
    if(!knowledgeText.empty())
    {
        if(!costText.empty())
            costText += " - ";
        costText += helpers::format(_("Wissenspunkte: %1%"), knowledgeText);
    }
    const bool waitingForKnowledge = hasMissingKnowledge(missing) && isWaitingOnlyForKnowledge(tech, progress);
    if(waitingForKnowledge)
        costText = _("Wartet auf Wissenspunkte. ") + costText;
    txtCosts->SetText(costText.empty() ? _("Keine Kosten") : costText);
    txtCosts->SetTextColor(waitingForKnowledge ? COLOR_RED : COLOR_YELLOW);
    unsigned paid = 0;
    if(progress)
    {
        for(unsigned i = 0; i < tech.cost.size(); ++i)
            paid += progress->paid.at(i);
    }
    const unsigned total = tech.GetTotalCost();
    currentProgress_ = static_cast<unsigned short>(total == 0 ? 100 : paid * 100 / total);
    GetCtrl<ctrlButton>(ID_btStop)->SetEnabled(true);
}

void iwResearch::RefreshSelection()
{
    auto* description = GetCtrl<ctrlMultiline>(ID_mlDescription);
    description->Clear();
    const std::optional<civ::TechIdx> tech = GetSelectedTech();
    auto* btResearch = GetCtrl<ctrlButton>(ID_btResearch);
    if(!tech)
    {
        btResearch->SetEnabled(false);
        if(gwv.GetWorld().GetTechTree().empty())
            description->AddString(_("Es sind keine Technologien definiert."), COLOR_RED, false);
        else
            description->AddString(_("Wähle eine Technologie aus der Liste aus."), COLOR_YELLOW, false);
        return;
    }
    const GameWorldBase& world = gwv.GetWorld();
    const civ::TechTree& techTree = world.GetTechTree();
    const civ::Technology& technology = techTree.get(*tech);
    const civ::PlayerResearch& research = gwv.GetPlayer().GetResearch();

    description->AddString(helpers::format("%1% (%2%)", _(technology.title), getEraName(technology.era)), COLOR_YELLOW,
                           false);
    if(!technology.description.empty())
        description->AddString(_(technology.description), COLOR_WHITE, false);
    if(!technology.unlockedBuildings.empty())
    {
        std::string buildings;
        for(const BuildingType bld : technology.unlockedBuildings)
        {
            if(!buildings.empty())
                buildings += ", ";
            buildings += _(BUILDING_NAMES[bld]);
        }
        description->AddString(helpers::format(_("Schaltet frei: %1%"), buildings), COLOR_GREEN, false);
    }
    if(!technology.requirements.empty())
    {
        std::string reqs;
        bool allResearched = true;
        for(const civ::TechIdx req : technology.requirements)
        {
            if(!reqs.empty())
                reqs += ", ";
            reqs += _(techTree.get(req).title);
            if(!research.IsResearched(req))
            {
                reqs += _(" (fehlt)");
                allResearched = false;
            }
        }
        description->AddString(helpers::format(_("Voraussetzungen: %1%"), reqs),
                               allResearched ? COLOR_WHITE : COLOR_RED, false);
    }
    const std::string knowledgeText = getKnowledgeCostText(technology);
    if(!knowledgeText.empty())
    {
        const civ::KnowledgePoints missing = civ::getMissingKnowledge(world, gwv.GetPlayerId(), *tech);
        const bool researched = research.IsResearched(*tech);
        if(researched || !hasMissingKnowledge(missing))
            description->AddString(helpers::format(_("Wissenspunkte: %1%"), knowledgeText), COLOR_WHITE, false);
        else
        {
            description->AddString(helpers::format(_("Wissenspunkte: %1%"), getKnowledgeCostText(technology, &missing)),
                                   COLOR_RED, false);
        }
    }
    const std::string wareText = getWareCostText(technology, research.GetProgress(*tech));
    description->AddString(helpers::format(_("Waren: %1%"), wareText.empty() ? _("keine") : wareText), COLOR_WHITE,
                           false);
    if(technology.exclusive)
        description->AddString(_("Exklusiv: Nur der Spieler, der sie zuerst erforscht, erhält diese Technologie."),
                               COLOR_ORANGE, false);
    if(const auto owner = civ::getExclusiveOwner(world, *tech))
    {
        if(*owner != gwv.GetPlayerId())
            description->AddString(helpers::format(_("Bereits erforscht von %1%."), world.GetPlayer(*owner).name),
                                   COLOR_RED, false);
    }

    const civ::TechState state = civ::getTechState(world, gwv.GetPlayerId(), *tech);
    btResearch->SetEnabled(state == civ::TechState::Available);
}

std::optional<civ::TechIdx> iwResearch::GetSelectedTech() const
{
    const auto* table = GetCtrl<ctrlTable>(ID_tblTechs);
    const auto& selection = table->GetSelection();
    if(!selection)
        return std::nullopt;
    const auto techIdx = s25util::fromStringClassicDef<unsigned>(table->GetItemText(*selection, COL_TECH_IDX),
                                                                 gwv.GetWorld().GetTechTree().size());
    if(techIdx >= gwv.GetWorld().GetTechTree().size())
        return std::nullopt;
    return techIdx;
}

void iwResearch::Msg_PaintBefore()
{
    IngameWindow::Msg_PaintBefore();
    const bool knowledgeChanged = RefreshKnowledge();
    if(GetStateSignature() != lastStateSignature_)
    {
        RefreshTable();
        RefreshSelection();
    } else if(knowledgeChanged)
        RefreshSelection(); // Missing knowledge points are shown in the description
    RefreshCurrentResearch();
}

void iwResearch::Msg_ButtonClick(const unsigned ctrl_id)
{
    switch(ctrl_id)
    {
        case ID_btResearch:
            if(const auto tech = GetSelectedTech())
                gcFactory.SetResearch(*tech);
            break;
        case ID_btStop: gcFactory.SetResearch(std::nullopt); break;
        default: break;
    }
}

void iwResearch::Msg_TableSelectItem(const unsigned /*ctrl_id*/, const std::optional<unsigned>& /*selection*/)
{
    RefreshSelection();
}
