--[[
Objekte auf der Karte, die eine Werkstatt einsammeln oder anbauen kann (wie der Steinmetz die Steinhaufen oder der
Bauer das Getreide), z.B. Lehmvorkommen.
rttr:AddMapObject{...}:
  name   Bezeichner. Nur A-Z, a-z, 0-9 und _. Wird in workshops.lua (collect, plant), in mapgen.lua (object) und in
         Karten und Spielständen verwendet.
  title  Anzeigename
  sizes  optional (Standard 6): Anzahl der Größen = Waren in einem vollen Objekt (1 bis 16). Jedes Einsammeln macht es
         eine Größe kleiner, nach der kleinsten verschwindet es.
  look   Platzhaltergrafik, bis es eigene Bilder gibt: "Granite1" oder "Granite2" (die beiden Steinhaufen)
  tint   optional: Die Platzhaltergrafik wird mit dieser Farbe eingefärbt, z.B. 0xC08050 (braun). Standard: unverändert
  growTime  optional: Sekunden, bis ein gepflanztes Objekt ausgewachsen ist. Nötig, damit eine Plantage
         (workshops.lua, plant) das Objekt anbauen kann. Ein gepflanztes Objekt beginnt mit der kleinsten Größe und
         wächst gleichmäßig bis plantSize. Erst dann kann es geerntet werden (auch von Sammlern).
  plantSize optional (Standard: sizes): Größe eines ausgewachsenen gepflanzten Objekts = Waren, die es bringt.

Wo eine Plantage ein Objekt pflanzen kann, legen die Regeln in world/mapgen.lua fest (rttr:AddMapResource mit diesem
Objekt): dort, wo es auch natürlich wächst (terrain, temperature, humidity, height und weights mit Faktor 0), aber
ohne die Zonen. Gibt es keine Regel, wächst es auf jedem Land. Temperatur, Feuchtigkeit und Höhe kennt das Spiel nur
auf erzeugten Karten (Zufallskarte); auf anderen Karten zählt nur das Terrain (kleine Gewässer sind dort Seen, Flüsse
gibt es dort nicht).

Eigene Grafiken: siehe data/RTTR/assets/civ/LIESMICH.txt (Ordner mit dem Bezeichner als Namen).

WICHTIG für Karten und Spielstände: Neue Objekte immer hinten anfügen, bestehende nicht löschen, umbenennen oder
umsortieren. Es sind höchstens 256 Objekte möglich.
Ohne Zivilisationsmodus gibt es diese Objekte im Spiel nicht (sie werden beim Laden der Karte weggelassen).
]]

-------------------------------------------------------------------------------
-- Lehm (Konzept: Rohstoff auf der Karte wie Bäume und Steinhaufen, Abbau wie beim Steinbruch)
-------------------------------------------------------------------------------

rttr:AddMapObject{
    name = "LoamDeposit",
    title = "Lehmvorkommen",
    sizes = 8,         
    look = "Granite1",
    tint = 0xC08050,    
}

rttr:AddMapObject{
    name = "PapyrusDeposit",
    title = "Papyrusvorkommen",
    sizes = 2,          
    look = "Granite1",
    tint = 0xc0ba10,    
    growTime = 300,
    plantSize = 2,
}

--[[
Bodenschätze (Erze) im Gebirge wie Kohle, Eisen, Gold und Granit, z.B. Kupfer.
rttr:AddOre{...}:
  name   Bezeichner. Nur A-Z, a-z, 0-9 und _. Wird in workshops.lua (mine), in mapgen.lua (object) und in Karten und
         Spielständen verwendet.
  title  Anzeigename (z.B. in der Meldung des Geologen "Kupfer gefunden")
  look   Platzhalter für das Schild des Geologen und die Anzeige der Bodenschätze: "Iron", "Gold", "Coal" oder "Granite"
  tint   optional: Das Schild wird mit dieser Farbe eingefärbt, z.B. 0xC07040. Standard: unverändert

Der Geologe findet die Erze und stellt Schilder auf. Abgebaut werden sie von einer Mine aus workshops.lua
(size = "mine", mine = { resource = "Copper" }). Auf erzeugten Karten verteilt sie der Kartengenerator über eine Regel
in world/mapgen.lua (object = "Copper"), zusammen mit Kohle, Eisen, Gold und Granit.
Eigene Grafiken: Ordner data/RTTR/assets/civ/<name>/ mit Schild0 (wenig), Schild1 (mittel) und Schild2 (viel).

WICHTIG für Karten und Spielstände: Neue Erze immer hinten anfügen, bestehende nicht löschen, umbenennen oder
umsortieren. Es sind höchstens 24 Erze möglich.
Ohne Zivilisationsmodus gibt es diese Erze im Spiel nicht (sie werden beim Laden der Karte weggelassen).
]]

rttr:AddOre{
    name = "Copper",
    title = "Kupfer",
    look = "Iron",
    tint = 0xC01080,
}
