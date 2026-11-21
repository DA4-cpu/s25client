-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Regler im Fenster "Verteilung" im Zivilisationsmodus: Startwerte der eingebauten Regler "Wissen" und "Werkzeuge".
Jeder Spieler kann die Werte im Spiel ändern (je 0 bis 10), der Knopf "Standard" stellt die Werte aus dieser Datei
wieder her.

rttr:SetKnowledgeDistribution{...}: Startwerte des Reglers "Wissen": Welches Wissensmedium Werkstätten mit
  outputChoice = "knowledge" herstellen (z.B. der Knochenschnitzer).
  Beispiel: LWP = 10, IWP = 5, MWP = 0 -> doppelt so viele LWP- wie IWP-Medien, keine MWP-Medien.
  Stehen alle möglichen Medien einer Werkstatt auf 0, wird gleich verteilt.

rttr:SetToolMaterialPriorities{...}: Startwerte des Reglers "Werkzeuge" (nur mit der Werkzeugreform, tools.lua):
  Vorrang der Materialien aus rttr:AddToolMaterial. Ein Gebäude bestellt das vorrätige Werkzeug mit dem höchsten
  Wert, bei Gleichstand das schnellere. 0 = dieses Material nie bestellen. Nicht genannte Materialien: 5.

Alle anderen Waren steuert die Gesamtverteilung (Fenster "Gesamtverteilung", doc/humankind/DESIGN.md): Ihre Werte
stellen sich selbst ein und steuern jedes Gebäude, das die Waren herstellt. Gebäude mit mehreren Produkten wählen
danach, Gebäude halten an, wenn alle ihre Produkte und Nebenprodukte Überschuss sind. Hier gibt es dafür nichts
einzustellen.
]]

rttr:SetKnowledgeDistribution{ LWP = 5, IWP = 5, MWP = 5 }

rttr:SetToolMaterialPriorities{ Iron = 4, Obsidian = 3.5, Bronze = 3, Copper = 2, Wood = 1 }
