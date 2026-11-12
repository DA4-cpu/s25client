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

Die Werte unten sind die bisherigen Baukosten.
]]

rttr:SetBuildingCost{
    -- Militär
    Barracks       = { Boards = 2 },             -- Baracke
    Guardhouse     = { Boards = 2, Stones = 3 }, -- Wachstube
    Watchtower     = { Boards = 3, Stones = 5 }, -- Wachturm
    Fortress       = { Boards = 4, Stones = 7 }, -- Festung
    LookoutTower   = { Boards = 4 },             -- Spähturm
    Catapult       = { Boards = 4, Stones = 2 }, -- Katapult

    -- Lager
    Storehouse     = { Boards = 4, Stones = 3 }, -- Lagerhaus

    -- Holz, Stein, Bergbau
    Woodcutter     = { Boards = 2 },             -- Holzfäller
    Forester       = { Boards = 2 },             -- Förster
    Sawmill        = { Boards = 2, Stones = 2 }, -- Sägewerk
    Quarry         = { Boards = 2 },             -- Steinbruch
    GraniteMine    = { Boards = 4 },             -- Granitmine
    CoalMine       = { Boards = 4 },             -- Kohlemine
    IronMine       = { Boards = 4 },             -- Eisenmine
    GoldMine       = { Boards = 4 },             -- Goldmine
    Charburner     = { Boards = 4, Stones = 3 }, -- Köhler (Köhlerei-Addon)

    -- Nahrung und Landwirtschaft
    Fishery        = { Boards = 2 },             -- Fischer
    Hunter         = { Boards = 2 },             -- Jäger
    Well           = { Boards = 2 },             -- Brunnen
    Farm           = { Boards = 3, Stones = 3 }, -- Bauernhof
    Mill           = { Boards = 2, Stones = 2 }, -- Mühle
    Bakery         = { Boards = 2, Stones = 2 }, -- Bäcker
    PigFarm        = { Boards = 3, Stones = 3 }, -- Schweinezucht
    Slaughterhouse = { Boards = 2, Stones = 2 }, -- Fleischerei
    Brewery        = { Boards = 2, Stones = 2 }, -- Brauerei
    DonkeyBreeder  = { Boards = 3, Stones = 3 }, -- Eselzucht
    Vineyard       = { Boards = 4, Stones = 4 }, -- Weinberg (Wein-Addon)
    Winery         = { Boards = 2, Stones = 3 }, -- Winzerei (Wein-Addon)

    -- Metall und Handwerk
    Ironsmelter    = { Boards = 2, Stones = 2 }, -- Eisenschmelze
    Metalworks     = { Boards = 2, Stones = 2 }, -- Schlosserei
    Armory         = { Boards = 2, Stones = 2 }, -- Waffenschmiede
    Mint           = { Boards = 2, Stones = 2 }, -- Münzprägerei
    Shipyard       = { Boards = 2, Stones = 3 }, -- Werft
    Temple         = { Boards = 4, Stones = 7 }, -- Tempel (Wein-Addon)
    Skinner        = { Boards = 2 },             -- Abdecker (Leder-Addon)
    Tannery        = { Boards = 2, Stones = 2 }, -- Gerberei (Leder-Addon)
    LeatherWorks   = { Boards = 2, Stones = 2 }, -- Lederei (Leder-Addon)
}
