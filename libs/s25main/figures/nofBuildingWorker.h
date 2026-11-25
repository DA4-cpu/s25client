// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "figures/noFigure.h"
#include "gameTypes/GoodTypes.h"
#include <vector>

class nobUsual;
class nobBaseWarehouse;
class SerializedGameData;
namespace civ {
struct Products;
}

/// Repräsentiert einen Arbeiter in einem Gebäude
class nofBuildingWorker : public noFigure
{
public:
    /// Was der gerade so schönes macht
    enum class State : uint8_t
    {
        FigureWork,                         /// Arbeiten der noFigure (Laufen zum Arbeitsplatz, Rumirren usw)
        EnterBuilding,                      /// Betreten des Gebäudes
        Waiting1,                           /// Warten, bis man anfängt zu produzieren
        Waiting2,                           /// Warten nach dem Produzieren, bis man Ware rausträgt (nur Handwerker)
        CarryoutWare,                       /// Raustragen der Ware
        Work,                               /// Arbeiten
        WaitingForWaresOrProductionStopped, /// Warten auf Waren oder weil Produktion eingetellt wurde
        WalkToWorkpoint,                    /// Zum "Arbeitspunkt" laufen (nur Landarbeiter)
        WalkingHome,                        /// vom Arbeitspunkt zurück nach Hause laufen (nur Landarbeiter)
        WaitForWareSpace,                   /// auf einen freien Platz an der Flagge vor dem Gebäude warten
        HunterChasing,                      /// Jäger: verfolgt das Tier bis auf eine gewisse Distanz
        HunterFindingShootingpoint,  /// Jäger: sucht einen Punkt rund um das Tier, von dem er es abschießen kann
        HunterShooting,              /// Jäger: Tier erschießen
        HunterWalkingToCadaver,      /// Jäger: Zum Kadaver laufen
        HunterEviscerating,          /// Jäger: Tier ausnehmen
        CatapultTargetBuilding,      /// Katapult: Dreht den Katapult oben auf das Ziel zu und schießt
        CatapultBackoff,             /// Katapult: beendet schießen und dreht Katapult in die Ausgangsstellung zurück
        HunterWaitingForAnimalReady, /// Hunter: Arrived at shooting pos and waiting for animal to be ready to
                                     /// be shot
        SkinnerWalkingToCarcass,     /// Skinner: walk to carcass
        SkinnerSkinningCarcass,      /// Skinner: get skin from carcass
    };
    friend constexpr auto maxEnumValue(State) { return State::SkinnerSkinningCarcass; }

protected:
    State state;

    /// Arbeitsplatz (Haus, in dem er arbeitet)
    nobUsual* workplace;

    // Ware, die er evtl gerade trägt
    helpers::OptionalEnum<GoodType> ware;

    /// Hat der Bauarbeiter bei seiner Arbeit Sounds von sich gegeben (zu Optimeriungszwecken)
    bool was_sounding;
    /// Tool reform (civilization mode): Tools of the demolished workplace the worker brings back to a warehouse
    std::vector<GoodType> carriedTools_;
    /// Civilization mode: All wares still to put at the flag (front = `ware`): the products of the work step (output
    /// ratio) and their byproducts. Empty while `ware` is set: only `ware` and its byproducts (older savegames)
    std::vector<GoodType> carryList_;

    /// To be called by derived classes right after a work step was done on `ware` (or produced nothing).
    /// Civilization mode: The output ratio of the step is saved for this ware (nobUsual::TakeNumProducts). Clears
    /// `ware` if its saved steps do not suffice yet, otherwise the step yields this many of `ware` (all the same ware,
    /// Nutzer, 2026-11-25) with their byproducts. Mines and harvesters take exactly one unit of their resource per
    /// work step, whatever the output ratio (Nutzer, 2026-11-24)
    void ProducedWare();

    /// wird von abgeleiteten Klassen aufgerufen, wenn sie die Ware an der Fahne vorm Gebäude ablegen wollen (oder auch
    /// nicht) also fertig mit Arbeiten sind
    void WorkingReady();
    /// wenn man beim Arbeitsplatz "kündigen" soll, man das Laufen zum Ziel unterbrechen muss (warum auch immer)
    void AbrogateWorkplace() override;
    /// Tries to start working.
    /// Checks preconditions (production enabled, wares available...) and starts the pre-Work-Waiting period if ok
    virtual void TryToWork();
    /// Returns true, when there are enough wares available for working.
    /// Note: On false, we will wait for the next ware or production change till checking again
    virtual bool AreWaresAvailable() const;
    /// Duration of the work outside of the building which is not changed by the work speed of the building (tool
    /// reform). The waiting time before is changed for it instead. 0: the worker works in the building
    virtual unsigned GetOutsideWorkLength() const { return 0; }

