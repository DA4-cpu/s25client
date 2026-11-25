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
    /// Wares of an input (see InputSlot in BldWorkDescription::inputs)
    struct InputStock
    {
        /// Wares in the building per ware of the input (same order as InputSlot::wares)
        std::vector<uint8_t> counts;
        /// Wares on their way to this input
        std::list<Ware*> ordered;
        /// Hundredths of a ware used in advance: a work step needing a part of a ware takes a whole one, the rest
        /// counts for the next steps (amount 0.5: 1 ware in step 1, none in step 2)
        uint16_t prepaid = 0;
    };

protected:
    /// Der Typ, der hier arbeitet
    nofBuildingWorker* worker;
    /// Produktivität
    unsigned short productivity;
    /// Produktion eingestellt? (letzteres nur visuell, um Netzwerk-Latenzen zu verstecken)
    bool disableProduction, disableProductionVirtual;
    /// Input (or library: knowledge medium) ordered last by the order event
    unsigned char lastOrderedWare;
    /// Wares of the inputs (same order as BldWorkDescription::inputs)
    std::vector<InputStock> inputs_;
    /// Input a ware is ordered for right now (not serialized), -1: none
    int orderingInput_ = -1;
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
    /// Ware consumed last from the first input (built-in mines: the ware consumed last). Decides the product of a
    /// workshop with inheritProperties
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
    /// Civilization mode: Tools in use when the current work step began (they count for the whole step, also if one
    /// broke at its beginning)
    std::vector<GoodType> stepTools_;
    /// Civilization mode: Saved work steps of each product in hundredths (output ratio of the past steps on it not yet
    /// spent on the product). Each product has its own (Nutzer, 2026-11-25). Products without saved steps are missing
    std::vector<std::pair<GoodType, uint16_t>> productSavings_;
    /// Civilization mode, workshops from lua: Product of the current work step, chosen anew when every step starts
    /// (Nutzer, 2026-11-25). Nothing: no step running
    helpers::OptionalEnum<GoodType> chosenProduct_;
    /// Tools in use (the tools of the tool slots)
    std::vector<GoodType> GetToolsInUse() const;
    /// Output ratio with these tools in use (GetOutputRatio)
    unsigned CalcOutputRatio(const std::vector<GoodType>& tools, helpers::OptionalEnum<GoodType> product) const;
    /// Wares per work step of the input with these tools in use (GetInputAmount)
    unsigned CalcInputAmount(unsigned input, const std::vector<GoodType>& tools) const;
    /// Number of wares a work step with these tools takes from the input
    unsigned CalcNumNeeded(unsigned input, const std::vector<GoodType>& tools) const;
    /// Work steps the product costs in hundredths (workshops from lua: steps in outputs, otherwise 100)
    unsigned GetProductSteps(GoodType product) const;
    /// False if no product of the building yields a ware with the tools of the work step (output ratio 0)
    bool CanYieldProducts() const;
    /// Tool reform: Slot a tool is ordered for right now (not serialized), -1: none
    int orderingToolSlot_ = -1;
    /// GF the building was finished and since which its worker is there (NO_GF: none). For the measured values per
    /// building of the forecast (civ::EconomyForecast)
    unsigned finishedGF_ = 0, workerSinceGF_ = 0xFFFFFFFF;

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

    /// Number of wares in the building of the input (all its wares together)
    unsigned GetNumWares(unsigned input) const;
    /// Wares in the building of the input, per ware of the input (same order as InputSlot::wares)
    const std::vector<uint8_t>& GetInputCounts(unsigned input) const { return inputs_[input].counts; }
    /// Number of wares the next work step takes from the input
    unsigned GetNumNeededForStep(unsigned input) const;
    /// Wares per work step of the input in hundredths with the tools in use: amount of the input plus the change of
    /// the tools (inputRatio), at least 0 and at most what the input holds
    unsigned GetInputAmount(unsigned input) const;
    /// True if the wares for a work step are there
    bool WaresAvailable();
    /// Use the wares of a work step
    void ConsumeWares();
    /// Ware consumed last from the first input (built-in mines: the ware consumed last), nothing if none yet
    helpers::OptionalEnum<GoodType> GetLastConsumedWare() const { return lastConsumedWare_; }

    /// Berechnet Punktewertung für Ware type
    unsigned CalcDistributionPoints(GoodType type);

    /// Wird aufgerufen, wenn eine neue Ware zum dem Gebäude geliefert wird (nicht wenn sie bestellt wurde vom Gebäude!)
    void TakeWare(Ware* ware) override;

    /// Bestellte Waren
    bool AreThereAnyOrderedWares() const;
    /// Number of wares of the input on the way to the building
    unsigned GetNumOrderedWares(unsigned input) const { return static_cast<unsigned>(inputs_[input].ordered.size()); }

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
    /// Civilization mode: Wares per work step in hundredths: ratio of the building plus the bonus of its tools in use
    /// (with their bonus for the product if given), at least 0 and at most MAX_OUTPUT_RATIO (8 wares). Always 100
    /// outside the civilization mode
    unsigned GetOutputRatio(helpers::OptionalEnum<GoodType> product = std::nullopt) const;
    /// The same with the tools in use when the current work step began
    unsigned GetStepOutputRatio(helpers::OptionalEnum<GoodType> product) const;
    /// Called by the worker when the current work step was done on the product: The output ratio of the step is
    /// saved for this product. Returns how many of it are made (saved steps / work steps of the product, 0 if they do
    /// not suffice yet), the rest stays saved for the product
    unsigned TakeNumProducts(GoodType product);
    /// Saved work steps of the product in hundredths (productSavings_)
    unsigned GetSavedSteps(GoodType product) const;
    /// Number of the product the current work step makes if it is done on it (by its saved steps and the output ratio
    /// with the tools of the step)
    unsigned GetNumMadeInStep(GoodType product) const;
    /// Workshops from lua: product of the current work step (chosenProduct_)
    helpers::OptionalEnum<GoodType> GetChosenProduct() const { return chosenProduct_; }
    void SetChosenProduct(helpers::OptionalEnum<GoodType> product) { chosenProduct_ = product; }
    /// Number of tools of this ware the building is missing (nothing in use or ordered)
    unsigned GetNumMissingTools(GoodType tool) const;
    /// Tool reform: True if a tool should be ordered for the slot (empty, or a replacement is due)
    bool NeedsToolOrder(const ToolSlot& slot) const;

    /// Gibt Pointer auf Produktivität zurück
    const unsigned short* GetProductivityPointer() const { return &productivity; }
    unsigned short GetProductivity() const { return productivity; }
    const nofBuildingWorker* GetWorker() const { return worker; }
    /// The worker, to tell him the production is allowed or stopped (distribution of all wares)
    nofBuildingWorker* GetWorkerToNotify() { return worker; }

    /// Stoppt/Erlaubt Produktion (visuell)
    void ToggleProductionVirtual() { disableProductionVirtual = !disableProductionVirtual; }
    /// Stoppt/Erlaubt Produktion (real)
    void SetProductionEnabled(bool enabled);
    /// Fragt ab, ob Produktion ausgeschaltet ist (visuell)
    bool IsProductionDisabledVirtual() const { return disableProductionVirtual; }
    /// Fragt ab, ob Produktion ausgeschaltet ist (real)
    bool IsProductionDisabled() const { return disableProduction; }
    /// Stopped by the player or, in the civilization mode, by the distribution of all wares (all its products and
    /// byproducts are not wanted, civ::WareDistribution). It does not work and orders no inputs and tools then
    bool IsProductionStopped() const;
    /// Civilization mode: stopped by the distribution of all wares (not by the player)
    bool IsPausedByDistribution() const;
    static constexpr unsigned NO_GF = 0xFFFFFFFF;
    unsigned GetFinishedGF() const { return finishedGF_; }
    unsigned GetWorkerSinceGF() const { return workerSinceGF_; }
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
    /// Tool reform: Order missing tools and replacements for tools which are nearly broken
    void OrderTools();
    /// Tool reform: Start the event which orders tools if there are tools and it is not running
    void StartToolOrderEvent();
    /// Order one ware for the input from the next warehouse having any of its wares
    Ware* OrderInput(unsigned input);
    /// Input a ware of this type goes to: the first one taking it with space left, -1 if none
    int FindInputFor(GoodType good) const;
    /// Wares of the input in the building and on their way
    unsigned GetNumStoredAndOrdered(unsigned input) const;
};
