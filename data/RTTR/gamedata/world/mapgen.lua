-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- =====================================================================================================================
-- Kartengenerator: Klima, Zonen und Ressourcenverteilung ("Welt")
-- =====================================================================================================================
--
-- Diese Datei steuert, was der Zufallskarten-Generator wo auf die Karte setzt. Sie gilt nur für erzeugte Karten.
--
-- ACHTUNG: Alle Inhalte und Zahlenwerte in dieser Datei sind PLATZHALTER von Claude (markiert mit "PLATZHALTER").
-- Gamedesign und Balancing legt der Nutzer fest. Die Platzhalter sind so gewählt, dass die Karten ungefähr wie bisher
-- aussehen.
--
-- Begriffe
-- --------
-- Jeder Knoten der Karte hat drei Klimawerte, jeweils 0 bis 100:
--   temperature  0 = am kältesten, 100 = am heißesten
--   humidity     0 = am trockensten, 100 = am feuchtesten
--   height       0 = Meereshöhe, 100 = höchster Punkt der Karte
-- Bereiche schreibt man als {von, bis}, z. B. temperature = {60, 100}. Fehlt ein Bereich, gilt er ganz (0 bis 100).
--
-- Außerdem gehört jeder Knoten zu genau einer Zone (rttr:AddMapZone). Jede Zone kommt auf jeder Karte genau einmal als
-- großes zusammenhängendes Gebiet vor. Die Zonen bestimmen nur, welche Pflanzen und Tiere bei der Erzeugung der Karte
-- wo wachsen und leben. Reale Geographie spielt keine Rolle.
--
-- Mögliche Terrain-Angaben (terrain = { ... }, mehrere möglich, es reicht, wenn eine passt):
--   "land"         Land (kein Wasser, kein Gebirge)
--   "mountain"     Gebirge
--   "freshwater"   im Süßwasser (Seen und Flüsse)
--   "saltwater"    im Salzwasser (Meer)
--   "river"        in einem Fluss
--   "shore_fresh"  am Ufer von Süßwasser (Land- oder Gebirgsknoten, der das Wasser berührt)
--   "shore_salt"   am Ufer von Salzwasser (Küste)
--   "shore_river"  am Flussufer
--   "near_fresh"   in der Nähe von Süßwasser (Abstand siehe nearWaterDistance, schließt das Ufer ein)
--   "near_salt"    in der Nähe von Salzwasser
--   "near_river"   in der Nähe eines Flusses
-- Ob ein Gewässer Süß- oder Salzwasser ist, hängt von seiner Größe ab (freshwaterMaxSize). Flüsse sind immer Süßwasser.
--
-- Vorhandene Baumarten (für trees): Pine (Kiefer), Birch (Birke), Oak (Eiche), Palm1 (Palme 1), Palm2 (Palme 2),
--   Pineapple (Ananas), Cypress (Zypresse), Cherry (Kirschbaum), Fir (Tanne)
-- Vorhandene Tierarten (für animals): Rabbit (Hase), Fox (Fuchs), Stag (Hirsch), Deer (Reh), Duck (Ente, nur auf dem
--   Wasser), Sheep (Schaf)
-- Neue Pflanzen und Tiere (Mahagoni, Schimpanse ...) brauchen erst Grafiken und Code. Bis dahin stehen die vorhandenen
-- Arten als Platzhalter.

-- ---------------------------------------------------------------------------------------------------------------------
-- Allgemeine Klima-Einstellungen
-- ---------------------------------------------------------------------------------------------------------------------
rttr:SetMapClimate{
	-- Um so viele Punkte sinkt die Temperatur am höchsten Punkt der Karte (gleichmäßig mit der Höhe). 0 = gar nicht
	heightCooling = 30, -- PLATZHALTER
	-- Abstand in Knoten für "near_fresh", "near_salt" und "near_river"
	nearWaterDistance = 4, -- PLATZHALTER
	-- Gewässer bis zu dieser Größe (Prozent der Kartenfläche) sind Süßwasser (Seen), größere sind Salzwasser (Meer)
	freshwaterMaxSize = 3, -- PLATZHALTER
	-- Vorschaubilder (Zonen, Temperatur, Feuchtigkeit, Höhe) neben der erzeugten Karte speichern
	-- (Ordner der gespielten Karten, Datei "Random_Vorschau.bmp")
	previewImages = true
}

-- ---------------------------------------------------------------------------------------------------------------------
-- Zonen
-- ---------------------------------------------------------------------------------------------------------------------
-- rttr:AddMapZone{
--   name = "Afrika",          -- Bezeichner (nur A-Z, 0-9 und _)
--   title = "Afrika",         -- Anzeigename
--   color = 0xC08040,         -- Farbe im Vorschaubild (0xRRGGBB)
--   packages = {              -- Pflanzen und Tiere je Klima. Das erste Paket, dessen Bereiche passen, gilt.
--     { name = "Regenwald", temperature = {65, 100}, humidity = {60, 100}, height = {0, 40},
--       trees = { Palm1 = 2, Palm2 = 1 },      -- Baumarten mit Gewicht (2 = doppelt so häufig wie 1)
--       animals = { Deer = 1, Fox = 1 } },     -- Tierarten mit Gewicht
--     ...
--   }
-- }
-- Passt an einem Ort kein Paket, wachsen dort keine Bäume und es leben dort keine Tiere. Deshalb sollte das letzte Paket
-- jeder Zone ohne Bereiche auskommen (gilt dann überall sonst).

