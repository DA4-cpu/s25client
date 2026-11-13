// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "buildings/noBuildingSite.h"
#include "FOWObjects.h"
#include "GamePlayer.h"
#include "Loader.h"
#include "SerializedGameData.h"
#include "Ware.h"
#include "civ/Content.h"
#include "figures/nofBuilder.h"
#include "figures/nofPlaner.h"
#include "helpers/containerUtils.h"
#include "helpers/serializeEnums.h"
#include "helpers/toString.h"
#include "ogl/glArchivItem_Bitmap.h"
#include "ogl/glSmartBitmap.h"
#include "world/GameWorld.h"
#include "gameData/BuildingConsts.h"
#include "gameData/MilitaryConsts.h"
#include "s25util/colors.h"
#include <algorithm>
#include <array>
#include <stdexcept>

noBuildingSite::noBuildingSite(const BuildingType type, const MapPoint pos, const unsigned char player)
    : noBaseBuilding(NodalObjectType::Buildingsite, type, pos, player), state(BuildingSiteState::Building),
      planer(nullptr), builder(nullptr), materials_(BUILDING_COSTS[type].size()), build_progress(0)
{
    // Überprüfen, ob die Baustelle erst noch planiert werden muss (nur bei mittleren/großen Gebäuden)
    if(GetSize() == BuildingQuality::House || GetSize() == BuildingQuality::Castle
       || GetSize() == BuildingQuality::Harbor)
    {
        // Höhe auf dem Punkt, wo die Baustelle steht
        int altitude = world->GetNode(pos).altitude;

        for(const auto dir : helpers::EnumRange<Direction>{})
        {
            // Richtung 4 wird nicht planiert (Flagge)
            if(dir != Direction::SouthEast)
            {
                // Gibt es da Differenzen?
                if(altitude - world->GetNeighbourNode(pos, dir).altitude != 0)
                    state = BuildingSiteState::Planing;
            }
        }
    }

    // Wir hätten gerne einen Planierer/Bauarbeiter...
    world->GetPlayer(player).AddJobWanted((state == BuildingSiteState::Planing) ? Job::Planer : Job::Builder, this);

    // Bauwaren anfordern
    OrderConstructionMaterial();

    // Baustelle in den Index eintragen, damit die Wirtschaft auch Bescheid weiß
    world->GetPlayer(player).AddBuildingSite(this);
}

/// Konstruktor für Hafenbaustellen vom Schiff aus
noBuildingSite::noBuildingSite(const MapPoint pos, const unsigned char player)
    : noBaseBuilding(NodalObjectType::Buildingsite, BuildingType::HarborBuilding, pos, player),
      state(BuildingSiteState::Building), planer(nullptr),
      materials_(BUILDING_COSTS[BuildingType::HarborBuilding].size()), build_progress(0)
{
    GamePlayer& owner = world->GetPlayer(player);
    // Baustelle in den Index eintragen, damit die Wirtschaft auch Bescheid weiß
    owner.AddBuildingSite(this);
    // Bauarbeiter auch auf der Karte auftragen
    builder = &world->AddFigure(pos, std::make_unique<nofBuilder>(pos, player, this));

    // The expedition brought all material (harbor costs: boards and stones)
    const BuildingCosts& costs = BUILDING_COSTS[BuildingType::HarborBuilding];
    for(unsigned i = 0; i < costs.size(); ++i)
        materials_[i].delivered.assign(costs[i].amount, costs[i].wares.front());
    // Baumaterialien in der Inventur verbuchen
    owner.DecreaseInventoryWare(GoodType::Boards, HARBOR_BUILDING_COST.boards, WareUse::Construction);
    owner.DecreaseInventoryWare(GoodType::Stones, HARBOR_BUILDING_COST.stones, WareUse::Construction);
}

noBuildingSite::~noBuildingSite() = default;

