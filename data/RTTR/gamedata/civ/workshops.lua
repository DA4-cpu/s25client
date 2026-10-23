-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Werkstätten im Zivilisationsmodus: einfache Gebäude, die Eingangswaren verbrauchen und eine Ware herstellen.

rttr:AddWorkshop{...}:
  name        Bezeichner. Nur A-Z, a-z, 0-9 und _. Wird in techtree.lua (unlocks), knowledge.lua und in Spielständen
              verwendet.
  title       Anzeigename
  description optional: Hilfetext
  size        "small", "medium" oder "large"
  cost        Baukosten, vorerst nur Bretter und Steine: { Boards = 2, Stones = 1 }
  look        Platzhaltergrafik: Das Gebäude sieht aus wie dieses eingebaute Gebäude (z.B. "Sawmill")
  worker      Arbeiter: { title = "...", tool = "Hammer" (oder "none"), look = "Carpenter" }
              tool: Werkzeug, das ein Helfer braucht, um Arbeiter zu werden. look: Platzhaltergrafik (eingebauter Beruf)
  inputs      optional: bis zu 3 Eingänge, jeder eine Tag-Abfrage, z.B. { "Bones" } oder { { "Bretter", "Nadelholz" } }
  consume     optional: "each" (Standard): von jedem Eingang 1 Ware pro Produkt, jeder Eingang muss genau eine Ware
              treffen. "any": 1 Ware des Eingangs mit dem größten Vorrat (wie bei den Minen), höchstens 3 Waren insgesamt.
  output      Tag-Abfrage der hergestellten Ware
  inheritProperties  optional (Standard false): Das Produkt hat dieselben Eigenschaften wie die verbrauchte Ware
              (z.B. Nadelholz -> Nadelholzbretter). Zu jeder möglichen Eingangsware muss es genau ein passendes Produkt
              geben.
  storage     optional (Standard 6): Lagerplätze je Eingang
  workTime    Arbeitszeit pro Produkt in Sekunden
  waitTime    optional: Wartezeit vor der Arbeit in Sekunden

Freischalten: In techtree.lua unter unlocks = { buildings = { "Name" } } eintragen. Wird eine Werkstatt von keiner
Technologie freigeschaltet, ist sie von Anfang an baubar.

WICHTIG für Spielstände: Neue Werkstätten immer hinten anfügen, bestehende nicht löschen, umbenennen oder umsortieren.
Es sind höchstens 32 Werkstätten möglich.

Beispiel (auskommentiert, nur zur Erklärung des Formats, braucht die Beispielwaren aus wares.lua):

rttr:AddWorkshop{
    name = "ExampleWorkshop",
    title = "Beispiel-Werkstatt",
    size = "small",
    cost = { Boards = 2 },
    look = "Woodcutter",
    worker = { title = "Beispiel-Arbeiter", tool = "none", look = "Carpenter" },
    inputs = { "ExampleRaw" },
    output = "ExampleProduct",
    workTime = 20,
}
]]

rttr:AddWorkshop{
    name = "ExampleWorkshop",
    title = "Beispiel-Werkstatt",
    size = "small",
    cost = { Boards = 2 },
    look = "Woodcutter",
    worker = { title = "Beispiel-Arbeiter", tool = "none", look = "Carpenter" },
    output = "Loam",
    workTime = 20,
}

rttr:AddWorkshop{
    name = "LoamProcessor",
    title = "Lehmverarbeiter",
    size = "medium",
    cost = { Boards = 2, Stones = 2 },
    look = "Woodcutter",
    worker = { title = "Lehmverarbeiter", tool = "none", look = "Miner" },
    inputs = { "muddy", "Stones" },
    consume = "any",
    output = "Clay",
    workTime = 40,
}

