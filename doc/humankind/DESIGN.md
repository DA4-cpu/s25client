<!--
Copyright (C) 2026 DA4-cpu

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Zivilisationsmodus („Humankind-Schicht“) – Designdokument

Lebendes Dokument. Jede Designentscheidung wird hier mit Datum festgehalten.
Offene Fragen stehen am Ende; beantwortete Fragen wandern nach oben in die Entscheidungen.

## Vision

Siedler-2-Wirtschaft (Echtzeit, Träger, Straßen, Warenketten, indirekte Steuerung) bleibt der Kern.
Darüber liegt eine Zivilisationsschicht nach dem Vorbild von *Humankind*:
Zeitalter, Kulturen, Technologien, später Gesellschaft, Religion, Diplomatie.
Alles ist ein **optionaler Spielmodus** (Addon „Zivilisationsmodus“); normales Siedler bleibt unverändert spielbar.

## Grundsatzentscheidungen

| Datum      | Entscheidung |
|------------|--------------|
| 2026-10-18 | Humankind-Schicht auf Siedler, kein 1:1-Nachbau, keine Rundenstrategie. |
| 2026-10-18 | Optionaler Spielmodus über ein Addon, kein harter Fork der Spiellogik. |
| 2026-10-18 | **Kein Ruhm, keine Ära-Sterne.** Der **Technologiebaum ist das Rückgrat**: Kulturen, Gebäude, Waren, Traits, Einheiten usw. hängen alle am Baum. |
| 2026-10-18 | Forschung kostet **Wissen als Ware** (Schriftrollen aus einem Gelehrtenhaus) **plus Warenkosten** pro Technologie. |
| 2026-10-18 | Kulturen werden **exklusiv** erforscht: Wer eine Kultur-Technologie zuerst abschließt, bekommt sie; für alle anderen ist sie gesperrt. |
| 2026-10-18 | Erforschte Kulturen **stapeln vollständig** (alle Traits und emblematischen Inhalte bleiben aktiv). |
| 2026-10-18 | Spielstart mit **Grundwirtschaft**; alle weiteren S2-Gebäude werden erforscht. |
| 2026-10-18 | Grafik: vorhandene S2/RttR-Grafiken wiederverwenden; wo das nicht passt, Platzhalter, die später (u. a. per KI, siehe [GRAFIK.md](GRAFIK.md)) ersetzt werden. |
| 2026-10-18 | Inhalte (Technologien, Kulturen …) sind **datengetrieben in Lua** (`data/RTTR/gamedata/civ/`), damit sie ohne Neukompilieren geändert werden können. Sie werden nur geladen, wenn der Modus aktiv ist. |
| 2026-10-18 | Texte in eigenen Worten. Mechaniken dürfen übernommen werden, Texte/Bilder/Icons von Amplitude nicht. |

## Zuordnung Humankind → Siedler

| Humankind | Umsetzung im Zivilisationsmodus | Status |
|-----------|----------------------------------|--------|
| Technologiebaum | Lua-definierter Baum, Forschung kostet Waren, schaltet Gebäude frei | **Fundament fertig** |
| Wissenschaft | Ware „Schriftrolle“ aus Gelehrtenhaus, wird von Technologien verbraucht | geplant (Phase 2) |
| Kulturen (exklusiv) | Exklusive Technologien (`exclusive = true`); wer zuerst fertig ist, bekommt sie | Exklusivität fertig, Kultur-Inhalte geplant |
| Legacy-Traits | Modifikatoren (Produktionstempo, Kampfkraft, Baukosten …), stapeln | geplant (Phase 3) |
| Emblematische Quartiere | Besondere Gebäude pro Kultur (Grafik der RttR-Völker wiederverwenden) | geplant |
| Emblematische Einheiten | Soldaten mit Kultur-Boni (Stärke, Tempo); echte Einheitentypen später | geplant |
| Zeitalter | Ebenen des Baums (`era = 0…6`), keine eigene Aufstiegsmechanik | fertig (als Datenfeld) |
| Strategische Ressourcen | Neue Bodenschätze/Warenketten pro Ära (Pferde, Salpeter, Öl …) | geplant (braucht Grafik) |
| Wunder | Großbaustellen mit hohen Warenkosten, exklusiv | geplant |
| Civics, Stabilität, Einfluss | Später; Stabilität z. B. über Versorgung mit Luxuswaren (Wein …) | später |
| Religion | Tempel-Addon als Ausgangspunkt | später |
| Diplomatie, Handel | Bündnis/Nichtangriffspakt und Handels-Addon existieren bereits | später |
| Ereignisse | Lua-Scripting | später |
| Straßen-Fortschritt | Pfad → Eselsweg → Pflasterstraße → Eisenbahn über Technologien | Idee |

