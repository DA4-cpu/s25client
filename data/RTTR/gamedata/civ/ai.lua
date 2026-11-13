-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- KI des Zivilisationsmodus.
--
-- rttr:SetAIExpandingBuildings{ ... }
--   Gebäude, die die KI bauen darf, um ihre Wirtschaft zu vergrößern ("expandierende" Aufträge, maximale
--   Ausbeutung). Alle anderen baut sie nur, wenn die Wirtschaft sie braucht ("benötigte" Aufträge: Mangel ausgleichen
--   oder einen Überschuss verarbeiten). Ein Förster z. B. wird nur gebraucht, wenn Bäume fehlen, und macht die
--   Wirtschaft nicht größer. Expansion über Endproduzenten unterstützt die Wirtschaft gezielter.
--   Namen wie in buildingcosts.lua bzw. der Name der Werkstatt aus workshops.lua.
rttr:SetAIExpandingBuildings{
    -- Militär und Lager
    "Barracks", "Guardhouse", "Watchtower", "Fortress", "Storehouse", "HarborBuilding",
    -- Rohstoffe
    "Woodcutter", "Quarry", "Fishery", "Hunter", "GoldMine", "CoalMine", "IronMine",
    "LoamPit", "CopperMine", "PapyrusPlantation",
    -- Verarbeitung
    "Sawmill", "Charburner", "Brewery", "Armory", "Mint", "Metalworks", "Copperworks", "BoneCarver", "LeatherWorks",
}
