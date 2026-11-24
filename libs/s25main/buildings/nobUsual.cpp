// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "nobUsual.h"
#include "EventManager.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "Loader.h"
#include "SerializedGameData.h"
#include "Ware.h"
#include "addons/const_addons.h"
#include "civ/Content.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "civ/WareDistribution.h"
#include "figures/nofBuildingWorker.h"
#include "figures/nofPigbreeder.h"
#include "helpers/containerUtils.h"
#include "helpers/format.hpp"
#include "helpers/serializeEnums.h"
#include "network/GameClient.h"
#include "notifications/BuildingNote.h"
#include "ogl/glArchivItem_Bitmap.h"
#include "ogl/glArchivItem_Bitmap_Player.h"
#include "postSystem/PostMsgWithBuilding.h"
#include "world/GameWorld.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/CivContentDesc.h"
#include <algorithm>
#include <numeric>

/// Number of GFs after which the productivity is recalculated, i.e. productivity is averaged over intervals of this
/// length
constexpr unsigned numProductivityGFs = 400u;
/// Marker for sinceNotWorking to mark the working state for productivity purposes
constexpr unsigned isWorkingMarker = 0xFFFFFFFF;

nobUsual::nobUsual(BuildingType type, MapPoint pos, unsigned char player, Nation nation)
    : noBuilding(type, pos, player, nation), worker(nullptr), disableProduction(false), disableProductionVirtual(false),
      lastOrderedWare(0), orderware_ev(nullptr), productivity_ev(nullptr), numGfNotWorking(0),
      sinceNotWorking(isWorkingMarker), outOfRessourcesMsgSent(false), ordertool_ev(nullptr), stepSpeed_(100),
      is_working(false)
{
    toolSlots_.resize(GetToolsDesc().slots.size());
    for(const InputSlot& slot : BLD_WORK_DESC[bldType_].inputs)
        inputs_.push_back(InputStock{std::vector<uint8_t>(slot.wares.size(), 0), {}, 0});

    // Tür aufmachen,bis Gebäude besetzt ist
    OpenDoor();

    finishedGF_ = GetEvMgr().GetCurrentGF();
    GamePlayer& owner = world->GetPlayer(player);
    // New building gets half the average productivity from all buildings of the same type
    productivity = owner.GetBuildingRegister().CalcAverageProductivity(type) / 2u;
    // Set last productivities to current to avoid resetting it on first recalculation event
    std::fill(lastProductivities.begin(), lastProductivities.end(), productivity);
}

nobUsual::nobUsual(SerializedGameData& sgd, const unsigned obj_id)
    : noBuilding(sgd, obj_id), worker(sgd.PopObject<nofBuildingWorker>()), productivity(sgd.PopUnsignedShort()),
      disableProduction(sgd.PopBool()), disableProductionVirtual(disableProduction),
      lastOrderedWare(sgd.PopUnsignedChar()), orderware_ev(sgd.PopEvent()), productivity_ev(sgd.PopEvent()),
      numGfNotWorking(sgd.PopUnsignedShort()), sinceNotWorking(sgd.PopUnsignedInt()),
      outOfRessourcesMsgSent(sgd.PopBool()), ordertool_ev(nullptr), stepSpeed_(100), is_working(sgd.PopBool())
{
    const auto& slots = BLD_WORK_DESC[bldType_].inputs;
    if(sgd.PopUnsignedChar() != slots.size())
        throw SerializedGameData::Error("Inputs of a building in the savegame do not fit the lua files");
    for(const InputSlot& slot : slots)
    {
        InputStock input;
        if(sgd.PopUnsignedChar() != slot.wares.size())
            throw SerializedGameData::Error("Inputs of a building in the savegame do not fit the lua files");
        input.counts.resize(slot.wares.size());
        for(uint8_t& count : input.counts)
            count = sgd.PopUnsignedChar();
        sgd.PopObjectContainer(input.ordered, GO_Type::Ware);
        input.prepaid = sgd.PopUnsignedShort();
        inputs_.push_back(std::move(input));
    }
    lastConsumedWare_ = sgd.PopOptionalEnum<GoodType>();
    helpers::popContainer(sgd, lastProductivities);

    // Civilization mode: library
    if(sgd.GetGameDataVersion() >= 19 && sgd.PopBool())
    {
        const unsigned numEntries = sgd.PopUnsignedInt();
        for(unsigned i = 0; i < numEntries; ++i)
        {
            const auto good = helpers::popEnum<GoodType>(sgd);
            libraryMedia_[good] = sgd.PopUnsignedInt();
        }
        sgd.PopObjectContainer(libraryOrderedMedia_, GO_Type::Ware);
    }

    // Civilization mode: tool reform
    if(sgd.GetGameDataVersion() >= 22)
    {
        toolSlots_.resize(sgd.PopUnsignedInt());
        for(ToolSlot& slot : toolSlots_)
        {
            slot.tool = sgd.PopOptionalEnum<GoodType>().value_or(GoodType::Nothing);
            slot.stepsLeft = sgd.PopUnsignedShort();
            slot.spare = sgd.PopOptionalEnum<GoodType>().value_or(GoodType::Nothing);
            slot.ordered = sgd.PopObject<Ware>(GO_Type::Ware);
        }
        ordertool_ev = sgd.PopEvent();
        stepSpeed_ = sgd.PopUnsignedShort();
    }
    // Civilization mode: output ratio
    if(sgd.GetGameDataVersion() >= 24)
    {
        stepOutputRatio_ = sgd.PopUnsignedShort();
        outputCredit_ = sgd.PopUnsignedShort();
    }
    if(sgd.GetGameDataVersion() >= 30)
    {
        finishedGF_ = sgd.PopUnsignedInt();
        workerSinceGF_ = sgd.PopUnsignedInt();
    }
    // The lua files may have changed (and older savegames have no tools): Buildings start without the new tools
    const std::size_t numSlots = GetToolsDesc().slots.size();
    for(std::size_t i = numSlots; i < toolSlots_.size(); ++i)
    {
        if(toolSlots_[i].ordered)
            throw SerializedGameData::Error("Tools of a building in the savegame do not fit the lua files");
    }
    toolSlots_.resize(numSlots);
}