void noBuildingSite::Destroy()
{
    // Bauarbeiter/Planierer Bescheid sagen
    if(builder)
    {
        builder->LostWork();
        builder = nullptr;
    } else if(planer)
    {
        planer->LostWork();
        planer = nullptr;
    } else
        world->GetPlayer(player).JobNotWanted(this);

    RTTR_Assert(!builder);
    RTTR_Assert(!planer);

    // Bestellte Waren Bescheid sagen
    for(Material& material : materials_)
    {
        for(Ware* ware : material.ordered)
            WareNotNeeded(ware);
        material.ordered.clear();
    }

    // und Feld wird leer
    world->SetNO(pos, nullptr);

    // Baustelle wieder aus der Liste entfernen - dont forget about expedition harbor status
    bool expeditionharbor = IsHarborBuildingSiteFromSea();
    world->GetPlayer(player).RemoveBuildingSite(this);

    noBaseBuilding::Destroy();

    // Hafenbaustelle?
    if(expeditionharbor)
    {
        world->RemoveHarborBuildingSiteFromSea(this);
        // Land neu berechnen nach zerstören weil da schon straßen etc entfernt werden
        world->RecalcTerritory(*this, TerritoryChangeReason::Destroyed);
    }
    world->RecalcBQAroundPointBig(pos);
}

void noBuildingSite::Serialize(SerializedGameData& sgd) const
{
    noBaseBuilding::Serialize(sgd);

    sgd.PushEnum<uint8_t>(state);
    sgd.PushObject(planer, true);
    sgd.PushObject(builder, true);
    sgd.PushUnsignedChar(build_progress);
    sgd.PushUnsignedInt(static_cast<unsigned>(materials_.size()));
    for(const Material& material : materials_)
    {
        sgd.PushUnsignedInt(static_cast<unsigned>(material.delivered.size()));
        for(const GoodType good : material.delivered)
            helpers::pushEnum<uint8_t>(sgd, good);
        sgd.PushUnsignedChar(material.used);
        sgd.PushObjectContainer(material.ordered, true);
    }
}

noBuildingSite::noBuildingSite(SerializedGameData& sgd, const unsigned obj_id)
    : noBaseBuilding(sgd, obj_id), state(sgd.Pop<BuildingSiteState>()),
      planer(sgd.PopObject<nofPlaner>(GO_Type::NofPlaner)), builder(sgd.PopObject<nofBuilder>(GO_Type::NofBuilder)),
      build_progress(0)
{
    const BuildingCosts& costs = BUILDING_COSTS[bldType_];
    if(sgd.GetGameDataVersion() >= 25)
    {
        build_progress = sgd.PopUnsignedChar();
        materials_.resize(sgd.PopUnsignedInt());
        for(Material& material : materials_)
        {
            material.delivered.resize(sgd.PopUnsignedInt());
            for(GoodType& good : material.delivered)
                good = helpers::popEnum<GoodType>(sgd);
            material.used = sgd.PopUnsignedChar();
            sgd.PopObjectContainer(material.ordered, GO_Type::Ware);
        }
        if(materials_.size() != costs.size())
            throw SerializedGameData::Error("Construction costs of a building site do not fit the lua files");
    } else
    {
        // Boards and stones: to the entries which take them
        const unsigned char boards = sgd.PopUnsignedChar(), stones = sgd.PopUnsignedChar();
        const unsigned char usedBoards = sgd.PopUnsignedChar(), usedStones = sgd.PopUnsignedChar();
        build_progress = sgd.PopUnsignedChar();
        std::list<Ware*> orderedBoards, orderedStones;
        sgd.PopObjectContainer(orderedBoards, GO_Type::Ware);
        sgd.PopObjectContainer(orderedStones, GO_Type::Ware);
        materials_.resize(costs.size());
        const auto entryOf = [&costs](const GoodType good) {
            for(unsigned i = 0; i < costs.size(); ++i)
            {
                if(helpers::contains(costs[i].wares, good))
                    return static_cast<int>(i);
            }
            return -1;
        };
        const int boardsEntry = entryOf(GoodType::Boards), stonesEntry = entryOf(GoodType::Stones);
        if((boards + usedBoards + orderedBoards.size() > 0 && boardsEntry < 0)
           || (stones + usedStones + orderedStones.size() > 0 && stonesEntry < 0))
            throw SerializedGameData::Error("Construction costs of a building site do not fit the lua files");
        if(boardsEntry >= 0)
        {
            Material& material = materials_[boardsEntry];
            material.delivered.assign(boards, GoodType::Boards);
            material.used = usedBoards;
            material.ordered = orderedBoards;
        }
        if(stonesEntry >= 0)
        {
            Material& material = materials_[stonesEntry];
            material.delivered.insert(material.delivered.end(), stones, GoodType::Stones);
            material.used += usedStones;
            material.ordered.insert(material.ordered.end(), orderedStones.begin(), orderedStones.end());
        }
    }
}

