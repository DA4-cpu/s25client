-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Technologiebaum des Zivilisationsmodus.

Felder von rttr:AddTechnology{...}:
  name        eindeutiger Bezeichner (wird in Spielständen gespeichert, NICHT nachträglich umbenennen)
  title       Anzeigename
  description Beschreibung
  era         Zeitalter 0..6 (0 Jungsteinzeit, 1 Antike, 2 Klassik, 3 Mittelalter, 4 Frühe Neuzeit,
              5 Industriezeitalter, 6 Moderne)
  requires    Liste von Voraussetzungen (Bezeichner). Müssen WEITER OBEN definiert sein.
  knowledge   Kosten in Wissenspunkten, z.B. { LWP = 4, IWP = 2 } (LWP Landwirtschaft, IWP Industrie, MWP Militär). Standardwert+Wissensmedienzusatz
              Werden erst beim Abschluss abgebucht. Die WP der Gebäude stehen in knowledge.lua.
  cost        Warenkosten, z.B. { Boards = 4, Stones = 2 }. Bezeichner wie im Enum GoodType:
              Beer Tongs Hammer Axe Saw PickAxe Shovel Crucible RodAndLine Scythe Water Cleaver Rollingpin Bow
              Boat Sword Iron Flour Fish Bread Shield Wood Boards Stones Grain Coins Gold IronOre Coal Meat Ham
              Grapes Wine Skins Leather Armor
  time        Mindestdauer in Sekunden ab Forschungsbeginn
  unlocks     { buildings = {...} } Bezeichner wie im Enum BuildingType:
              Barracks Guardhouse Watchtower Fortress LookoutTower Catapult Woodcutter Forester Quarry Fishery
              Hunter Well Sawmill Slaughterhouse Mill Bakery Ironsmelter Metalworks Armory Mint Shipyard Brewery
              Storehouse Farm PigFarm DonkeyBreeder Charburner GoldMine IronMine CoalMine GraniteMine
              HarborBuilding Vineyard Winery Temple Skinner Tannery LeatherWorks
  exclusive   true: nur der Spieler, der sie zuerst erforscht, bekommt sie (Kulturen, Wunder)
  addon       Technologie existiert nur, wenn dieses Addon aktiv ist (z.B. "WINE", "LEATHER", "CHARBURNER")
  aiPriority  höher = die KI forscht sie früher

Alles, was hier von einer Technologie freigeschaltet wird, ist zu Spielbeginn gesperrt.

Balancing-Hinweis: Forschung bezahlt nur mit Waren, die frei im Lager liegen. Zwischenprodukte wie Getreide, Eisen
oder Münzen werden meist direkt an verarbeitende Gebäude geliefert und liegen selten im Lager - solche Kosten
werden nur langsam bezahlt. Gleiches gilt für Holz (geht direkt ins Sägewerk). Getreide kostet deshalb nur "Mahlen und Backen"
(bevor es Abnehmer gibt); sonst werden v. a. Bretter, Steine, Fisch und Wasser verlangt.
Ohne Forschung verfügbar (Grundwirtschaft): Holzfäller, Förster, Steinbruch, Sägewerk, Fischer, Jäger,
Brunnen, Baracke, Wachstube, Spähturm.

WP-Hinweis: Wissenspunkte werden nicht verbraucht, sondern "belegt". Um alle Technologien zu haben, braucht ein Spieler
mindestens so viele Gesamt-WP wie die Summe aller WP-Kosten (derzeit LWP 43, IWP 50, MWP 41 mit allen Addons).
Zum Start bringt das Hauptquartier 5/5/5. Die WP-Kosten waren Startwerte; Balancing macht der Nutzer.
]]

-------------------------------------------------------------------------------
-- 0: Jungsteinzeit
-------------------------------------------------------------------------------

rttr:AddTechnology{
    name = "storage",
    title = "Vorratshaltung",
    description = "Überschüsse werden nicht mehr nur im Lager des Stammes gesammelt, sondern in eigenen Vorratshäusern. "
        .. "Das verkürzt die Wege in entlegene Gebiete.",
    era = 0,
    knowledge = { LWP = 6, IWP = 6, MWP = 6 },
    time = 30,
    unlocks = { buildings = { "Storehouse" } },
    aiPriority = 100,
}

rttr:AddTechnology{
    name = "agriculture",
    title = "Ackerbau",
    description = "Statt nur zu jagen und zu sammeln, säen die Menschen Getreide aus und ernten es. "
        .. "Die Grundlage für Brot, Bier und Viehzucht.",
    era = 0,
    knowledge = { LWP = 5 },
    time = 60,
    unlocks = { buildings = { "Farm" } },
    aiPriority = 90,
}

rttr:AddTechnology{
    name = "milling",
    title = "Mahlen und Backen",
    description = "Getreide wird zu Mehl gemahlen und mit Wasser zu Brot gebacken. Brot ernährt die Bergleute.",
    era = 0,
    requires = { "agriculture" },
    knowledge = { LWP = 6 },
    time = 1,
    unlocks = { buildings = { "Mill", "Bakery" } },
    aiPriority = 80,
}

