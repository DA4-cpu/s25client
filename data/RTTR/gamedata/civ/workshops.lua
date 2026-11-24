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
  worker      Arbeiter: { title = "...", look = "Carpenter" }
              look: Platzhaltergrafik (eingebauter Beruf). Jeder Helfer kann ohne Werkzeug Arbeiter einer Werkstatt
              werden. Welche Werkzeuge in der Werkstatt liegen, steht unter tools.
              Eigene Bilder für Gebäude und Arbeiter: Ordner data/RTTR/assets/civ/<name>/,
              siehe data/RTTR/assets/civ/LIESMICH.txt
  inputs      optional: bis zu 10 Eingänge. Jeder Eingang ist ein Platz für alle Waren einer Tag-Abfrage, auch gemischt:
              inputs = { "Wood", { tags = "Food", amount = 1, storage = 6 }, { tags = { "Bretter", "Nadelholz" } } }
              Kurzform: eine Tag-Abfrage ("Wood" oder { "Bretter", "Nadelholz" }) = ein Platz mit amount 1.
              tags     Tag-Abfrage: jede Ware mit allen diesen Tags kann geliefert werden (z.B. "Food": Fisch, Fleisch,
                       Brot, Honig)
              amount   optional (Standard 1): Waren je Arbeitsschritt, auch Kommazahlen. 2 = 2 Waren je Schritt, 0.5 = 1
                       Ware jeden 2. Schritt (der Schritt, der eine Ware anbricht, nimmt sie ganz, der Rest zählt für die
                       nächsten Schritte), 1.5 = abwechselnd 2 und 1. Der Verbrauch hängt nur vom Arbeitsschritt ab, nicht
                       davon, wie viele Waren herauskommen.
              storage  optional (Standard: storage der Werkstatt): Plätze dieses Eingangs
              Ein Arbeitsschritt beginnt erst, wenn jeder Eingang die Waren für diesen Schritt hat. Aus einem gemischten
              Platz wird die Ware genommen, von der am meisten dort liegt (gleich viele: die zuerst definierte).
              Bestellt wird jede passende Ware aus dem nächsten Lager, das eine davon hat. Eine Ware, die zu mehreren
              Eingängen passt, kommt in den ersten, der noch Platz hat.
  output      Tag-Abfrage der hergestellten Ware. Sie darf mehrere Waren treffen (mehrere Produkte), z.B.
              output = "GeritzterKnochen" für alle geritzten Knochen.
  outputs     optional statt output: Liste der Produkte, jeder Eintrag eine Tag-Abfrage, die genau eine Ware trifft:
              outputs = { "CopperAxe", { "Hammer", "Copper" } } (ein Text = ein Tag, eine Liste = alle diese Tags).
              Mehrere Einträge: mehrere Produkte. Nicht für Sammler, Plantagen und Minen und nicht
              zusammen mit inheritProperties.
  outputRatio optional (Standard 1): Waren pro Arbeitsschritt, 0 bis 8, auch Kommazahlen. 0.5 = eine Ware jeden 2.
              Schritt, 2 = zwei Waren je Schritt, 1.5 = abwechselnd 1 und 2. Der Rest einer Ware wird mitgezählt, bis
              eine ganze erreicht ist. Die Eingänge werden in jedem Schritt verbraucht. 
              Die Werkzeuge in Gebrauch zählen ihr outputRatio aus tools.lua dazu (0.5 + Werkzeug 1 = 1.5), höchstens
              8 insgesamt. Ist die Summe 0, arbeitet die Werkstatt nicht.
              Mehrere Waren je Schritt: Bei mehreren Produkten wird jede einzeln gewählt, Nebenprodukte
              (byproducts.lua) gibt es je Ware. Minen, Sammler und Plantagen nehmen je Arbeitsschritt genau 1 Einheit
              ihres Erzes bzw. Objekts, egal wie viele Waren herauskommen. Der Arbeiter legt alle zusammen an die Fahne, mehr als 8 nacheinander.
              Eingebaute Gebäude: rttr:SetBuildingOutputRatio in tools.lua.
  outputChoice optional, nur "knowledge": Die Produkte (mehrere) werden nach dem Regler "Wissen" im Fenster
              "Verteilung" gewählt (z.B. geritzte Knochen für LWP, IWP und MWP). Jedes Produkt braucht knowledge in
              wares.lua (genau eine Art).
              Ohne outputChoice: Hat die Werkstatt mehrere Produkte, steuert sie die Gesamtverteilung (Fenster
              "Gesamtverteilung"): Bei jedem Arbeitsschritt wählt sie gewichtet nach den Werten ihrer Produkte, Bestelltes
              zuerst.
  inheritProperties  optional (Standard false): Das Produkt hat dieselben Eigenschaften wie die Ware, die zuletzt aus dem
              ersten Eingang verbraucht wurde (z.B. Nadelholz -> Nadelholzbretter). Zu jeder Ware des ersten Eingangs
              muss es genau ein passendes Produkt geben.
  storage     optional (Standard 6): Lagerplätze je Eingang (ein Eingang kann sein eigenes storage haben, siehe inputs)
  library     optional: macht die Werkstatt zur Bibliothek, z.B. library = { capacity = 20 }.
              Eine Bibliothek bestellt alle Wissensmedien (Waren mit knowledge in wares.lua) per Träger, bis capacity
              erreicht ist, und stellt nichts her. Darin zählen die WP der Medien voll (in Lagern nur halb). Wird sie
              zerstört, sind die Medien verloren. Ohne inputs, output und workTime.
  collect     optional: macht die Werkstatt zum Sammler wie den Steinbruch. Der Arbeiter geht zu Objekten auf der Karte
              (mapobjects.lua) im Umkreis radius, arbeitet dort workTime Sekunden und bringt output nach Hause:
              collect = { object = "LoamDeposit", radius = 8 }. Ohne inputs. Man sollte auch mehrere objects zum Collecten angeben können  
              Arbeitsanimation je nach worker.look: "Planer" gräbt mit der Schaufel, "Woodcutter" hackt,
              "Farmer" mäht mit der Sense, sonst klopft er wie der Steinmetz.
              Noch wachsende Objekte (von einer Plantage gepflanzt) sammelt er erst, wenn sie ausgewachsen sind.
  plant       optional: macht die Werkstatt zur Plantage wie den Bauernhof, den Förster oder den Köhler. Der Arbeiter
              pflanzt ein Kartenobjekt (mapobjects.lua, braucht dort growTime) im Umkreis radius:
              plant = { object = "PapyrusDeposit", radius = 4 }.
              Gepflanzt wird nur dort, wo das Objekt auch natürlich wächst (Regeln in world/mapgen.lua, aber ohne die
              Zonen), auf freien Plätzen ohne Straße und nicht direkt neben Gebäuden, Feldern und anderen Objekten
              (damit Wege frei bleiben). Fahnen dürfen daneben stehen wie bei Steinen.
              Jedes Pflanzen verbraucht die Eingänge (inputs wie oben), z.B. inputs = { "Water" } zum Gießen.
              Ohne inputs braucht er dafür nichts.
              Mit output erntet er außerdem selbst wie der Bauer: ausgewachsene Objekte dieser Art im Umkreis (auch
              natürlich gewachsene), eine Ware pro Gang; Ernten geht vor Pflanzen. Ohne output pflanzt er nur (wie der
              Förster), ernten kann dann z.B. ein Sammler (collect).
              Arbeitsanimation beim Pflanzen: worker.look "Farmer" sät, sonst pflanzt er wie der Förster.
              Beim Ernten wie beim Sammler (siehe collect).
              Eigene Bilder fürs Pflanzen: Arbeiter_Pflanzen_<n> (siehe data/RTTR/assets/civ/LIESMICH.txt).
  mine        optional: macht die Werkstatt zur Mine für ein Erz aus mapobjects.lua (rttr:AddOre):
              mine = { resource = "Copper" }. Nur mit size = "mine". Der Arbeiter baut wie in den eingebauten Minen
              je Arbeitsschritt eine Einheit des Erzes im Umkreis von 2 Knoten ab. Ist nichts mehr da, kommt die Meldung
              "Diese Mine ist erschöpft". Eingänge wie bei jeder Werkstatt, z.B. Nahrung:
              inputs = { { tags = "Food", storage = 6 } }.
  warehouse   optional: macht das Gebäude zum Lager statt zur Werkstatt:
              warehouse = { wares = { "Wood", "Stones", "Lehm" }, capacity = 100 }.
              wares = die Waren, die es aufnimmt (Tag-Abfragen wie bei inputs), capacity = Höchstzahl aller Waren
              zusammen (siehe storage.lua). Es nimmt nur diese Waren auf und keine Personen, erzeugt keine Gehilfen
              und hat keinen Arbeiter. Baustellen und Werkstätten holen ihre Waren auch von dort.
              Ein Lager hat nur name, title, description, size, cost, look, knowledge und aiNearBorder; worker, inputs,
              output, workTime usw. sind nicht erlaubt. Die KI baut solche Lager noch nicht.
              Beispiel:
                rttr:AddWorkshop{ name = "WoodYard", title = "Holzlager", size = "medium", cost = { Boards = 3 },
                    look = "Storehouse", warehouse = { wares = { "Wood", "Boards" }, capacity = 100 } }
  workTime    Arbeitszeit pro Produkt in Sekunden (Sammler und Plantagen: pro Gang)
  waitTime    optional: Wartezeit vor der Arbeit in Sekunden
  knowledge   optional: Wissenspunkte des fertigen Gebäudes wie in knowledge.lua, z.B. knowledge = { IWP = 2 }.
              Ein Gebäude darf nur an einer der beiden Stellen Wissenspunkte bekommen.
  tools       optional, nur mit der Werkzeugreform (tools.lua): Werkzeuge der Werkstatt und ihr Tempo ohne Werkzeug,
              z.B. tools = { baseSpeed = 25, tools = { "Scythe", "Plough" } }. baseSpeed in Prozent (Standard 0 = ohne
              Werkzeug keine Arbeit), tools = Werkzeugtypen als Tag-Abfragen (höchstens 4).
              Ohne diese Angabe braucht die Werkstatt kein Werkzeug und arbeitet mit Tempo 100.
  aiNearBorder optional (Standard false): true erlaubt der KI, die Werkstatt in Grenznähe zu bauen (höchstens 4 Knoten
              von einer Landgrenze entfernt). Sonst hält die KI diesen Platz für Militärgebäude frei, wie bei allen
              eingebauten Gebäuden außer Holzfäller, Steinbruch, Fischer, Minen, Hafen und Katapult
              (ai/aijh/AIConstruction.cpp, BORDER_RESERVE_RADIUS und BUILDINGS_NEAR_BORDER).

