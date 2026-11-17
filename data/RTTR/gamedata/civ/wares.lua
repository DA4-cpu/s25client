-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Neue Waren und Tags im Zivilisationsmodus.

rttr:AddWare{...} definiert eine neue Ware:
  name        Bezeichner (zugleich der Name-Tag). Nur A-Z, a-z, 0-9 und _ (keine Umlaute, keine Leerzeichen).
              Wird in anderen Lua-Dateien und in Spielständen verwendet.
  title       Anzeigename (darf Umlaute enthalten)
  kind        optional: Warenart-Tag, z.B. "Bretter". Diese festen Tag Grenzen "kind" und "properties" sind dumm, da sie die Tagfreiheit einschränken. Lieber eine Tagliste wo alle Tags drinnen stehen ohne Limit
  properties  optional: Eigenschafts-Tags, z.B. { "Nadelholz" }. Werkstätten mit inheritProperties = true geben
              die Eigenschaften der verbrauchten Ware an ihr Produkt weiter.
  look        Platzhaltergrafik: Die Ware sieht aus wie diese eingebaute Ware (z.B. "Stones").
              Eigene Bilder: Ordner data/RTTR/assets/civ/<name>/, siehe data/RTTR/assets/civ/LIESMICH.txt
  transport   Transportpriorität: gleiche Gruppe wie diese eingebaute Ware (z.B. "Stones")
  knowledge   optional: Wissenspunkte, wenn die Ware als Wissensmedium eingelagert ist, z.B. { LWP = 1 }
              (wird erst mit den Bibliotheken in Phase 2 ausgewertet)

rttr:SetWareTags{...} gibt eingebauten Waren zusätzliche Tags:
  rttr:SetWareTags{ Boards = { kind = "Bretter", properties = { "Nadelholz" } } }

Tags: Jede Ware hat ihren Bezeichner als Tag, dazu kind und properties. Eine Tag-Abfrage (z.B. in den Eingängen einer
Werkstatt) ist ein einzelner Tag "Bretter" oder eine Liste { "Bretter", "Nadelholz" }; passend sind alle Waren, die alle
genannten Tags haben.

Ich  möchte hier auch Tags von Waren, die bereits fest im Spiel integriert sind ändern können. 

WICHTIG für Spielstände: Neue Waren immer hinten anfügen. Bestehende Waren nicht löschen, umbenennen oder umsortieren,
sonst passen alte Spielstände nicht mehr. Es sind höchstens 64 neue Waren möglich. Das Warenlimit muss dringend aufgehobenoder auf über  500 erhöht werden

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

rttr:SetWareTags{ Wood = { kind = "BasicBuildingMaterial", properties = {"BasicRoofMaterial"} } }
rttr:SetWareTags{ Boards = { kind = "BasicBuildingMaterial", properties = {"BasicRoofMaterial"} } }
rttr:SetWareTags{ Stones = { kind = "BasicBuildingMaterial", properties = {"BasicRoofMaterial"} } }


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

rttr:AddWare{    name = "Bones",    title = "Knochen",    look = "Stones",     transport = "Stones"  }

rttr:AddWare{    name = "CarvedBoneLWP",    title = "Geritzter Knochen (L)",    kind = "GeritzterKnochen",    properties = { "LWP" },    
    look = "Stones",        transport = "Stones",  knowledge = { LWP = 1 }}
rttr:AddWare{    name = "CarvedBoneIWP",    title = "Geritzter Knochen (I)",    kind = "GeritzterKnochen",    properties = { "IWP" },    
    look = "Stones",          transport = "Stones",     knowledge = { IWP = 1 }}
rttr:AddWare{    name = "CarvedBoneMWP",    title = "Geritzter Knochen (M)",    kind = "GeritzterKnochen",    properties = { "MWP" },    
    look = "Stones",          transport = "Stones",     knowledge = { MWP = 1 }}


-- Werkstoffe 
rttr:AddWare{    name = "TinOre",    title = "Tin Ore",    kind = "Ore",    look = "Gold",    transport = "Gold"}
rttr:AddWare{    name = "Lime",    title = "Lime",    look = "Stones",    transport = "Stones"}

rttr:AddWare{    name = "CopperOre",    title = "Copper Ore",    kind = "Ore",    look = "IronOre",    transport = "IronOre"}

rttr:AddWare{name = "CopperSword", title = "Copper Sword", kind = "Sword", look = "Sword", transport = "Sword"}
rttr:AddWare{name = "CopperShield", title = "Copper Shield", kind = "Shield", look = "Shield", transport = "Shield"}
rttr:AddWare{name = "CopperSpear", title = "Copper Spear", kind = "Spear", look = "Sword", transport = "Sword"}
rttr:AddWare{name = "CopperArmor", title = "Copper Armor", kind = "Armor", look = "Armor", transport = "Armor"} -- Armour soll später eine andere Funktionsweise haben und die Stats der Einheit beeifnlussen, aber das sollen sowieso alle Ausrüstungen für Einheiten