void nobUsual::Serialize(SerializedGameData& sgd) const
{
    noBuilding::Serialize(sgd);

    sgd.PushObject(worker);
    sgd.PushUnsignedShort(productivity);
    sgd.PushBool(disableProduction);
    sgd.PushUnsignedChar(lastOrderedWare);
    sgd.PushEvent(orderware_ev);
    sgd.PushEvent(productivity_ev);
    sgd.PushUnsignedShort(numGfNotWorking);
    sgd.PushUnsignedInt(sinceNotWorking);
    sgd.PushBool(outOfRessourcesMsgSent);
    sgd.PushBool(is_working);

    sgd.PushUnsignedChar(static_cast<uint8_t>(inputs_.size()));
    for(const InputStock& input : inputs_)
    {
        sgd.PushUnsignedChar(static_cast<uint8_t>(input.counts.size()));
        for(const uint8_t count : input.counts)
            sgd.PushUnsignedChar(count);
        sgd.PushObjectContainer(input.ordered, true);
        sgd.PushUnsignedShort(input.prepaid);
    }
    sgd.PushOptionalEnum<uint16_t>(lastConsumedWare_);
    helpers::pushContainer(sgd, lastProductivities);

    // Civilization mode: library
    const bool hasLibraryData = !libraryMedia_.empty() || !libraryOrderedMedia_.empty();
    sgd.PushBool(hasLibraryData);
    if(hasLibraryData)
    {
        sgd.PushUnsignedInt(static_cast<unsigned>(libraryMedia_.size()));
        for(const auto& entry : libraryMedia_)
        {
            helpers::pushEnum<uint16_t>(sgd, entry.first);
            sgd.PushUnsignedInt(entry.second);
        }
        sgd.PushObjectContainer(libraryOrderedMedia_, true);
    }

    // Civilization mode: tool reform
    sgd.PushUnsignedInt(static_cast<unsigned>(toolSlots_.size()));
    for(const ToolSlot& slot : toolSlots_)
    {
        // GoodType::Nothing is no valid value in savegames
        const auto toOptional = [](const GoodType good) {
            return good == GoodType::Nothing ? helpers::OptionalEnum<GoodType>() :
                                               helpers::OptionalEnum<GoodType>(good);
        };
        sgd.PushOptionalEnum<uint16_t>(toOptional(slot.tool));
        sgd.PushUnsignedShort(slot.stepsLeft);
        sgd.PushOptionalEnum<uint16_t>(toOptional(slot.spare));
        sgd.PushObject(slot.ordered, true);
    }
    sgd.PushEvent(ordertool_ev);
    sgd.PushUnsignedShort(stepSpeed_);
    sgd.PushUnsignedShort(stepOutputRatio_);
    sgd.PushUnsignedShort(outputCredit_);
    sgd.PushUnsignedInt(finishedGF_);
    sgd.PushUnsignedInt(workerSinceGF_);
}

nobUsual::~nobUsual() = default;

void nobUsual::DestroyBuilding()
{
    // Arbeiter Bescheid sagen
    if(worker)
    {
        // Tool reform: The worker takes the tools along which are still good (at least half of their durability
        // left) and brings them to a warehouse. The others are lost with the building
        std::vector<GoodType> tools;
        for(const ToolSlot& slot : toolSlots_)
        {
            const civ::ToolInfo* info = civ::getActiveContent().GetTool(slot.tool);
            if(info && slot.stepsLeft * 2u >= info->durability)
                tools.push_back(slot.tool);
            if(slot.spare != GoodType::Nothing)
                tools.push_back(slot.spare);
        }
        if(!tools.empty())
            worker->TakeTools(std::move(tools));
        worker->LostWork();
        worker = nullptr;
    } else
        world->GetPlayer(player).JobNotWanted(this);

    // Bestellte Waren Bescheid sagen
    for(InputStock& input : inputs_)
    {
        for(Ware* ware : input.ordered)
            WareNotNeeded(ware);
        input.ordered.clear();
    }
    for(Ware* ware : libraryOrderedMedia_)
        WareNotNeeded(ware);
    libraryOrderedMedia_.clear();
    // Tools in the building are lost with it (they are not part of the inventory anymore)
    for(ToolSlot& slot : toolSlots_)
    {
        if(slot.ordered)
            WareNotNeeded(slot.ordered);
        slot = ToolSlot();
    }

    // Events löschen
    GetEvMgr().RemoveEvent(ordertool_ev);
    GetEvMgr().RemoveEvent(orderware_ev);
    GetEvMgr().RemoveEvent(productivity_ev);

    // Inventur entsprechend verringern wegen den Waren, die vernichtetet werden
    const auto& slots = BLD_WORK_DESC[bldType_].inputs;
    for(unsigned i = 0; i < slots.size(); ++i)
    {
        for(unsigned w = 0; w < slots[i].wares.size(); ++w)
            world->GetPlayer(player).DecreaseInventoryWare(slots[i].wares[w], inputs_[i].counts[w], WareUse::Lost);
    }
    // Media in a library are lost with the building (and so are their knowledge points)
    for(const auto& entry : libraryMedia_)
        world->GetPlayer(player).DecreaseInventoryWare(entry.first, entry.second, WareUse::Lost);
    libraryMedia_.clear();
}