Man sollte hier auch custom Militärgebäude ergänzen können. und auch die im Spiel integrierten Gebäude anpassen können bzw. alle Gebäude sollten hier als
lua Werkstatt stehen oder zumdindest von hier aus ansteuerbar sein

Freischalten: In techtree.lua unter unlocks = { buildings = { "Name" } } eintragen. Wird eine Werkstatt von keiner
Technologie freigeschaltet, ist sie von Anfang an baubar.

WICHTIG für Spielstände: Neue Werkstätten immer hinten anfügen, bestehende nicht löschen, umbenennen oder umsortieren.
Es sind höchstens 128 Werkstätten möglich.

Beispiel (auskommentiert, nur zur Erklärung des Formats, braucht die Beispielwaren aus wares.lua):

rttr:AddWorkshop{
    name = "ExampleWorkshop",
    title = "Beispiel-Werkstatt",
    size = "small",
    cost = { Boards = 2 },
    look = "Woodcutter",
    worker = { title = "Beispiel-Arbeiter", look = "Carpenter" },
    inputs = { "ExampleRaw" },
    output = "ExampleProduct",
    workTime = 20,
}
]]

rttr:AddWorkshop{
    name = "LoamProcessor",
    title = "Lehmverarbeiter",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2  },
    look = "Woodcutter",
    worker = { title = "Lehmverarbeiter", look = "Miner" },
    inputs = { "Loam" },
    output = "Clay",
    outputRatio = 0.5,
    workTime = 37,
}

