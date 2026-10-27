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
| 2026-10-21 | **Gamedesign und Balancing macht ausschließlich der Nutzer.** Claude setzt technisch um, legt keine Inhalte, Zahlenwerte, Kosten oder Regeln fest und ändert keine. Braucht der Code Werte, die noch niemand festgelegt hat, verwendet Claude deutlich markierte Platzhalter und fragt nach. |
| 2026-10-18 | Humankind-Schicht auf Siedler, kein 1:1-Nachbau, keine Rundenstrategie. |
| 2026-10-18 | Optionaler Spielmodus über ein Addon, kein harter Fork der Spiellogik. |
| 2026-10-18 | **Kein Ruhm, keine Ära-Sterne.** Der **Technologiebaum ist das Rückgrat**: Kulturen, Gebäude, Waren, Traits, Einheiten usw. hängen alle am Baum. |
| 2026-10-18 | ~~Forschung kostet **Wissen als Ware** (Schriftrollen aus einem Gelehrtenhaus) **plus Warenkosten** pro Technologie.~~ Ersetzt am 2026-10-21 durch Wissenspunkte (siehe unten). |
| 2026-10-18 | Kulturen werden **exklusiv** erforscht: Wer eine Kultur-Technologie zuerst abschließt, bekommt sie; für alle anderen ist sie gesperrt. |
| 2026-10-18 | Erforschte Kulturen **stapeln vollständig** (alle Traits und emblematischen Inhalte bleiben aktiv). |
| 2026-10-18 | Spielstart mit **Grundwirtschaft**; alle weiteren S2-Gebäude werden erforscht. |
| 2026-10-18 | Grafik: vorhandene S2/RttR-Grafiken wiederverwenden; wo das nicht passt, Platzhalter, die später (u. a. per KI, siehe [GRAFIK.md](GRAFIK.md)) ersetzt werden. |
| 2026-10-18 | Inhalte (Technologien, Kulturen …) sind **datengetrieben in Lua** (`data/RTTR/gamedata/civ/`), damit sie ohne Neukompilieren geändert werden können. Sie werden nur geladen, wenn der Modus aktiv ist. |
| 2026-10-18 | Texte in eigenen Worten. Mechaniken dürfen übernommen werden, Texte/Bilder/Icons von Amplitude nicht. |
| 2026-10-20 | Grafik: Neue Grafiken erzeugt der Nutzer mit Gemini und bearbeitet sie nach. Im Code werden bis dahin Platzhalter verwendet. |
| 2026-10-21 | Forschung kostet **Wissenspunkte (WP) plus Waren**. Es gibt drei Arten: landwirtschaftlich (LWP), industriell (IWP), militärisch (MWP). Die Mindestdauer pro Technologie bleibt. |
| 2026-10-21 | WP kommen aus **Gebäuden** (fester Wert pro fertigem Gebäude, Liste unten) und aus **eingelagerten Wissensmedien**. Wird ein Gebäude zerstört, verfallen seine WP und die darin gelagerten Medien. |
| 2026-10-21 | Es gibt **gesamte WP** (alles, was man gerade hat) und **verfügbare WP** (gesamt minus ausgegeben). Verfügbare WP dürfen **negativ** werden; erforschte Technologien gehen **nie** verloren. |
| 2026-10-21 | WP werden **beim Abschluss** einer Forschung abgebucht: Fertig ist sie, wenn die Mindestdauer um ist, alle Waren bezahlt sind und genug WP verfügbar sind. |
| 2026-10-21 | Medien gibt es je Wissensart (z. B. Tontafel LWP/IWP/MWP). Was eine Medien-Werkstatt herstellt, legt **ein gemeinsamer Regler** fest (eigener Reiter „Wissen“ im Verteilungsfenster). |
| 2026-10-21 | Medien zählen in Lagern (Lagerhaus, Hauptquartier, Hafen) **0,5-fach**, in einer **Bibliothek** 1-fach. Lager bestellen keine Medien (sie werden nur eingelagert), Bibliotheken bestellen sie per Träger. Die Bibliothek gibt es ab **Ära 2**. |
| 2026-10-21 | Erster neuer Rohstoff im Spiel: **Knochen** (Jäger, Fleischerei) → geritzte Knochen als Medium. Die Knochen-Technologie ist ab **Ära 0** erforschbar. |
| 2026-10-21 | Neue Waren, einfache Werkstätten, Tags und alle Zahlenwerte werden in **Lua** definiert; C++ liefert das allgemeine Verhalten. |
| 2026-10-21 | **Tag-System:** Jede Ware hat Tags für Name (z. B. „Nadelholzbretter“), Warenart („Bretter“) und Eigenschaft („Nadelholz“). Gebäude und Baustellen bestellen nach einer Tag-Kombination oder nach dem genauen Namen. Verarbeitete Waren übernehmen die Eigenschaft der Eingangsware. |
| 2026-10-23 | **Geritzte Knochen sind drei Waren** (LWP, IWP, MWP) mit gemeinsamer Warenart `GeritzterKnochen` und der Wissensart als Eigenschaft. Eine Ware mit drei Arten gibt es technisch nicht, weil jede Ware ein eigener Lagerplatz ist; über die Tags lassen sie sich trotzdem gemeinsam bestellen. |
| 2026-10-23 | Der Knochenschnitzer braucht ein **Beil** (Cleaver). Elfenbein kommt später (mit den Tieren der Zonen). |
| 2026-10-23 | Umsetzung Regler „Wissen“ (technische Auslegung, vom Nutzer änderbar): drei Regler 0–10 je Wissensart, gewichtete Zufallswahl pro Produkt (wie Werkzeug-Prioritäten der Schlosserei); stehen alle möglichen Medien einer Werkstatt auf 0, wird gleich verteilt. Startwerte in `knowledge.lua`. |
| 2026-10-26 | **Welt:** Der Kartengenerator wird nach dem Online-Generator der RttR-Seite umgebaut und **ersetzt** die Stile Wasser/Land/Gemischt (Landmasse Inseln/Normal/Kontinente, Wasser %, Gebirge %, Oberfläche, Zufallswert). Flüsse, Häfen und HQ-Abstand zum Berg bleiben. |
| 2026-10-26 | Menge im Generatorfenster: ein Regler **„Ressourcen (in %)“** vervielfacht alle Lua-Regeln (auch Bäume und Steine), dazu **Wald %** und **Steinhaufen %** wie im Online-Generator. |
| 2026-10-26 | Die neuen Terraingrafiken (Rusty Valley, Polar Night, Wetlands) werden **zusätzliche Böden** der Landschaft „world“; die Dateien liegen im Spielordner, nicht in Git. |
| 2026-10-26 | Auf jeder Karte kommen **immer alle Zonen** vor (auch auf kleinen Karten). |
| 2026-10-27 | Nach Test: Höhen abwechslungsreich (Ebenen und Hügel, flache und steile Berge, Berge nie unter dem Umland, Minen auf den Bergen möglich), Regler **Unebenheit**; **Bauflächen im Gebirge** in festem Abstand wie im Online-Generator; Klima um die Mitte statt zu Extremen; **Flüsse** neu (aus der Höhe bis ins Wasser, durchgehend, Mindesthöhe). |
| 2026-10-27 | Flüsse wieder **wie im Original angeordnet** (2 Dreiecke je Schritt entlang einer Kante, Richtung ändert sich bei jedem Schritt um 60°: durchgehend und schmal), **keine Furten**. Seen ohne Strand, Süßwasser nach absoluter Größe (`lakeMaxSize`, nicht mehr Prozent der Karte), feuchter an Seen und Flüssen (`waterHumidity`), Strand halb so breit. |
| 2026-10-23 | Medien-WP zählen aus dem **echten Bestand**: in Lagerhaus/HQ/Hafen 0,5-fach, in der Bibliothek 1-fach. Medien unterwegs oder an Flaggen zählen nicht. Die Bibliothek nimmt **alle** Waren mit `knowledge` an (keine Grenze von 3 Eingängen) und bestellt erst, wenn der Bibliothekar da ist. |