void nobUsual::Draw(DrawPoint drawPt)
{
    // Gebäude an sich zeichnen
    DrawBaseBuilding(drawPt);

    // Wenn Produktion gestoppt ist, Schild außen am Gebäude zeichnen zeichnen
    if(disableProductionVirtual)
        LOADER.GetMapTexture(46)->DrawFull(drawPt + BUILDING_SIGN_CONSTS[nation][civ::getLook(bldType_)]);

    // Rauch zeichnen

    // Raucht dieses Gebäude und ist es in Betrieb? (nur arbeitende Gebäude rauchen schließlich)
    if(is_working && BUILDING_SMOKE_CONSTS[nation][civ::getLook(bldType_)].type)
    {
        // Dann Qualm zeichnen (damit Qualm nicht synchron ist, x- und y- Koordinate als Unterscheidung
        LOADER
          .GetMapTexture(692 + BUILDING_SMOKE_CONSTS[nation][civ::getLook(bldType_)].type * 8
                         + GAMECLIENT.GetGlobalAnimation(8, 5, 2, (GetX() + GetY()) * 100))
          ->DrawFull(drawPt + BUILDING_SMOKE_CONSTS[nation][civ::getLook(bldType_)].offset, 0x99EEEEEE);
    }

    // TODO: zusätzliche Dinge wie Mühlenräder, Schweinchen etc bei bestimmten Gebäuden zeichnen

    // Bei Mühle, wenn sie nicht arbeitet, immer Mühlenräder (nichtdrehend) zeichnen
    if(bldType_ == BuildingType::Mill && !is_working)
    {
        // Flügel der Mühle
        LOADER.GetNationImage(nation, 250 + 5 * 49)->DrawFull(drawPt);
        // Schatten der Flügel
        LOADER.GetNationImage(nation, 250 + 5 * 49 + 1)->DrawFull(drawPt, COLOR_SHADOW);
    }
    // Esel in den Kammer bei Eselzucht zeichnen
    else if(bldType_ == BuildingType::DonkeyBreeder)
    {
        // Für alle Völker jeweils
        // X-Position der Esel
        constexpr helpers::EnumArray<std::array<DrawPoint, 3>, Nation> DONKEY_OFFSETS = {
          {{{{13, -9}, {26, -9}, {39, -9}}},
           {{{3, -17}, {16, -17}, {30, -17}}},
           {{{2, -21}, {15, -21}, {29, -21}}},
           {{{7, -17}, {18, -17}, {30, -17}}},
           {{{3, -22}, {16, -22}, {30, -22}}}}};
        // Animations-IDS des Esels
        const std::array<unsigned char, 25> DONKEY_ANIMATION = {
          {0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 4, 5, 6, 5, 7, 6, 5, 4, 3, 2, 1, 0}};

        // Die drei Esel zeichnen mithilfe von Globalanimation
        // Anzahl hängt von Produktivität der Eselzucht ab:
        // 0-29 - kein Esel
        // 30-60 - 1 Esel
        // 60-90 - 2 Esel
        // 90-100 - 3 Esel
        RTTR_Assert(productivity <= 100u);
        for(unsigned i = 0; i < productivity / 30u; i++)
        {
            unsigned animationFrame = DONKEY_ANIMATION[GAMECLIENT.GetGlobalAnimation(
              DONKEY_ANIMATION.size(), 5, 2, GetX() * (player + 2) + GetY() * i)];
            LOADER.GetMapTexture(2180 + animationFrame)->DrawFull(drawPt + DONKEY_OFFSETS[nation][i]);
        }
    }
    // Bei Katapulthaus Katapult oben auf dem Dach zeichnen, falls er nicht "arbeitet"
    else if(bldType_ == BuildingType::Catapult && !is_working)
    {
        LOADER.GetPlayerImage("rom_bobs", 1776)->DrawFull(drawPt - DrawPoint(7, 19));
    }

    // Bei Schweinefarm Schweinchen auf dem Hof zeichnen
    else if(bldType_ == BuildingType::PigFarm && this->HasWorker())
    {
        // Position der 5 Schweinchen für alle 4 Völker (1. ist das große Schwein)
        constexpr helpers::EnumArray<std::array<DrawPoint, 5>, Nation> PIG_POSITIONS = {{
          //  gr. S. 1.klS 2. klS usw
          {{{3, -8}, {17, 3}, {-12, 4}, {-2, 10}, {-22, 11}}},    // Afrikaner
          {{{-16, 0}, {-37, 0}, {-32, 8}, {-16, 10}, {-22, 18}}}, // Japaner
          {{{-15, 0}, {-4, 9}, {-22, 10}, {2, 19}, {-15, 20}}},   // Römer
          {{{5, -5}, {25, -12}, {-7, 7}, {-23, 11}, {-10, 14}}},  // Wikinger
          {{{-16, 5}, {-37, 5}, {-32, -1}, {-16, 15}, {-27, 18}}} // Babylonier
        }};

        /// Großes Schwein zeichnen
        LOADER.GetMapTexture(2160)->DrawFull(drawPt + PIG_POSITIONS[nation][0], COLOR_SHADOW);
        LOADER.GetMapTexture(2100 + GAMECLIENT.GetGlobalAnimation(12, 3, 1, GetX() + GetY() + GetObjId()))
          ->DrawFull(drawPt + PIG_POSITIONS[nation][0]);

        // Die 4 kleinen Schweinchen, je nach Produktivität
        for(unsigned i = 1; i < std::min(unsigned(productivity) / 20u + 1u, 5u); ++i)
        {
            // A random (really, dice-rolled by hand:) ) order of the four possible pig animations, with eating three
            // times as much as the others ones  To get random-looking, non synchronous, sweet little pigs
            const std::array<unsigned char, 63> smallpig_animations = {
              0, 0, 3, 2, 0, 0, 1, 3, 0, 3, 1, 3, 2, 0, 0, 1, 0, 0, 1, 3, 2, 0, 1, 1, 0, 0, 2, 1, 0, 1, 0, 2,
              2, 0, 0, 2, 2, 0, 1, 0, 3, 1, 2, 0, 1, 2, 2, 0, 0, 0, 3, 0, 2, 0, 3, 0, 3, 0, 1, 1, 0, 3, 0};
            const unsigned short animpos =
              GAMECLIENT.GetGlobalAnimation(63 * 12, 63 * 4 - i * 5, 1, 183 * i + GetX() * GetObjId() + GetY() * i);
            LOADER.GetMapTexture(2160)->DrawFull(drawPt + PIG_POSITIONS[nation][i], COLOR_SHADOW);
            LOADER.GetMapTexture(2112 + smallpig_animations[animpos / 12] * 12 + animpos % 12)
              ->DrawFull(drawPt + PIG_POSITIONS[nation][i]);
        }

        // Ggf. Sounds abspielen (oink oink), da soll sich der Schweinezüchter drum kümmen
        dynamic_cast<nofPigbreeder*>(worker)->MakePigSounds(); //-V522
    }
    // Bei nubischen Bergwerken das Feuer vor dem Bergwerk zeichnen
    else if(BuildingProperties::IsMine(bldType_) && worker && nation == Nation::Africans)
    {
        DrawPoint offset;
        // Mines of the civilization mode look like their look building
        switch(civ::getLook(bldType_))
        {
            case BuildingType::GraniteMine: offset = NUBIAN_MINE_FIRE[0]; break;
            case BuildingType::CoalMine: offset = NUBIAN_MINE_FIRE[1]; break;
            case BuildingType::IronMine: offset = NUBIAN_MINE_FIRE[2]; break;
            case BuildingType::GoldMine: offset = NUBIAN_MINE_FIRE[3]; break;
            default: return; // Custom mine looking like another building: No fire
        }
        LOADER.GetMapTexture(740 + GAMECLIENT.GetGlobalAnimation(8, 5, 2, GetObjId() + GetX() + GetY()))
          ->DrawFull(drawPt + offset);
    }
}

