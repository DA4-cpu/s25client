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
--   Namen wie in buildingcosts.lua bzw. der Name der Werkstatt aus workshops.lua.
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
rttr:SetAIStartStockNoSurplus{ "Wood", "Boards", "Stones", "Fish", "Bread", "Meat"} -- hier sollten die Tags (zB. "BasicBuildingMaterial") zählen, nicht die Warennamen

-- rttr:SetAIBuildingLimit{ base = ..., perStartArea = ... }
--   Höchstzahl der Gebäude je Typ (Holzfäller, Steinbruch, Sägewerk ...) für expandierende Aufträge: base +
--   perStartArea × Startgebiete. Benötigte Aufträge lösen ein Problem und beachten sie nicht.
--   Ein Startgebiet ist das Land eines Hauptquartiers (Radius 10, 331 Knoten), gezählt wird ganz (abgerundet), am
--   Anfang 1. Beispiel mit base = 3, perStartArea = 1: am Start 4 Steinbrüche, bei doppelter Landgröße 5, bei
--   dreifacher 6. Wieso kann ich die Größe nicht selbst anpassen? Wieso perStartArea? Ich will die Größe des Gebiets selbst anpassen können
rttr:SetAIBuildingLimit{ base = 5, perStartArea = 1 }

-- rttr:SetAIStorageFull(Prozent)
--   Ab dieser Belegung aller Lager zusammen (siehe storage.lua) gilt das Lager für die KI als voll: Sie baut dann nur
--   noch benötigte Gebäude und bestellt ein weiteres Lagerhaus (das billigste erlaubte Lager, das alle Waren und
--   Personen aufnimmt).
rttr:SetAIStorageFull(90)