## Zuordnung Humankind → Siedler

| Humankind | Umsetzung im Zivilisationsmodus | Status |
|-----------|----------------------------------|--------|
| Technologiebaum | Lua-definierter Baum, Forschung kostet Wissenspunkte und Waren, schaltet Gebäude frei | **Fundament fertig** |
| Wissenschaft | Wissenspunkte (LWP/IWP/MWP) aus Gebäuden und eingelagerten Medien | WP aus Gebäuden fertig, Medien geplant (Phase 2) |
| Kulturen (exklusiv) | Exklusive Technologien (`exclusive = true`); wer zuerst fertig ist, bekommt sie | Exklusivität fertig, Kultur-Inhalte geplant |
| Legacy-Traits | Modifikatoren (Produktionstempo, Kampfkraft, Baukosten …), stapeln | geplant (Phase 3) |
| Emblematische Quartiere | Besondere Gebäude pro Kultur (Grafik der RttR-Völker wiederverwenden) | geplant |
| Emblematische Einheiten | Soldaten mit Kultur-Boni (Stärke, Tempo); echte Einheitentypen später | geplant |
| Zeitalter | Ebenen des Baums (`era = 0…6`), keine eigene Aufstiegsmechanik | fertig (als Datenfeld) |
| Strategische Ressourcen | Neue Rohstoffe und Warenketten (Lehm, Ton, Papyrus, Baumwolle …, später Pferde, Salpeter …), verteilt nach Klimazonen | Verteilung nach Klima/Zone/Terrain fertig (Welt Stufe 1), neue Rohstoffe geplant (Phase 3) |
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
    knowledge = { LWP = 4 },         -- Kosten in Wissenspunkten (LWP, IWP, MWP)
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
  bestellt sind). Sind alle Waren bezahlt, ist die Mindestdauer um und sind genug Wissenspunkte verfügbar, ist die
  Technologie erforscht; erst dann werden die WP abgebucht.
- Exklusive Technologien: Sobald ein Spieler sie abschließt, sind sie für alle anderen gesperrt;
  deren bereits bezahlte Waren gehen zurück in ihr erstes Lagerhaus (WP waren noch nicht abgebucht).
- Savegames speichern die Technologien **namentlich**, damit Änderungen am Lua-Baum alte Spielstände nicht zerstören.
- Die Computer-KI (AIJH) wählt selbstständig Forschungen: zuerst, was sie sofort bezahlen kann, dann nach `aiPriority`,
  dann die günstigste. Hängt eine Forschung länger als ca. 2,5 Minuten mangels Waren, wechselt sie zu einer bezahlbaren
  (der Fortschritt bleibt erhalten). Gesperrte Gebäude plant sie nicht.