-------------------------------------------------------------------------------
-- Knochen-Kette (Schritt 4a): 1 Knochen -> 1 geritzter Knochen (LWP, IWP oder MWP).
-- Entschieden: kleines Gebäude, Arbeiter mit Beil. Freigeschaltet durch "bone_carving" (techtree.lua).
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "BoneCarver",
    title = "Knochenschnitzer",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 }, 
    look = "Hunter",      
    worker = { title = "BoneCarver", look = "Butcher" },
    inputs = { "Bones" },
    output = "GeritzterKnochen",
    outputChoice = "knowledge",
    tools = { baseSpeed = 0, tools = { "Cleaver" } },
    workTime = 50,         
}
-------------------------------------------------------------------------------
-- Bibliothek (Schritt 4c). Entschieden: mittelgroß, fasst 20 Medien, Arbeiter "Bibliothekar" ohne Werkzeug, erst ab
-- Ära 2 (Technologie "libraries" in techtree.lua).
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "Library",
    title = "Bibliothek",
    size = "large",
    cost = { BasicBuildingMaterial = 8, BasicRoofMaterial = 6, Stones = 6  }, 
    look = "Temple",
    worker = { title = "Bibliothekar", look = "Helper" },
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
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2  },           
    look = "Quarry",                
    worker = { title = "Lehmstecher", look = "Planer" },
    collect = { object = "LoamDeposit", radius = 9 }, 
    output = "Loam",
    outputRatio = 0.75,
    tools = { baseSpeed = 25, tools = { "Shovel" } },
    workTime = 7,                    
    waitTime = 27,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "PeatCollector",
    title = "PeatCollector",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },           
    look = "Quarry",                
    worker = { title = "Lehmstecher", look = "Planer" },
    collect = { object = "PeatDeposit", radius = 9 }, 
    output = "Coal",
    outputRatio = 0.4,
    tools = { baseSpeed = 25, tools = { "Shovel", "Bucket" } },
    workTime = 7,                    
    waitTime = 20,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "TinCollector",
    title = "TinCollector",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },           
    look = "Quarry",                
    worker = { title = "Miner", look = "Miner" },
    collect = { object = "TinDeposit", radius = 9 }, 
    output = "TinOre",
    outputRatio = 0.6,
    tools = { baseSpeed = 0, tools = { "PickAxe", "Bucket" } },
    workTime = 7,                    
    waitTime = 27,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "CopperCollector",
    title = "CopperCollector",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },           
    look = "Quarry",                
    worker = { title = "Miner", look = "Miner" },
    collect = { object = "CopperDeposit", radius = 9 }, 
    output = "CopperOre",
    outputRatio = 0.5,
    tools = { baseSpeed = 25, tools = { "PickAxe", "Bucket" } },
    workTime = 7,                    
    waitTime = 30,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "IronCollector",
    title = "IronCollector",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },           
    look = "Quarry",                
    worker = { title = "Miner", look = "Miner" },
    collect = { object = "IronDeposit", radius = 9 }, 
    output = "IronOre",
    outputRatio = 0.7,
    tools = { baseSpeed = 25, tools = { "PickAxe", "Bucket" } },
    workTime = 7,                    
    waitTime = 32,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "ObsidianCollector",
    title = "ObsidianCollector",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },           
    look = "Quarry",                
    worker = { title = "Miner", look = "Miner" },
    collect = { object = "ObsidianDeposit", radius = 9 }, 
    output = "Obsidian",
    outputRatio = 0.3,
    tools = { baseSpeed = 25, tools = { "PickAxe", "Bucket" } },
    workTime = 7,                    
    waitTime = 27,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

