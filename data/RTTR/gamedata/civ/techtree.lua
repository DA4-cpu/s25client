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
    cost = { Boards = 4, Stones = 4 },
    time = 30,
    unlocks = { buildings = { "Storehouse" } },
    aiPriority = 95,
}

rttr:AddTechnology{
    name = "agriculture",
    title = "Ackerbau",
    description = "Statt nur zu jagen und zu sammeln, säen die Menschen Getreide aus und ernten es. "
        .. "Die Grundlage für Brot, Bier und Viehzucht.",
    era = 0,
    cost = { Boards = 6 },
    time = 30,
    unlocks = { buildings = { "Farm" } },
    aiPriority = 90,
}

rttr:AddTechnology{
    name = "toolmaking",
    title = "Werkzeugbau",
    description = "Ein Werkzeugmacher stellt Äxte, Sägen, Hämmer und andere Werkzeuge her, "
        .. "damit neue Berufe ausgebildet werden können.",
    era = 0,
    cost = { Boards = 6, Stones = 2 },
    time = 30,
    unlocks = { buildings = { "Metalworks" } },
    aiPriority = 85,
}

rttr:AddTechnology{
    name = "milling",
    title = "Mahlen und Backen",
    description = "Getreide wird zu Mehl gemahlen und mit Wasser zu Brot gebacken. Brot ernährt die Bergleute.",
    era = 0,
    requires = { "agriculture" },
    cost = { Grain = 4, Stones = 4, Boards = 2 },
    time = 45,
    unlocks = { buildings = { "Mill", "Bakery" } },
    aiPriority = 80,
}

rttr:AddTechnology{
    name = "animal_husbandry",
    title = "Viehzucht",
    description = "Schweine werden mit Getreide und Wasser gemästet und in der Metzgerei zu Fleisch verarbeitet.",
    era = 0,
    requires = { "agriculture" },
    cost = { Boards = 6, Stones = 2 },
    time = 45,
    unlocks = { buildings = { "PigFarm", "Slaughterhouse" } },
    aiPriority = 70,
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
    requires = { "toolmaking" },
    cost = { Boards = 6, Fish = 4 },
    time = 60,
    unlocks = { buildings = { "CoalMine", "IronMine", "GraniteMine" } },
    aiPriority = 88,
}

rttr:AddTechnology{
    name = "metallurgy",
    title = "Eisenverhüttung",
    description = "In der Eisenschmelze wird Erz mit Kohle zu Eisen geschmolzen.",
    era = 1,
    requires = { "mining" },
    cost = { IronOre = 4, Coal = 4, Stones = 6 },
    time = 60,
    unlocks = { buildings = { "Ironsmelter" } },
    aiPriority = 86,
}

rttr:AddTechnology{
    name = "charcoal",
    title = "Köhlerei",
    description = "Holz wird in Meilern langsam zu Holzkohle verschwelt - Kohle ganz ohne Bergwerk.",
    era = 1,
    requires = { "metallurgy" },
    cost = { Boards = 4, Stones = 4 },
    time = 45,
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
    cost = { Water = 4, Boards = 4 },
    time = 45,
    unlocks = { buildings = { "Brewery" } },
    aiPriority = 78,
}

rttr:AddTechnology{
    name = "weaponsmithing",
    title = "Waffenschmiede",
    description = "Schmiede fertigen aus Eisen und Kohle Schwerter und Schilde für neue Soldaten.",
    era = 1,
    requires = { "metallurgy", "brewing" },
    cost = { Iron = 6, Coal = 6, Boards = 4 },
    time = 60,
    unlocks = { buildings = { "Armory" } },
    aiPriority = 75,
}

rttr:AddTechnology{
    name = "pack_animals",
    title = "Lasttiere",
    description = "Esel werden gezüchtet und tragen auf stark genutzten Straßen zusätzlich Waren.",
    era = 1,
    requires = { "animal_husbandry" },
    cost = { Water = 6, Boards = 6 },
    time = 60,
    unlocks = { buildings = { "DonkeyBreeder" } },
    aiPriority = 45,
}

rttr:AddTechnology{
    name = "viticulture",
    title = "Weinbau",
    description = "An sonnigen Hängen werden Reben gepflanzt; der Winzer keltert aus den Trauben Wein.",
    era = 1,
    requires = { "agriculture" },
    cost = { Boards = 6, Stones = 2 },
    time = 45,
    unlocks = { buildings = { "Vineyard", "Winery" } },
    addon = "WINE",
    aiPriority = 35,
}

rttr:AddTechnology{
    name = "tanning",
    title = "Gerberei",
    description = "Häute aus der Schweinezucht werden gegerbt und zu Leder und Rüstungen verarbeitet.",
    era = 1,
    requires = { "animal_husbandry" },
    cost = { Ham = 4, Boards = 4 },
    time = 45,
    unlocks = { buildings = { "Skinner", "Tannery", "LeatherWorks" } },
    addon = "LEATHER",
    aiPriority = 35,
}

-------------------------------------------------------------------------------
-- 2: Klassik
-------------------------------------------------------------------------------

rttr:AddTechnology{
    name = "fortification",
    title = "Befestigungsbau",
    description = "Steinerne Wachtürme sichern die Grenzen und beherbergen mehr Soldaten.",
    era = 2,
    requires = { "mining" },
    cost = { Stones = 10, Boards = 6 },
    time = 60,
    unlocks = { buildings = { "Watchtower" } },
    aiPriority = 65,
}

rttr:AddTechnology{
    name = "currency",
    title = "Münzprägung",
    description = "Gold wird abgebaut und zu Münzen geprägt. Münzen spornen Soldaten zur Ausbildung an.",
    era = 2,
    requires = { "metallurgy" },
    cost = { Iron = 2, Coal = 4, Stones = 6, Boards = 6 },
    time = 90,
    unlocks = { buildings = { "GoldMine", "Mint" } },
    aiPriority = 55,
}

rttr:AddTechnology{
    name = "shipbuilding",
    title = "Schiffbau",
    description = "Werften bauen Boote für Wasserstraßen und später hochseetaugliche Schiffe.",
    era = 2,
    requires = { "toolmaking" },
    cost = { Boards = 14, Stones = 4 },
    time = 60,
    unlocks = { buildings = { "Shipyard" } },
    aiPriority = 30,
}

rttr:AddTechnology{
    name = "siege_engines",
    title = "Belagerungstechnik",
    description = "Katapulte schleudern Steine auf feindliche Militärgebäude.",
    era = 2,
    requires = { "fortification" },
    cost = { Stones = 10, Boards = 8 },
    time = 90,
    unlocks = { buildings = { "Catapult" } },
    aiPriority = 35,
}

rttr:AddTechnology{
    name = "temple_cult",
    title = "Tempelkult",
    description = "Tempel nehmen Opfergaben aus Wein, Fleisch und Brot entgegen und bringen dafür Gold hervor.",
    era = 2,
    requires = { "viticulture" },
    cost = { Wine = 4, Stones = 8, Boards = 6 },
    time = 90,
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
    cost = { Stones = 20, Boards = 12, Coins = 4 },
    time = 120,
    unlocks = { buildings = { "Fortress" } },
    aiPriority = 50,
}

rttr:AddTechnology{
    name = "navigation",
    title = "Hochseeschifffahrt",
    description = "Häfen ermöglichen Expeditionen über das Meer, Handel mit Übersee und Angriffe von See aus.",
    era = 3,
    requires = { "shipbuilding" },
    cost = { Boards = 16, Stones = 10 },
    time = 120,
    unlocks = { buildings = { "HarborBuilding" } },
    aiPriority = 30,
}
