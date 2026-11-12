-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Regler im Fenster "Verteilung" im Zivilisationsmodus: Startwerte der eingebauten Regler "Wissen" und "Werkzeuge"
und eigene Regler für Werkstätten. Jeder Spieler kann alle Werte im Spiel ändern (je 0 bis 10), der Knopf
"Standard" stellt die Werte aus dieser Datei wieder her.

rttr:SetKnowledgeDistribution{...}: Startwerte des Reglers "Wissen": Welches Wissensmedium Werkstätten mit
  outputChoice = "knowledge" herstellen (z.B. der Knochenschnitzer).
  Beispiel: LWP = 10, IWP = 5, MWP = 0 -> doppelt so viele LWP- wie IWP-Medien, keine MWP-Medien.
  Stehen alle möglichen Medien einer Werkstatt auf 0, wird gleich verteilt.

rttr:SetToolMaterialPriorities{...}: Startwerte des Reglers "Werkzeuge" (nur mit der Werkzeugreform, tools.lua):
  Vorrang der Materialien aus rttr:AddToolMaterial. Ein Gebäude bestellt das vorrätige Werkzeug mit dem höchsten
  Wert, bei Gleichstand das schnellere. 0 = dieses Material nie bestellen. Nicht genannte Materialien: 5.

rttr:AddRegulator{...}: eigener Regler mit einem eigenen Reiter im Fenster "Verteilung"
  name      Bezeichner. Nur A-Z, a-z, 0-9 und _. "knowledge" und "tools" sind vergeben. Wird in workshops.lua
            (outputChoice = name) und in Spielständen verwendet.
  title     Anzeigename (Tooltip des Reiters)
  entries   Liste der Einträge (höchstens 8), jeder ein Schieberegler:
              tags   Tag-Abfrage: Welche Waren zu diesem Eintrag gehören, z.B. "Krug" oder { "Topf", "Ton" }
              title  Anzeigename
              value  Startwert 0..10 (Standard 5)
  Eine Werkstatt mit outputChoice = name wählt ihr Produkt zufällig, gewichtet mit den Werten der Einträge. Jedes
  Produkt der Werkstatt muss genau einen Eintrag treffen. Mehrere Werkstätten dürfen denselben Regler nutzen.
  Stehen alle Einträge der Produkte einer Werkstatt auf 0, wird gleich verteilt.
  Höchstens 8 eigene Regler. Neue Einträge immer hinten anfügen (Spielstände).

Beispiel (auskommentiert, nur zur Erklärung des Formats): eine Töpferei, die Krüge und Töpfe aus Ton herstellt,
gesteuert mit einem eigenen Regler.

rttr:AddWare{ name = "ClayJug", title = "Tonkrug", kind = "Jug", properties = { "Clay" }, look = "Water",
              transport = "Water" }
rttr:AddWare{ name = "ClayPot", title = "Tontopf", kind = "Pot", properties = { "Clay" }, look = "Water",
              transport = "Water" }
rttr:AddRegulator{
    name = "Pottery",
    title = "Töpferei",
    entries = {
        { tags = "Jug", title = "Krüge", value = 8 },
        { tags = { "Pot", "Clay" }, title = "Tontöpfe", value = 3 },
    },
}
rttr:AddWorkshop{
    name = "Pottery",
    title = "Töpferei",
    size = "small",
    cost = { Boards = 2 },
    look = "Bakery",
    worker = { title = "Töpfer", tool = "none", look = "Baker" },
    inputs = { "Stones" },
    outputs = { "ClayJug", "ClayPot" },
    outputChoice = "Pottery",
    workTime = 40,
}
]]

rttr:SetKnowledgeDistribution{ LWP = 5, IWP = 5, MWP = 5 }

rttr:SetToolMaterialPriorities{ Iron = 8, Copper = 5 }
