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