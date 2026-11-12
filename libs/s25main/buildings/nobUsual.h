// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "helpers/OptionalEnum.h"
#include "noBuilding.h"
#include "gameTypes/GoodTypes.h"
#include <array>
#include <list>
#include <map>
#include <vector>

class Ware;
class nofBuildingWorker;
namespace civ {
struct BuildingTools;
}
class SerializedGameData;
class noFigure;
class noRoadNode;
class GameEvent;

// Gewöhnliches Gebäude mit einem Arbeiter und Waren
class nobUsual : public noBuilding
{
public:
    /// Civilization mode, tool reform: One tool of the building (see civ::BuildingTools)
    struct ToolSlot
    {
        /// Tool in use or Nothing
        GoodType tool = GoodType::Nothing;
        /// Work steps until the tool in use is broken
        uint16_t stepsLeft = 0;
        /// Replacement waiting in the building or Nothing
        GoodType spare = GoodType::Nothing;
        /// Tool on its way to the building or nullptr
        Ware* ordered = nullptr;
    };

protected:
    /// Der Typ, der hier arbeitet
    nofBuildingWorker* worker;
    /// Produktivität
    unsigned short productivity;
    /// Produktion eingestellt? (letzteres nur visuell, um Netzwerk-Latenzen zu verstecken)
    bool disableProduction, disableProductionVirtual;
    /// Warentyp, den er zuletzt bestellt hatte (bei >1 Waren)
    unsigned char lastOrderedWare;
    /// Rohstoffe, die zur Produktion benötigt werden
    std::array<uint8_t, 3> numWares;
    /// Bestellte Waren
    std::vector<std::list<Ware*>> orderedWares;
    /// Bestell-Ware-Event
    const GameEvent* orderware_ev;
    /// Rechne-Produktivität-aus-Event
    const GameEvent* productivity_ev;
    /// Letzte Produktivitäten (Durchschnitt = Gesamt produktivität), vorne das neuste !
    std::array<uint16_t, 6> lastProductivities;
    /// How many GFs he did not work since the last productivity calculation
    unsigned short numGfNotWorking;
    /// Since which GF he did not work (0xFFFFFFFF = currently working)
    unsigned sinceNotWorking;
    /// Did we notify the player that we are out of resources?
    bool outOfRessourcesMsgSent;
    /// Ware consumed by the last call to ConsumeWares (first one if several). Not serialized, read right after
    /// consuming
    helpers::OptionalEnum<GoodType> lastConsumedWare_;
    /// Civilization mode, library: stored knowledge media (count per ware) and ordered media
    std::map<GoodType, unsigned> libraryMedia_;
    std::list<Ware*> libraryOrderedMedia_;
    /// Tool reform: Tools of the building (same order as civ::BuildingTools::slots). Empty without the tool reform
    std::vector<ToolSlot> toolSlots_;
    /// Tool reform: Event to order tools
    const GameEvent* ordertool_ev;
    /// Tool reform: Work speed in percent of the current work step
    uint16_t stepSpeed_;
    /// Civilization mode: Output ratio of the current work step in hundredths and the fraction of a ware left over
    /// from the previous work steps (e.g. 50 after the first step with ratio 0.5)
    uint16_t stepOutputRatio_ = 100;
    uint16_t outputCredit_ = 0;
    /// Tool reform: Slot a tool is ordered for right now (not serialized), -1: none
    int orderingToolSlot_ = -1;

    friend class SerializedGameData;
    friend class BuildingFactory;
    nobUsual(BuildingType type, MapPoint pos, unsigned char player, Nation nation);
    nobUsual(SerializedGameData& sgd, unsigned obj_id);

    void DestroyBuilding() override;

public:
    /// Wird gerade gearbeitet oder nicht?
    bool is_working;

    ~nobUsual() override;

    void Serialize(SerializedGameData& sgd) const override;

    GO_Type GetGOT() const override { return GO_Type::NobUsual; }
    unsigned GetMilitaryRadius() const override { return 0; }

    void Draw(DrawPoint drawPt) override;

    bool HasWorker() const;

    /// Event-Handler
    void HandleEvent(unsigned id) override;
    /// Legt eine Ware am Objekt ab (an allen Straßenknoten (Gebäude, Baustellen und Flaggen) kann man Waren ablegen
    void AddWare(std::unique_ptr<Ware> ware) override;
    /// Wird aufgerufen, wenn von der Fahne vor dem Gebäude ein Rohstoff aufgenommen wurde
    bool FreePlaceAtFlag() override;
    /// Eine bestellte Ware konnte doch nicht kommen
    void WareLost(Ware& ware) override;
    /// Wird aufgerufen, wenn ein Arbeiter für das Gebäude gefunden werden konnte
    void GotWorker(Job job, noFigure& worker) override;
    /// Wird vom Arbeiter aufgerufen, wenn er im Gebäude angekommen ist
    void WorkerArrived();
    /// Wird vom Arbeiter aufgerufen, wenn er nicht (mehr) zur Arbeit kommen kann
    void WorkerLost();