-- Platzhalter-Pakete nach Klima, solange es für eine Zone noch keine eigenen gibt. PLATZHALTER
local function platzhalterPakete()
	return {
		{ name = "heiss_feucht", temperature = {65, 100}, humidity = {55, 100},
		  trees = { Palm1 = 2, Palm2 = 2, Pineapple = 1 }, animals = { Deer = 1, Fox = 1, Duck = 1 } },
		{ name = "heiss_trocken", temperature = {65, 100}, humidity = {0, 54},
		  trees = { Palm2 = 1, Cypress = 2 }, animals = { Rabbit = 1, Fox = 1 } },
		{ name = "kalt", temperature = {0, 34},
		  trees = { Fir = 3, Pine = 2, Birch = 1 }, animals = { Stag = 2, Rabbit = 1, Fox = 1, Duck = 1 } },
		{ name = "gemaessigt",
		  trees = { Oak = 3, Birch = 2, Cherry = 1, Pine = 1 },
		  animals = { Deer = 2, Stag = 1, Rabbit = 2, Fox = 1, Sheep = 1, Duck = 1 } }
	}
end

rttr:AddMapZone{
	name = "Nordamerika",
	title = "Nordamerika",
	color = 0x4060E0,
	packages = platzhalterPakete() -- PLATZHALTER
}

rttr:AddMapZone{
	name = "Suedamerika",
	title = "Südamerika",
	color = 0x40C060,
	packages = platzhalterPakete() -- PLATZHALTER
}

rttr:AddMapZone{
	name = "Europa",
	title = "Europa",
	color = 0xE0E040,
	-- Beispiel aus dem Konzept (gemäßigt): Schwein, Rind, Wolf; Schilfrohr, Haselnuss, Apfelbaum.
	-- Bis es diese Arten gibt, stehen vorhandene Arten da. PLATZHALTER
	packages = {
		{ name = "gemaessigt", temperature = {35, 64},
		  trees = { Oak = 3, Birch = 2, Cherry = 2 }, animals = { Sheep = 2, Stag = 1, Fox = 1, Duck = 1 } },
		{ name = "kalt", temperature = {0, 34},
		  trees = { Fir = 3, Pine = 2, Birch = 1 }, animals = { Stag = 2, Rabbit = 1, Fox = 1 } },
		{ name = "warm",
		  trees = { Cypress = 2, Cherry = 1, Oak = 1 }, animals = { Rabbit = 2, Fox = 1, Sheep = 1 } }
	}
}

rttr:AddMapZone{
	name = "Afrika",
	title = "Afrika",
	color = 0xE08030,
	-- Beispiel aus dem Konzept (heiß, feucht, nicht sehr hoch): Schimpanse, Leopard, Waldelefant; Mahagoni, Kola-Baum,
	-- Gummibaum. Bis es diese Arten gibt, stehen vorhandene Arten da. PLATZHALTER
	packages = {
		{ name = "regenwald", temperature = {65, 100}, humidity = {55, 100}, height = {0, 40},
		  trees = { Palm1 = 2, Palm2 = 2, Pineapple = 1 }, animals = { Deer = 2, Fox = 1, Duck = 1 } },
		{ name = "savanne", temperature = {50, 100}, humidity = {0, 54},
		  trees = { Palm2 = 1, Cypress = 1 }, animals = { Stag = 1, Deer = 1, Rabbit = 1 } },
		{ name = "rest",
		  trees = { Palm1 = 1, Cypress = 1, Oak = 1 }, animals = { Deer = 1, Rabbit = 1 } }
	}
}

rttr:AddMapZone{
	name = "Asien",
	title = "Asien",
	color = 0xC04040,
	packages = platzhalterPakete() -- PLATZHALTER
}

rttr:AddMapZone{
	name = "Ozeanien",
	title = "Ozeanien",
	color = 0x40C0C0,
	packages = platzhalterPakete() -- PLATZHALTER
}

rttr:AddMapZone{
	name = "Australien",
	title = "Australien",
	color = 0xA040C0,
	packages = platzhalterPakete() -- PLATZHALTER
}

