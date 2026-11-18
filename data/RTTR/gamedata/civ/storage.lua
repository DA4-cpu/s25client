-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- Lagerkapazität im Zivilisationsmodus.
--
-- rttr:SetWarehouseCapacity{ Gebäude = Anzahl, ... }
--   So viele Waren fasst ein Lager dieses Typs höchstens (nur Waren, keine Personen). Waren auf dem Weg zum Lager
--   zählen schon mit. Ist ein Lager voll, nimmt es keine Waren mehr an; sind alle voll, bleiben die Waren an der Flagge
--   ihres Gebäudes liegen, bis ein Abnehmer sie braucht oder wieder Platz ist.
--   Was im Lager selbst entsteht (Startvorrat, Werkzeuge zurückkehrender Arbeiter), wird immer eingelagert, auch über
--   die Grenze hinaus. Lager ohne Eintrag fassen beliebig viel.
--   Eigene Lager aus workshops.lua (storage = { ... }) haben ihre Kapazität dort.
rttr:SetWarehouseCapacity{
    Headquarters   = 700, -- Hauptquartier
    HarborBuilding = 500, -- Hafen
    Storehouse     = 400, -- Lagerhaus
}

-- rttr:SetFlagWorkLimit(Anzahl)
--   Ein Arbeiter legt sein Produkt nur an die Flagge seines Gebäudes, wenn dort danach höchstens so viele Waren liegen
--   (eine Flagge fasst 8). Mit 7 bleibt ein Platz frei, damit Baumaterial noch durchkommt, wenn die Lager voll sind.
--   Liegen mehr Waren dort, wartet das Gebäude mit der Arbeit.
rttr:SetFlagWorkLimit(7)