rttr:AddWare{    name = "CopperAxe",    title = "Copper Axe",    kind = "Axe",    look = "Axe",    transport = "Axe"}
rttr:AddWare{    name = "CopperTongs",    title = "Copper Tongs",    kind = "Tongs",    look = "Tongs",    transport = "Tongs"}
rttr:AddWare{    name = "CopperHammer",    title = "Copper Hammer",    kind = "Hammer",    look = "Hammer",    transport = "Hammer"}
rttr:AddWare{    name = "CopperSaw",    title = "Copper Saw",    kind = "Saw",    look = "Saw",    transport = "Saw"}
rttr:AddWare{name = "CopperPickAxe", title = "Copper PickAxe", kind = "PickAxe", look = "PickAxe", transport = "PickAxe"}
rttr:AddWare{name = "CopperShovel", title = "Copper Shovel", kind = "Shovel", look = "Shovel", transport = "Shovel"}
rttr:AddWare{name = "CopperCrucible", title = "Copper Crucible", kind = "Crucible", look = "Crucible", transport = "Crucible"}
rttr:AddWare{name = "CopperScythe", title = "Copper Scythe", kind = "Scythe", look = "Scythe", transport = "Scythe"}
rttr:AddWare{name = "CopperCleaver", title = "Copper Cleaver", kind = "Cleaver", look = "Cleaver", transport = "Cleaver"}

rttr:AddWare{name = "BronzeIngot", title = "Bronze Ingot", look = "Iron", transport = "Iron"}

rttr:AddWare{name = "BronzeSword", title = "Bronze Sword", kind = "Sword", look = "Sword", transport = "Sword"}
rttr:AddWare{name = "BronzeShield", title = "Bronze Shield", kind = "Shield", look = "Shield", transport = "Shield"}
rttr:AddWare{name = "BronzeSpear", title = "Bronze Spear", kind = "Spear", look = "Sword", transport = "Sword"}
rttr:AddWare{name = "BronzeArmor", title = "Bronze Armor", kind = "Armor", look = "Armor", transport = "Armor"} -- Armor soll später eine andere Funktionsweise haben und die Stats der Einheit beeifnlussen, aber das sollen sowieso alle Ausrüstungen für Einheiten

rttr:AddWare{    name = "BronzeAxe",    title = "Bronze Axe",    kind = "Axe",    look = "Axe",    transport = "Axe"}
rttr:AddWare{    name = "BronzeTongs",    title = "Bronze Tongs",    kind = "Tongs",    look = "Tongs",    transport = "Tongs"}
rttr:AddWare{    name = "BronzeHammer",    title = "Bronze Hammer",    kind = "Hammer",    look = "Hammer",    transport = "Hammer"}
rttr:AddWare{    name = "BronzeSaw",    title = "Bronze Saw",    kind = "Saw",    look = "Saw",    transport = "Saw"}
rttr:AddWare{name = "BronzePickAxe", title = "Bronze PickAxe", kind = "PickAxe", look = "PickAxe", transport = "PickAxe"}
rttr:AddWare{name = "BronzeShovel", title = "Bronze Shovel", kind = "Shovel", look = "Shovel", transport = "Shovel"}
rttr:AddWare{name = "BronzeCrucible", title = "Bronze Crucible", kind = "Crucible", look = "Crucible", transport = "Crucible"}
rttr:AddWare{name = "BronzeScythe", title = "Bronze Scythe", kind = "Scythe", look = "Scythe", transport = "Scythe"}
rttr:AddWare{name = "BronzeCleaver", title = "Bronze Cleaver", kind = "Cleaver", look = "Cleaver", transport = "Cleaver"}

rttr:AddWare{name = "Obsidian", title = "Obsidian", look = "Coal", transport = "Coal"}

rttr:AddWare{name = "ObsidianSword", title = "Obsidian Sword", look = "Sword", transport = "Sword"}
rttr:AddWare{name = "ObsidianSpear", title = "Obsidian Spear", look = "Sword", transport = "Sword"}
rttr:AddWare{name = "ObsidianArrow", title = "Obsidian Arrow", look = "Bow", transport = "Bow"}

rttr:AddWare{name = "ObsidianAxe", title = "Obsidian Axe", kind = "Axe", look = "Axe", transport = "Axe"}
rttr:AddWare{name = "ObsidianSaw", title = "Obsidian Saw",    kind = "Saw", look = "Saw", transport = "Saw"}
rttr:AddWare{name = "ObsidianScythe", title = "Obsidian Scythe", kind = "Scythe", look = "Scythe", transport = "Scythe"}
rttr:AddWare{name = "ObsidianCleaver", title = "Obsidian Cleaver", kind = "Cleaver", look = "Cleaver", transport = "Cleaver"}