unsigned noBuildingSite::GetNumMissing(const unsigned entry) const
{
    const Material& material = materials_[entry];
    const unsigned have = material.used + material.delivered.size() + material.ordered.size();
    const unsigned amount = BUILDING_COSTS[bldType_][entry].amount;
    return have < amount ? amount - have : 0u;
}

int noBuildingSite::FindEntryForWare(const Ware& ware) const
{
    for(unsigned i = 0; i < materials_.size(); ++i)
    {
        if(helpers::contains(materials_[i].ordered, &ware))
            return static_cast<int>(i);
    }
    return -1;
}

unsigned noBuildingSite::GetNumUsed(const GoodType good) const
{
    const BuildingCosts& costs = BUILDING_COSTS[bldType_];
    unsigned result = 0;
    for(unsigned i = 0; i < costs.size(); ++i)
    {
        if(helpers::contains(costs[i].wares, good))
            result += materials_[i].used + materials_[i].delivered.size() + materials_[i].ordered.size();
    }
    return result;
}

std::pair<unsigned, unsigned> noBuildingSite::GetMaxProgress(const BuildingType type)
{
    const BuildingCosts& costs = BUILDING_COSTS[type];
    unsigned total = 0;
    for(const CostEntry& entry : costs)
        total += entry.amount;
    total *= 8;
    // Original costs: The frame is made of the boards (first entry), the walls of the stones. Only boards: 50:50.
    // Civilization mode: In the order of the costs, the first half is the frame
    if(!civ::getActiveContent().IsFromLua() && costs.size() == 2u)
        return {costs[0].amount * 8u, total};
    return {total / 2u, total};
}

void noBuildingSite::OrderConstructionMaterial()
{
    // Bei Planieren keine Waren bestellen
    if(state == BuildingSiteState::Planing)
        return;

    // Optimization: return early if building is isolated
    if(!IsConnected())
        return;

    // Every material in the order of the costs, each from the next warehouse having any matching ware
    GamePlayer& owner = world->GetPlayer(player);
    const BuildingCosts& costs = BUILDING_COSTS[bldType_];
    for(unsigned entry = 0; entry < costs.size(); ++entry)
    {
        orderingEntry_ = static_cast<int>(entry);
        for(unsigned i = GetNumMissing(entry); i > 0; --i)
        {
            Ware* w = owner.OrderWare(costs[entry].wares, *this);
            if(!w)
                break;
            RTTR_Assert(helpers::contains(materials_[entry].ordered, w));
        }
    }
    orderingEntry_ = -1;
}

unsigned noBuildingSite::GetMilitaryRadius() const
{
    /// Note: This actually only applies to harbor buildings made from expeditions. We rely on the calling functions to
    /// only take those into account
    return bldType_ == BuildingType::HarborBuilding ? HARBOR_RADIUS : 0;
}