- Bekannte Schwäche des Lagerhaus-Modells: Zwischenprodukte (Getreide, Eisen, Münzen …) gehen meist direkt an
  verarbeitende Gebäude und liegen selten im Lager. Solche Kosten werden dann langsam bezahlt. Eine Möglichkeit wäre ein
  Gebäude, das die Waren wie ein normales Gebäude **bestellt** (siehe offene Fragen).

### Testen ohne Grafik

`ai-battle` (Headless-KI-gegen-KI) zeigt im Zivilisationsmodus zusätzlich den Forschungsstand:

```sh
# civ.ini:  [addons]  19922944=1   (0x01300000 = Zivilisationsmodus)
ai-battle --map RTTR/MAPS/NEW/GreenPlains.SWD --ai aijh --ai aijh --settings civ.ini --maxGF 144000
```

Stand 2026-10-18 (Karte GreenPlains, 2 Stunden Spielzeit): Ohne Wein/Leder/Köhlerei erforschen zwei AIJH alle bzw.
fast alle der 16 verfügbaren Technologien; mit allen Addons erforschen drei AIJH 17–18 von 20.
Am längsten hängen Technologien, die Zwischenprodukte kosten (Burgenbau: Münzen, Gerberei: Schinken).

Stand 2026-10-21 mit Wissenspunkten (gleiche Karte, 2 Stunden): ohne Addons 16 von 16, mit allen Addons 17–18 von 20.
Die WP bremsen die KI also nicht. Ihre Gesamt-WP liegen nach 15 Minuten bei etwa LWP 30 / IWP 50 / MWP 25, nach einer
Stunde bei 90–135 je Art und nach zwei Stunden bei 80–210, die Summe aller WP-Kosten dagegen nur bei 41–50 je Art.
Testweise verdreifachte WP-Kosten: Die KI erforscht trotzdem 16–19 von 20, die verfügbaren WP sinken dabei teils unter
null. `ai-battle` zeigt die WP jedes Spielers als „verfügbar/gesamt“ an.

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
| Tests | `tests/s25Main/integration/testResearch.cpp`, `testCivContent.cpp`, `tests/libGameData/testGameData.cpp` |
| Wissenspunkte (Arten, Berechnung) | `libs/s25main/civ/Knowledge.*` |
| Waren, Werkstätten, Tags aus Lua | `libs/libGamedata/gameData/CivContentDesc.*`, `libs/s25main/civ/Content.*`, `civ/Looks.h`, `figures/nofCustomWorker.*` |
| Nebenprodukte | `ByproductDesc` (`CivContentDesc.*`), `civ::Byproduct` (`TechTree.*`), `civ::getByproducts` (`Research.*`), `nofBuildingWorker::WorkingReady` |
| Regler „Wissen“ | `PlayerResearch::Get/SetKnowledgeDistribution`, `gc::SetKnowledgeDistribution`, `iwDistribution` (Reiter), `nofCustomWorker::ProduceWare` |
| Medien-WP, Bibliothek | `civ::calcTotalKnowledge` (`Knowledge.cpp`), `nobUsual` (Bibliotheks-Modus), `iwBuilding`, KI: `BuildingPlanner::UpdateCustomBuildingsWanted` |
| Inhalte | `data/RTTR/gamedata/civ/` (`wares.lua`, `workshops.lua`, `knowledge.lua`, `techtree.lua`, `byproducts.lua`) |

## Wissenspunkte

### Regeln

- Drei Arten: **LWP** (Landwirtschaft), **IWP** (Industrie), **MWP** (Militär).
- **Gesamt-WP** = WP aller fertigen Gebäude (+ später WP der eingelagerten Medien). Baustellen zählen nicht,
  Militärgebäude zählen ab Fertigstellung (auch unbesetzt). Ein Gebäude kann negative WP haben (Katapult −5 MWP).
- **Verfügbare WP** = Gesamt-WP minus die WP, die für abgeschlossene Forschungen ausgegeben wurden. Sie dürfen negativ
  werden (Gebäude zerstört, Katapult gebaut); dann schließt keine Forschung mit Kosten in dieser Art mehr ab, bis wieder
  genug da ist. Erforschte Technologien bleiben immer erhalten.
- Die Kosten werden erst beim Abschluss abgebucht; eine Forschung darf also auch ohne genug WP begonnen werden
  (Waren und Mindestdauer laufen schon). Folge: Die Summe der WP-Kosten aller erforschten Technologien kann die
  Gesamt-WP nur übersteigen, wenn danach Gebäude verloren gingen.
- Intern wird in Zehntel-WP gerechnet (für die 0,5-fache Wertung der Medien in Lagern), angezeigt mit einer
  Nachkommastelle.
- Medien (Phase 2): zählen in Lagerhaus, Hauptquartier und Hafen 0,5-fach, in einer Bibliothek 1-fach.

### Datenformat

Datei: `data/RTTR/gamedata/civ/knowledge.lua`. Jede Zeile gibt die WP eines fertigen Gebäudes an; nicht genannte
Gebäude und Arten zählen 0. Die Kosten einer Technologie stehen in `techtree.lua` unter `knowledge`.

```lua
rttr:SetBuildingKnowledge{
    Headquarters = { LWP = 5, IWP = 5, MWP = 5 },
    Woodcutter   = { IWP = 2 },
    Catapult     = { MWP = -5 },
}
```