void nobUsual::HandleEvent(const unsigned id)
{
    if(id == 2)
    {
        // Tool reform: order tools regularly (they may not have been in stock)
        OrderTools();
        ordertool_ev = GetEvMgr().AddEvent(this, 210, 2);
    } else if(id)
    {
        const unsigned short currentProductivity = CalcCurrentProductivity();
        // Sum over all last productivities and current (as start value)
        productivity = std::accumulate(lastProductivities.begin(), lastProductivities.end(), currentProductivity);
        // And average over those N+1 values
        productivity /= lastProductivities.size() + 1u;

        // Move productivities to the right (removing the last element)
        std::copy_backward(lastProductivities.begin(), lastProductivities.end() - 1, lastProductivities.end());
        lastProductivities.front() = currentProductivity;

        // Add next event
        productivity_ev = GetEvMgr().AddEvent(this, numProductivityGFs, 1);
    } else if(IsLibrary())
    {
        OrderLibraryMedium();
        orderware_ev = GetEvMgr().AddEvent(this, 210);
    } else
    {
        // Ware bestellen (falls noch Platz ist) und nicht an Betriebe, die stillgelegt wurden!
        if(!IsProductionStopped())
        {
            const BldWorkDescription& workDesc = BLD_WORK_DESC[bldType_];
            RTTR_Assert(lastOrderedWare < workDesc.inputs.size());
            // One ware for the next input, if it has space left
            if(GetNumStoredAndOrdered(lastOrderedWare) < workDesc.inputs[lastOrderedWare].storage)
                OrderInput(lastOrderedWare);

            ++lastOrderedWare;
            if(lastOrderedWare >= workDesc.inputs.size())
                lastOrderedWare = 0;
        }

        // Nach ner bestimmten Zeit dann nächste Ware holen
        orderware_ev = GetEvMgr().AddEvent(this, 210);
    }
}

