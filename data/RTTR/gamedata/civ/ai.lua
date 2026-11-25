-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- KI des Zivilisationsmodus.
--
-- rttr:SetAIExpandingBuildings{ ... }
--   Gebäude, die die KI ohne Problem bauen darf, um ihre Wirtschaft zu vergrößern ("expandierende" Aufträge, maximale
--   Ausbeutung): für eine Ware im Zustand "normal" oder als Verbraucher eines Überschusses, dessen Produkt "normal" ist.
--   Die Liste ist eine Erlaubnis, keine Eigenschaft des Gebäudes: Gibt es ein gemessenes Problem (Mangel, fehlender
--   Eingang oder fehlendes Baumaterial eines benötigten Auftrags, stehende Gebäude, fehlende Soldaten, Werkzeuge,
--   Gehilfen, volles Lager, wenige Bäume am Holzfäller), baut die KI jedes Gebäude als "benötigten" Auftrag. Ein
--   expandierender Auftrag löst nie ein Problem; ein benötigter ersetzt den gleichen expandierenden (gleiches Gebäude,
--   gleiche Ware). Unabhängig von der Liste expandieren die Sammler von Vorkommen (Holzfäller, Steinbruch, Sammler aus
--   workshops.lua), sobald genug Vorkommen im Gebiet sind (EconomyPlanner.h: CLEARING_MIN_TREES, MIN_DEPOSITS_IN_REACH).
--   Namen wie in workshops.lua (rttr:SetBuilding bzw. rttr:AddWorkshop).
rttr:SetAIExpandingBuildings{
    -- Militär und Lager
    "Barracks", "Guardhouse", "Watchtower", "Fortress", "Storehouse", "HarborBuilding",
    -- Rohstoffe
    "Woodcutter", "Quarry", "Fishery", "Hunter", "GoldMine", "CoalMine", "IronMine",
    "LoamPit", "CopperMine", "PapyrusPlantation",
    -- Verarbeitung
    "Sawmill", "Charburner", "Brewery", "Armory", "Mint", "Metalworks", "Copperworks", "BoneCarver", "LeatherWorks", "PigFarm", "Bakery", "Ironsmelter",
    "AdobeProducer", "Brickworks", "SpinningMill",
    -- Werkzeughersteller
    "ObsidianWorkshop", "Bronzeworks", "WoodWorkshop",
    -- Plantagen bzw. Erzeuger
    "Farm" 
}

-- rttr:SetAIBasicEconomy{ { ... }, { ... } }
--   Grundwirtschaft in Stufen, die erste Stufe am wichtigsten. Innerhalb der benötigten und innerhalb der
--   expandierenden Aufträge kommen zuerst die Gebäude der ersten Stufe, dann die der zweiten, dann alle anderen.
--   Benötigt kommt immer vor expandierend. Ein Auftrag, der für einen anderen gebaut wird (z. B. das Sägewerk für
--   die Bretter einer Brauerei), rückt in seine eigene Stufe auf.
--rttr:SetAIBasicEconomy{
  --  { "Woodcutter", "Quarry", "Sawmill", "Forester" },
    --{ "IronMine", "CopperMine", "CoalMine", "Charburner", "Metalworks", "Copperworks", "Ironsmelter", "Armory", "Brewery" },
--}

rttr:SetAIBasicEconomy{
    { "Woodcutter" },
    { "CoalMine"},
}

-- rttr:SetAIStartStockNoSurplus{ ... }
--   Waren, deren Startvorrat für die KI kein Überschuss ist. Liegt von ihnen mehr im Lager als die Schwelle, baut die
--   KI trotzdem keine Verbraucher dafür, solange der Startvorrat nicht aufgebraucht ist. So expandiert sie am Anfang
--   mit Holzfällern, Sägewerken und Steinbrüchen, statt den Vorrat nur zu verarbeiten.
--   Jeder Eintrag ist eine Tag-Abfrage wie bei den Eingängen der Werkstätten ("BasicBuildingMaterial" trifft Holz,
--   Bretter, Steine und Adobe). Gerechnet wird wie im Wirtschaftsplaner: Startvorrat übrig = beim Start eingelagert
--   minus seitdem verbraucht, je Ware und für eine Warengruppe als Summe ihrer Waren.
rttr:SetAIStartStockNoSurplus{ "BasicBuildingMaterial", "Fish", "Bread", "Meat" }

-- rttr:SetAIBuildingLimit{ base = ..., perArea = ..., areaSize = ... }
--   Höchstzahl der Gebäude je Typ (Holzfäller, Steinbruch, Sägewerk ...) für expandierende Aufträge:
--   base + perArea × (Land ÷ areaSize, abgerundet). Benötigte Aufträge lösen ein Problem und beachten sie nicht.
--   areaSize ist die Größe eines Gebiets in Landknoten (mindestens 1). Zum Umrechnen: Radius r um einen Punkt =
--   1 + 3 × r × (r + 1) Knoten, das Land eines Hauptquartiers (Radius 10) also 331 Knoten.
--   Beispiel mit base = 5, perArea = 1, areaSize = 500: Land 331 (Spielbeginn) → 5, Land 1000 → 7, Land 1600 → 8.
rttr:SetAIBuildingLimit{ base = 5, perArea = 1, areaSize = 331 }

-- rttr:SetAIStorageFull(Prozent)
--   Ab dieser Belegung aller Lager zusammen (capacity in workshops.lua) gilt das Lager für die KI als voll: Sie baut dann nur
--   noch benötigte Gebäude und bestellt ein weiteres Lagerhaus (das billigste erlaubte Lager, das alle Waren und
--   Personen aufnimmt).
rttr:SetAIStorageFull(90)
