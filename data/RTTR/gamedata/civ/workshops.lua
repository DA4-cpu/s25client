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
  size        "small", "medium", "large" oder "mine" (Minenplatz im Gebirge, nur zusammen mit mine)
  cost        Baukosten wie in buildingcosts.lua: { Boards = 2, Stones = 1 } (je ein Tag, alphabetisch verbaut) oder
              { { tags = { "Bretter", "Nadelholz" }, amount = 2 }, { tags = "Stones", amount = 1 } } (in dieser
              Reihenfolge verbaut). Jede Ware mit allen Tags eines Eintrags kann geliefert werden.
  look        Platzhaltergrafik: Das Gebäude sieht aus wie dieses eingebaute Gebäude (z.B. "Sawmill")
  worker      Arbeiter: { title = "...", tool = "Hammer" (oder "none"), look = "Carpenter" }
              tool: Werkzeug, das ein Helfer braucht, um Arbeiter zu werden. Mit der Werkzeugreform (tools.lua)
              braucht der Helfer kein Werkzeug mehr; tool ist dann das Werkzeug, das in der Werkstatt liegen muss
              (siehe tools). look: Platzhaltergrafik (eingebauter Beruf)
              Eigene Bilder für Gebäude und Arbeiter: Ordner data/RTTR/assets/civ/<name>/,
              siehe data/RTTR/assets/civ/LIESMICH.txt
  inputs      optional: bis zu 3 Eingänge, jeder eine Tag-Abfrage, z.B. { "Bones" } oder { { "Bretter", "Nadelholz" } }
  consume     optional: "each" (Standard): von jedem Eingang 1 Ware pro Produkt, jeder Eingang muss genau eine Ware
              treffen. "any": 1 Ware des Eingangs mit dem größten Vorrat (wie bei den Minen), höchstens 3 Waren insgesamt.
  output      Tag-Abfrage der hergestellten Ware. Mit outputChoice darf sie mehrere Waren treffen, z.B.
              output = "GeritzterKnochen" für alle geritzten Knochen.
  outputs     optional statt output: Liste der Produkte, jeder Eintrag eine Tag-Abfrage, die genau eine Ware trifft:
              outputs = { "CopperAxe", { "Hammer", "Copper" } } (ein Text = ein Tag, eine Liste = alle diese Tags).
              Bei mehr als einem Eintrag ist outputChoice nötig. Nicht für Sammler, Plantagen und Minen und nicht
              zusammen mit inheritProperties.
  outputRatio optional (Standard 1): Waren pro Arbeitsschritt, 0 bis 8, auch Kommazahlen. 0.5 = eine Ware jeden 2.
              Schritt, 2 = zwei Waren je Schritt, 1.5 = abwechselnd 1 und 2. Der Rest einer Ware wird mitgezählt, bis
              eine ganze erreicht ist. Die Eingänge werden in jedem Schritt verbraucht.
              Die Werkzeuge in Gebrauch zählen ihr outputRatio aus tools.lua dazu (0.5 + Werkzeug 1 = 1.5), höchstens
              8 insgesamt. Ist die Summe 0, arbeitet die Werkstatt nicht.
              Mehrere Waren je Schritt: Mit outputChoice wird jede einzeln gewählt, Minen und Sammler nehmen je Ware
              eine Einheit ihres Erzes bzw. Objekts (ist nichts mehr da, gibt es weniger Waren), Nebenprodukte
              (byproducts.lua) gibt es je Ware. Der Arbeiter legt alle zusammen an die Fahne, mehr als 8 nacheinander.
              Eingebaute Gebäude: rttr:SetBuildingOutputRatio in tools.lua.
  outputChoice optional: wie die Werkstatt ihr Produkt wählt, wenn es mehrere gibt:
              "knowledge": nach dem Regler "Wissen" im Fenster "Verteilung" (z.B. geritzte Knochen für LWP, IWP und
                MWP). Jedes Produkt braucht knowledge in wares.lua (genau eine Art).
              "tools": wie die Schlosserei, nach dem Werkzeug-Regler und den Werkzeugbestellungen des Spielers. Jedes
                Produkt muss ein Werkzeugtyp sein: der Bezeichner genau eines eingebauten Werkzeugs als Tag, z.B.
                kind = "Axe" in wares.lua. Jeder Typ höchstens einmal. Bestellungen teilt sie sich mit der Schlosserei:
                Wer ein bestelltes Werkzeug zuerst anfängt, erfüllt die Bestellung. Stehen alle ihre Werkzeuge auf 0,
                gilt die Einstellung "Schlosserei bei 0" wie bei der Schlosserei.
              sonst der Bezeichner eines eigenen Reglers aus distribution.lua (rttr:AddRegulator): nach dessen Werten.
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
              "Farmer" mäht mit der Sense, sonst klopft er wie der Steinmetz.
              Noch wachsende Objekte (von einer Plantage gepflanzt) sammelt er erst, wenn sie ausgewachsen sind.
  plant       optional: macht die Werkstatt zur Plantage wie den Bauernhof, den Förster oder den Köhler. Der Arbeiter
              pflanzt ein Kartenobjekt (mapobjects.lua, braucht dort growTime) im Umkreis radius:
              plant = { object = "PapyrusDeposit", radius = 4 }.
              Gepflanzt wird nur dort, wo das Objekt auch natürlich wächst (Regeln in world/mapgen.lua, aber ohne die
              Zonen), auf freien Plätzen ohne Straße und nicht direkt neben Gebäuden, Feldern und anderen Objekten
              (damit Wege frei bleiben). Fahnen dürfen daneben stehen wie bei Steinen.
              Jedes Pflanzen verbraucht die Eingänge (inputs, consume wie oben), z.B. inputs = { "Water" } zum Gießen.
              Ohne inputs braucht er dafür nichts.
              Mit output erntet er außerdem selbst wie der Bauer: ausgewachsene Objekte dieser Art im Umkreis (auch
              natürlich gewachsene), eine Ware pro Gang; Ernten geht vor Pflanzen. Ohne output pflanzt er nur (wie der
              Förster), ernten kann dann z.B. ein Sammler (collect).
              Arbeitsanimation beim Pflanzen: worker.look "Farmer" sät, sonst pflanzt er wie der Förster.
              Beim Ernten wie beim Sammler (siehe collect).
              Eigene Bilder fürs Pflanzen: Arbeiter_Pflanzen_<n> (siehe data/RTTR/assets/civ/LIESMICH.txt).
  mine        optional: macht die Werkstatt zur Mine für ein Erz aus mapobjects.lua (rttr:AddOre):
              mine = { resource = "Copper" }. Nur mit size = "mine". Der Arbeiter baut wie in den eingebauten Minen
              pro Produkt eine Einheit des Erzes im Umkreis von 2 Knoten ab. Ist nichts mehr da, kommt die Meldung
              "Diese Mine ist erschöpft". Eingänge wie bei den eingebauten Minen, z.B.
              inputs = { "Fish", "Meat", "Bread" }, consume = "any".
  workTime    Arbeitszeit pro Produkt in Sekunden (Sammler und Plantagen: pro Gang)
  waitTime    optional: Wartezeit vor der Arbeit in Sekunden
  knowledge   optional: Wissenspunkte des fertigen Gebäudes wie in knowledge.lua, z.B. knowledge = { IWP = 2 }.
              Ein Gebäude darf nur an einer der beiden Stellen Wissenspunkte bekommen.
  tools       optional, nur mit der Werkzeugreform (tools.lua): Werkzeuge der Werkstatt und ihr Tempo ohne Werkzeug,
              z.B. tools = { baseSpeed = 25, tools = { "Scythe", "Plough" } }. baseSpeed in Prozent (Standard 0 = ohne
              Werkzeug keine Arbeit), tools = Werkzeugtypen als Tag-Abfragen (höchstens 4).
              Ohne diese Angabe gilt worker.tool mit baseSpeed = 0 (bei tool = "none": kein Werkzeug, Tempo 100).
  aiNearBorder optional (Standard false): true erlaubt der KI, die Werkstatt in Grenznähe zu bauen (höchstens 4 Knoten
              von einer Landgrenze entfernt). Sonst hält die KI diesen Platz für Militärgebäude frei, wie bei allen
              eingebauten Gebäuden außer Holzfäller, Steinbruch, Fischer, Minen, Hafen und Katapult
              (ai/aijh/AIConstruction.cpp, BORDER_RESERVE_RADIUS und BUILDINGS_NEAR_BORDER).

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
    aiNearBorder = true,
}