Werte des Nutzers (2026-10-20): LWP – Fischer 2, Jäger 2, Brauerei 1, Brunnen 1, Bauernhof 3, Bäcker 3, Mühle 3,
Schweinezucht 4, Eselzucht 4, Fleischerei 3, Förster 1, Lagerhaus 1, Hafen 1, Hauptquartier 5, Weinberg 4, Winzerei 3.
IWP – Holzfäller 2, Sägewerk 3, Förster 1, Steinbruch 2, jede Mine 1, Köhler 4, Bauernhof 1, Lagerhaus 1,
Waffenschmiede 2, Eisenschmelze 3, Schlosserei 3, Werft 3, Münzprägerei 3, Hauptquartier 5, Hafen 1, Abdecker 2,
Gerberei 3, Tempel 3. MWP – Lagerhaus 1, Hafen 1, Hauptquartier 5, Baracke 2, Wachstube 3, Wachturm 4, Festung 5,
Spähturm 1, Brauerei 1, Katapult −5, Waffenschmiede 1, Lederei 3.

Die WP-Kosten der Technologien sind Startwerte von Claude (2026-10-21). Alle Werte legt ab jetzt der Nutzer fest.

## Datenschicht: Waren, Werkstätten und Tags aus Lua

### Prinzip

- Die Enums `GoodType`, `BuildingType` und `Job` haben **freie Plätze** (Slots) hinter den eingebauten Werten:
  64 Waren (`GoodType::Custom0` …), 32 Gebäude (`BuildingType::Custom0` …) und 32 Berufe (`Job::Custom0` …, einer pro
  Werkstatt). Lua belegt sie in der Reihenfolge der Definition. Unbenutzte Plätze sind überall ausgeblendet.
- `civ::Content` (`libs/s25main/civ/Content.*`) löst die Lua-Definitionen auf. Der `TechTree` besitzt den Content; im
  laufenden Spiel ist er „aktiv“ (`civ::getActiveContent()`), damit globale Tabellen ohne Zugriff auf die Welt die
  neuen Einträge kennen: `WARE_NAMES`, `BUILDING_NAMES`, `BUILDING_COSTS`, `BUILDING_SIZE`, `BLD_WORK_DESC`,
  `JOB_NAMES`, `JOB_CONSTS`, `JOB_SPRITE_CONSTS`, `STD_TRANSPORT_PRIO` liefern für die Slots die Werte aus Lua.
- **Platzhaltergrafik:** Jede neue Ware, Werkstatt und jeder Arbeiter nennt ein eingebautes Vorbild (`look`), dessen
  Grafiken verwendet werden (`civ::getLook`). Eigene Grafiken folgen später.
- **Werkstätten** sind normale Produktionsgebäude (`nobUsual`) mit dem Arbeiter `nofCustomWorker`: Er holt die
  Eingangswaren, arbeitet `workTime` Sekunden im Gebäude und trägt das Produkt heraus. Die Werkstatt wird
  automatisch als Abnehmer ihrer Eingangswaren eingetragen.
- **Tags:** Jede Ware hat ihren Bezeichner als Name-Tag, dazu optional Warenart (`kind`) und Eigenschaften
  (`properties`). Eine Tag-Abfrage trifft alle Waren, die alle genannten Tags haben. Mit `inheritProperties` bekommt
  das Produkt die Eigenschaften der verbrauchten Ware (Nadelholz → Nadelholzbretter).
- Bisherige Grenzen: höchstens 3 Eingangswaren pro Werkstatt (bei `consume = "each"` trifft jeder Eingang genau eine
  Ware, bei `"any"` wird wie bei Minen eine Ware des größten Vorrats verbraucht); Baukosten nur Bretter und Steine;
  Arbeiter ohne eigene Arbeitsanimation.
- **Spielstände:** Version 18. Gespeichert wird die Liste der belegten Slots; ein Spielstand lädt nur, wenn die
  Lua-Definitionen dazu passen. Neue Waren und Werkstätten dürfen hinten angefügt werden, bestehende dürfen nicht
  gelöscht, umbenannt oder umsortiert werden. Ältere Spielstände (ohne Slots) laden weiter.
- In der Lager- und Warenübersicht stehen neue Waren und Arbeiter auf eigenen Seiten. Neue Werkstätten erscheinen im
  Baumenü (nach Größe), in der Gebäudestatistik, der Produktivitätsübersicht und der Baureihenfolge.

### Nebenprodukte

- `rttr:AddByproduct{ building = "Hunter", ware = "Bones", amount = 1, technology = "bone_carving" }` in
  `civ/byproducts.lua`: Jedes Mal, wenn der Arbeiter des Gebäudes ein Produkt an die Flagge bringt, legt er das
  Nebenprodukt mit dazu. Optional `per` (nur für bestimmte Produkte) und `technology` (erst danach). Geht für
  eingebaute Produktionsgebäude und Werkstätten, höchstens 7 Nebenprodukte pro Gebäude.
- Umgesetzt an einer Stelle für alle Arbeiter (`nofBuildingWorker::WorkingReady`). Ob es ein Nebenprodukt gibt, wird
  beim Ablegen geprüft. Ist an der Flagge nicht für alles Platz, wartet der Arbeiter mit dem Produkt wie bei voller
  Flagge, bis genug Plätze frei sind. Es wird also nichts zurückgelassen, und der Spielstand braucht keine neuen Daten.

### Mehrere mögliche Produkte

