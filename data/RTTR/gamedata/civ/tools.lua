-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Werkzeugreform im Zivilisationsmodus.

Sobald hier mindestens ein Material steht, gilt:
  - Jeder freie Helfer kann ohne Werkzeug einen Beruf in einem Gebäude annehmen. Kommt ein Arbeiter ins Lager zurück,
    ist er wieder Helfer. Nur Bauarbeiter, Planierer, Geologe und Späher brauchen weiter ihr Werkzeug im Lager.
  - Die Werkzeuge liegen in den Gebäuden (eigener Werkzeugplatz neben Eingang und Ausgang). Jedes Gebäude bestellt
    seine Werkzeuge selbst aus den Lagern.
  - Bei jedem Arbeitsschritt nutzen sich alle Werkzeuge im Gebäude um 1 ab. Bei 0 ist das Werkzeug kaputt.
  - Arbeitstempo des Gebäudes in Prozent = baseSpeed + speed aller Werkzeuge im Gebäude. 100 = bisheriges Tempo,
    0 = das Gebäude arbeitet nicht.
  - Zum Spielstart werden die Arbeiter im Hauptquartier zu Helfern, ihre Werkzeuge kommen als Waren ins Lager.
Ohne Material in dieser Datei bleibt alles beim alten Werkzeugsystem.

rttr:AddToolMaterial{...}: Material der Werkzeuge
  name      Bezeichner. Nur A-Z, a-z, 0-9 und _.
  title     Anzeigename
  Höchstens 8 Materialien. Neue Materialien immer hinten anfügen (Spielstände). Den Startwert im Regler "Werkzeuge"
  (Vorrang beim Bestellen) setzt distribution.lua mit rttr:SetToolMaterialPriorities.

rttr:AddTool{...}: macht eine Ware (eingebaut oder aus wares.lua) zum Werkzeug
  ware        Bezeichner der Ware
  material    Bezeichner des Materials
  durability  Haltbarkeit in Arbeitsschritten
  speed       Bonus auf das Arbeitstempo in Prozent
  outputRatio optional (Standard 0): Bonus auf die Waren pro Arbeitsschritt des Gebäudes (siehe outputRatio in
              workshops.lua), -8 bis 8, auch negativ. Zählt, solange das Werkzeug in Gebrauch ist. Zwei Formen:
              outputRatio = 0.5             +0.5 für alle Produkte des Gebäudes
              outputRatio = { Sword = 0.5, Copper = -0.2 }   nur für die Produkte mit diesem Tag (ein Produkt mit
                                            beiden Tags bekommt beides: +0.3)
              Die Summe des Gebäudes bleibt zwischen 0 und 8.
  inputRatio  optional (Standard 0): Änderung der Waren je Arbeitsschritt der Eingänge (amount in workshops.lua), -99
              bis 99, auch Kommazahlen. Zwei Formen:
              inputRatio = -0.2             0.2 Waren weniger je Schritt bei jedem Eingang
              inputRatio = { Coal = -0.25 } nur bei den Eingängen, deren Waren alle diesen Tag haben (ein Eingang
                                            { tags = "Food" } mit Fisch, Fleisch, Brot nur bei einem Tag, den alle drei
                                            haben)
              Ein Eingang braucht dann mindestens 0 (0 = er braucht nichts) und höchstens so viele, wie er Plätze hat.
              Eingebaute Minen (mehrere Nahrungsplätze, eine beliebige Nahrung je Schritt) beachten inputRatio nicht.
  Der Werkzeugtyp ergibt sich aus den Tags der Ware: Eine neue Ware "CopperAxe" mit kind = "Axe" zählt überall als
  Axt, wo unten "Axe" steht.

rttr:SetBuildingTools{...}: Werkzeuge der eingebauten Gebäude
  baseSpeed   Arbeitstempo ohne Werkzeug in Prozent (Standard 0)
  tools       Liste der Werkzeugtypen (Tag-Abfragen wie bei den Eingängen), höchstens 4. Von jedem Typ liegt höchstens
              ein Werkzeug in Gebrauch im Gebäude, dazu höchstens ein Ersatz.
  Nicht genannte Gebäude nutzen das bisherige Werkzeug ihres Arbeiters mit baseSpeed = 0; Gebäude, deren Arbeiter
  kein Werkzeug hatte, arbeiten wie bisher (Tempo 100).
  Werkstätten aus workshops.lua bekommen ihre Werkzeuge dort: tools = { baseSpeed = 0, tools = { "Saw" } }.
  Ohne diese Angabe braucht eine Werkstatt kein Werkzeug und arbeitet mit Tempo 100.

