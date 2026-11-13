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

-- rttr:SetAIBasicEconomy{ { ... }, { ... } }
--   Grundwirtschaft in Stufen, die erste Stufe am wichtigsten. Innerhalb der benötigten und innerhalb der
--   expandierenden Aufträge kommen zuerst die Gebäude der ersten Stufe, dann die der zweiten, dann alle anderen.
--   Benötigt kommt immer vor expandierend. Ein Auftrag, der für einen anderen gebaut wird (z. B. das Sägewerk für
--   die Bretter einer Brauerei), rückt in seine eigene Stufe auf.
rttr:SetAIBasicEconomy{
    { "Woodcutter", "Quarry", "Sawmill", "Forester" },
    { "IronMine", "CopperMine", "CoalMine", "Charburner", "Metalworks", "Copperworks", "Ironsmelter", "Armory" },
}

-- rttr:SetAIStartStockNoSurplus{ ... }
--   Waren, deren Startvorrat für die KI kein Überschuss ist. Liegt von ihnen mehr im Lager als die Schwelle, baut die
--   KI trotzdem keine Verbraucher dafür, solange der Startvorrat nicht aufgebraucht ist. So expandiert sie am Anfang
--   mit Holzfällern, Sägewerken und Steinbrüchen, statt den Vorrat nur zu verarbeiten.
rttr:SetAIStartStockNoSurplus{ "Stones", "Wood", "Boards" }