- `outputChoice = "knowledge"` erlaubt, dass `output` mehrere Waren trifft (geritzte Knochen LWP/IWP/MWP). Jede davon
  braucht `knowledge` in `wares.lua`. Welche hergestellt wird, entscheidet später der Regler „Wissen“ (Schritt 4b);
  bis dahin wählt der Arbeiter mit dem synchronen Spiel-Zufall gleich verteilt.

### Regler „Wissen“ und Bibliothek (Schritte 4b, 4c)

- Reiter „Wissen“ im Verteilungsfenster (nur im Zivilisationsmodus und wenn es Wissensmedien gibt): drei Regler
  LWP/IWP/MWP. Eigener Spielbefehl `gc::SetKnowledgeDistribution`, gespeichert in `civ::PlayerResearch`
  (Spielstand-Version 19). Das normale Verteilungssystem bleibt unverändert.
- Startwerte: `rttr:SetKnowledgeDistribution{ LWP = 5, IWP = 5, MWP = 5 }` in `knowledge.lua` (ohne Angabe 5/5/5).
- Bibliothek: Werkstatt mit `library = { capacity = 20 }` (ohne inputs/output/workTime). Sie ist Abnehmer aller
  Wissensmedien, bestellt sie alle 10 Sekunden aus den Lagern (eins pro Durchlauf), solange Platz ist. „Produktion
  stoppen“ stoppt die Annahme. Das Gebäudefenster zeigt den Bestand. Wird sie zerstört, sind die Medien verloren.
- KI: Lua-Werkstätten mit Eingängen baut sie, sobald Eingangswaren im Lager liegen (1 + 1 je 10 Waren, höchstens 3),
  Werkstätten ohne Eingang einmal, Bibliotheken, wenn mehr Medien in Lagern warten als die vorhandenen noch aufnehmen
  (höchstens 5). Werkzeuge bestellt sie über die vorhandene Werkzeug-Logik.
- KI und Regler (Wunsch des Nutzers 2026-10-23): Alle 200 GF (10 Sekunden) stellt die KI den Regler nach den WP ein,
  die ihrer aktuellen Forschung noch fehlen: die am meisten fehlende Art 10, andere fehlende anteilig (mindestens 1),
  nicht benötigte 0. Forscht sie nichts oder fehlen keine WP: Startwerte aus `knowledge.lua`. Einstellbar oben in
  `libs/s25main/ai/aijh/AIPlayerJH.cpp` (`KNOWLEDGE_DISTRIBUTION_INTERVAL_GF`, `KNOWLEDGE_WEIGHT_NOT_NEEDED`,
  `KNOWLEDGE_WEIGHT_MOST_NEEDED`), Logik in `AIPlayerJH::AdjustKnowledgeDistribution`.

### Datenformat

Dateien: `data/RTTR/gamedata/civ/wares.lua` und `civ/workshops.lua` (dort ausführlich erklärt, mit Beispiel).

```lua
rttr:AddWare{ name = "NeedleWood", title = "Nadelholz", kind = "Holz", properties = { "Nadelholz" },
              look = "Wood", transport = "Wood" }
rttr:AddWare{ name = "NeedleBoards", title = "Nadelholzbretter", kind = "Bretter", properties = { "Nadelholz" },
              look = "Boards", transport = "Boards" }
rttr:SetWareTags{ Fish = { kind = "Nahrung" } }   -- Tags für eingebaute Waren

rttr:AddWorkshop{
    name = "Sawmill2", title = "Sägewerk", size = "medium", cost = { Boards = 2, Stones = 2 }, look = "Sawmill",
    worker = { title = "Säger", tool = "Saw", look = "Carpenter" },
    inputs = { "Holz" }, consume = "any", output = "Bretter", inheritProperties = true, workTime = 20,
}
```

Die Beispiele zeigen nur das Format; welche Waren und Werkstätten es gibt, legt der Nutzer fest.

## Welt: Kartengenerator mit Klima und Zonen (Stufe 1)

Stand 2026-10-27 (Überarbeitung nach Test des Nutzers). Gilt nur für **erzeugte Karten** (Zufallskarte im
Kartenauswahlmenü).

### Ablauf der Erzeugung (`mapGenerator/RandomMap.cpp`)

1. **Wasser, Land, Gebirge wie im Online-Generator** (`Landscape.cpp`, `CreateLandMasses`): ein fraktales Rauschen
   (wiederholt sich an den Kartenrändern, Zeilen halb so hoch wie Spalten wie im Spiel) wird nach Rang geschnitten:
   die untersten `Wasser %` werden Wasser, die obersten `Gebirge %` des Landes Gebirge. Landmasse bestimmt die Größe
   der Formen (Inseln klein, Kontinente groß), Oberfläche die Feinheit der Küsten und Bergränder.
2. **Höhen** (`CreateHeights`): Land steigt flach von der Küste an (flache Strände). Hügel gibt es je nach Gegend:
   manche Gegenden sind Ebenen, andere hügelig. Gebirge stehen auf dem Land und steigen vom Fuß an; jede Bergkette hat
   eigene Steilheit und Höhe (flache niedrige und steile hohe Berge), Berge sind nie niedriger als das Land daneben.
   Die Hänge bleiben so, dass auf fast allen Bergknoten Minen gehen (Nachbar im Südosten höchstens 3 höher).
   Regler **„Unebenheit (in %)“**: 0 = alles flach, 100 = sehr hügelig und steile Berge (Standard 50).
