// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "noBaseBuilding.h"
#include "gameTypes/BuildingTypes.h"
#include "gameTypes/GoodTypes.h"
#include <cstdint>
#include <list>
#include <utility>
#include <vector>

class nofBuilder;
class nofPlaner;
class FOWObject;
class SerializedGameData;
class Ware;
class noFigure;
class noRoadNode;

/// repräsentiert eine Baustelle
class noBuildingSite : public noBaseBuilding
{
    enum class BuildingSiteState : uint8_t
    {
        Planing,
        Building
    };
    friend constexpr auto maxEnumValue(BuildingSiteState) { return noBuildingSite::BuildingSiteState::Building; }
    friend class nofBuilder;

    /// Typ/Status der Baustelle
    BuildingSiteState state;
    /// Planierer
    nofPlaner* planer;
    /// Bauarbeiter, der an dieser Baustelle arbeitet
    nofBuilder* builder;

public:
    /// Construction material of one entry of the costs (BUILDING_COSTS)
    struct Material
    {
        /// Wares lying at the site (not used yet)
        std::vector<GoodType> delivered;
        /// Number of wares already used by the builder
        unsigned char used = 0;
        /// Wares ordered but not yet here
        std::list<Ware*> ordered;
    };

private:
    /// Material per entry of the costs of the building type
    std::vector<Material> materials_;
    /// Gibt den Baufortschritt an, wie hoch das Gebäude schon gebaut ist, gemessen in 8 Stufen für jede verbaute Ware
    unsigned char build_progress;
    /// Entry of the costs material is ordered for right now (see TakeWare), -1: none
    int orderingEntry_ = -1;

    /// Number of wares of the entry still to order (not used, lying here or ordered)
    unsigned GetNumMissing(unsigned entry) const;
    /// Entry the ware belongs to: the one it was ordered for, otherwise the first that needs it. -1: none
    int FindEntryForWare(const Ware& ware) const;

public:
    /// Material per entry of the costs (same order as BUILDING_COSTS)
    const std::vector<Material>& GetMaterials() const { return materials_; }
    /// Number of wares used, lying at the site and ordered of all entries whose wares contain the ware type
    /// (counts each entry only once)
    unsigned GetNumUsed(GoodType good) const;
    /// Progress (out of the maximum) at which the frame is complete and the walls begin, and the maximum progress
    static std::pair<unsigned, unsigned> GetMaxProgress(BuildingType type);
    /// Ist bereits ein Bauarbeiter (oder Planierer) dieser Baustelle zugewiesen, oder wartet sie noch auf einen?
    bool HasBuilder() const { return builder != nullptr; }

    noBuildingSite(BuildingType type, MapPoint pos, unsigned char player);
    /// Konstruktor für Hafenbaustellen vom Schiff aus
    noBuildingSite(MapPoint pos, unsigned char player);
    noBuildingSite(SerializedGameData& sgd, unsigned obj_id);

    ~noBuildingSite() override;

    void Destroy() override;
    void Serialize(SerializedGameData& sgd) const override;

    GO_Type GetGOT() const final { return GO_Type::Buildingsite; }
    unsigned GetMilitaryRadius() const override;

    void Draw(DrawPoint drawPt) override;

    /// Erzeugt von ihnen selbst ein FOW Objekt als visuelle "Erinnerung" für den Fog of War
    std::unique_ptr<FOWObject> CreateFOWObject() const override;

    void AddWare(std::unique_ptr<Ware> ware) override;
    void GotWorker(Job job, noFigure& worker) override;

    /// Fordert Baumaterial an
    void OrderConstructionMaterial();
    /// Wird aufgerufen, wenn der Bauarbeiter kündigt
    void Abrogate();
    /// Eine bestellte Ware konnte doch nicht kommen
    void WareLost(Ware& ware) override;
    /// Gibt den Bau-Fortschritt zurück
    unsigned char GetBuildProgress(bool percent = true) const;

    unsigned CalcDistributionPoints(GoodType goodtype);

    /// Wird aufgerufen, wenn eine neue Ware zum dem Gebäude geliefert wird (nicht wenn sie bestellt wurde vom Gebäude!)
    void TakeWare(Ware* ware) override;
    /// Gibt zurück, ob die Baustelle fertiggestellt ist
    bool IsBuildingComplete();

    /// Aufgerufen, wenn Planierung beendet wurde
    void PlaningFinished();
    /// Gibt zurück, ob eine bestimmte Baustellen eine Baustelle ist, die vom Schiff aus errichtet wurde
    bool IsHarborBuildingSiteFromSea() const;
};
