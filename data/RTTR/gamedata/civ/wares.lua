-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Neue Waren und Tags im Zivilisationsmodus.

rttr:AddWare{...} definiert eine neue Ware:
  name        Bezeichner (zugleich der Name-Tag). Nur A-Z, a-z, 0-9 und _ (keine Umlaute, keine Leerzeichen).
              Wird in anderen Lua-Dateien und in Spielständen verwendet.
  title       Anzeigename (darf Umlaute enthalten)
  kind        optional: Warenart-Tag, z.B. "Bretter"
  properties  optional: Eigenschafts-Tags, z.B. { "Nadelholz" }. Werkstätten mit inheritProperties = true geben
              die Eigenschaften der verbrauchten Ware an ihr Produkt weiter.
  look        Platzhaltergrafik: Die Ware sieht aus wie diese eingebaute Ware (z.B. "Stones")
  transport   Transportpriorität: gleiche Gruppe wie diese eingebaute Ware (z.B. "Stones")
  knowledge   optional: Wissenspunkte, wenn die Ware als Wissensmedium eingelagert ist, z.B. { LWP = 1 }
              (wird erst mit den Bibliotheken in Phase 2 ausgewertet)

rttr:SetWareTags{...} gibt eingebauten Waren zusätzliche Tags:
  rttr:SetWareTags{ Boards = { kind = "Bretter", properties = { "Nadelholz" } } }

Tags: Jede Ware hat ihren Bezeichner als Tag, dazu kind und properties. Eine Tag-Abfrage (z.B. in den Eingängen einer
Werkstatt) ist ein einzelner Tag "Bretter" oder eine Liste { "Bretter", "Nadelholz" }; passend sind alle Waren, die alle
genannten Tags haben.

WICHTIG für Spielstände: Neue Waren immer hinten anfügen. Bestehende Waren nicht löschen, umbenennen oder umsortieren,
sonst passen alte Spielstände nicht mehr. Es sind höchstens 64 neue Waren möglich.

Beispiel (auskommentiert, nur zur Erklärung des Formats):

rttr:AddWare{
    name = "ExampleRaw",
    title = "Beispiel-Rohstoff",
    kind = "Beispiel",
    look = "Stones",
    transport = "Stones",
}
rttr:AddWare{
    name = "ExampleProduct",
    title = "Beispiel-Produkt",
    look = "Boards",
    transport = "Boards",
}
]]

rttr:AddWare{
    name = "Loam",
    title = "Lehm",
    properties = { "muddy" },
    look = "Leather",
    transport = "Boards",
}

rttr:AddWare{
    name = "Clay",
    title = "Ton",
    look = "Wood",
    transport = "Boards",
}

-- Funkitoniert einwandtfrei!

-------------------------------------------------------------------------------
-- Knochen-Kette (Schritt 4a). Entscheidungen des Nutzers: Jäger 1 Knochen pro Tier, Fleischerei 1 pro Schinken,
-- erst nach der Knochen-Technologie (siehe byproducts.lua). Geritzte Knochen: drei Waren mit gemeinsamer Warenart,
-- je 1 WP. Anzeigenamen (title) dürfen jederzeit geändert werden, Bezeichner (name) nicht.
-- PLATZHALTER: look (Grafik-Vorbild) und transport (Transportpriorität) sind noch nicht festgelegt.
-------------------------------------------------------------------------------

rttr:AddWare{
    name = "Bones",
    title = "Knochen",
    look = "Stones",      -- PLATZHALTER
    transport = "Stones", -- PLATZHALTER
}

rttr:AddWare{
    name = "CarvedBoneLWP",
    title = "Geritzter Knochen (Landwirtschaft)",
    kind = "GeritzterKnochen",
    properties = { "LWP" },
    look = "Stones",      -- PLATZHALTER
    transport = "Stones", -- PLATZHALTER
    knowledge = { LWP = 1 },
}

rttr:AddWare{
    name = "CarvedBoneIWP",
    title = "Geritzter Knochen (Industrie)",
    kind = "GeritzterKnochen",
    properties = { "IWP" },
    look = "Stones",      -- PLATZHALTER
    transport = "Stones", -- PLATZHALTER
    knowledge = { IWP = 1 },
}

rttr:AddWare{
    name = "CarvedBoneMWP",
    title = "Geritzter Knochen (Militär)",
    kind = "GeritzterKnochen",
    properties = { "MWP" },
    look = "Stones",      -- PLATZHALTER
    transport = "Stones", -- PLATZHALTER
    knowledge = { MWP = 1 },
}
