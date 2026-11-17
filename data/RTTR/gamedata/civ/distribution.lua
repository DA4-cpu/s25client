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

rttr:SetToolMaterialPriorities{ Iron = 4, Obsidian = 3.5, Bronze = 3, Copper = 2, Wood = 1 }

rttr:AddRegulator{    name = "Equipment",    title = "Equipment",
    entries = {  { tags = "Sword", title = "Sword", value = 5 },
        { tags = "Shield", title = "Shield", value = 5 },    },
}

rttr:AddRegulator{    name = "WoodenTools",    title = "WoodenTools", --nur ein temporärer Übergangsregler bis tools überarbeitet wurde und die Gesamtverteilung umgesetzt wurde
    entries = {  { tags = "WoodenHammer", title = "WoodenHammer", value = 5 },
        { tags = "WoodenSpindle", title = "WoodenSpindle", value = 8 },
        { tags = "WoodenBucket", title = "WoodenBucket", value = 5 },
        { tags = "Bow", title = "Bow", value = 5 },
        { tags = "RodAndLine", title = "RodAndLine", value = 5 },
        { tags = "Rollingpin", title = "Rollingpin", value = 5 },    },
}

rttr:AddRegulator{    name = "ObsidianTools",    title = "ObsidianTools", -- Platzhalter. Es sollte für alle Werkzeuge und Ausrüstung eine einzige gesammelte Bestellungliste 
-- und Verteilungsliste geben. Und die Verteilung soll sich automatisch nach dem aktuellen Bedarf einstellen. Diese GesamtVerteilung steuert dann automatisch 
-- alle Gebäude, die die Waren in der Liste herstellen. Die Gebäude stellen dann unter Berücksichtigung der Verteilung die Waren her, die sie herstellen 
-- können. Die Liste der Waren erstelle ich manuell und schreibe nur grobe Tags wie "Axe" oder "Sword" rein. 
    entries = {  { tags = "ObsidianAxe", title = "ObsidianAxe", value = 8 },
        { tags = "ObsidianSaw", title = "ObsidianSaw", value = 5 },
        { tags = "ObsidianScythe", title = "ObsidianScythe", value = 8 },
        { tags = "ObsidianSword", title = "ObsidianSword", value = 1 },
        { tags = "ObsidianSpear", title = "ObsidianSpear", value = 1 },
        { tags = "ObsidianArrow", title = "ObsidianArrow", value = 1 },
        { tags = "ObsidianCleaver", title = "ObsidianCleaver", value = 2 },  },
}

