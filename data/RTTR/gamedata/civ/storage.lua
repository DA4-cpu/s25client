-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- Lager im Zivilisationsmodus.
--
-- Die Kapazität der Lager steht in workshops.lua: warehouse = { capacity = N } in rttr:SetBuilding (Hauptquartier,
-- Lagerhaus, Hafen) bzw. in rttr:AddWorkshop (eigene Lager), dort auch, was "voll" bedeutet.

-- rttr:SetFlagWorkLimit(Anzahl)
--   Ein Arbeiter legt sein Produkt nur an die Flagge seines Gebäudes, wenn dort danach höchstens so viele Waren liegen
--   (eine Flagge fasst 8). Mit 7 bleibt ein Platz frei, damit Baumaterial noch durchkommt, wenn die Lager voll sind.
--   Liegen mehr Waren dort, wartet das Gebäude mit der Arbeit.
rttr:SetFlagWorkLimit(7)