rttr:AddTechnology{
    name = "animal_husbandry",
    title = "Viehzucht",
    description = "Schweine werden mit Getreide und Wasser gemästet und in der Metzgerei zu Fleisch verarbeitet.",
    era = 0,
    requires = { "agriculture" },  
    knowledge = { LWP = 6 },
    time = 1,
    unlocks = { buildings = { "PigFarm", "Slaughterhouse" } },
    aiPriority = 80,
}

rttr:AddTechnology{
    name = "stone_walls",
    title = "Steinmauern",
    description = "Steinmauern bieten Schutz vor Feinden und verbessern die Verteidigungsfähigkeit des Dorfes.",
    era = 0,
    knowledge = { MWP = 9 },
    time = 60,
    unlocks = { buildings = { "Guardhouse"} },
    aiPriority = 80,
}

rttr:AddTechnology{
    name = "bone_carving",
    title = "Knochenschnitzerei", -- PLATZHALTER: Anzeigename
    description = "Jäger und Metzger heben die Knochen auf. In Knochen geritzte Zeichen bewahren Wissen.", -- PLATZHALTER
    era = 0,
    knowledge = { LWP = 7, IWP = 7 }, -- PLATZHALTER: jede Technologie braucht WP-Kosten, Wert noch offen
    -- PLATZHALTER: Voraussetzungen, Warenkosten und Dauer sind noch nicht festgelegt, z.B.
    requires = { "storage" },
    -- cost = { ... },
    time = 60,
    unlocks = { buildings = { "BoneCarver" } },
    aiPriority = 80,
}


-------------------------------------------------------------------------------
-- 1: Antike
-------------------------------------------------------------------------------

rttr:AddTechnology{
    name = "mining",
    title = "Bergbau",
    description = "Stollen werden in die Berge getrieben, um Kohle, Eisenerz und Granit abzubauen. "
        .. "Bergleute brauchen Nahrung.",
    era = 1,
    requires = { "agriculture", "storage" }, -- hier soll eigentlich milling oder animal_husbandry als Voraussetzung hin (mindestens 1 von den beiden muss erforscht sein), aber es gibt kein "or" bei der Abfrage glaube ich
    knowledge = { IWP = 15 },
    time = 60,
    unlocks = { buildings = { "CoalMine", "IronMine", "GraniteMine", "GoldMine" } },
    aiPriority = 90,
}

rttr:AddTechnology{
    name = "metallurgy",
    title = "Eisenverhüttung",
    description = "In der Eisenschmelze wird Erz mit Kohle zu Eisen geschmolzen.",
    era = 1,
    requires = { "mining" },
    knowledge = { IWP = 12+2 },
    cost = {},
    time = 120,
    unlocks = { buildings = { "Ironsmelter" } },
    aiPriority = 90,
}
rttr:AddTechnology{
    name = "toolmaking",
    title = "Werkzeugbau",
    description = "Ein Werkzeugmacher stellt Äxte, Sägen, Hämmer und andere Werkzeuge her, "
        .. "damit neue Berufe ausgebildet werden können.",
    era = 1,
    requires = { "metallurgy" },
    knowledge = { IWP = 12+2 },
    cost = {},
    time = 60,
    unlocks = { buildings = { "Metalworks" } },
    aiPriority = 90,
}

rttr:AddTechnology{
    name = "charcoal",
    title = "Köhlerei",
    description = "Holz wird in Meilern langsam zu Holzkohle verschwelt - Kohle ganz ohne Bergwerk.",
    era = 1,
    requires = { "metallurgy" },
    knowledge = { IWP = 12+2 },
    cost = { Wood = 4 },
    time = 120,
    unlocks = { buildings = { "Charburner" } },
    addon = "CHARBURNER",
    aiPriority = 40,
}

rttr:AddTechnology{
    name = "brewing",
    title = "Brauwesen",
    description = "Aus Getreide und Wasser wird Bier gebraut. Ohne Bier lassen sich keine Soldaten ausheben.",
    era = 1,
    requires = { "agriculture" },
    knowledge = { LWP = 12+2 },
    time = 60,
    unlocks = { buildings = { "Brewery" } },
    aiPriority = 90,
}

rttr:AddTechnology{
    name = "weaponsmithing",
    title = "Waffenschmiede",
    description = "Schmiede fertigen aus Eisen und Kohle Schwerter und Schilde für neue Soldaten.",
    era = 1,
    requires = { "metallurgy"},
    knowledge = { IWP = 8+1, MWP = 8+1 },
    cost = {},
    time = 60,
    unlocks = { buildings = { "Armory" } },
    aiPriority = 91,
}

rttr:AddTechnology{
    name = "currency",
    title = "Münzprägung",
    description = "Gold wird abgebaut und zu Münzen geprägt. Münzen spornen Soldaten zur Ausbildung an.",
    era = 1,
    requires = { "mining" },
    knowledge = { IWP = 15+2 },
    cost = { },
    time = 90,
    unlocks = { buildings = { "Mint" } },
    aiPriority = 80,
}

