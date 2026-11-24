-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Baukosten der eingebauten Gebäude im Zivilisationsmodus. Die Werkstätten aus workshops.lua haben ihre Baukosten dort
(cost in rttr:AddWorkshop), im selben Format.

rttr:SetBuildingCost{ Gebäude = Kosten, ... }
  Kosten, kurz: { Boards = 2, Stones = 1 }  - je Eintrag ein Tag und die Anzahl. Verbaut wird in alphabetischer
                Reihenfolge der Tags (Lua-Tabellen haben keine Reihenfolge).
  Kosten, lang: { { tags = "Boards", amount = 2 }, { tags = { "Bretter", "Nadelholz" }, amount = 1 } }
                - verbaut wird in dieser Reihenfolge. tags ist eine Tag-Abfrage wie bei den Eingängen der Werkstätten.
  Jede Ware, die alle Tags eines Eintrags hat, kann dafür geliefert werden, auch gemischt (z.B. Laub- und
  Nadelholzbretter für "Bretter"). Die Baustelle bestellt aus dem nächsten Lager, das irgendeine passende Ware hat.
  Höchstens 4 Einträge und höchstens 31 Waren insgesamt je Gebäude.

Der Bauarbeiter verbaut die Einträge nacheinander. Die erste Hälfte des Baufortschritts zeigt das Gerüst, die zweite
die Mauern.
"Boards" und "Stones" sind die Bezeichner der eingebauten Bretter und Steine (zugleich ihre Tags). Neue Waren
zählen dazu, wenn sie diesen Tag haben (z.B. kind = "Boards"), oder man gibt den eingebauten Waren mit
rttr:SetWareTags (wares.lua) eine Warenart und fragt nach ihr.
Der Hafen kostet immer 4 Bretter und 6 Steine (Expeditionen bringen genau diese Waren mit) und steht deshalb nicht
hier. Nicht genannte Gebäude behalten ihre bisherigen Kosten.
Beim Abriss mit Rückerstattung (Addon) gibt es von jedem Eintrag die erste passende Ware zurück.

Gebäude sollen später je nach aus welchen Materialien erbaut worden sind Eigenschaften erhalten. So kann man bspw. eine Baracke aus Adobes oder aus Holz bauen,
und diese können sich auch mischen. Für jedes Baumaterial müssen dafür Eigenschaften für Dämmung, 
Stabilität (Wie viele Lebenspunkte erhält das Gebäude) in konkreten Werten angegeben werden: 
Adobes: Dämmung = 40 (von 0-100 wobei 0 extrem schlecht und 100 sehr gut ist), Stabilität = 20 (ohne Obergrenze); Wood: Dämmung = 10, Stabilität = 50;
Stones: D = 25, S = 90, Boards:  D = 20, Stabilität = 50; LoamBrick: D = 90; StoneBlock: D = 35, S = 120; Thatch: D = 40, S = 10; Reed = D = 40, S = 20;
Wird die Baracke nun aus 2 Holz, 2 Adobes und 2 Thatch errichtet erhält sie für Dämmung den durchschnittlichen Wert aller verwendeten Baumaterialien und die Lebenspunkte 
ist die Summe der Stabilität aller verwendeten Materialien: D = 30, S = 160. Die Bedeutung von D und S muss ich noch genau im Konzept festlegen. Nicht jedes
Baumaterial hat diese Eigenschaften bspw. hat Rope keine Eigenschaften. Dieses Baumaterial zählt dann einfach nicht zur Rechnung dazu

Die Werte unten sind die bisherigen Baukosten.
]]