void nobUsual::AddWare(std::unique_ptr<Ware> ware)
{
    for(ToolSlot& slot : toolSlots_)
    {
        if(slot.ordered != ware.get())
            continue;
        // A tool arrived: Use it or keep it as replacement. Tools in buildings are not part of the inventory
        slot.ordered = nullptr;
        if(slot.tool == GoodType::Nothing)
        {
            const civ::ToolInfo* info = civ::getActiveContent().GetTool(ware->type);
            slot.tool = ware->type;
            slot.stepsLeft = static_cast<uint16_t>(info ? info->durability : 1u);
        } else
            slot.spare = ware->type;
        GamePlayer& owner = world->GetPlayer(player);
        owner.DecreaseInventoryWare(ware->type, 1, WareUse::Tool);
        owner.RemoveWare(*ware);
        if(worker)
            worker->GotWareOrProductionAllowed();
        return;
    }

    if(IsLibrary())
    {
        // Knowledge medium arrived: store it (it stays in the inventory of the player)
        ++libraryMedia_[ware->type];
        RTTR_Assert(helpers::contains(libraryOrderedMedia_, ware.get()));
        libraryOrderedMedia_.remove(ware.get());
        world->GetPlayer(player).RemoveWare(*ware);
        return;
    }

    // Into the input it was ordered for
    const auto& slots = BLD_WORK_DESC[bldType_].inputs;
    for(unsigned i = 0; i < inputs_.size(); ++i)
    {
        if(!helpers::contains(inputs_[i].ordered, ware.get()))
            continue;
        const auto it = std::find(slots[i].wares.begin(), slots[i].wares.end(), ware->type);
        RTTR_Assert(it != slots[i].wares.end());
        if(it != slots[i].wares.end())
            ++inputs_[i].counts[static_cast<unsigned>(it - slots[i].wares.begin())];
        inputs_[i].ordered.remove(ware.get());
        break;
    }

    // Ware vernichten
    world->GetPlayer(player).RemoveWare(*ware);

    // Arbeiter Bescheid sagen, dass es neue Waren gibt
    if(worker)
        worker->GotWareOrProductionAllowed();
}

bool nobUsual::FreePlaceAtFlag()
{
    // Arbeiter Bescheid sagen, falls es noch keinen gibt, brauch keine Ware rausgetragen werden
    if(worker)
        return worker->FreePlaceAtFlag();
    else
        return false;
}

void nobUsual::WareLost(Ware& ware)
{
    for(ToolSlot& slot : toolSlots_)
    {
        if(slot.ordered == &ware)
        {
            slot.ordered = nullptr;
            return;
        }
    }

    if(IsLibrary())
    {
        RTTR_Assert(helpers::contains(libraryOrderedMedia_, &ware));
        libraryOrderedMedia_.remove(&ware);
        return;
    }

    // Ware konnte nicht kommen --> raus damit
    for(InputStock& input : inputs_)
    {
        if(helpers::contains(input.ordered, &ware))
        {
            input.ordered.remove(&ware);
            return;
        }
    }
    RTTR_Assert(false);
}

void nobUsual::GotWorker(Job /*job*/, noFigure& worker)
{
    this->worker = checkedCast<nofBuildingWorker*>(&worker);
    StartToolOrderEvent();

    if(!BLD_WORK_DESC[bldType_].inputs.empty() || IsLibrary())
        // erste Ware bestellen
        HandleEvent(0);
}

void nobUsual::WorkerLost()
{
    workerSinceGF_ = NO_GF;
    // Check if worker is or was here (e.g. hunter could currently be outside)
    if(HasWorker())
    {
        // If we have a worker, we must be producing something
        RTTR_Assert(productivity_ev);
        // Open the door till we get a new worker
        OpenDoor();
    }
    // Produktivitätsevent ggf. abmelden
    GetEvMgr().RemoveEvent(productivity_ev);

    // Waren-Bestell-Event abmelden
    GetEvMgr().RemoveEvent(orderware_ev);
    GetEvMgr().RemoveEvent(ordertool_ev);

    // neuen Arbeiter bestellen
    worker = nullptr;
    world->GetPlayer(player).AddJobWanted(BLD_WORK_DESC[bldType_].job.value(), this);
}

unsigned nobUsual::GetNumWares(const unsigned input) const
{
    return std::accumulate(inputs_[input].counts.begin(), inputs_[input].counts.end(), 0u);
}

unsigned nobUsual::GetNumStoredAndOrdered(const unsigned input) const
{
    return GetNumWares(input) + static_cast<unsigned>(inputs_[input].ordered.size());
}

unsigned nobUsual::GetNumNeededForStep(const unsigned input) const
{
    const unsigned amount = BLD_WORK_DESC[bldType_].inputs[input].amount;
    const unsigned prepaid = inputs_[input].prepaid;
    // A part of a ware counts as a whole one (it is taken now, the rest counts for the next steps)
    return amount > prepaid ? (amount - prepaid + 99u) / 100u : 0u;
}

bool nobUsual::WaresAvailable()
{
    const BldWorkDescription& workDesc = BLD_WORK_DESC[bldType_];
    if(workDesc.useOneWareEach)
    {
        // Every input has the wares of a step
        for(unsigned i = 0; i < inputs_.size(); ++i)
        {
            if(GetNumWares(i) < GetNumNeededForStep(i))
                return false;
        }
        return true;
    } else
    {
        // Built-in mines: any ware there
        for(unsigned i = 0; i < inputs_.size(); ++i)
        {
            if(GetNumWares(i) != 0)
                return true;
        }
        return false;
    }
}