rttr:AddTechnology{
    name = "pack_animals",
    title = "Lasttiere",
    description = "Esel werden gezüchtet und tragen auf stark genutzten Straßen zusätzlich Waren.",
    era = 1,
    requires = { "animal_husbandry" },
    knowledge = { LWP = 12+2 },
    time = 1,
    unlocks = { buildings = { "DonkeyBreeder" } },
    aiPriority = 50,
}

rttr:AddTechnology{
    name = "viticulture",
    title = "Weinbau",
    description = "An sonnigen Hängen werden Reben gepflanzt; der Winzer keltert aus den Trauben Wein.",
    era = 1,
    requires = { "agriculture" },
    knowledge = { LWP = 12+2 },
    time = 30,
    unlocks = { buildings = { "Vineyard", "Winery" } },
    addon = "WINE",
    aiPriority = 30,
}

rttr:AddTechnology{
    name = "tanning",
    title = "Gerberei",
    description = "Häute aus der Schweinezucht werden gegerbt und zu Leder und Rüstungen verarbeitet.",
    era = 1,
    requires = { "animal_husbandry" },
    knowledge = { LWP = 8+1, IWP = 8+1 },
    cost = { },
    time = 60,
    unlocks = { buildings = { "Skinner", "Tannery", "LeatherWorks" } },
    addon = "LEATHER",
    aiPriority = 70,
}

rttr:AddTechnology{
    name = "lookout",
    title = "Spähturm",
    description = "Durch Späher kann man Feinde schon frühzeitig ausfindig machen.",
    era = 1,
    requires = { "stone_walls" },
    knowledge = { MWP = 12+2 },
    time = 60,
    unlocks = { buildings = { "LookoutTower" } },
    addon = "LEATHER",
    aiPriority = 89,
}

rttr:AddTechnology{
    name = "libraries",
    title = "Bibliotheken",
    description = "Wissensmedien werden in eigenen Häusern gesammelt und gepflegt. Dort zählen sie voll statt nur halb.",
    era = 1,
    requires = { "bone_carving" },
    knowledge = { LWP = 10, IWP = 10, MWP = 10 },      
    -- cost = { ... },
    time = 120,
    unlocks = { buildings = { "Library" } },
}


-------------------------------------------------------------------------------
-- 2: Klassik
-------------------------------------------------------------------------------

rttr:AddTechnology{
    name = "fortification",
    title = "Befestigungsbau",
    description = "Steinerne Wachtürme sichern die Grenzen und beherbergen mehr Soldaten.",
    era = 2,
    requires = { "metallurgy", "lookout" },
    knowledge = { MWP = 12+4 },
    cost = { Stones = 6 },
    time = 120,
    unlocks = { buildings = { "Watchtower" } },
    aiPriority = 91,
}

rttr:AddTechnology{
    name = "shipbuilding",
    title = "Schiffbau",
    description = "Werften bauen Boote für Wasserstraßen und später hochseetaugliche Schiffe.",
    era = 2,
    requires = { "toolmaking" },
    knowledge = { IWP = 16+4 },
    cost = { Boards = 8 },
    time = 60,
    unlocks = { buildings = { "Shipyard" } },
    aiPriority = 40,
}

rttr:AddTechnology{
    name = "navigation",
    title = "Seefahrt",
    description = "Häfen ermöglichen Expeditionen über das Meer, Handel mit Übersee und Angriffe von See aus.",
    era = 2,
    requires = { "shipbuilding" },
    knowledge = { LWP = 8+2, IWP = 8+2, MWP = 8+2 },
    cost = {},
    time = 120,
    unlocks = { buildings = { "HarborBuilding" } },
    aiPriority = 40,
}

rttr:AddTechnology{
    name = "siege_engines",
    title = "Belagerungstechnik",
    description = "Katapulte schleudern Steine auf feindliche Militärgebäude.",
    era = 2,
    requires = { "fortification" },
    knowledge = { MWP = 30+4 },
    cost = { Stones = 1 },
    time = 160,
    unlocks = { buildings = { "Catapult" } },
    aiPriority = 20,
}

rttr:AddTechnology{
    name = "temple_cult",
    title = "Tempelkult",
    description = "Tempel nehmen Opfergaben aus Wein, Fleisch und Brot entgegen und bringen dafür Gold hervor.",
    era = 2,
    requires = { "viticulture" },
    knowledge = { LWP = 5+2, IWP = 10+2 },
    cost = {},
    time = 160,
    unlocks = { buildings = { "Temple" } },
    addon = "WINE",
    aiPriority = 30,
}

-------------------------------------------------------------------------------
-- 3: Mittelalter
-------------------------------------------------------------------------------

rttr:AddTechnology{
    name = "castles",
    title = "Burgenbau",
    description = "Mächtige Burgen kontrollieren ein großes Gebiet und bieten Platz für eine ganze Garnison.",
    era = 3,
    requires = { "fortification", "currency" },
    knowledge = { MWP = 20+6 },
    time = 240,
    unlocks = { buildings = { "Fortress" } },
    aiPriority = 90,
}