3. **Seen** (`MarkLakes`): Gewässer bis `lakeMaxSize` Knoten (absolut, unabhängig von der Kartengröße) sind Seen
   (Süßwasser, kein Strand), größere sind Meer (Salzwasser).
   **Flüsse** (`Rivers.cpp`): Jeder Landknoten bekommt einen Abfluss-Nachbarn aus einer „Priority Flood“ ab allen
   Wasserknoten. Flüsse entspringen ab `riverMinHeight` (meist im Gebirge) und folgen dem Abfluss bis ins Meer oder in
   einen See – über den niedrigsten Pass, nie im Kreis, nie ins Nichts; trifft einer auf einen anderen Fluss, mündet er.
   Das Wasser wird **wie im Original-Generator** (`CreateStream`) angeordnet: Der Lauf geht entlang der Kanten des
   Kartengitters, bei jedem Schritt werden die 2 Dreiecke links und rechts der Kante Wasser, und die Richtung ändert sich
   bei **jedem** Schritt um 60° (nie geradeaus, höchstens 2 Drehungen in dieselbe Richtung hintereinander). Nur so
   teilen sich die Dreiecke zweier Schritte eine Seite: Der Fluss ist durchgehend und ein Dreieck breit (geradeaus
   würden sich die Wasserflächen nur an einer Ecke berühren = „Pfützen“). Der Lauf ist der kürzeste solche Weg in
   einem Korridor von 3 Knoten um den Abflussweg (bergauf teuer, etwas Zufall für natürliche Biegungen). Keine Furten:
   Flüsse trennen das Land wie im Original. Das Flussbett wird gegraben (flussabwärts nie höher), die Ufer bekommen
   keinen Sand und keinen Sumpf. Kürzere als `riverMinLength` Schritte entfallen. Regler „Flüsse“: bei 100 % etwa ein
   Fluss je 2000 Knoten.
4. **Klima:** Temperatur und Feuchtigkeit sind **normalverteilt** um den Mittelwert der Voreinstellung (Gemäßigt = 50)
   mit der Streuung `temperatureSpread`/`humiditySpread`: der größte Teil der Karte ist gemäßigt, Extreme sind selten.
   Mit der Höhe wird es kälter (`heightCooling`). Vorher waren die Werte gleichverteilt (gleich viel extrem wie mild).
   An Seen und Flüssen steigt die Feuchtigkeit um `waterHumidity` Punkte, mit dem Abstand weniger, ab
   `nearWaterDistance` + 1 Knoten gar nicht mehr (nicht am Meer).
5. **Texturen:** Dreieck = Wasser, wenn alle Ecken Wasser sind, Gebirge, wenn mindestens 2 Ecken Gebirge sind, sonst
   Land nach Klima. Gebirgstexturen nach Temperatur (Grönland/Ödland/Winter), innerhalb nach Höhe über dem Bergfuß;
   Schnee nur auf den höchsten 8 % der Gipfel der Karte. Sandstrand nur am Meer, in Dreiecksreihen ab dem Wasser
   (`GetBeachWidth`: 1 Reihe bis 128×128, 2 bis 512×512, 3 bis 1024×1024, sonst 4; halb so breit wie vorher).
6. **Bauflächen im Gebirge** (`PlaceMountainBuildingSites`) wie im Online-Generator: auf einem regelmäßigen Raster
   (`mountainBuildSpacing`, Standard 6 Knoten, jede zweite Reihe versetzt) bekommen die 6 Dreiecke um einen Knoten
   Bergwiese (bebaubares Gebirge passend zur Temperatur) und die Höhen werden eingeebnet, sodass dort große Gebäude
   (Militär) gehen. Liegt ein Rasterpunkt nicht ganz im Gebirge, wird ein Knoten in der Nähe genommen.
7. **Häfen, Hauptquartiere:** Jeder Spieler kommt auf die Landmasse mit dem meisten Platz pro Spieler. Auf Inselkarten
   bekommt so jeder seine eigene Insel, auf Karten mit einem großen Kontinent starten alle dort. Ein HQ braucht im
   Umkreis von 8 Knoten mindestens 40 % bebaubares Land (sonst landete es z. B. auf einer schmalen Halbinsel, und die
   KI kam dort nicht weg); nur wenn es keine solche Stelle gibt, wird eine engere genommen.
8. **Zonen** (`Zones.cpp`): alle Zonen aus Lua, jede genau einmal als zusammenhängendes Gebiet, jeder Knoten in genau
   einer Zone. Sie wachsen gleichzeitig von weit verteilten Startpunkten mit welligen Grenzen.
9. **Terrain-Merkmale:** Seen und Flüsse Süßwasser, Meer Salzwasser (`lakeMaxSize`), Ufer und Nähe
   (`nearWaterDistance`).
10. **Ressourcen nach Lua-Regeln** (`Resources.cpp`), in der Reihenfolge der Datei.
11. Speichern als `Random.swd` (Name „Zufall <Zufallswert>“) und ein **Vorschaubild** `Random_Vorschau.bmp` im
    Kartenordner (`<RTTR_USERDATA>/MAPS`), von oben nach unten: Zonen, Temperatur, Feuchtigkeit, Höhe, Ressourcen.

Messung (256×256, Unebenheit 50): Minen auf 99 % der Bergknoten möglich, Burgen (nur nach Höhe) auf ~78 % des Landes;
Unebenheit 0: 91 %, Unebenheit 100: 23 %. Zum Vergleich die Online-Karten: 97–100 % Minen, 34–54 % Burgen.