void nobUsual::ConsumeWares()
{
    const BldWorkDescription& workDesc = BLD_WORK_DESC[bldType_];
    if(inputs_.empty())
        return;

    GamePlayer& owner = world->GetPlayer(player);
    // One ware of the input: the ware the input has most of, equal: the one first in the list of the input
    const auto takeOne = [&](const unsigned i) {
        InputStock& input = inputs_[i];
        unsigned best = 0;
        for(unsigned w = 1; w < input.counts.size(); ++w)
        {
            if(input.counts[w] > input.counts[best])
                best = w;
        }
        RTTR_Assert(input.counts[best] != 0);
        --input.counts[best];
        const GoodType good = workDesc.inputs[i].wares[best];
        owner.DecreaseInventoryWare(good, 1, WareUse::Production);
        owner.CountUsedBy(GetBuildingType(), good);
        return good;
    };
    // Try to get wares from the warehouses: below the wares of one more step (at least 2 wares), as many as a step
    // takes
    const auto refill = [&](const unsigned i) {
        const unsigned perStep = std::max(1u, (workDesc.inputs[i].amount + 99u) / 100u);
        if(GetNumWares(i) >= perStep + 1u)
            return;
        for(unsigned n = 0; n < perStep && GetNumStoredAndOrdered(i) < workDesc.inputs[i].storage; ++n)
            OrderInput(i);
    };

    if(!workDesc.useOneWareEach)
    {
        // Built-in mines: 1 ware of the input with the most wares
        unsigned best = 0, numBest = 0;
        for(unsigned i = 0; i < inputs_.size(); ++i)
        {
            if(GetNumWares(i) > numBest)
            {
                best = i;
                numBest = GetNumWares(i);
            }
        }
        lastConsumedWare_ = takeOne(best);
        refill(best);
        return;
    }

    for(unsigned i = 0; i < inputs_.size(); ++i)
    {
        const unsigned needed = GetNumNeededForStep(i);
        for(unsigned n = 0; n < needed; ++n)
        {
            const GoodType good = takeOne(i);
            if(i == 0)
                lastConsumedWare_ = good;
        }
        // What was taken beyond this step counts for the next ones
        inputs_[i].prepaid = static_cast<uint16_t>(inputs_[i].prepaid + needed * 100u - workDesc.inputs[i].amount);
        refill(i);
    }
}

Ware* nobUsual::OrderInput(const unsigned input)
{
    orderingInput_ = static_cast<int>(input);
    Ware* ware = world->GetPlayer(player).OrderWare(BLD_WORK_DESC[bldType_].inputs[input].wares, *this);
    orderingInput_ = -1;
    RTTR_Assert(!ware || helpers::contains(inputs_[input].ordered, ware));
    return ware;
}

int nobUsual::FindInputFor(const GoodType good) const
{
    const auto& slots = BLD_WORK_DESC[bldType_].inputs;
    for(unsigned i = 0; i < slots.size(); ++i)
    {
        if(helpers::contains(slots[i].wares, good) && GetNumStoredAndOrdered(i) < slots[i].storage)
            return static_cast<int>(i);
    }
    return -1;
}

unsigned nobUsual::CalcDistributionPoints(const GoodType type)
{
    // No production -> nothing needed
    if(IsProductionStopped())
        return 0;

    if(IsLibrary())
    {
        // A library takes every knowledge medium while there is space. No worker -> no orders yet
        if(!worker || !IsKnowledgeMedium(type))
            return 0;
        const unsigned numMedia = GetNumLibraryMedia() + GetNumOrderedLibraryMedia();
        if(numMedia >= GetLibraryCapacity())
            return 0;
        // Same rating as for other buildings: 10000 minus 30 per ware stored or on the way (capacity <= 255)
        return 10000 - numMedia * 30;
    }

    // The input taking this ware (the first one with space left). None: enough, don't request more
    const int id = FindInputFor(type);
    if(id < 0)
        return 0;
    const unsigned stored = GetNumStoredAndOrdered(static_cast<unsigned>(id));

    // 10000 as base points then subtract some
    // Note: Subtracted points must be at most this.
    unsigned points = 10000;

    // Every ware we have or is on the way reduces rating
    // Note: maxValue is storage * 30 which is <= 255*30=7650 (< 10000 base value)
    points -= stored * 30;

    RTTR_Assert(points <= 10000);

    return points;
}

void nobUsual::TakeWare(Ware* ware)
{
    if(orderingToolSlot_ >= 0)
    {
        // Ordered tool (see OrderTools)
        toolSlots_[static_cast<unsigned>(orderingToolSlot_)].ordered = ware;
        return;
    }

    if(IsLibrary())
    {
        RTTR_Assert(!helpers::contains(libraryOrderedMedia_, ware));
        libraryOrderedMedia_.push_back(ware);
        return;
    }

    // Ware in die Bestellliste aufnehmen: the input ordering it, otherwise the input taking it
    const int id = orderingInput_ >= 0 ? orderingInput_ : FindInputFor(ware->type);
    RTTR_Assert(id >= 0);
    if(id >= 0)
    {
        RTTR_Assert(!helpers::contains(inputs_[id].ordered, ware));
        inputs_[id].ordered.push_back(ware);
    }
}

bool nobUsual::AreThereAnyOrderedWares() const
{
    return !libraryOrderedMedia_.empty()
           || helpers::contains_if(inputs_, [](const InputStock& input) { return !input.ordered.empty(); })
           || helpers::contains_if(toolSlots_, [](const ToolSlot& slot) { return slot.ordered != nullptr; });
}

const civ::BuildingTools& nobUsual::GetToolsDesc() const
{
    static const civ::BuildingTools noTools;
    return civ::isEnabled(*world) ? civ::getActiveContent().GetBuildingTools(bldType_) : noTools;
}

unsigned nobUsual::GetWorkSpeed() const
{
    if(toolSlots_.empty())
        return civ::isEnabled(*world) ? GetToolsDesc().baseSpeed : 100u;
    const civ::Content& content = civ::getActiveContent();
    unsigned speed = GetToolsDesc().baseSpeed;
    for(const ToolSlot& slot : toolSlots_)
    {
        if(const civ::ToolInfo* info = content.GetTool(slot.tool))
            speed += info->speed;
    }
    return speed;
}