-------------------------------------------------------------------------------
-- Kupfermine: baut das Erz Kupfer (mapobjects.lua) ab wie die Eisenmine das Eisenerz.
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "CopperMine",
    title = "Kupfermine",
    size = "mine",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2, Rope = 1 },
    look = "IronMine",
    worker = { title = "Miner", look = "Miner" },
    inputs = { { tags = "Food", storage = 6 } },
    mine = { resource = "Copper" },
    output = "CopperOre",
    outputRatio = 0.5,
    tools = { baseSpeed = 25, tools = { "PickAxe" } },
    workTime = 28,
    waitTime = 15,
    knowledge = { IWP = 1 },
    aiNearBorder = true,
}

rttr:AddWorkshop{
    name = "TinMine",
    title = "TinMine",
    size = "mine",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2, Rope = 1 },
    look = "GraniteMine",
    worker = { title = "Miner", look = "Miner" }, -- es sollte nicht für jedes Gebäude ein neuer Job erstellt werden, sondern auch vorhandene 
    -- Jobs  wie zB. Miner übernommen werden können
    inputs = { "Wood", { tags = "Food", storage = 6 } },
    storage = 2,
    mine = { resource = "Tin" },
    output = "TinOre",
    outputRatio = 0.7,
    tools = { baseSpeed = 0, tools = { "PickAxe" } },
    workTime = 28,
    waitTime = 15,
    knowledge = { IWP = 1 },
    aiNearBorder = true,
}