Gleicher Zufallswert und gleiche Einstellungen ergeben dieselbe Karte. Leeres Feld = jedes Mal ein neuer Wert; der
zuletzt benutzte steht im Generatorfenster.

### Datenformat (`data/RTTR/gamedata/world/mapgen.lua`)

Ausführliche Anleitung steht in der Datei. Kurz:

```lua
rttr:SetMapClimate{ heightCooling = 20, temperatureSpread = 19, humiditySpread = 19, nearWaterDistance = 4,
                    lakeMaxSize = 500, waterHumidity = 10, riverMinHeight = 30, riverMinLength = 15,
                    mountainBuildSpacing = 6, previewImages = true }
rttr:AddMapZone{ name = "Afrika", title = "Afrika", color = 0xE08030, packages = {
    { name = "regenwald", temperature = {65, 100}, humidity = {55, 100}, height = {0, 40},
      trees = { Palm1 = 2, Pineapple = 1 }, animals = { Deer = 2, Duck = 1 } },
    { trees = { Oak = 1 } } -- ohne Bereiche: gilt überall sonst
} }
rttr:AddMapResource{ name = "Steinhaufen", object = "stones", amount = "stonePiles",
    terrain = { "land", "mountain" }, cluster = 8, density = 80,
    weights = { { temperature = {60, 100}, humidity = {60, 100}, factor = 0.3 } } }
```

- Objekte: `trees`, `stones`, `animals`, `decoration`, `fish`, `groundwater`, `coal`, `iron`, `gold`, `granite`.
  Baum- und Tierarten kommen aus dem Paket der Zone (erstes passendes Paket). Enten nur auf Wasser.
- Terrain: `land`, `mountain`, `freshwater`, `saltwater`, `river`, `shore_fresh`, `shore_salt`, `shore_river`,
  `near_fresh`, `near_salt`, `near_river` (mehrere = eines muss passen).
- Menge: `amount` = Prozent der passenden Knoten (Mittel), oder ein Regler: `"forest"`, `"stonePiles"`, `"coal"`,
  `"iron"`, `"gold"`, `"granite"`. Alles mal Regler „Ressourcen (in %)“. `weights` ändern die Menge je Klima.
- Klumpen: `cluster` = Größe in Knoten (glattes Rauschen unter einer Schwelle), `density` = belegter Anteil darin.
  Bodenschätze werden gemeinsam in Klumpen verteilt (Anteile nach ihren Mengen, in der Mitte reich).
- Optional: `temperature`/`humidity`/`height` (harte Grenzen), `zones`, `hqDistance = {min, max}`.
- Fehler im Lua (Tippfehler, unbekannte Art, Bereich falsch herum …) werden beim Laden gemeldet.

### Neue Böden (Texturpaket)

18 zusätzliche Böden in `world.lua` (`w_wet_*`, `w_rust_*`, `w_polar_*`, s2Id 44–61) mit `optional = true`: Der
Generator nutzt sie nur, wenn die Datei im Spielordner liegt (`GFX/TEXTURES/TEX5_WETLANDS.LBM`, `TEX6_RUSTY.LBM`,
`TEX7_POLAR.LBM`, Kopien aus dem Zip). Ihre Klimawerte sind Platzhalter. Das S2-Kartenformat erlaubt höchstens 64
Böden je Landschaft (6 Bit); „world“ hat jetzt 62.

### Grenzen

- Neue Pflanzen und Tiere (Mahagoni, Schimpanse …) und Lehm brauchen erst Kartenobjekte, Grafiken und ein Speicherformat
  (das S2-Format kennt nur 9 Baumarten und 6 Tierarten). Bis dahin stehen vorhandene Arten in den Paketen.
- Eisbären lassen sich im Kartenformat nicht speichern.
- Originalkarten werden nicht verändert.

## Phasen (Roadmap)

1. **Fundament** – Addon, Lua-Technologiebaum, Forschung mit Warenkosten, Freischalten von S2-Gebäuden, Exklusivität,
   Savegames, KI-Grundverhalten, Forschungsfenster. *(fertig, siehe oben)*
2. **Wissen**
   1. Wissenspunkte aus Gebäuden, Technologiekosten in WP, Anzeige im Forschungsfenster, KI. *(fertig, siehe oben)*
   2. Datenschicht: neue Waren, einfache Werkstätten (Eingang → Ausgang) und Tags in Lua, Platzhaltergrafik.
      *(fertig, siehe oben)*
   3. Knochen-Kette als erster Durchstich:
      - 4a: Nebenprodukte aus Lua (Knochen von Jäger und Fleischerei nach der Technologie `bone_carving`),
        Knochenschnitzer, drei geritzte Knochen, Werkstätten mit mehreren Produkten. *(fertig, Zahlenwerte sind
        Platzhalter)*
      - 4b: Regler „Wissen“ im Verteilungsfenster, Medien-WP in Lagern (0,5-fach). *(fertig)*
      - 4c: Bibliothek (Ära 2, 1-fach, bestellt Medien per Träger), einfache KI-Regel für Werkstätten. *(fertig,
        Zahlenwerte sind Platzhalter)*
3. **Neue Rohstoffe und Warenketten** – Lehm (Kartenobjekt, Lehmgrube), Ton, Tontafeln, Ziegel, Vasen; Lehm und Ziegel
   als Baustoff (Baukosten über Tags); Papyrus, Baumwolle/Knotenschnüre, Steintafeln. Später Pferde, Salpeter …