unsigned nobUsual::GetOutputRatio() const
{
    if(!civ::isEnabled(*world))
        return 100;
    const civ::Content& content = civ::getActiveContent();
    unsigned ratio = content.GetOutputRatio(bldType_);
    for(const ToolSlot& slot : toolSlots_)
    {
        if(const civ::ToolInfo* info = content.GetTool(slot.tool))
            ratio += info->outputRatio;
    }
    return std::min(ratio, MAX_OUTPUT_RATIO);
}

unsigned nobUsual::TakeNumProducts()
{
    const unsigned total = outputCredit_ + stepOutputRatio_;
    outputCredit_ = static_cast<uint16_t>(total % 100u);
    return total / 100u;
}

bool nobUsual::BeginWorkStep()
{
    // Output ratio 0: The work would be in vain
    stepOutputRatio_ = static_cast<uint16_t>(GetOutputRatio());
    if(stepOutputRatio_ == 0)
        return false;
    if(toolSlots_.empty())
    {
        stepSpeed_ = static_cast<uint16_t>(GetWorkSpeed());
        return stepSpeed_ > 0;
    }
    // Savegames from before the tool reform have no event yet
    StartToolOrderEvent();
    const unsigned speed = GetWorkSpeed();
    if(speed == 0)
        return false;
    stepSpeed_ = static_cast<uint16_t>(std::min(speed, 60000u));
    // Every work step wears the tools out. A broken tool is replaced by the replacement waiting in the building
    const civ::Content& content = civ::getActiveContent();
    bool needsOrder = false;
    for(ToolSlot& slot : toolSlots_)
    {
        if(slot.tool == GoodType::Nothing)
            continue;
        if(slot.stepsLeft > 0)
            --slot.stepsLeft;
        if(slot.stepsLeft == 0)
        {
            slot.tool = slot.spare;
            slot.spare = GoodType::Nothing;
            const civ::ToolInfo* info = content.GetTool(slot.tool);
            slot.stepsLeft = static_cast<uint16_t>(info ? info->durability : 0u);
        }
        needsOrder |= NeedsToolOrder(slot);
    }
    if(needsOrder)
        OrderTools();
    return true;
}

unsigned nobUsual::ScaleWorkDuration(const unsigned gfs) const
{
    if(stepSpeed_ == 100 || stepSpeed_ == 0 || gfs == 0)
        return gfs;
    return std::max(1u, (gfs * 100u + stepSpeed_ / 2u) / stepSpeed_);
}

unsigned nobUsual::GetNumMissingTools(const GoodType tool) const
{
    if(toolSlots_.empty())
        return 0;
    const auto& slotDescs = GetToolsDesc().slots;
    unsigned result = 0;
    for(unsigned i = 0; i < toolSlots_.size(); ++i)
    {
        if(NeedsToolOrder(toolSlots_[i]) && helpers::contains(slotDescs[i].wares, tool))
            ++result;
    }
    return result;
}

bool nobUsual::NeedsToolOrder(const ToolSlot& slot) const
{
    if(slot.ordered || slot.spare != GoodType::Nothing)
        return false;
    if(slot.tool == GoodType::Nothing)
        return true;
    // Order the replacement shortly before the tool breaks
    return slot.stepsLeft <= civ::getActiveContent().GetToolReorderSteps(slot.tool);
}

void nobUsual::OrderTools()
{
    if(toolSlots_.empty() || IsProductionStopped())
        return;
    const civ::Content& content = civ::getActiveContent();
    const auto& slotDescs = GetToolsDesc().slots;
    GamePlayer& owner = world->GetPlayer(player);
    const auto priorities = owner.GetResearch().GetToolMaterialPriorities(content);
    for(unsigned i = 0; i < toolSlots_.size(); ++i)
    {
        ToolSlot& slot = toolSlots_[i];
        if(!NeedsToolOrder(slot))
            continue;
        // Tools by the priority of their material (regulator "Werkzeuge"), then by speed. Priority 0: never
        std::vector<GoodType> candidates;
        for(const GoodType good : slotDescs[i].wares)
        {
            if(priorities[content.GetTool(good)->material] > 0)
                candidates.push_back(good);
        }
        std::stable_sort(candidates.begin(), candidates.end(), [&](const GoodType lhs, const GoodType rhs) {
            const civ::ToolInfo& lhsInfo = *content.GetTool(lhs);
            const civ::ToolInfo& rhsInfo = *content.GetTool(rhs);
            if(priorities[lhsInfo.material] != priorities[rhsInfo.material])
                return priorities[lhsInfo.material] > priorities[rhsInfo.material];
            return lhsInfo.speed > rhsInfo.speed;
        });
        for(const GoodType good : candidates)
        {
            orderingToolSlot_ = static_cast<int>(i);
            Ware* ware = owner.OrderWare(good, *this);
            orderingToolSlot_ = -1;
            if(ware)
            {
                slot.ordered = ware;
                break;
            }
        }
    }
}

void nobUsual::StartToolOrderEvent()
{
    if(toolSlots_.empty() || ordertool_ev)
        return;
    OrderTools();
    ordertool_ev = GetEvMgr().AddEvent(this, 210, 2);
}

bool nobUsual::IsLibrary() const
{
    return GetLibraryCapacity() > 0u;
}

unsigned nobUsual::GetLibraryCapacity() const
{
    const civ::CustomBuilding* building = civ::getActiveContent().GetBuilding(bldType_);
    return building ? building->libraryCapacity : 0u;
}

unsigned nobUsual::GetNumLibraryMedia() const
{
    unsigned result = 0;
    for(const auto& entry : libraryMedia_)
        result += entry.second;
    return result;
}