rttr:AddWorkshop{
    name = "LimeMine",
    title = "LimeMine",
    size = "mine",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2, Rope = 1 },
    look = "GraniteMine",
    worker = { title = "Miner", look = "Miner" }, -- es sollte nicht für jedes Gebäude ein neuer Job erstellt werden, sondern auch vorhandene 
    -- Jobs  wie zB. Miner übernommen werden können
    inputs = { "Wood", { tags = "Food", storage = 6 } },
    storage = 2,
    mine = { resource = "Lime" },
    output = "Lime",
    outputRatio = 0.8,
    tools = { baseSpeed = 0, tools = { "PickAxe" } },
    workTime = 28,
    waitTime = 18,
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
    title = "PapyrusPlantation",
    size = "small",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4 },
    look = "Farm",
    worker = { title = "Papyrusbauer", look = "Farmer" },
    inputs = { "Water" },
    plant = { object = "PapyrusDeposit", radius = 9 },
    output = "PapyrusPlant",
    outputRatio = 0.5,
    tools = { baseSpeed = 0, tools = { "Scythe" } },
    workTime = 6,
    waitTime = 51,
    knowledge = { LWP = 2 },
}

rttr:AddWorkshop{
    name = "SpinningMill",
    title = "SpinningMill",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 3 },
    look = "Winery",
    worker = { title = "Spinner", look = "Farmer" },
    inputs = { { tags = "Fibers" } },
    output = "Rope",
    outputRatio = 0.8,
    workTime = 6,
    waitTime = 30,
    tools = { baseSpeed = 25, tools = { "Spindle" } },
    knowledge = { LWP = 1, IWP = 2 },
}

-------------------------------------------------------------------------------
-- Werkzeughersteller
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "WoodWorkshop",
    title = "WoodWorkshop",
    size = "small",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 3 },
    look = "Sawmill",
    worker = { title = "Metalworker", look = "Metalworker" },
    inputs = { "Wood", "Rope" }, 
    outputs = { "Rollingpin", "RodAndLine", "Bow", "WoodenSpindle", "WoodenBucket", "WoodenHammer" },
    outputRatio = 0.6,
    tools = { baseSpeed = 25, tools = { "Saw" } },
    workTime = 35,
    knowledge = { IWP = 2 },
}

rttr:AddWorkshop{
    name = "ObsidianWorkshop",
    title = "ObsidianWorkshop",
    size = "small",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4 },
    look = "Metalworks",
    worker = { title = "Metalworker", look = "Metalworker" },
    inputs = { "Obsidian", "Wood" }, -- "adhesive" ergänzen wenn diese endlich collectable auf der Map sind
    outputs = { "ObsidianAxe", "ObsidianSaw", "ObsidianScythe", "ObsidianCleaver", "ObsidianSword", "ObsidianSpear", "ObsidianArrow"}, -- eigentlich ObsidianSword auch für Rekrutierung nutzbar sein, aber das funktioniert noch nicht. 
    outputRatio = 0.8,
    workTime = 30,
    knowledge = { IWP = 2 },
}

rttr:AddWorkshop{
    name = "Copperworks",
    title = "Copperworks",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 4, Stones = 4 },
    look = "Metalworks",
    worker = { title = "Metalworker", look = "Metalworker" },
    inputs = { "CopperOre", "Coal" },
    outputs = { "CopperTongs", "CopperHammer", "CopperAxe", "CopperSaw", "CopperPickAxe" },
    outputRatio = 0.5,
    tools = { baseSpeed = 25, tools = { "Tongs" } },
    workTime = 30,
    knowledge = { IWP = 2 },
}