void noBuildingSite::Draw(DrawPoint drawPt)
{
    if(state == BuildingSiteState::Planing)
    {
        // Baustellenschild mit Schatten zeichnen
        LOADER.GetNationImage(world->GetPlayer(player).nation, 450)->DrawFull(drawPt);
        LOADER.GetNationImage(world->GetPlayer(player).nation, 451)->DrawFull(drawPt, COLOR_SHADOW);
    } else
    {
        // Baustellenstein und -schatten zeichnen
        LOADER.GetNationImage(world->GetPlayer(player).nation, 455)->DrawFull(drawPt);
        LOADER.GetNationImage(world->GetPlayer(player).nation, 456)->DrawFull(drawPt, COLOR_SHADOW);

        // Waren auf der Baustelle: the first material left of the door (like the boards), the second right of it
        // (like the stones), further ones stacked on top of them
        const DrawPoint doorPos = drawPt + DrawPoint(GetDoorPointX(), GetDoorPointY());
        std::array<unsigned, 2> stackHeight{};
        for(unsigned entry = 0; entry < materials_.size(); ++entry)
        {
            const unsigned side = entry % 2u;
            for(const GoodType good : materials_[entry].delivered)
            {
                const DrawPoint offset = side == 0 ? DrawPoint(-5, -10) : DrawPoint(8, -12);
                LOADER.GetWareStackTex(good)->DrawFull(doorPos + offset
                                                       - DrawPoint(0, static_cast<int>(stackHeight[side]) * 4));
                ++stackHeight[side];
            }
        }

        // bis dahin gebautes Haus zeichnen: erst der Rohbau, dann die Mauern
        const auto maxProgress = GetMaxProgress(bldType_);
        const unsigned maxProgressRaw = std::max(1u, maxProgress.first);
        const unsigned maxProgressBld = std::max(1u, maxProgress.second - maxProgress.first);
        const unsigned progressRaw = std::min<unsigned>(build_progress, maxProgressRaw);
        const unsigned progressBld = ((build_progress > maxProgressRaw) ? (build_progress - maxProgressRaw) : 0);

        LOADER.getBuildingSprites(nation, bldType_).skeleton.drawPercent(drawPt, progressRaw * 100 / maxProgressRaw);
        LOADER.getBuildingSprites(nation, bldType_).building.drawPercent(drawPt, progressBld * 100 / maxProgressBld);
    }
}

/// Erzeugt von ihnen selbst ein FOW Objekt als visuelle "Erinnerung" für den Fog of War
std::unique_ptr<FOWObject> noBuildingSite::CreateFOWObject() const
{
    return std::make_unique<fowBuildingSite>(state == BuildingSiteState::Planing, bldType_, nation, build_progress);
}

void noBuildingSite::GotWorker(Job /*job*/, noFigure& worker)
{
    // Aha, wir haben nen Planierer/Bauarbeiter bekommen
    if(state == BuildingSiteState::Planing)
    {
        RTTR_Assert(worker.GetGOT() == GO_Type::NofPlaner);
        planer = static_cast<nofPlaner*>(&worker);
    } else
    {
        RTTR_Assert(worker.GetGOT() == GO_Type::NofBuilder);
        builder = static_cast<nofBuilder*>(&worker);
    }
}

void noBuildingSite::Abrogate()
{
    planer = nullptr;
    builder = nullptr;

    world->GetPlayer(player).AddJobWanted((state == BuildingSiteState::Planing) ? Job::Planer : Job::Builder, this);
}

