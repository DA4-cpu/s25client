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
  priority  Startwert 0..10 im Regler "Werkzeuge" (Fenster "Verteilung"). Ein Gebäude bestellt das vorrätige Werkzeug
            mit dem höchsten Wert, bei Gleichstand das schnellere. 0 = dieses Material nie bestellen.
            Höchstens 8 Materialien. Neue Materialien immer hinten anfügen (Spielstände).

rttr:AddTool{...}: macht eine Ware (eingebaut oder aus wares.lua) zum Werkzeug
  ware        Bezeichner der Ware
  material    Bezeichner des Materials
  durability  Haltbarkeit in Arbeitsschritten
  speed       Bonus auf das Arbeitstempo in Prozent
  Der Werkzeugtyp ergibt sich aus den Tags der Ware: Eine neue Ware "CopperAxe" mit kind = "Axe" zählt überall als
  Axt, wo unten "Axe" steht.

rttr:SetBuildingTools{...}: Werkzeuge der eingebauten Gebäude
  baseSpeed   Arbeitstempo ohne Werkzeug in Prozent (Standard 0)
  tools       Liste der Werkzeugtypen (Tag-Abfragen wie bei den Eingängen), höchstens 4. Von jedem Typ liegt höchstens
              ein Werkzeug in Gebrauch im Gebäude, dazu höchstens ein Ersatz.
  Nicht genannte Gebäude nutzen das bisherige Werkzeug ihres Arbeiters mit baseSpeed = 0; Gebäude, deren Arbeiter
  kein Werkzeug hatte, arbeiten wie bisher (Tempo 100).
  Werkstätten aus workshops.lua bekommen ihre Werkzeuge dort: tools = { baseSpeed = 0, tools = { "Saw" } }.
  Ohne diese Angabe gilt worker.tool mit baseSpeed = 0.

rttr:SetToolReorder(n): Ein Ersatzwerkzeug wird bestellt, sobald das Werkzeug nur noch n Prozent seiner Haltbarkeit
  hat. Es wartet im Gebäude, bis das alte kaputt ist.
]]

rttr:AddToolMaterial{ name = "Iron", title = "Eisen", priority = 5 }

rttr:AddTool{ ware = "Tongs",      material = "Iron", durability = 150, speed = 100 } -- Zange
rttr:AddTool{ ware = "Hammer",     material = "Iron", durability = 150, speed = 100 } -- Hammer
rttr:AddTool{ ware = "Axe",        material = "Iron", durability = 150, speed = 100 } -- Axt
rttr:AddTool{ ware = "Saw",        material = "Iron", durability = 150, speed = 100 } -- Säge
rttr:AddTool{ ware = "PickAxe",    material = "Iron", durability = 150, speed = 100 } -- Spitzhacke
rttr:AddTool{ ware = "Shovel",     material = "Iron", durability = 150, speed = 100 } -- Schaufel
rttr:AddTool{ ware = "Crucible",   material = "Iron", durability = 150, speed = 100 } -- Schmelztiegel
rttr:AddTool{ ware = "RodAndLine", material = "Iron", durability = 150, speed = 100 } -- Angel
rttr:AddTool{ ware = "Scythe",     material = "Iron", durability = 150, speed = 100 } -- Sense
rttr:AddTool{ ware = "Cleaver",    material = "Iron", durability = 150, speed = 100 } -- Beil
rttr:AddTool{ ware = "Rollingpin", material = "Iron", durability = 150, speed = 100 } -- Nudelholz
rttr:AddTool{ ware = "Bow",        material = "Iron", durability = 150, speed = 100 } -- Bogen

rttr:SetToolReorder(10)

-- Alles, was man braucht, um neue Werkzeuge herzustellen (Holz, Bretter, Stein, Bergbau, Eisenschmelze, Schlosserei),
-- arbeitet auch ohne Werkzeug langsam weiter. Sonst gäbe es nach dem Bruch des letzten Werkzeugs nie wieder welche.
rttr:SetBuildingTools{
    -- Holz und Stein
    Woodcutter     = { baseSpeed = 25, tools = { "Axe" } },       -- Holzfäller
    Forester       = { baseSpeed = 0, tools = { "Shovel" } },     -- Förster
    Sawmill        = { baseSpeed = 25, tools = { "Saw" } },        -- Sägewerk
    Quarry         = { baseSpeed = 25, tools = { "PickAxe" } },    -- Steinbruch

    -- Nahrung und Landwirtschaft
    Fishery        = { baseSpeed = 0, tools = { "RodAndLine" } }, -- Fischer
    Hunter         = { baseSpeed = 0, tools = { "Bow" } },        -- Jäger
    Farm           = { baseSpeed = 0, tools = { "Scythe" } },     -- Bauernhof
    Bakery         = { baseSpeed = 0, tools = { "Rollingpin" } }, -- Bäcker
    Slaughterhouse = { baseSpeed = 0, tools = { "Cleaver" } },    -- Fleischerei
    Vineyard       = { baseSpeed = 0, tools = { "Shovel" } },     -- Weinberg

    -- Bergbau und Metall
    GraniteMine    = { baseSpeed = 25, tools = { "PickAxe" } },    -- Granitmine
    CoalMine       = { baseSpeed = 25, tools = { "PickAxe" } },    -- Kohlemine
    IronMine       = { baseSpeed = 25, tools = { "PickAxe" } },    -- Eisenmine
    GoldMine       = { baseSpeed = 25, tools = { "PickAxe" } },    -- Goldmine
    Charburner     = { baseSpeed = 0, tools = { "Shovel" } },     -- Köhler
    Ironsmelter    = { baseSpeed = 25, tools = { "Crucible" } },   -- Eisenschmelze
    Mint           = { baseSpeed = 0, tools = { "Crucible" } },   -- Münzprägerei
    Armory         = { baseSpeed = 0, tools = { "Hammer" } },     -- Waffenschmiede
    Metalworks     = { baseSpeed = 50, tools = { "Tongs" } },     -- Schlosserei
    Shipyard       = { baseSpeed = 0, tools = { "Hammer" } },     -- Werft

    -- Leder und Tempel
    Skinner        = { baseSpeed = 0, tools = { "Cleaver" } },    -- Abdecker
    Tannery        = { baseSpeed = 0, tools = { "Saw" } },        -- Gerberei
    LeatherWorks   = { baseSpeed = 0, tools = { "Tongs" } },      -- Lederwerkstatt
    Temple         = { baseSpeed = 0, tools = { "Crucible" } },   -- Tempel
}