rttr:AddWorkshop{
    name = "AdobeProducer",
    title = "AdobeProducer",
    size = "small",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 2  },
    look = "Skinner",
    worker = { title = "AdobeProducer", look = "Metalworker" },
    inputs = { "Loam", { tags = "Fibers" } },
    outputs = { "Adobe" },
    workTime = 35,
    knowledge = { IWP = 2 },
}

rttr:AddWorkshop{
    name = "Brickworks",
    title = "Brickworks",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 4 },
    look = "LeatherWorks",
    worker = { title = "Brickworker", look = "Metalworker" },
    inputs = { "Loam", "Coal" },
    outputs = { "LoamBrick" },
    workTime = 50,
    knowledge = { IWP = 3 },
}
--[[
rttr:AddWorkshop{
    name = "AsphaltDrawer",
    title = "AsphaltDrawer",
    size = "small",
    cost = { Wood = 4 },
    look = "Fishery",
    worker = { title = "AsphaltDrawer", look = "AsphaltDrawer" },
    inputs = { "Bucket" }, 
    outputs = { "Asphalt" },
    outputRatio = 1,
    workTime = 120,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "BitumenProducer",
    title = "BitumenProducer",
    size = "medium",
    cost = { Boards = 2, Stones = 4 },
    look = "Leatherworks",
    worker = { title = "BitumenProducer", look = "Metalworker" },
    inputs = { "Asphalt", "Firewood" }, 
    outputs = { "Bitumen" },
    outputRatio = 0.5,
    workTime = 120,
    knowledge = { IWP = 3 },
}

rttr:AddWorkshop{
    name = "FirewoodSplitter",
    title = "FirewoodSplitter",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 4 },
    look = "Woodcutter",
    worker = { title = "Woodcutter", look = "Woodcutter" },
    inputs = { "Wood" }, 
    outputs = { "Firewood" },
    outputRatio = 2,
    tools = { baseSpeed = 0, tools = { "Axe" } },
    workTime = 40,
    knowledge = { LWP = 1, IWP = 1 },
}]]

rttr:AddWorkshop{
    name = "BronzeSmelter",
    title = "Bronze Smelter",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 6, Stones = 4 },
    look = "Ironsmelter",
    worker = { title = "Iron founder", look = "Minter" },
    inputs = { "CopperOre", "Coal", "TinOre" },
    output = { "BronzeIngot" },
    outputRatio = 0.5,
    tools = { baseSpeed = 25, tools = { "Crucible" } },
    workTime = 50,
    knowledge = { IWP = 3 },
}

rttr:AddWorkshop{
    name = "BronzeArmory",
    title = "Bronze Armory",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 6, Stones = 5 },
    look = "Armory",
    worker = { title = "Armorer", look = "Armorer" },
    inputs = { "BronzeIngot", "Coal"},
    outputs = { "BronzeSword", "BronzeShield" }, --  BronzeSword und BronzeShield kann noch nicht Verwendet werden. 
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Hammer" } },
    workTime = 50,
    knowledge = { IWP = 2, MWP = 2 },
}

rttr:AddWorkshop{
    name = "Bronzeworks",
    title = "Bronzeworks",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 6, Stones = 5 },
    look = "Metalworks",
    worker = { title = "Armorer", look = "Armorer" },
    inputs = { "BronzeIngot", "Coal"},
    outputs = { "BronzeTongs", "BronzeAxe", "BronzeSaw",  "BronzeHammer", "BronzePickAxe", "BronzeScythe", "BronzeShovel", "BronzeCrucible", "BronzeCleaver" }, 
    outputRatio = 0.8,
    tools = { baseSpeed = 25, tools = { "Tongs" } },
    workTime = 45,
    knowledge = { IWP = 2, MWP = 2 },
}
--[[
rttr:AddWorkshop{
    name = "Sawwmill",
    title = "Sawwmill",
    size = "medium",
    cost = { Boards = 2 },
    look = "Sawmill",
    worker = { title = "Copperworker", look = "Metalworker" },
    inputs = { "Holzstamm"},
    output = { "Boards" },
    tools = { baseSpeed = 0, tools = { "Scythe" } },
    workTime = 120,
}]]