bool nobUsual::IsKnowledgeMedium(const GoodType good)
{
    return helpers::contains(civ::getActiveContent().GetKnowledgeMedia(), good);
}

void nobUsual::OrderLibraryMedium()
{
    if(IsProductionStopped() || GetNumLibraryMedia() + GetNumOrderedLibraryMedia() >= GetLibraryCapacity())
        return;
    // Try the media in turn, starting after the last one ordered, until one is available in a warehouse
    const std::vector<GoodType>& media = civ::getActiveContent().GetKnowledgeMedia();
    for(unsigned i = 0; i < media.size(); ++i)
    {
        const unsigned idx = (lastOrderedWare + i) % media.size();
        if(world->GetPlayer(player).OrderWare(media[idx], *this))
        {
            lastOrderedWare = static_cast<unsigned char>((idx + 1) % media.size());
            return;
        }
    }
}

void nobUsual::WorkerArrived()
{
    productivity_ev = GetEvMgr().AddEvent(this, numProductivityGFs, 1);
    workerSinceGF_ = GetEvMgr().GetCurrentGF();
}

bool nobUsual::IsPausedByDistribution() const
{
    return civ::isPausedByDistribution(*world, *this);
}

bool nobUsual::IsProductionStopped() const
{
    return disableProduction || IsPausedByDistribution();
}

void nobUsual::SetProductionEnabled(const bool enabled)
{
    if(disableProduction == !enabled)
        return;
    // Umstellen
    disableProduction = !enabled;
    // Wenn das von einem fremden Spieler umgestellt wurde (oder vom Replay), muss auch das visuelle umgestellt werden
    if(GAMECLIENT.GetPlayerId() != player || GAMECLIENT.IsReplayModeOn())
        disableProductionVirtual = disableProduction;

    if(disableProduction)
    {
        // Wenn sie deaktiviert wurde, dem Arbeiter Bescheid sagen, damit er entsprechend stoppt, falls er schon
        // auf die Arbeit warteet
        if(worker)
            worker->ProductionStopped();
    } else
    {
        // Wenn sie wieder aktiviert wurde, evtl wieder mit arbeiten anfangen, falls es einen Arbeiter gibt
        if(worker)
            worker->GotWareOrProductionAllowed();
    }
}

bool nobUsual::HasWorker() const
{
    return worker && worker->GetState() != nofBuildingWorker::State::FigureWork;
}

void nobUsual::OnOutOfResources()
{
    // Post verschicken, keine Rohstoffe mehr da
    if(outOfRessourcesMsgSent)
        return;
    outOfRessourcesMsgSent = true;
    productivity = 0;
    std::fill(lastProductivities.begin(), lastProductivities.end(), 0);

    std::string error;
    const civ::CustomBuilding* customBld = civ::getActiveContent().GetBuilding(GetBuildingType());
    if(GetBuildingType() == BuildingType::Well)
        error = _("This well has dried out");
    else if(BuildingProperties::IsMine(GetBuildingType()))
        error = _("This mine is exhausted");
    else if(GetBuildingType() == BuildingType::Quarry)
        error = _("No more stones in range");
    else if(GetBuildingType() == BuildingType::Fishery)
        error = _("No more fish in range");
    else if(customBld && customBld->IsCollector())
    {
        // Collector of a workshop defined in lua (civilization mode)
        const civ::CustomMapObject* obj =
          civ::getActiveContent().GetMapObject(static_cast<unsigned>(customBld->collectObject));
        error = helpers::format(_("No more %1% in range"), obj ? obj->title : std::string());
    } else
        return;

    SendPostMessage(
      player, std::make_unique<PostMsgWithBuilding>(GetEvMgr().GetCurrentGF(), error, PostCategory::Economy, *this));
    world->GetNotifications().publish(BuildingNote(BuildingNote::NoRessources, player, GetPos(), GetBuildingType()));

    if(GAMECLIENT.GetPlayerId() == player && world->GetGGS().isEnabled(AddonId::DEMOLISH_BLD_WO_RES))
    {
        GAMECLIENT.DestroyBuilding(GetPos());
    }
}

void nobUsual::StartNotWorking()
{
    // If we haven't stopped working already, then this is the GF since we are not working anymore
    if(sinceNotWorking == isWorkingMarker)
        sinceNotWorking = GetEvMgr().GetCurrentGF();
}

void nobUsual::StopNotWorking()
{
    // Record the number of GFs we haven't worked, if any
    if(sinceNotWorking != isWorkingMarker)
    {
        numGfNotWorking += static_cast<unsigned short>(GetEvMgr().GetCurrentGF() - sinceNotWorking);
        sinceNotWorking = isWorkingMarker;
    }
}

unsigned short nobUsual::CalcCurrentProductivity()
{
    if(outOfRessourcesMsgSent)
        return 0;
    // Possibly add the number of GFs we haven't worked until now
    if(sinceNotWorking != isWorkingMarker)
    {
        const auto currentGF = GetEvMgr().GetCurrentGF();
        numGfNotWorking += static_cast<unsigned short>(currentGF - sinceNotWorking);
        // Reset marker
        sinceNotWorking = currentGF;
    }

    RTTR_Assert(numGfNotWorking <= numProductivityGFs);
    // Calculate the productivity
    static_assert(numProductivityGFs / 100u == 4u, "Cannot use simplified percentage calculation");
    const unsigned short curProductivity = (numProductivityGFs - numGfNotWorking) / 4u;

    // Reset counter
    numGfNotWorking = 0;

    return curProductivity;
}
