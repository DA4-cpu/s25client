-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Objekte auf der Karte, die eine Werkstatt einsammeln kann (wie der Steinmetz die Steinhaufen), z.B. Lehmvorkommen.
rttr:AddMapObject{...}:
  name   Bezeichner. Nur A-Z, a-z, 0-9 und _. Wird in workshops.lua (collect), in mapgen.lua (object) und in Karten und
         Spielständen verwendet.
  title  Anzeigename
  sizes  optional (Standard 6): Anzahl der Größen = Waren in einem vollen Objekt (1 bis 16). Jedes Einsammeln macht es
         eine Größe kleiner, nach der kleinsten verschwindet es.
  look   Platzhaltergrafik, bis es eigene Bilder gibt: "Granite1" oder "Granite2" (die beiden Steinhaufen)
  tint   optional: Die Platzhaltergrafik wird mit dieser Farbe eingefärbt, z.B. 0xC08050 (braun). Standard: unverändert

Eigene Grafiken: siehe data/RTTR/assets/civ/LIESMICH.txt (Ordner mit dem Bezeichner als Namen).

WICHTIG für Karten und Spielstände: Neue Objekte immer hinten anfügen, bestehende nicht löschen, umbenennen oder
umsortieren. Es sind höchstens 16 Objekte möglich (muss zwingend erhöht werden, da ich viel mehr Ressourcen, Waren, etc. plane).
Ohne Zivilisationsmodus gibt es diese Objekte im Spiel nicht (sie werden beim Laden der Karte weggelassen).
]]

-------------------------------------------------------------------------------
-- Lehm (Konzept: Rohstoff auf der Karte wie Bäume und Steinhaufen, Abbau wie beim Steinbruch)
-------------------------------------------------------------------------------

rttr:AddMapObject{
    name = "LoamDeposit",
    title = "Lehmvorkommen",
    sizes = 16,         
    look = "Granite1",
    tint = 0xC08050,    
}

rttr:AddMapObject{
    name = "PapyrusDeposit",
    title = "Papyrusvorkommen",
    sizes = 16,          
    look = "Granite1",
    tint = 0xC01010,    
}

rttr:AddMapObject{
    name = "CopperDeposit", -- Soll wie die anderen Erze auch als Bodenressource vorkommen. 
    title = "Kupfervorkommen",
    sizes = 8,          
    look = "Granite2",
    tint = 0xC01080,    
}