rttr:SetBuildingOutputRatio{...}: Waren pro Arbeitsschritt der eingebauten Gebäude ohne Werkzeug (Standard 1), 0 bis 8,
  z.B. rttr:SetBuildingOutputRatio{ Woodcutter = 1, Quarry = 1.5 }. Bedeutung wie outputRatio in workshops.lua
  (dort für die Werkstätten). Gilt auch ohne Werkzeugreform. Ein Holzfäller mit 2 bringt 2 Holz je Baum. Minen und
  Abbauer nehmen je Arbeitsschritt genau 1 Einheit ihres Vorkommens, egal wie viele Waren herauskommen: Ein Steinmetz
  mit 2 nimmt 1 Stück vom Steinhaufen und bringt 2 Steine, ein Fischer mit 2 fängt 1 Fisch und bringt 2. Das braucht
  man nicht mehr, wenn die Gebäude alle in workshops.lua stehen

rttr:SetToolReorder(n): Ein Ersatzwerkzeug wird bestellt, sobald das Werkzeug nur noch n Prozent seiner Haltbarkeit
  hat. Es wartet im Gebäude, bis das alte kaputt ist.
]]

rttr:AddToolMaterial{ name = "Iron", title = "Iron" }

rttr:AddTool{ ware = "Tongs",      material = "Iron", durability = 200, speed = 100, outputRatio = 0 } -- Zange
rttr:AddTool{ ware = "Hammer",     material = "Iron", durability = 200, speed = 100, outputRatio = 0 } -- Hammer
rttr:AddTool{ ware = "Axe",        material = "Iron", durability = 200, speed = 100, outputRatio = 0 } -- Axt
rttr:AddTool{ ware = "Saw",        material = "Iron", durability = 200, speed = 30, outputRatio = 0.7 } -- Säge
rttr:AddTool{ ware = "PickAxe",    material = "Iron", durability = 200, speed = 100, outputRatio = 0 } -- Spitzhacke
rttr:AddTool{ ware = "Shovel",     material = "Iron", durability = 200, speed = 100, outputRatio = 0 } -- Schaufel
rttr:AddTool{ ware = "Crucible",   material = "Iron", durability = 200, speed = 100, outputRatio = 0 } -- Schmelztiegel
rttr:AddTool{ ware = "Scythe",     material = "Iron", durability = 200, speed = 50, outputRatio = 0.5 } -- Sense
rttr:AddTool{ ware = "Cleaver",    material = "Iron", durability = 200, speed = 100, outputRatio = 0 } -- Beil
rttr:AddTool{ ware = "IronPlough",    material = "Iron", durability = 200, speed = 100, outputRatio = 0 }

rttr:AddToolMaterial{ name = "Copper", title = "Copper" }

rttr:AddTool{ ware = "CopperTongs",      material = "Copper", durability = 100, speed = 50, outputRatio = 0 } -- Zange
rttr:AddTool{ ware = "CopperHammer",     material = "Copper", durability = 100, speed = 50, outputRatio = 0 } -- Hammer
rttr:AddTool{ ware = "CopperAxe",        material = "Copper", durability = 100, speed = 50, outputRatio = 0 } -- Axt
rttr:AddTool{ ware = "CopperSaw",        material = "Copper", durability = 100, speed = 10, outputRatio = 0.4 } -- Säge
rttr:AddTool{ ware = "CopperPickAxe",    material = "Copper", durability = 100, speed = 50, outputRatio = 0 } -- Spitzhacke
rttr:AddTool{ ware = "CopperShovel",     material = "Copper", durability = 100, speed = 50, outputRatio = 0 } -- Schaufel
rttr:AddTool{ ware = "CopperCrucible",   material = "Copper", durability = 100, speed = 50, outputRatio = 0 } -- Schmelztiegel
rttr:AddTool{ ware = "CopperScythe",     material = "Copper", durability = 100, speed = 25, outputRatio = 0.25 } 
rttr:AddTool{ ware = "CopperCleaver",    material = "Copper", durability = 100, speed = 50, outputRatio = 0 } -- Beil
rttr:AddTool{ ware = "CopperPlough",    material = "Copper", durability = 100, speed = 50, outputRatio = 0 }

rttr:AddToolMaterial{ name = "Bronze", title = "Bronze" }

rttr:AddTool{ ware = "BronzeTongs",      material = "Bronze", durability = 150, speed = 75, outputRatio = 0 } -- Zange
rttr:AddTool{ ware = "BronzeHammer",     material = "Bronze", durability = 150, speed = 75, outputRatio = 0 } -- Hammer
rttr:AddTool{ ware = "BronzeAxe",        material = "Bronze", durability = 150, speed = 75, outputRatio = 0 } -- Axt
rttr:AddTool{ ware = "BronzeSaw",        material = "Bronze", durability = 150, speed = 20, outputRatio = 0.55 } -- Säge
rttr:AddTool{ ware = "BronzePickAxe",    material = "Bronze", durability = 150, speed = 75, outputRatio = 0 } -- Spitzhacke
rttr:AddTool{ ware = "BronzeShovel",     material = "Bronze", durability = 150, speed = 75, outputRatio = 0 } -- Schaufel
rttr:AddTool{ ware = "BronzeCrucible",   material = "Bronze", durability = 150, speed = 75, outputRatio = 0 } -- Schmelztiegel
rttr:AddTool{ ware = "BronzeScythe",     material = "Bronze", durability = 150, speed = 37, outputRatio = 0.38 } -- Sense
rttr:AddTool{ ware = "BronzeCleaver",    material = "Bronze", durability = 150, speed = 75, outputRatio = 0 } -- Beil
rttr:AddTool{ ware = "BronzePlough",    material = "Bronze", durability = 150, speed = 75, outputRatio = 0 }