-- ---------------------------------------------------------------------------------------------------------------------
-- Ressourcen
-- ---------------------------------------------------------------------------------------------------------------------
-- rttr:AddMapResource{
--   name = "Steinhaufen",        -- Bezeichner (nur A-Z, 0-9 und _)
--   object = "stones",           -- Was gesetzt wird, siehe unten
--   amount = 5,                  -- Prozent der passenden Knoten, die das Objekt bekommen
--                                -- oder ein Regler des Generatorfensters: "forest" (Wald %), "stonePiles"
--                                -- (Steinhaufen %), "coal", "iron", "gold", "granite" (Bodenschätze)
--   terrain = { "land" },        -- Wo (siehe oben), Pflicht
--   temperature = {0, 100},      -- Klima-Grenzen, optional
--   humidity = {0, 100},
--   height = {0, 100},
--   zones = { "Afrika" },        -- nur in diesen Zonen, optional (sonst überall)
--   cluster = 5,                 -- Klumpengröße in Knoten, 0 = verstreut
--   density = 80,                -- Prozent der Knoten innerhalb eines Klumpens, die belegt werden
--   weights = {                  -- Häufigkeit je Klima, optional. Der erste passende Eintrag gilt, sonst Faktor 1
--     { temperature = {60, 100}, humidity = {60, 100}, factor = 0.2 }
--   },
--   hqDistance = {0, 20}         -- nur in diesem Abstand (Knoten) zum nächsten Hauptquartier, optional
-- }
--
-- Mögliche Objekte:
--   "trees"        Bäume. Die Baumart kommt aus dem Paket der Zone (siehe oben).
--   "stones"       Steinhaufen (Granit an der Oberfläche, für den Steinbruch)
--   "animals"      Tiere. Die Art kommt aus dem Paket der Zone; Enten nur auf Wasser, die anderen nur an Land.
--   "decoration"   Büsche, Blumen, Pilze und kleine Steine ohne Funktion
--   "fish"         Fische (Wasser)
--   "groundwater"  Grundwasser für Brunnen
--   "coal", "iron", "gold", "granite"   Bodenschätze im Gebirge
-- Später kommen neue Objekte dazu (z. B. Lehm), sobald es sie im Spiel gibt.
-- Die Regeln werden in dieser Reihenfolge abgearbeitet; ein Knoten bekommt höchstens ein Objekt (Bäume, Steine, Deko)
-- und höchstens einen Bodenschatz (Kohle, Eisen, Gold, Granit, Fische, Grundwasser).
-- Der Regler "Ressourcenmenge" im Generatorfenster vervielfacht alle Mengen (100 % = Werte aus dieser Datei).

rttr:AddMapResource{
	name = "Wald",
	object = "trees",
	amount = "forest",
	terrain = { "land", "mountain" },
	cluster = 12, -- PLATZHALTER
	density = 85, -- PLATZHALTER
	-- In heißen, trockenen Gebieten weniger Wald. PLATZHALTER
	weights = {
		{ temperature = {70, 100}, humidity = {0, 30}, factor = 0.3 }
	}
}

rttr:AddMapResource{
	name = "Steinhaufen",
	object = "stones",
	amount = "stonePiles",
	terrain = { "land", "mountain" },
	cluster = 8, -- PLATZHALTER
	density = 80, -- PLATZHALTER
	-- Im feuchten, warmen Gebiet sind Steine selten, weil sie von der Vegetation verdeckt werden (Konzept). PLATZHALTER
	weights = {
		{ temperature = {60, 100}, humidity = {60, 100}, factor = 0.3 }
	}
}

rttr:AddMapResource{
	name = "Deko",
	object = "decoration",
	amount = 2, -- PLATZHALTER
	terrain = { "land" }
}

rttr:AddMapResource{
	name = "Landtiere",
	object = "animals",
	amount = 2, -- PLATZHALTER
	terrain = { "land" }
}

rttr:AddMapResource{
	name = "Wasservoegel",
	object = "animals",
	amount = 2, -- PLATZHALTER
	terrain = { "freshwater", "saltwater" }
}

rttr:AddMapResource{
	name = "Fische",
	object = "fish",
	amount = 100, -- PLATZHALTER
	terrain = { "freshwater", "saltwater", "river" }
}

rttr:AddMapResource{
	name = "Grundwasser",
	object = "groundwater",
	amount = 100, -- PLATZHALTER
	terrain = { "land", "shore_fresh", "shore_salt", "shore_river" }
}

-- Bodenschätze: Die Menge kommt aus den Reglern Kohle/Eisen/Gold/Granit im Generatorfenster.
-- Im feuchten, warmen Gebiet sind Minenplätze selten (Konzept). PLATZHALTER
local wenigerImRegenwald = { { temperature = {60, 100}, humidity = {60, 100}, factor = 0.5 } }

rttr:AddMapResource{
	name = "Kohle",
	object = "coal",
	amount = "coal",
	terrain = { "mountain" },
	cluster = 9,
	weights = wenigerImRegenwald
}

rttr:AddMapResource{
	name = "Eisen",
	object = "iron",
	amount = "iron",
	terrain = { "mountain" },
	cluster = 9,
	weights = wenigerImRegenwald
}

rttr:AddMapResource{
	name = "Gold",
	object = "gold",
	amount = "gold",
	terrain = { "mountain" },
	cluster = 9,
	weights = wenigerImRegenwald
}

rttr:AddMapResource{
	name = "Granit",
	object = "granite",
	amount = "granite",
	terrain = { "mountain" },
	cluster = 9,
	weights = wenigerImRegenwald
}