rttr:SetBuildingCost{
    -- Militär
    Barracks       = { { tags = "BasicBuildingMaterial", amount = 4 }, { tags = "BasicRoofMaterial", amount = 2 } },             -- Baracke
    Guardhouse     = { { tags = "BasicBuildingMaterial", amount = 6 }, { tags = "BasicRoofMaterial", amount = 3 } }, -- Wachstube
    Watchtower     = { BasicBuildingMaterial = 4, Stones = 4, BasicRoofMaterial = 5 }, -- Wachturm
    Fortress       = { BasicBuildingMaterial = 6, Stones = 8, BasicRoofMaterial = 6 }, -- Festung
    LookoutTower   = { BasicBuildingMaterial = 5 },             -- Spähturm
    Catapult       = { BasicBuildingMaterial = 5 }, -- Katapult

    -- Lager
    Storehouse     = { BasicBuildingMaterial = 7, BasicRoofMaterial = 7  }, -- Lagerhaus

    -- Holz, Stein, Bergbau
    Woodcutter     = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },             -- Holzfäller
    Forester       = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2},             -- Förster
    Sawmill        = { BasicBuildingMaterial = 4, BasicRoofMaterial = 3 }, -- Sägewerk
    Quarry         = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },             -- Steinbruch
    GraniteMine    = { BasicBuildingMaterial = 4, Rope = 1 },             -- Granitmine
    CoalMine       = { BasicBuildingMaterial = 4, Rope = 1  },             -- Kohlemine
    IronMine       = { BasicBuildingMaterial = 4, Rope = 1  },             -- Eisenmine
    GoldMine       = { BasicBuildingMaterial = 4, Rope = 1  },             -- Goldmine
    Charburner     = { BasicBuildingMaterial = 4, BasicRoofMaterial = 3 }, -- Köhler (Köhlerei-Addon)

    -- Nahrung und Landwirtschaft
    Fishery        = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },             -- Fischer
    Hunter         = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },             -- Jäger
    Well           = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2, Rope = 1 },             -- Brunnen
    Farm           = { BasicBuildingMaterial = 6, BasicRoofMaterial = 4 }, -- Bauernhof
    Mill           = { BasicBuildingMaterial = 6, BasicRoofMaterial = 2 }, -- Mühle
    Bakery         = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2, Stones = 3 }, -- Bäcker
    PigFarm        = { BasicBuildingMaterial = 8, BasicRoofMaterial = 5 }, -- Schweinezucht
    Slaughterhouse = { BasicBuildingMaterial = 5, BasicRoofMaterial = 2 }, -- Fleischerei
    Brewery        = { BasicBuildingMaterial = 6, BasicRoofMaterial = 5 }, -- Brauerei
    DonkeyBreeder  = { BasicBuildingMaterial = 8, BasicRoofMaterial = 5 }, -- Eselzucht
    Vineyard       = { BasicBuildingMaterial = 3, BasicRoofMaterial = 2 }, -- Weinberg (Wein-Addon)
    Winery         = { BasicBuildingMaterial = 6, BasicRoofMaterial = 3 }, -- Winzerei (Wein-Addon)

    -- Metall und Handwerk
    Ironsmelter    = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4, Stones = 4 }, -- Eisenschmelze
    Metalworks     = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4, Stones = 2 }, -- Schlosserei
    Armory         = { BasicBuildingMaterial = 6, BasicRoofMaterial = 5, Stones = 5 }, -- Waffenschmiede
    Mint           = { BasicBuildingMaterial = 6, BasicRoofMaterial = 5, Stones = 5 }, -- Münzprägerei
    Shipyard       = { BasicBuildingMaterial = 7, BasicRoofMaterial = 6, Rope = 2 }, -- Werft
    Temple         = { BasicBuildingMaterial = 5, BasicRoofMaterial = 6, Stones = 8, Rope = 2 }, -- Tempel (Wein-Addon)
    Skinner        = { BasicBuildingMaterial = 5, BasicRoofMaterial = 1  },             -- Abdecker (Leder-Addon)
    Tannery        = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4  }, -- Gerberei (Leder-Addon)
    LeatherWorks   = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4  }, -- Lederei (Leder-Addon)
}