    /// Zivilisationsmodus: Produkte des Arbeitsplatzes (Werkstatt oder eingebautes Produktionsgebäude, outputs).
    /// nullptr außerhalb des Modus und bei Gebäuden, deren Produkt der Code festlegt (Tempel, Eselzucht ...)
    const civ::Products* GetCivProducts() const;
    /// Produkt des Arbeitsschritts nach outputChoice: nach der Gesamtverteilung, nach dem Regler "Wissen" oder das eine
    /// Produkt (mit inheritProperties nach der zuletzt aus dem ersten Eingang verbrauchten Ware). Bei einer Wahl nur
    /// Produkte, die mit den Werkzeugen des Schritts eine Ware ergeben (wenn es solche gibt)
    GoodType ChooseProduct(const civ::Products& products);
    /// outputChoice = "knowledge": Produkt gewichtet nach dem Regler "Wissen" des Besitzers
    GoodType ChooseKnowledgeProduct(const civ::Products& products) const;
    /// Wählt das Produkt beim Beginn der Arbeit im Gebäude (nofWorkman::StartWorking): Bei einer Wahl
    /// (outputChoice, mehrere Produkte) vor dem Verbrauch der Eingänge (beforeConsume), sonst danach (dann entscheidet
    /// die verbrauchte Ware über das Produkt). Ohne Produkte aus dem Inhalt geschieht nichts
    void ChooseStepProduct(bool beforeConsume);
    /// Produkt des fertigen Arbeitsschritts: im Zivilisationsmodus das gewählte bzw. das nach den Produkten des
    /// Gebäudes (outputs), sonst `original` (das Produkt des Originalspiels). Ein Schild wird das Schild des Volkes
    helpers::OptionalEnum<GoodType> TakeStepProduct(helpers::OptionalEnum<GoodType> original);
    /// Der Arbeiter hat eine eigene Grafik, um diese Ware zu tragen (GetCarryID). Sonst trägt er sie wie ein Träger
    /// (z.B. ein Holzfäller, der eine andere Ware als Holz bringt)
    virtual bool HasCarryGraphic(GoodType good) const;
    /// Zeichnet den Arbeiter mit einer Ware ohne eigene Grafik wie einen Träger. false: die Ware hat eine eigene
    /// Grafik (oder er trägt nichts), es wurde nichts gezeichnet
    bool DrawCarryingOtherWare(DrawPoint drawPt);

private:
    /// Wares to put at the flag when carrying out `ware`: the ware itself and its byproducts (civilization mode)
    std::vector<GoodType> GetWaresToCarryOut() const;
    /// True if the flag has space for all wares to carry out (ware + byproducts), at most a full flag
    bool HasSpaceForWaresAtFlag() const;
    /// Number of wares put at the flag together: all of them, but at most a full flag
    static unsigned GetNumWaresToPutNow(std::size_t numWares);
    /// Create the ware at the flag in front of the workplace and send it on its way
    void PutWareAtFlag(GoodType good);

    /// von noFigure aufgerufen
    void Walked() override;      // wenn man gelaufen ist
    void GoalReached() override; // wenn das Ziel erreicht wurde

protected:
    /// Malt den Arbeiter beim Arbeiten
    virtual void DrawWorking(DrawPoint drawPt) = 0;
    static constexpr unsigned short CARRY_ID_CARRIER_OFFSET = 100;
    /// Ask derived class for an ID into JOBS.BOB when the figure is carrying a ware
    /// Use GD_* + CARRY_ID_CARRIER_OFFSET for using carrier graphics
    virtual unsigned short GetCarryID() const = 0;
    /// Laufen an abgeleitete Klassen weiterleiten
    virtual void WalkedDerived() = 0;
    /// Arbeit musste wegen Arbeitsplatzverlust abgebrochen werden
    virtual void WorkAborted();
    /// Arbeitsplatz wurde erreicht
    virtual void WorkplaceReached();

    /// Draws the figure while returning home / entering the building (often carrying wares)
    virtual void DrawWalkingWithWare(DrawPoint drawPt);
    /// Zeichnen der Figur in sonstigen Arbeitslagen
    virtual void DrawOtherStates(DrawPoint drawPt);

public:
    /// Tool reform: Take the tools of the demolished workplace along
    void TakeTools(std::vector<GoodType> tools);
    /// Tool reform: Hand over the tools taken along (called by the warehouse he arrives at)
    std::vector<GoodType> ReleaseTools();
    State GetState() const { return state; }

    nofBuildingWorker(Job job, MapPoint pos, unsigned char player, nobUsual* workplace);
    nofBuildingWorker(SerializedGameData& sgd, unsigned obj_id);
    nofBuildingWorker(const nofBuildingWorker&) = delete;

    void Destroy() override
    {
        RTTR_Assert(!workplace);
        noFigure::Destroy();
    }
    void Serialize(SerializedGameData& sgd) const override;

    void Draw(DrawPoint drawPt) override;

    /// Wenn eine neue Ware kommt oder die Produktion wieder erlaubt wurde, wird das aufgerufen
    void GotWareOrProductionAllowed();
    /// Wenn wieder Platz an der Flagge ist und eine Ware wieder rausgetragen werden kann
    bool FreePlaceAtFlag();
    /// Wenn das Haus des Arbeiters abbrennt
    void LostWork();
    /// Wird aufgerufen, nachdem die Produktion in dem Gebäude, wo er arbeitet, verboten wurde
    void ProductionStopped();
};
