-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Wissenspunkte (WP) der Gebäude im Zivilisationsmodus.

Jedes FERTIGE Gebäude bringt seinem Besitzer diese Punkte, solange es steht (Baustellen zählen nicht).
Arten:  LWP = Landwirtschaft,  IWP = Industrie,  MWP = Militär.
Nicht genannte Gebäude und Arten zählen 0. Negative Werte sind erlaubt (Katapult).
Gebäudenamen wie im Enum BuildingType (siehe techtree.lua), zusätzlich "Headquarters" für das Hauptquartier.

Die WP-Kosten der Technologien stehen in techtree.lua unter "knowledge".
Änderungen gelten ab dem nächsten Spielstart, Neukompilieren ist nicht nötig.
]]

rttr:SetBuildingKnowledge{
    -- Lager
    Headquarters   = { LWP = 7, IWP = 7, MWP = 7 }, -- Hauptquartier
    Storehouse     = { LWP = 1, IWP = 1, MWP = 1 }, -- Lagerhaus
    HarborBuilding = { LWP = 1, IWP = 1, MWP = 1 }, -- Hafen

    -- Nahrung und Landwirtschaft
    Fishery        = { LWP = 2 },                   -- Fischer
    Hunter         = { LWP = 2 },                   -- Jäger
    Well           = { LWP = 1 },                   -- Brunnen
    Farm           = { LWP = 3 },                   -- Bauernhof
    Mill           = { LWP = 3 },                   -- Mühle
    Bakery         = { LWP = 3 },                   -- Bäcker
    PigFarm        = { LWP = 4 },                   -- Schweinezucht
    Slaughterhouse = { LWP = 3 },                   -- Fleischerei
    DonkeyBreeder  = { LWP = 4 },                   -- Eselzucht
    Brewery        = { LWP = 1, MWP = 1 },          -- Brauerei
    Vineyard       = { LWP = 4 },                   -- Weinberg (Wein-Addon)
    Winery         = { LWP = 3 },                   -- Winzerei (Wein-Addon)

    -- Holz, Stein, Metall
    Woodcutter     = { IWP = 2 },                   -- Holzfäller
    Forester       = { LWP = 1, IWP = 1 },          -- Förster
    Sawmill        = { IWP = 3 },                   -- Sägewerk
    Quarry         = { IWP = 2 },                   -- Steinbruch
    GraniteMine    = { IWP = 1 },                   -- Granitmine
    CoalMine       = { IWP = 1 },                   -- Kohlemine
    IronMine       = { IWP = 1 },                   -- Eisenmine
    GoldMine       = { IWP = 1 },                   -- Goldmine
    Charburner     = { IWP = 4 },                   -- Köhler (Köhlerei-Addon)
    Ironsmelter    = { IWP = 3 },                   -- Eisenschmelze
    Metalworks     = { IWP = 3 },                   -- Schlosserei
    Armory         = { IWP = 2 },                   -- Waffenschmiede
    Mint           = { IWP = 3 },                   -- Münzprägerei
    Shipyard       = { IWP = 3 },                   -- Werft
    Temple         = { IWP = 3 },                   -- Tempel (Wein-Addon)
    Skinner        = { IWP = 2 },                   -- Abdecker (Leder-Addon)
    Tannery        = { IWP = 3 },                   -- Gerberei (Leder-Addon)
    LeatherWorks   = { MWP = 3 },                   -- Lederei (Leder-Addon)

    -- Militär
    Barracks       = { MWP = 1 },                   -- Baracke
    Guardhouse     = { MWP = 2 },                   -- Wachstube
    Watchtower     = { MWP = 3 },                   -- Wachturm
    Fortress       = { MWP = 4 },                   -- Festung
    LookoutTower   = { MWP = 1 },                   -- Spähturm
    Catapult       = { MWP = -5 },                  -- Katapult (absichtlich negativ)
}

--[[
Startwerte des Reglers "Wissen" im Verteilungsfenster (je 0 bis 10): Welches Wissensmedium Werkstätten mit
outputChoice = "knowledge" herstellen (z.B. der Knochenschnitzer). Jeder Spieler kann sie im Spiel ändern.
Beispiel: LWP = 10, IWP = 5, MWP = 0 -> doppelt so viele LWP- wie IWP-Medien, keine MWP-Medien.
Stehen alle möglichen Medien einer Werkstatt auf 0, wird gleich verteilt.
]]
rttr:SetKnowledgeDistribution{ LWP = 5, IWP = 5, MWP = 5 } -- PLATZHALTER: Startwerte noch offen