--rttr:AddWare{ name = "Bow", title = "Bow", kind = "Bow", look = "Bow", transport = "Bow"} 
--rttr:AddWare{ name = "Rollingpin", title = "Rollingpin", kind = "Rollingpin", look = "Rollingpin", transport = "Rollingpin" } 
--rttr:AddWare{ name = "RodAndLine", title = "RodAndLine", kind = "RodAndLine", look = "RodAndLine", transport = "RodAndLine" } 
rttr:AddWare{ name = "WoodenHammer", title = "WoodenHammer", kind = "Hammer", look = "Hammer", transport = "Hammer" } 
rttr:AddWare{ name = "WoodenSpindle", title = "WoodenSpindle", kind = "Spindle", look = "Bow", transport = "Bow" } 
rttr:AddWare{ name = "WoodenBucket", title = "WoodenBucket", kind = "Bucket", look = "Water", transport = "Water" } 

rttr:AddWare{ name = "Rope", title = "Rope", look = "Wood", transport = "Wood" } 

-- pflanzliche Waren
rttr:AddWare{name = "PapyrusPlant", title = "Papyruspflanze", kind = "Fibers", look = "Grain", transport = "Grain"} -- bitte Waren nur in 1 Zeile schreiben
rttr:AddWare{ name = "Thatch", title = "Thatch", kind = "Fibers", properties = {"BasicRoofMaterial"}, look = "Grain", transport = "Grain"} 
--[[Warenlimit von 64 erreicht, deswegen werden diese Waren ausgeblendet

rttr:AddWare{ name = "Reed", title = "Red", kind = "Fibers", properties = {"BasicRoofMaterial"}, look = "Grain", transport = "Grain"} 
rttr:AddWare{ name = "Hemp", title = "Hemp", kind = "Fibers", look = "Grain", transport = "Grain"}
rttr:AddWare{ name = "Flax", title = "Flax", kind = "Fibers", look = "Grain", transport = "Grain"}
rttr:AddWare{ name = "LindenBast", title = "LindenBast", kind = "Fibers", look = "Grain", transport = "Grain"} -- soll als nebenprodukt des Holzfällers anfallen, aber nur wenn er gerade eine Linde gefällt hat
rttr:AddWare{ name = "BirchBark", title = "BirchBark", kind = "Bark", look = "Wood", transport = "Wood"} -- soll als nebenprodukt des Holzfällers anfallen, aber nur wenn er gerade eine Birke gefällt hat

rttr:AddWare{ name = "Beeswax", title = "Beeswax", kind = "Wax", look = "Coin", transport = "Coin"} -- soll von Bäumen mit Bienenstöcken gesammelt werden. Bäume haben eine gewisse Wahrscheinlichkeit mit einem Bienenstock zu spawnen. Allerdings unterliegen Bienenstöcken auch klimatischen Bedingungen und können nicht überall spawnen. Später soll ein Imker auch eigene Bienenstöcke "anbauen" können
rttr:AddWare{ name = "Honey", title = "Honey", kind = "Food", look = "Gold", transport = "Gold"} -- wie Beeswax

-- Klebstoffe
rttr:AddWare{ name = "Resin", title = "Resin", kind ="adhesive", look = "Gold", transport = "Gold"} -- Resin soll von Fichten, Pinien und Kiefern gesammelt werden. Die Bäume selbst sollen mit der Zeit Harz bilden was man sammelen kann. Wie setzt man einen Workshop um, der nur das Harz dieser Bäume sammelt, aber nicht die Bäume fällt. Außerdem soll der Holzfäller auch die Holzart an seine Fahne legen, die er auch gefällt hat. 
rttr:AddWare{ name = "Pitch", title = "Pitch", kind ="adhesive", look = "Coal", transport = "Coal"} -- wird aus BirchBast und Firewood hergestellt
rttr:AddWare{ name = "Tar", title = "Tar", kind ="adhesive", look = "Coal", transport = "Coal"}
rttr:AddWare{ name = "Bitumen", title = "Bitumen", kind ="adhesive", look = "Coal", transport = "Coal"}

rttr:AddWare{ name = "Asphalt", title = "Asphalt", look = "Water", transport = "Water"}
]]
rttr:AddWare{ name = "Firewood", title = "Firewood", look = "Wood", transport = "Wood"}

-- Baumaterial
rttr:AddWare{    name = "Adobe",    title = "Adobe",        kind = "BasicBuildingMaterial",     look = "Leather",    transport = "Stones"}

rttr:AddWare{    name = "LoamBrick",    title = "LoamBrick",    kind = "Stones",    look = "Leather",    transport = "Stones"} -- Dachmaterial als Tag ergänzen

-- Mörtel bzw. römischen Beton hinzufügen. Basierend auf Bindemittel wie Kalk oder Gips
