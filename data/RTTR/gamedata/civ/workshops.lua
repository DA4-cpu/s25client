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
              Eigene Bilder für Gebäude und Arbeiter: Ordner data/RTTR/assets/civ/<name>/,
              siehe data/RTTR/assets/civ/LIESMICH.txt
  inputs      optional: bis zu 3 Eingänge, jeder eine Tag-Abfrage, z.B. { "Bones" } oder { { "Bretter", "Nadelholz" } }
  consume     optional: "each" (Standard): von jedem Eingang 1 Ware pro Produkt, jeder Eingang muss genau eine Ware
              treffen. "any": 1 Ware des Eingangs mit dem größten Vorrat (wie bei den Minen), höchstens 3 Waren insgesamt.
  output      Tag-Abfrage der hergestellten Ware
  outputChoice optional: "knowledge" erlaubt, dass output mehrere Waren trifft (z.B. geritzte Knochen für LWP, IWP
              und MWP). Welche davon hergestellt wird, legt später der Regler "Wissen" im Verteilungsfenster fest;
              bis es ihn gibt, wird zufällig gleich verteilt. Jede dieser Waren braucht knowledge in wares.lua.
  inheritProperties  optional (Standard false): Das Produkt hat dieselben Eigenschaften wie die verbrauchte Ware
              (z.B. Nadelholz -> Nadelholzbretter). Zu jeder möglichen Eingangsware muss es genau ein passendes Produkt
              geben.
  storage     optional (Standard 6): Lagerplätze je Eingang
  library     optional: macht die Werkstatt zur Bibliothek, z.B. library = { capacity = 20 }.
              Eine Bibliothek bestellt alle Wissensmedien (Waren mit knowledge in wares.lua) per Träger, bis capacity
              erreicht ist, und stellt nichts her. Darin zählen die WP der Medien voll (in Lagern nur halb). Wird sie
              zerstört, sind die Medien verloren. Ohne inputs, output und workTime.
  collect     optional: macht die Werkstatt zum Sammler wie den Steinbruch. Der Arbeiter geht zu Objekten auf der Karte
              (mapobjects.lua) im Umkreis radius, arbeitet dort workTime Sekunden und bringt output nach Hause:
              collect = { object = "LoamDeposit", radius = 8 }. Ohne inputs.
              Arbeitsanimation je nach worker.look: "Planer" gräbt mit der Schaufel, "Woodcutter" hackt,
              sonst klopft er wie der Steinmetz.
  plant       (fehlt aktuell noch) optional: macht die Werkstatt zum Anbauer wie der Farm oder dem Köhler. 
  workTime    Arbeitszeit pro Produkt in Sekunden
  waitTime    optional: Wartezeit vor der Arbeit in Sekunden

  Es fehlt der Wissenswert pro Gebäude!!! knowledge =  ...

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

-------------------------------------------------------------------------------
-- Knochen-Kette (Schritt 4a): 1 Knochen -> 1 geritzter Knochen (LWP, IWP oder MWP).
-- Entschieden: kleines Gebäude, Arbeiter mit Beil. Freigeschaltet durch "bone_carving" (techtree.lua).
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "BoneCarver",
    title = "Knochenschnitzer",
    size = "small",
    cost = { Boards = 2 }, 
    look = "Hunter",      
    worker = { title = "BoneCarver", tool = "Cleaver", look = "Butcher" }, -- look = PLATZHALTER
    inputs = { "Bones" },
    output = "GeritzterKnochen",
    outputChoice = "knowledge",
    workTime = 120,         
}

-------------------------------------------------------------------------------
-- Bibliothek (Schritt 4c). Entschieden: mittelgroß, fasst 20 Medien, Arbeiter "Bibliothekar" ohne Werkzeug, erst ab
-- Ära 2 (Technologie "libraries" in techtree.lua).
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "Library",
    title = "Bibliothek",
    size = "large",
    cost = { Boards = 5, Stones = 5 }, 
    look = "Temple",                   -- PLATZHALTER: Grafik-Vorbild
    worker = { title = "Bibliothekar", tool = "none", look = "Helper" },
    library = { capacity = 20 },
}

-------------------------------------------------------------------------------
-- Lehmgrube: sammelt Lehmvorkommen (mapobjects.lua) wie der Steinbruch die Steinhaufen.
-- Der Lehmstecher gräbt mit der Schaufel (Grafik-Vorbild: Planierer).
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "LoamPit",
    title = "Lehmgrube",
    size = "small",
    cost = { Boards = 2 },           
    look = "Quarry",                
    worker = { title = "Lehmstecher", tool = "Shovel", look = "Planer" },
    collect = { object = "LoamDeposit", radius = 8 }, -- PLATZHALTER: Reichweite wie der Steinbruch
    output = "Loam",
    workTime = 7,                    
    waitTime = 40,                   
}