    /// Gibt den Warenbestand (eingehende Waren - Rohstoffe) zurück
    unsigned char GetNumWares(unsigned id) const { return numWares[id]; }
    /// Prüft, ob Waren für einen Arbeitsschritt vorhanden sind
    bool WaresAvailable();
    /// Verbraucht Waren
    void ConsumeWares();
    /// Ware consumed by the last call to ConsumeWares (first one if several) or nothing if nothing was consumed
    helpers::OptionalEnum<GoodType> GetLastConsumedWare() const { return lastConsumedWare_; }

    /// Berechnet Punktewertung für Ware type
    unsigned CalcDistributionPoints(GoodType type);

    /// Wird aufgerufen, wenn eine neue Ware zum dem Gebäude geliefert wird (nicht wenn sie bestellt wurde vom Gebäude!)
    void TakeWare(Ware* ware) override;

    /// Bestellte Waren
    bool AreThereAnyOrderedWares() const;

    /// Civilization mode: True if this is a library (workshop from lua storing knowledge media)
    bool IsLibrary() const;
    /// Library: Maximum number of stored media (0 if no library)
    unsigned GetLibraryCapacity() const;
    /// Library: Stored media per ware
    const std::map<GoodType, unsigned>& GetLibraryMedia() const { return libraryMedia_; }
    /// Library: Number of stored media
    unsigned GetNumLibraryMedia() const;
    /// Library: Number of media on the way to the library
    unsigned GetNumOrderedLibraryMedia() const { return static_cast<unsigned>(libraryOrderedMedia_.size()); }

    /// Tool reform: Tools of the building (empty without the tool reform)
    const std::vector<ToolSlot>& GetToolSlots() const { return toolSlots_; }
    /// Work speed in percent of the normal speed with the tools in the building. Always 100 without the tool reform
    unsigned GetWorkSpeed() const;
    /// Called by the worker when he wants to start a work step. Returns false if the building can not work with its
    /// tools (speed 0). Otherwise the tools wear out and the speed of this step is set
    bool BeginWorkStep();
    /// Duration of a part of the current work step at its work speed
    unsigned ScaleWorkDuration(unsigned gfs) const;
    /// Civilization mode: Wares per work step in hundredths: ratio of the building plus the bonus of its tools in use,
    /// at most MAX_OUTPUT_RATIO (8 wares). Always 100 outside the civilization mode
    unsigned GetOutputRatio() const;
    /// Called by the worker when the current work step produced a ware: Number of wares it yields according to the
    /// output ratio of the step (0 if only a fraction is reached yet, the fraction is kept for the next steps)
    unsigned TakeNumProducts();
    /// Number of tools of this ware the building is missing (nothing in use or ordered)
    unsigned GetNumMissingTools(GoodType tool) const;

    /// Gibt Pointer auf Produktivität zurück
    const unsigned short* GetProductivityPointer() const { return &productivity; }
    unsigned short GetProductivity() const { return productivity; }
    const nofBuildingWorker* GetWorker() const { return worker; }

    /// Stoppt/Erlaubt Produktion (visuell)
    void ToggleProductionVirtual() { disableProductionVirtual = !disableProductionVirtual; }
    /// Stoppt/Erlaubt Produktion (real)
    void SetProductionEnabled(bool enabled);
    /// Fragt ab, ob Produktion ausgeschaltet ist (visuell)
    bool IsProductionDisabledVirtual() const { return disableProductionVirtual; }
    /// Fragt ab, ob Produktion ausgeschaltet ist (real)
    bool IsProductionDisabled() const { return disableProduction; }
    /// Called when there are no more resources
    void OnOutOfResources();
    /// Fängt an NICHT zu arbeiten (wird gemessen fürs Ausrechnen der Produktivität)
    void StartNotWorking();
    /// Hört auf, nicht zu arbeiten, sprich fängt an zu arbeiten (fürs Ausrechnen der Produktivität)
    void StopNotWorking();

private:
    /// Calculates the productivity and resets the counter
    unsigned short CalcCurrentProductivity();
    /// Library: Order a knowledge medium from a warehouse if there is space left
    void OrderLibraryMedium();
    /// Library: True if the ware is a knowledge medium
    static bool IsKnowledgeMedium(GoodType good);
    /// Tool reform: Tools of this building type (none without the tool reform)
    const civ::BuildingTools& GetToolsDesc() const;
    /// Tool reform: True if a tool should be ordered for the slot
    bool NeedsToolOrder(const ToolSlot& slot) const;
    /// Tool reform: Order missing tools and replacements for tools which are nearly broken
    void OrderTools();
    /// Tool reform: Start the event which orders tools if there are tools and it is not running
    void StartToolOrderEvent();
};