## Technologiebaum (Fundament)

### Datenformat

Datei: `data/RTTR/gamedata/civ/techtree.lua` (eingebunden von `civ/default.lua`).
Wird nur geladen, wenn der Zivilisationsmodus aktiv ist; ein Fehler darin betrifft normale Spiele also nicht
(er wird ins Log geschrieben und der Baum ist dann leer). Im Debug-Build hält ein unbekannter Schlüssel (Tippfehler)
mit einer Assertion an.

```lua
rttr:AddTechnology{
    name = "agriculture",            -- eindeutiger Bezeichner (für Savegames, Voraussetzungen)
    title = "Ackerbau",              -- Anzeigename
    description = "…",               -- Beschreibungstext
    era = 0,                         -- Zeitalter/Ebene 0…6
    requires = { "toolmaking" },     -- Voraussetzungen; müssen VORHER definiert sein (verhindert Zyklen)
    cost = { Boards = 4, Stones = 2 }, -- Warenkosten (Bezeichner wie im Enum GoodType)
    time = 60,                       -- Mindestdauer in Sekunden ab Forschungsbeginn (optional)
    unlocks = { buildings = { "Farm", "Mill" } }, -- Bezeichner wie im Enum BuildingType
    exclusive = false,               -- true: nur ein Spieler kann sie bekommen (Kulturen, Wunder)
    addon = "WINE",                  -- optional: Technologie gibt es nur, wenn dieses Addon aktiv ist
    aiPriority = 50,                 -- optional: höher = KI forscht früher
}
```

### Regeln

- Jedes Gebäude, das von irgendeiner Technologie freigeschaltet wird, ist zu Spielbeginn gesperrt.
  Alle anderen Gebäude (Grundwirtschaft) sind sofort verfügbar.
- Ein Spieler forscht immer an höchstens einer Technologie. Bereits bezahlte Waren bleiben pro Technologie gespeichert,
  ein Wechsel kostet also nichts.
- Alle 2 Sekunden werden fehlende Kosten aus dem freien Bestand der Lagerhäuser entnommen (nur Waren, die nicht schon
  bestellt sind). Sind alle Kosten bezahlt und ist die Mindestdauer um, ist die Technologie erforscht.
- Exklusive Technologien: Sobald ein Spieler sie abschließt, sind sie für alle anderen gesperrt;
  deren bereits bezahlte Waren gehen zurück in ihr erstes Lagerhaus.
- Savegames speichern die Technologien **namentlich**, damit Änderungen am Lua-Baum alte Spielstände nicht zerstören.
- Die Computer-KI (AIJH) wählt selbstständig Forschungen: zuerst, was sie sofort bezahlen kann, dann nach `aiPriority`,
  dann die günstigste. Hängt eine Forschung länger als ca. 2,5 Minuten mangels Waren, wechselt sie zu einer bezahlbaren
  (der Fortschritt bleibt erhalten). Gesperrte Gebäude plant sie nicht.
- Bekannte Schwäche des Lagerhaus-Modells: Zwischenprodukte (Getreide, Eisen, Münzen …) gehen meist direkt an
  verarbeitende Gebäude und liegen selten im Lager. Solche Kosten werden dann langsam bezahlt. In Phase 2 soll eine
  Akademie die Waren wie ein normales Gebäude **bestellen** (Transportsystem + Verteilung), dann ist das gelöst.

### Testen ohne Grafik

`ai-battle` (Headless-KI-gegen-KI) zeigt im Zivilisationsmodus zusätzlich den Forschungsstand:

```sh
# civ.ini:  [addons]  19922944=1   (0x01300000 = Zivilisationsmodus)
ai-battle --map RTTR/MAPS/NEW/GreenPlains.SWD --ai aijh --ai aijh --settings civ.ini --maxGF 144000
```