4. **Welt** – *(Stufe 1 fertig 2026-10-26, siehe oben)* Kartengenerator mit Klimazonen (Temperatur, Feuchtigkeit, Höhe) und 7 geographischen Zonen mit eigener
   Flora und Fauna; Rohstoffe wie Lehm nach Klima verteilt. Nach Wunsch des Nutzers (Konzept 2026-10-23): Umbau mit
   dem Online-Kartengenerator der RttR-Seite als Vorbild (Beispielkarten in `MAPS/`), neue Terraingrafiken
   (`TEX_RUSTY_POLAR_RC2_neue_Landschaftsgrafiken.zip`), Ressourcenverteilung per Lua (Höhe/Temperatur/Feuchte,
   optional Zone, Terrain wie Süß-/Salzwasser, Land, Gebirge, Fluss- und Ufernähe), Gesamtmenge per Regler im UI.
5. **Modifikatoren & Kulturen** – generisches Modifikatorsystem (Produktionstempo, Kampfkraft, Baukosten, Reichweiten,
   WP-Werte …), Kulturen als exklusive Technologien mit stapelnden Traits.
6. **Emblematische Inhalte** – Kulturgebäude und -soldaten (Grafik der RttR-Völker wiederverwenden).
7. **Gesellschaft** – Civics, Stabilität, Einfluss; Wunder.
8. **Militär, Religion, Diplomatie, Ereignisse**; die KI wird in jeder Phase mitgezogen.

## Offene Fragen

- Soll man mehrere Technologien in eine **Warteschlange** legen können?
- Zeitalter: Soll es eine Bedingung für den Übergang in die nächste Ära geben (z. B. X Technologien der aktuellen Ära),
  oder reichen die Voraussetzungen im Baum?
- Sollen Gegner sehen können, was man erforscht hat (Spionage/Statistik)?
- Wie viele Kulturen pro Ära (Humankind hat 10+)?
- Warenkosten der Forschung: weiter aus dem Lagerbestand nehmen oder von einem Gebäude per Träger bestellen lassen?
- Warenlimit für Lager (Idee des Nutzers, noch nicht ausgearbeitet).
- Welt: Gelten Klimazonen und Rohstoffverteilung nur für generierte Karten oder auch für Originalkarten? (Stufe 1: nur
  generierte Karten)

## KI-Bauregeln (Wunsch des Nutzers 2026-10-23, gilt in allen Spielen)

- **Expansion gebremst:** höchstens `MAX_MILITARY_SITES` = 3 Militär-Baustellen gleichzeitig (auch gerade beauftragte),
  und nur wenn mindestens `MIN_SOLDIERS_PER_MILITARY_SITE` = 2 Soldaten je Militär-Baustelle (inkl. der neuen) in den
  Lagern sind. Sonst wird der Militärauftrag verworfen (die KI plant regelmäßig neue).
- **Material vor jedem Bauauftrag:** Bretter und Steine im echten Lagerbestand minus das, was bestehende und gerade
  beauftragte Baustellen noch brauchen, müssen für das Gebäude reichen. Sonst wartet der Auftrag (Militär: verworfen).
  Gilt auch für das Sägewerk, das die KI an neuen Lagern direkt setzt (sonst normaler Bauauftrag).
- **Wichtige Gebäude zuerst:** `IMPORTANT_BUILDINGS` (Waffenschmiede, Schlosserei, Eisenschmelze, Münzprägerei,
  Kohle-, Eisen-, Goldmine) werden in jeder Runde vor allen anderen Aufträgen bearbeitet. Wartet eines davon auf
  Material, bleibt dieses Material für die anderen Gebäude gesperrt.
- Einstellbar oben in `libs/s25main/ai/aijh/AIConstruction.cpp`; Prüfung in `BuildJob::CheckLimitsAndMaterial`
  (`Jobs.cpp`).
- Vergleich GreenPlains, 2 KI, 2 h, gleicher Seed: Militärgebäude 23/40 statt 50/61, höchstens 3 statt bis zu 10
  Militär-Baustellen, deutlich mehr Bretter/Steine im Lager. Die Zahl der Industriegebäude hängt weiter an den
  Bauwünschen in `BuildingPlanner.cpp` (z. B. Minen nur mit erreichten Bergen).

## Bekannte Probleme im Fork (Stand 40aa9a2, nicht durch den Zivilisationsmodus verursacht)

- `Test_gameData/LoadGameData` erwartet 14 Kanten, durch `world.lua` sind es 28 (seit Welt Stufe 1 mit den Böden des
  Texturpakets 33).
- 3 KI-Tests (`AI/BuildWoodIndustry`, `AI/ExpandWhenNoSpace`, `AI/DoBuildMilitaryBuildingsOutsideComputerBarrier`)
  schlagen nach den eigenen KI-Änderungen fehl.
- 22 Cheat-Tests (`CheatsTests/*`, `CheatCommandTrackerTests/*`) schlagen fehl.
- `ResearchSuite/DefaultTechTreeIsValid` prüfte feste Zahlenwerte (HQ 5 WP …), die der Nutzer inzwischen in Lua
  geändert hat. Seit 2026-10-23 prüft der Test nur noch die Struktur, keine Balancing-Werte.
- `Test_UI/IwMapGenerator` kompilierte nicht mehr (Landschaftsauswahl entfernt) – im Branch `humankind` repariert.
- `GamePlayer::IsBuildingEnabled` stürzte ohne GameInterface (Tests, Headless) bei gesperrten Gebäuden ab – repariert.