rttr:AddToolMaterial{ name = "Obsidian", title = "Obsidian" }

rttr:AddTool{ ware = "ObsidianAxe",        material = "Obsidian", durability = 50, speed = 100, outputRatio = 0 } 
rttr:AddTool{ ware = "ObsidianSaw",        material = "Obsidian", durability = 50, speed = 30, outputRatio = 0.7 }
rttr:AddTool{ ware = "ObsidianScythe",     material = "Obsidian", durability = 50, speed = 50, outputRatio = 0.5 }
rttr:AddTool{ ware = "ObsidianCleaver",    material = "Obsidian", durability = 50, speed = 100, outputRatio = 0 } 

rttr:AddToolMaterial{ name = "Wood", title = "Wood" }

rttr:AddTool{ ware = "Bow",      material = "Wood", durability = 100, speed = 100 } 
rttr:AddTool{ ware = "Rollingpin",     material = "Wood", durability = 100, speed = 100 } 
rttr:AddTool{ ware = "RodAndLine",        material = "Wood", durability = 100, speed = 100 } 
rttr:AddTool{ ware = "WoodenHammer",        material = "Wood", durability = 50, speed = 50 } 
rttr:AddTool{ ware = "WoodenSpindle",        material = "Wood", durability = 100, speed = 100 } 
rttr:AddTool{ ware = "WoodenBucket",        material = "Wood", durability = 100, speed = 100 } 

rttr:SetToolReorder(10)

-- Alles, was man braucht, um neue Werkzeuge herzustellen (Holz, Bretter, Stein, Bergbau, Eisenschmelze, Schlosserei),
-- arbeitet auch ohne Werkzeug langsam weiter. Sonst gäbe es nach dem Bruch des letzten Werkzeugs nie wieder welche. Wenn 
-- die ingame Gebäude alle in workshops.lua stehen, braucht man diese Tabelle nicht mehr
rttr:SetBuildingTools{
    -- Holz und Stein
    Woodcutter     = { baseSpeed = 25, tools = { "Axe" } },       -- Holzfäller
    Forester       = { baseSpeed = 25, tools = { "Shovel" } },     -- Förster
    Sawmill        = { baseSpeed = 25, tools = { "Saw" } },        -- Sägewerk
    Quarry         = { baseSpeed = 25, tools = { "PickAxe" } },    -- Steinbruch

    -- Nahrung und Landwirtschaft
    Fishery        = { baseSpeed = 0, tools = { "RodAndLine" } }, -- Fischer
    Hunter         = { baseSpeed = 0, tools = { "Bow" } },        -- Jäger
    Farm           = { baseSpeed = 25, tools = { "Scythe" } },    -- Bauernhof
    Bakery         = { baseSpeed = 0, tools = { "Rollingpin" } }, -- Bäcker
    Slaughterhouse = { baseSpeed = 0, tools = { "Cleaver" } },    -- Fleischerei
    Vineyard       = { baseSpeed = 0, tools = { "Shovel" } },     -- Weinberg

    -- Bergbau und Metall
    GraniteMine    = { baseSpeed = 25, tools = { "PickAxe" } },    -- Granitmine
    CoalMine       = { baseSpeed = 25, tools = { "PickAxe" } },    -- Kohlemine
    IronMine       = { baseSpeed = 25, tools = { "PickAxe" } },    -- Eisenmine
    GoldMine       = { baseSpeed = 0, tools = { "PickAxe" } },    -- Goldmine
    Charburner     = { baseSpeed = 25, tools = { "Shovel" } },     -- Köhler
    Ironsmelter    = { baseSpeed = 25, tools = { "Crucible" } },   -- Eisenschmelze
    Mint           = { baseSpeed = 0, tools = { "Crucible" } },   -- Münzprägerei
    Armory         = { baseSpeed = 0, tools = { "Hammer" } },     -- Waffenschmiede
    Metalworks     = { baseSpeed = 25, tools = { "Tongs" } },     -- Schlosserei
    Shipyard       = { baseSpeed = 0, tools = { "Hammer" } },     -- Werft

    -- Leder und Tempel
    Skinner        = { baseSpeed = 0, tools = { "Cleaver" } },    -- Abdecker
    Tannery        = { baseSpeed = 0, tools = { "Saw" } },        -- Gerberei
    LeatherWorks   = { baseSpeed = 0, tools = { "Tongs" } },      -- Lederwerkstatt
    Temple         = { baseSpeed = 0, tools = { "Crucible" } },   -- Tempel
}
