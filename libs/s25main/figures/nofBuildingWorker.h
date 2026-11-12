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

    /// To be called by derived classes right after a work step produced `ware` (or not). Civilization mode: Applies
    /// the output ratio of the step. Clears `ware` if the step yields no ware yet, adds the further products of the
    /// step (ProduceExtraWare) and the byproducts of all of them
    void ProducedWare();
    /// A further product of the current work step (output ratio above 1). Default: the same ware again. Workers
    /// choosing their product choose again, miners and collectors take one more unit of their resource. Nothing: no
    /// further ware (e.g. the resource is used up)
    virtual helpers::OptionalEnum<GoodType> ProduceExtraWare(GoodType firstWare) { return firstWare; }

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