-------------------------------------------------------------------------------
-- Kupfermine: baut das Erz Kupfer (mapobjects.lua) ab wie die Eisenmine das Eisenerz.
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "CopperMine",
    title = "Kupfermine",
    size = "mine",
    cost = { Boards = 4 },
    look = "IronMine",
    worker = { title = "Kupferbergmann", tool = "PickAxe", look = "Miner" },
    inputs = { "Fish", "Meat", "Bread" },
    consume = "any",
    storage = 2,
    mine = { resource = "Copper" },
    output = "CopperOre",
    workTime = 28,
    waitTime = 29,
    knowledge = { IWP = 1 },
    aiNearBorder = true,
}

-------------------------------------------------------------------------------
-- Papyrusplantage: pflanzt Papyrus (mapobjects.lua) dort, wo er auch natürlich wächst (am Süßwasser mit passenden
-- Temperatur- und Feuchtigkeitswerten, siehe world/mapgen.lua, ohne Zonenbeschränkung), gießt ihn mit Wasser und
-- erntet ihn selbst.
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "PapyrusPlantation",
    title = "Papyrusplantage",
    size = "small",
    cost = { Boards = 2 },
    look = "Farm",
    worker = { title = "Papyrusbauer", tool = "Scythe", look = "Farmer" },
    inputs = { "Water" },
    plant = { object = "PapyrusDeposit", radius = 6 },
    output = "PapyrusPlant",
    workTime = 6,
    waitTime = 6,
    knowledge = { LWP = 2 },
}

-------------------------------------------------------------------------------
-- Kupferwerkstatt: stellt Kupferwerkzeuge her wie die Schlosserei die Eisenwerkzeuge. Welches, legen der
-- Werkzeug-Regler und die Werkzeugbestellungen fest (outputChoice = "tools").
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "Copperworks",
    title = "Copperworks",
    size = "medium",
    cost = { Boards = 2 },
    look = "Metalworks",
    worker = { title = "Copperworker", tool = "Tongs", look = "Metalworker" },
    inputs = { "CopperOre", "Coal" },
    outputs = { "CopperTongs", "CopperHammer", "CopperAxe", "CopperSaw", "CopperPickAxe" },
    outputChoice = "tools",
    workTime = 120,
}