Stand 2026-10-18 (Karte GreenPlains, 2 Stunden Spielzeit): Ohne Wein/Leder/Köhlerei erforschen zwei AIJH alle bzw.
fast alle der 16 verfügbaren Technologien; mit allen Addons erforschen drei AIJH 17–18 von 20.
Am längsten hängen Technologien, die Zwischenprodukte kosten (Burgenbau: Münzen, Gerberei: Schinken).

### Code-Überblick

| Teil | Ort |
|------|-----|
| Addon-Schalter (ID `0x01300000`) | `libs/s25main/addons/AddonCivilization.h` |
| Rohdaten aus Lua | `libs/libGamedata/gameData/TechnologyDesc.*`, `GameDataLoader::AddTechnology` |
| Aufgelöster Baum (Enums) | `libs/s25main/civ/TechTree.*` |
| Forschungsstand pro Spieler | `libs/s25main/civ/PlayerResearch.*` (in `GamePlayer`) |
| Forschungs-Logik | `libs/s25main/civ/Research.*` (aufgerufen aus `Game::RunGF`) |
| Spielbefehl | `gc::SetResearch` (`GameCommands.h`) |
| Fenster | `libs/s25main/ingameWindows/iwResearch.*` (Hauptauswahl → „Forschung“) |
| Tests | `tests/s25Main/integration/testResearch.cpp`, `tests/libGameData/testGameData.cpp` |
| Inhalte | `data/RTTR/gamedata/civ/techtree.lua` |

## Phasen (Roadmap)

1. **Fundament** – Addon, Lua-Technologiebaum, Forschung mit Warenkosten, Freischalten von S2-Gebäuden, Exklusivität,
   Savegames, KI-Grundverhalten, Forschungsfenster. *(fertig, siehe oben)*
2. **Wissen** – Ware „Schriftrolle“ + Gelehrtenhaus (Platzhaltergrafik), Schriftrollen als Kostenbestandteil.
3. **Modifikatoren & Kulturen** – generisches Modifikatorsystem (Produktionstempo, Kampfkraft, Baukosten, Reichweiten …),
   Kulturen als exklusive Technologien mit stapelnden Traits.
4. **Emblematische Inhalte** – Kulturgebäude und -soldaten (Grafik der RttR-Völker wiederverwenden).
5. **Neue Warenketten pro Ära** – z. B. Pferde, Salpeter; Grafik-Pipeline aus [GRAFIK.md](GRAFIK.md).
6. **Gesellschaft** – Civics, Stabilität, Einfluss; Wunder.
7. **Militär, Religion, Diplomatie, Ereignisse**; die KI wird in jeder Phase mitgezogen.

## Offene Fragen

- Soll man mehrere Technologien in eine **Warteschlange** legen können? (Vorschlag: ja, in Phase 2)
- Zeitalter: Soll es eine Bedingung für den Übergang in die nächste Ära geben (z. B. X Technologien der aktuellen Ära),
  oder reichen die Voraussetzungen im Baum?
- Sollen Gegner sehen können, was man erforscht hat (Spionage/Statistik)?
- Wie viele Kulturen pro Ära (Humankind hat 10+)? Vorschlag: anfangs 3–4 pro Ära, weil jede Grafik/Balancing braucht.

## Bekannte Probleme im Fork (Stand 40aa9a2, nicht durch den Zivilisationsmodus verursacht)

- `Test_gameData/LoadGameData` erwartet 14 Kanten, durch `world.lua` sind es 28.
- 3 KI-Tests (`AI/BuildWoodIndustry`, `AI/ExpandWhenNoSpace`, `AI/DoBuildMilitaryBuildingsOutsideComputerBarrier`)
  schlagen nach den eigenen KI-Änderungen fehl.
- 22 Cheat-Tests (`CheatsTests/*`, `CheatCommandTrackerTests/*`) schlagen fehl.
- `Test_UI/IwMapGenerator` kompilierte nicht mehr (Landschaftsauswahl entfernt) – im Branch `humankind` repariert.
- `GamePlayer::IsBuildingEnabled` stürzte ohne GameInterface (Tests, Headless) bei gesperrten Gebäuden ab – repariert.