unsigned noBuildingSite::CalcDistributionPoints(const GoodType goodtype)
{
    // Beim Planieren brauchen wir noch gar nichts
    if(state == BuildingSiteState::Planing)
        return 0;

    // Only the construction materials which are still missing
    const BuildingCosts& costs = BUILDING_COSTS[bldType_];
    bool needed = false;
    unsigned numMissing = 0;
    for(unsigned entry = 0; entry < costs.size(); ++entry)
    {
        const unsigned missing = GetNumMissing(entry);
        numMissing += missing;
        if(missing > 0 && helpers::contains(costs[entry].wares, goodtype))
            needed = true;
    }
    if(!needed)
        return 0;

    // 10000 als Basis wählen, damit man auch noch was abziehen kann
    constexpr unsigned basePoints = 10000;
    unsigned points = basePoints;

    // Baumaterial mit einberechnen (wer noch am wenigsten braucht, soll mehr Punkte kriegen, da ja möglichst
    // zuerst Gebäude fertiggestellt werden sollten)
    points -= numMissing * 20;

    // Baupriorität mit einberechnen (niedriger = höhere Priorität, daher - !)
    const unsigned buildingSitePrio = world->GetPlayer(player).GetBuidingSitePriority(this) * 30;

    if(points > buildingSitePrio)
        points -= buildingSitePrio;
    else
        points = 0;

    RTTR_Assert(points <= basePoints); // Underflow detection. Should never happen...

    return points;
}

void noBuildingSite::AddWare(std::unique_ptr<Ware> ware)
{
    RTTR_Assert(state == BuildingSiteState::Building);

    const int entry = FindEntryForWare(*ware);
    if(entry < 0)
        throw std::logic_error("Wrong ware type " + helpers::toString(ware->type));
    materials_[entry].ordered.remove(ware.get());
    materials_[entry].delivered.push_back(ware->type);

    // Inventur entsprechend verringern
    world->GetPlayer(player).DecreaseInventoryWare(ware->type, 1, WareUse::Construction);
    world->GetPlayer(player).RemoveWare(*ware);
}

void noBuildingSite::WareLost(Ware& ware)
{
    RTTR_Assert(state == BuildingSiteState::Building);

    const int entry = FindEntryForWare(ware);
    if(entry < 0)
        throw std::logic_error("Wrong ware type lost " + helpers::toString(ware.type));
    materials_[entry].ordered.remove(&ware);

    OrderConstructionMaterial();
}

void noBuildingSite::TakeWare(Ware* ware)
{
    RTTR_Assert(state == BuildingSiteState::Building);

    // Ware in die Bestellliste aufnehmen: for the material it is ordered for, otherwise the first needing it
    RTTR_Assert(FindEntryForWare(*ware) < 0);
    const BuildingCosts& costs = BUILDING_COSTS[bldType_];
    int entry = -1;
    if(orderingEntry_ >= 0 && helpers::contains(costs[orderingEntry_].wares, ware->type))
        entry = orderingEntry_;
    for(unsigned i = 0; entry < 0 && i < costs.size(); ++i)
    {
        if(GetNumMissing(i) > 0 && helpers::contains(costs[i].wares, ware->type))
            entry = static_cast<int>(i);
    }
    if(entry < 0)
        throw std::logic_error("Wrong ware type " + helpers::toString(ware->type));
    materials_[entry].ordered.push_back(ware);
}

bool noBuildingSite::IsBuildingComplete()
{
    return build_progress == GetMaxProgress(bldType_).second;
}

unsigned char noBuildingSite::GetBuildProgress(bool percent) const
{
    if(!percent)
        return build_progress;

    const unsigned costs = std::max(1u, GetMaxProgress(bldType_).second);
    unsigned progress = (((unsigned)build_progress) * 100) / costs;

    return (unsigned char)progress;
}

/// Aufgerufen, wenn Planierung beendet wurde
void noBuildingSite::PlaningFinished()
{
    /// Normale Baustelle
    state = BuildingSiteState::Building;
    planer = nullptr;

    // Wir hätten gerne einen Bauarbeiter...
    world->GetPlayer(player).AddJobWanted(Job::Builder, this);

    // Bauwaren anfordern
    OrderConstructionMaterial();
}

/// Gibt zurück, ob eine bestimmte Baustellen eine Baustelle ist, die vom Schiff aus errichtet wurde
bool noBuildingSite::IsHarborBuildingSiteFromSea() const
{
    if(this->bldType_ == BuildingType::HarborBuilding)
        return world->IsHarborBuildingSiteFromSea(this);
    else
        return false;
}
