-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Nebenprodukte im Zivilisationsmodus.

rttr:AddByproduct{...} gibt einem Produktionsgebäude ein Nebenprodukt: Jedes Mal, wenn der Arbeiter ein Produkt an die
Flagge bringt, legt er das Nebenprodukt mit dazu (z.B. Jäger: Fleisch + Knochen).
  building    Gebäude: eingebautes Produktionsgebäude (Bezeichner wie in techtree.lua) oder Werkstatt aus workshops.lua
  ware        Tag-Abfrage des Nebenprodukts, muss genau eine Ware treffen (z.B. "Bones")
  amount      optional (Standard 1): Anzahl Nebenprodukte pro Produkt, 1 bis 7
  per         optional: nur für diese Produkte (Tag-Abfrage, z.B. "Meat"). Ohne: für jedes Produkt des Gebäudes
  technology  optional: erst nachdem der Spieler diese Technologie erforscht hat (Bezeichner aus techtree.lua)

Der Arbeiter legt Produkt und Nebenprodukte gemeinsam ab. Ist an der Flagge nicht genug Platz für alle, wartet er
wie sonst auch, bis genug Platz frei ist. Pro Gebäude sind höchstens 7 Nebenprodukte möglich (eine Flagge fasst 8 Waren).
Mehrere rttr:AddByproduct für dasselbe Gebäude sind erlaubt.
]]

-- Knochen-Kette (Schritt 4a), Entscheidung des Nutzers: Jäger 1 Knochen pro Tier, Fleischerei 1 Knochen pro Schinken
-- (also pro Fleisch), beides erst nach der Knochen-Technologie.
rttr:AddByproduct{ building = "Hunter", ware = "Bones", amount = 1, technology = "bone_carving" }
rttr:AddByproduct{ building = "Slaughterhouse", ware = "Bones", amount = 1, technology = "bone_carving" }
