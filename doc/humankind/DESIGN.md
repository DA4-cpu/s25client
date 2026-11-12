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
| 2026-10-30 | Flüsse ohne abstehende Wasserdreiecke; Feuchte an Seen/Flüssen mit einstellbarer Abnahme (`waterHumidityFalloff`, `waterHumidityStep`); im feucht-warmen Gebiet **weniger Gebirge** (`mountainWeights`, Regler „Gebirge %“ wird dort nur verringert) statt weniger Erz. |
| 2026-11-10 | Feuchte an Seen und Flüssen reicht so weit, bis die Abnahme (`waterHumidityFalloff`, `waterHumidityStep`) sie aufgebraucht hat, ohne Grenze durch `nearWaterDistance` (das gilt nur noch für „near_fresh“, „near_salt“, „near_river“). Abnahme 0 bei gesetzter `waterHumidity` ist ein Ladefehler. |
| 2026-10-30 | **Lehm** ist zunächst nur Rohstoff: Lehmvorkommen auf der Karte (vor allem feucht-warm), **Lehmgrube** mit Lehmstecher baut sie ab wie der Steinbruch, Lagern/Tragen/Statistik, die KI baut Lehmgruben. Ton, Ziegel und Stampflehm kommen später. |
| 2026-10-30 | **Eigene Grafiken:** Der Nutzer legt selbst gezeichnete Bilder (PNG/BMP) unter festen Namen in `data/RTTR/assets/civ/<Bezeichner>/` ab; fehlende Bilder bleiben Platzhalter. Das Spiel schreibt die Platzhalter als Vorlagen mit Namen, Größe und Ankerpunkt in den Benutzerordner (`civ-vorlagen`, nicht in Git). |
| 2026-10-23 | Medien-WP zählen aus dem **echten Bestand**: in Lagerhaus/HQ/Hafen 0,5-fach, in der Bibliothek 1-fach. Medien unterwegs oder an Flaggen zählen nicht. Die Bibliothek nimmt **alle** Waren mit `knowledge` an (keine Grenze von 3 Eingängen) und bestellt erst, wenn der Bibliothekar da ist. |
| 2026-11-11 | **Werkzeugreform** (Konzept des Nutzers): Jeder freie Helfer nimmt ohne Werkzeug einen Beruf in einem Gebäude an. Werkzeuge liegen in den Gebäuden (eigener Werkzeugplatz), nutzen sich pro Arbeitsschritt ab und bestimmen das Arbeitstempo. Es gibt sie in mehreren Materialien. |
| 2026-11-11 | Bauarbeiter, Planierer, Geologe und Späher (Berufe ohne eigenes Gebäude) brauchen **weiter ihr Werkzeug im Lager**. |
| 2026-11-11 | Welches Material ein Gebäude bestellt, **stellt der Spieler ein** (Regler „Werkzeuge“ im Verteilungsfenster, Wert 0–10 je Material). |
| 2026-11-11 | Ersatz wird **kurz vor dem Bruch** bestellt und wartet als zweites Werkzeug im Gebäude (Abweichung von „nur ein Werkzeug je Typ“). |
| 2026-11-11 | Erster Schritt: nur die 12 vorhandenen Werkzeuge (Material Eisen) plus das System; weitere Materialien trägt der Nutzer in Lua ein. |
| 2026-11-12 | Werkstätten mit mehreren Produkten: neues Feld **`outputs`** (Liste, jeder Eintrag eine Tag-Abfrage für genau eine Ware; ein Text = ein Tag, eine Liste = alle Tags). `output` bleibt wie bisher. |
| 2026-11-12 | **`outputChoice = "tools"`**: Die Werkstatt (z. B. Kupferwerkstatt) teilt sich **Werkzeug-Regler und Bestellungen** mit der Schlosserei, Zuordnung über den Werkzeugtyp (`kind = "Axe"`). Eine Bestellung erfüllt, wer das Werkzeug zuerst anfängt. |
| 2026-11-12 | Neue Datei **`distribution.lua`**: eigene Regler für `outputChoice` (`rttr:AddRegulator`, je ein Reiter im Verteilungsfenster) und die Startwerte der Regler „Wissen“ und „Werkzeuge“ (vorher `knowledge.lua` bzw. `priority` in `tools.lua`). |
| 2026-11-12 | KI baut Werkstätten mit `outputChoice = "tools"` wie die Schlosserei, sobald die Eingänge im Lager sind; ihre Werkzeug-Regler gelten dort mit. |
| 2026-11-12 | **`outputRatio`** (Waren pro Arbeitsschritt, 0–8, Kommazahlen) für **alle Gebäude**: Werkstätten in `rttr:AddWorkshop`, eingebaute Gebäude mit `rttr:SetBuildingOutputRatio`, Werkzeuge geben einen Bonus (`outputRatio` in `rttr:AddTool`), der zum Grundwert addiert wird (0,5 + 1 = 1,5 = abwechselnd 1 und 2 Waren). |
| 2026-11-12 | Mehrere Waren in einem Schritt: Mit `outputChoice` wird **jede Ware einzeln gewählt**; **Nebenprodukte je Ware**. |
| 2026-11-12 | **Neue KI** (Konzept von Claude, vom Nutzer übernommen): datengetrieben in fünf Schichten (Wahrnehmung, Wirtschaftsmodell, Ziele, Planung, Ausführung), siehe Abschnitt „Neue KI“. Die KI **kennt die ganze Karte**; ihre **Ausrichtung ergibt sich nur aus Karte und Lage** (keine festen Persönlichkeiten); Schwierigkeitsgrade **ohne Boni**; Alternativen kommen über **abstrakte Tags** in den Daten. Alle Schritte so effizient wie möglich. |
| 2026-11-12 | **Baukosten nach Tags**: jede Ware kann Baumaterial sein. Auch die eingebauten Gebäude bekommen ihre Kosten aus Lua (`rttr:SetBuildingCost`). Kurzform `{ Boards = 2 }` und Langform `{ { tags = ..., amount = ... } }`. Verbaut wird **nur nach Reihenfolge** (erste Hälfte des Fortschritts = Gerüst). Die Baustelle bestellt aus dem **nächsten Lager mit irgendeiner passenden Ware**. |

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
| Eigene Regler, `outputChoice = "tools"` | `civ::Regulator`, `PlayerResearch::Get/SetRegulatorValues`, `gc::SetRegulatorValues`, `iwDistribution::AddRegulatorTab`, `nofCustomWorker::ChooseTool` / `ChooseWeightedProduct` |
| Medien-WP, Bibliothek | `civ::calcTotalKnowledge` (`Knowledge.cpp`), `nobUsual` (Bibliotheks-Modus), `iwBuilding`, KI: `BuildingPlanner::UpdateCustomBuildingsWanted` |
| Inhalte | `data/RTTR/gamedata/civ/` (`wares.lua`, `workshops.lua`, `knowledge.lua`, `distribution.lua`, `tools.lua`, `techtree.lua`, `byproducts.lua`) |

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

### Mehrere mögliche Produkte (Stand 2026-11-12)

- Produkte: `output` (eine Tag-Abfrage, darf mit `outputChoice` mehrere Waren treffen) oder `outputs` (Liste, jeder
  Eintrag trifft genau eine Ware, höchstens 16; nicht für Sammler, Plantagen, Minen und nicht mit `inheritProperties`).
  Mehr als ein Produkt braucht `outputChoice`. Aufgelöst in `civ::Content` (`CustomBuilding::productChoice`,
  `productChoices`).
- `outputChoice = "knowledge"`: nach dem Regler „Wissen“; jedes Produkt braucht `knowledge` in `wares.lua` (genau eine
  Art).
- `outputChoice = "tools"`: wie die Schlosserei (`nofCustomWorker::ChooseTool`). Jedes Produkt ist ein Werkzeugtyp
  (`Content::GetToolTypeOf`: der Bezeichner genau eines eingebauten Werkzeugs als Tag), jeder Typ höchstens einmal.
  Beim Arbeitsbeginn zuerst Bestellungen (`GamePlayer::ToolOrderProcessed`, Meldung „Completed the ordered amount of
  tools.“ wenn keine mehr offen sind), dann gewichtet mit den Werkzeug-Prioritäten. Alle 0: Addon
  „Schlosserei bei 0“ entscheidet (nichts oder irgendeins). Wartet sie, weckt sie ein neuer Auftrag oder eine neue
  Einstellung (`ToolNote`).
- `outputChoice = <Name>`: eigener Regler aus `distribution.lua` (`rttr:AddRegulator`, höchstens 8 Regler mit je
  höchstens 8 Einträgen). Jedes Produkt trifft genau einen Eintrag (Tag-Abfrage `tags`). Gewichtete Zufallswahl wie
  beim Regler „Wissen“, alle 0 = gleich verteilt. Werte je Spieler nach Reglername (`PlayerResearch::
  Get/SetRegulatorValues`, Befehl `gc::SetRegulatorValues`), Reiter im Verteilungsfenster mit „Standard“-Knopf.
- Spielstand Version 23: Reglerwerte (nach Name; unbekannte Regler fallen weg, neue Einträge bekommen den Startwert)
  und das gewählte Werkzeug des Arbeiters.

### Ertrag pro Arbeitsschritt: `outputRatio` (Stand 2026-11-12)

- Wert in Hundertsteln (`parseOutputRatio`, 0–800): Werkstatt `WorkshopDesc::outputRatio`, eingebautes Gebäude
  `WorldDescription::buildingOutputRatios` (`rttr:SetBuildingOutputRatio`, nur eingebaute Gebäude mit Arbeiter),
  Bonus des Werkzeugs `ToolDesc::outputRatio`. `Content::GetOutputRatio(BuildingType)` ist der Grundwert (Standard 100).
- `nobUsual::GetOutputRatio` = Grundwert + Bonus aller Werkzeuge in Gebrauch, höchstens 800; außerhalb des
  Zivilisationsmodus immer 100. `BeginWorkStep` merkt sich den Wert des Schritts (vor der Abnutzung, wie das Tempo);
  ist er 0, arbeitet das Gebäude nicht (die Eingänge bleiben liegen).
- Ergibt ein Schritt eine Ware, ruft der Arbeiter `nofBuildingWorker::ProducedWare` (in `nofWorkman`, `nofFarmhand`,
  `nofHunter`, `nofSkinner`). `nobUsual::TakeNumProducts` zählt den Wert zum Rest der früheren Schritte
  (`outputCredit_`) und gibt die ganzen Waren zurück. Fest, ohne Zufall: 1,5 = 1, 2, 1, 2 …; Schritte ohne Ware (Fischer
  ohne Fang, Bauer beim Säen) zählen nicht.
- 0 Waren: Der Arbeiter trägt nichts hinaus (draußen: er kommt mit leeren Händen zurück). Mehr als 1: weitere Waren
  über `ProduceExtraWare` (Standard dieselbe Ware). Schlosserei, Kupferwerkstatt (`outputChoice`), Waffenschmiede
  (Schwert/Schild abwechselnd) und Tempel wählen jede Ware neu. Abbau je Ware: Minen (auch Lua-Minen) und Brunnen eine
  Einheit, Steinmetz ein Stück des Steinhaufens, Sammler und Plantagen ein Stück des Objekts, Fischer ein Fisch; ist
  nichts mehr da, gibt es weniger Waren. Holzfäller, Bauer, Jäger usw.: alle Waren aus demselben Baum, Feld, Tier.
- Alle Waren eines Schritts samt Nebenprodukten liegen in `carryList_` (Spielstand) und werden zusammen an die Fahne
  gelegt, mehr als 8 nacheinander (`GetNumWaresToPutNow`); bis 8 wartet der Arbeiter wie bisher, bis für alle Platz ist.
- Gebäudefenster: „Ertrag xN“ unter dem Tempo (wenn nicht 1 oder das Gebäude Werkzeugplätze hat).
- Spielstand Version 24: `stepOutputRatio_`, `outputCredit_`, `carryList_`.
- Ausgelieferte Daten setzen keine Werte (alle 1, Werkzeuge ohne Bonus); das Balancing macht der Nutzer.

### Baukosten nach Tags (Stand 2026-11-12)

- Lua: `rttr:SetBuildingCost{ Gebäude = Kosten }` in `buildingcosts.lua` (dort stehen die bisherigen Kosten aller
  eingebauten Gebäude), `cost` in `rttr:AddWorkshop` im selben Format. Kurzform `{ Boards = 2, Stones = 1 }` (ein Tag je
  Eintrag, alphabetisch, da Lua-Tabellen keine Reihenfolge haben), Langform
  `{ { tags = { "Bretter", "Nadelholz" }, amount = 2 }, ... }` (in dieser Reihenfolge). Höchstens 4 Einträge und 31 Waren
  (`MAX_COST_ENTRIES`, `MAX_TOTAL_COST`; der Baufortschritt bleibt 1 Byte). Nicht genannte eingebaute Gebäude:
  `Boards`/`Stones` als Tags (passen auch auf neue Waren mit diesem Tag).
- Der Hafen ist ausgenommen: Expeditionen tragen Bretter und Steine (`HARBOR_BUILDING_COST`).
- Code: `CostEntry { tags, wares, amount }`, `BuildingCosts` (`gameTypes/BuildingTypes.h`); `BUILDING_COSTS[bld]` liefert
  `civ::Content::GetBuildingCost` (auch ohne Zivilisationsmodus, dann die Originalkosten).
- Baustelle (`noBuildingSite::Material` je Eintrag: geliefert mit Warentyp, verbaut, bestellt): `OrderConstructionMaterial`
  bestellt je Eintrag mit `GamePlayer::OrderWare(wares, goal)` (nächstes Lager mit irgendeiner passenden Ware
  `FW::HasAnyWare`, dort die erste vorrätige der Liste; sonst herrenlose Waren). `TakeWare` ordnet eine Ware dem Eintrag
  zu, für den gerade bestellt wird, sonst dem ersten, der sie braucht. Waren ohne Baustellen in der Verteilung (z. B.
  neue Bretter) finden Baustellen trotzdem (`FindClientForWare`).
- Bauarbeiter (`nofBuilder::ChooseWare`): streng in Listenreihenfolge, wartet auf den nächsten Eintrag, auch wenn
  spätere Waren schon da sind. Anzeige: im Zivilisationsmodus Gerüst = erste Hälfte, außerhalb wie im Original
  (`noBuildingSite::GetMaxProgress`, auch im Nebel).
- Baustellenfenster: eine Reihe je Eintrag (mehr als 8 Waren umbrechen, Fenster wächst); Bau-Tooltip zeigt „N boards“,
  den Warennamen oder die Tags. Waren an der Baustelle: Einträge abwechselnd links/rechts der Tür.
- Abriss mit Rückerstattung: je Eintrag die erste passende Ware.
- KI (`AIConstruction`): freier Bestand je Ware (`ConstructionMaterial = EnumArray<int, GoodType>`), Bedarf der Baustellen
  und Reserven werden greedy auf die passenden Waren verteilt.
- Spielstand Version 25 (ältere: Bretter/Steine kommen in die Einträge, die sie enthalten).

### Regler „Wissen“ und Bibliothek (Schritte 4b, 4c)

- Reiter „Wissen“ im Verteilungsfenster (nur im Zivilisationsmodus und wenn es Wissensmedien gibt): drei Regler
  LWP/IWP/MWP. Eigener Spielbefehl `gc::SetKnowledgeDistribution`, gespeichert in `civ::PlayerResearch`
  (Spielstand-Version 19). Das normale Verteilungssystem bleibt unverändert.
- Startwerte: `rttr:SetKnowledgeDistribution{ LWP = 5, IWP = 5, MWP = 5 }` in `distribution.lua` (ohne Angabe 5/5/5).
- Bibliothek: Werkstatt mit `library = { capacity = 20 }` (ohne inputs/output/workTime). Sie ist Abnehmer aller
  Wissensmedien, bestellt sie alle 10 Sekunden aus den Lagern (eins pro Durchlauf), solange Platz ist. „Produktion
  stoppen“ stoppt die Annahme. Das Gebäudefenster zeigt den Bestand. Wird sie zerstört, sind die Medien verloren.
- KI: Lua-Werkstätten mit Eingängen baut sie, sobald Eingangswaren im Lager liegen (1 + 1 je 10 Waren, höchstens 3),
  Werkstätten ohne Eingang einmal, Bibliotheken, wenn mehr Medien in Lagern warten als die vorhandenen noch aufnehmen
  (höchstens 5). Werkzeuge bestellt sie über die vorhandene Werkzeug-Logik.
- KI und Regler (Wunsch des Nutzers 2026-10-23): Alle 200 GF (10 Sekunden) stellt die KI den Regler nach den WP ein,
  die ihrer aktuellen Forschung noch fehlen: die am meisten fehlende Art 10, andere fehlende anteilig (mindestens 1),
  nicht benötigte 0. Forscht sie nichts oder fehlen keine WP: Startwerte aus `distribution.lua`. Einstellbar oben in
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

## Kartenobjekte und eigene Grafiken (Lehm)

### Kartenobjekte

`civ/mapobjects.lua` definiert Objekte, die eine Werkstatt einsammelt (`rttr:AddMapObject{ name, title, sizes, look,
tint }`), z. B. `LoamDeposit`. Im Spiel ist das ein `noDeposit` mit Objektindex und Größe (0 = kleinste); jedes
Einsammeln macht es eine Größe kleiner, nach der kleinsten verschwindet es. In Karten stehen sie als Objekttyp
`0xD0 + Index % 32` mit dem Info-Byte `Größe + 32 * (Index / 32)` (bis zu 256 Objekte; Karten mit höchstens 16
Objekten sehen aus wie vorher: Typ `0xD0 + Index`, Info = Größe), nur im Zivilisationsmodus geladen. Kodierung in
`civ/MapCodes.h`. Der Kartengenerator setzt sie über eine Regel in `world/mapgen.lua` (`object = "LoamDeposit"`).

Eine Werkstatt mit `collect = { object = "LoamDeposit", radius = 8 }` ist ein **Sammler** (`nofCustomCollector`, wie
der Steinmetz): Der Arbeiter geht zum nächsten Objekt im Umkreis, arbeitet `workTime` und bringt das Produkt zurück.
Gibt es keins mehr, kommt die Meldung „Kein … mehr im Umkreis“. Die KI baut Sammler in die Nähe der Objekte und
reißt sie ab, wenn nichts mehr da ist (wie Steinbrüche).

### Plantagen (Stand 2026-11-06)

- Kartenobjekte mit `growTime = <Sekunden>` (und optional `plantSize`, Standard `sizes`) können gepflanzt werden.
  Ein gepflanztes `noDeposit` (`noDeposit::CreatePlanted`) beginnt mit Größe 0 und wächst in gleichen Schritten bis
  `plantSize - 1` (Ereignis, gespeichert). Solange es wächst (`IsGrowing`), sammeln und ernten es weder Sammler noch
  Plantagen. Die Größen-Bilder (`Groesse<N>`) zeigen das Wachsen.
- Werkstatt mit `plant = { object = "PapyrusDeposit", radius = 4 }` = **Plantage** (`nofCustomPlanter`, Mischung aus
  Bauer, Förster und Köhler): Mit `output` erntet der Arbeiter ausgewachsene Objekte dieser Art im Umkreis (auch
  natürliche, eine Ware pro Gang, Klasse 1) und pflanzt sonst ein neues (Klasse 2). Ohne `output` pflanzt er nur.
  Die Eingänge (`inputs`, `consume` wie bisher, z. B. Wasser) werden beim Losgehen zum Pflanzen verbraucht (wie beim
  Köhler); Ernten braucht nichts. Wird die Stelle unterwegs unbrauchbar, sind die Waren verloren.
- **Wo gepflanzt wird** (`nofCustomPlanter::CanPlantAt`): freier Platz (nur Deko/Schilder werden entfernt), kein
  Grenzstein, keine Straße, kein Gebäude, keine Baustelle, kein Feld, Köhlerhaufen oder anderes Kartenobjekt
  direkt daneben (damit Wege frei bleiben, da Objekte Wege sperren). Fahnen dürfen daneben stehen wie bei Steinen
  (`BlockingManner::FlagsAround`); sonst bleibt bei den dichten Straßennetzen (vor allem der KI) im Umkreis kaum ein
  Platz (seit 2026-11-11). Das Objekt muss dort **natürlich wachsen** (`GrowsNaturallyAt`): eine Regel in
  `world/mapgen.lua` mit diesem Objekt passt mit `terrain`, `temperature`, `humidity`, `height` und hat dort ein
  Gewicht > 0. `zones`, `amount`, `hqDistance` zählen nicht (Wunsch des Nutzers: kein Zonen-Zwang). Ohne Regel wächst
  es auf jedem Land.
- Die Regeln kommen aus der Weltbeschreibung des Spiels (`world.GetDescription().mapGen.resources`). Der Inhalt des
  Zivilisationsmodus (`loadDefaultTechTree`) wird nur aus `gamedata/civ` geladen und kennt `mapgen.lua` nicht; bis
  2026-11-11 standen die Regeln dort und waren im Spiel leer, gepflanzt wurde deshalb auf jedem Land.
- Animation: Pflanzen wie der Förster bzw. säen wie der Bauer (`worker.look = "Farmer"`), Ernten wie der Sammler
  (zusätzlich Sense für `"Farmer"`). Eigene Bilder `Arbeiter_Pflanzen_<n>`. Beim Ernten keine „Kein … mehr“-Meldung
  (wie Bauer).
- KI: Plantagen und Minen will sie je 1 (wie Werkstätten ohne Eingang, begrenzt durch Arbeiter/Werkzeug).
  Plantage: Platz mit den meisten pflanzbaren Stellen und eigenen Objekten im Umkreis (mindestens 3, nicht neben einer
  gleichen Plantage), `AIPlayerJH::FindPlantationPosition`.

### Klima im Spiel (`civ/Climate.*`)

- Damit der Pflanzfilter Temperatur und Feuchtigkeit kennt, schreibt der Generator das Klima in drei Ebenen der
  .swd, die S2 nicht benutzt (`MapCodes.h`): Ebene 7 = `0x80 | Temperatur`, Ebene 9 = Feuchtigkeit, Ebene 10 =
  Bits Süßwasser/Fluss. S2-Karten haben dort 0 / 7 / 0–1, ohne die Markierung 0x80 gibt es kein Klima.
- `civ::ClimateMap` (in `GameWorldBase`, `GetClimate()`): beim Laden einer Karte im Zivilisationsmodus aus der Datei,
  sonst beim ersten Gebrauch aus dem Terrain. Höhe in Prozent wie im Generator aus den Höhen beim Laden.
  Die Terrain-Merkmale (`land`, `shore_fresh`, …) berechnet dieselbe Funktion wie im Generator
  (`mapGenerator::ComputeTerrainFlags(info, nearWaterDistance)`); ohne Klima (Originalkarten, alte Zufallskarten)
  sind Gewässer bis `lakeMaxSize` Knoten Seen und es gibt keine Flüsse; Klimagrenzen und Gewichte gelten dann nicht.
- Spielstand: Klima, Höhe und Wasser-Bits (wenn vorhanden), die Merkmale werden beim Laden neu berechnet.

### Erze (Bodenschätze)

- `rttr:AddOre{ name = "Copper", title = "Kupfer", look = "Iron", tint = 0xC01080 }` in `civ/mapobjects.lua`.
  `ResourceType` hat 24 Plätze `Custom0…` (wie Waren). `Resource` hat dafür jetzt 16 Bit (Typ im oberen Byte,
  Menge 0–15 wie bisher); alte Spielstände (8 Bit) werden umgerechnet (`Resource::fromLegacyValue`).
- Karten: Ressourcen-Byte = Code des Erzes + Menge 1–7, Codes aus von S2 nicht benutzten Werten (`ORE_MAP_CODES`:
  0x90–0xF8, 0x60–0x78, 0x28–0x38, 0x08–0x18). Ohne Zivilisationsmodus oder ohne Definition: kein Bodenschatz.
- Generator: Regel mit `object = "Copper"` wird mit Kohle, Eisen, Gold und Granit zusammen verteilt
  (`PlaceMineResources`, Anteile nach Mengen); `amount` als Zahl.
- Geologe: findet sie, Schild = eigenes Bild (`<Erz>/Schild0..2`) oder Schild des `look`-Erzes eingefärbt, Meldung
  „<Titel> gefunden“, Lua-Ereignis wie bisher. Bodenschatz-Anzeige (Cheat) mit der Ware des `look`-Erzes.
- **Minen:** Werkstatt mit `size = "mine"` und `mine = { resource = "Copper" }` (`BuildingQuality::Mine`, Reiter
  Minen im Baumenü, `BuildingProperties::IsMine`). Der Arbeiter (`nofCustomWorker`) baut wie `nofMiner` pro Produkt
  eine Einheit im Umkreis `MINER_RADIUS` ab (Addon „Unerschöpfliche Minen“ gilt), sonst „Diese Mine ist erschöpft“,
  die KI reißt sie dann ab. KI-Platz: meiste Erzmenge im Umkreis (`FindCustomMinePosition`).
- Signatur des Spielstands: `R:<Erz>` (anhängen erlaubt, sonst nicht).

### Wissenspunkte in `rttr:AddWorkshop`

`knowledge = { IWP = 2 }` in einer Werkstatt trägt die Punkte wie `rttr:SetBuildingKnowledge` ein. Steht dieselbe
Werkstatt auch in `knowledge.lua`, ist das ein Fehler („defined twice“).

Spielstand-Version 21: 16-Bit-Ressourcen (Knoten, Schilder), Geologe mit Anzahl der Ressourcenarten, wachsende
Objekte, `nofCustomPlanter` (GO_Type), Klima.

### Eigene Grafiken (`civ/CustomGraphics.cpp`)

- Ordner `data/RTTR/assets/civ/<Bezeichner>/`, Dateinamen siehe `data/RTTR/assets/civ/LIESMICH.txt`:
  Ware (`Symbol`, `Fahne`, `Esel`, `Traeger_<Richtung>`), Werkstatt (`Gebaeude`, `Baustelle`, je mit `_Schatten`,
  `Tuer`, `Icon`, je Volk überschreibbar), Arbeiter (`Arbeiter_Symbol`, `Arbeiter_<Richtung>_<0-7>`,
  `Arbeiter_Arbeit_<n>`, Plantagen `Arbeiter_Pflanzen_<n>`), Kartenobjekt (`Groesse<N>` mit `_Schatten`), Erz
  (`Schild0..2`). Landschafts-Varianten mit `_Winter` usw.
- PNG (über `external/stb`, Public Domain/MIT) oder BMP; Ankerpunkt `.nx<X>.ny<Y>` im Namen, sonst an derselben
  relativen Stelle wie im Platzhalter. Pixel in den vier blauen Spielerfarben (PAL5 128–131) werden Spielerfarbe.
- Gelesen in `Loader::fillCaches` (beim Laden eines Spiels). Eigene Bilder füllen die Cache-Plätze der Lua-Typen
  (`building_cache`, `bob_jobs_cache`, `carrier_cache`); leere Plätze fallen auf den Platzhalter (`look`) zurück.
- Vorlagen: Fehlende Dateien in `<Benutzerordner>/civ-vorlagen/<Bezeichner>/` werden aus den Platzhaltern geschrieben
  (Original-S2-Grafik, darum nicht in Git).

## Werkzeugreform (Stand 2026-11-11)

Daten in `data/RTTR/gamedata/civ/tools.lua`. Aktiv, sobald dort mindestens ein Material steht (`Content::HasToolReform`),
sonst gilt das alte System.

### Regeln

- **Berufe ohne Werkzeug:** Im Lager wird aus einem Helfer ohne Werkzeug ein Arbeiter (`nobBaseWarehouse::CanRecruit`,
  `Content::IsRecruitedWithoutTool`). Ausnahmen: Bauarbeiter, Planierer, Geologe, Späher. Kommt ein Arbeiter ins Lager
  zurück, ist er wieder Helfer (`ConvertToHelpers`).
- **Spielstart:** Die Arbeiter im Lager werden zu Helfern, ihre Werkzeuge kommen als Waren ins Lager
  (`civ::startNewGame`, nur bei neuem Spiel).
- **Werkzeugplätze:** Jedes Gebäude hat je Werkzeugtyp einen Platz (`nobUsual::ToolSlot`: Werkzeug in Gebrauch,
  verbleibende Schritte, Ersatz, bestelltes Werkzeug). Höchstens 4 Typen je Gebäude.
- **Werkzeugtyp** = Tag-Abfrage wie bei Eingängen. `"Axe"` trifft die eingebaute Axt (Name-Tag) und jede neue Ware mit
  `kind = "Axe"`.
- **Tempo** in Prozent = `baseSpeed` des Gebäudes + `speed` aller Werkzeuge in Gebrauch. 100 = bisheriges Tempo, 0 = keine
  Arbeit. Im Gebäude werden Warten und Arbeiten skaliert. Bei Arbeitern, die draußen arbeiten (Holzfäller, Bauer …),
  bleibt die Arbeit draußen gleich lang (Animationen, fallende Bäume); die Wartezeit davor gleicht das aus.
- **Abnutzung:** Mit jedem Arbeitsschritt (`nobUsual::BeginWorkStep`, aufgerufen in `nofBuildingWorker::TryToWork`)
  verliert jedes Werkzeug 1 Haltbarkeit. Bei 0 ist es weg, der Ersatz rückt nach.
- **Bestellen:** Das Gebäude bestellt selbst aus den Lagern, sobald es einen Arbeiter hat, dann alle 210 GF und nach
  jedem Schritt (`OrderTools`). Ersatz, wenn höchstens `rttr:SetToolReorder(n)` Prozent Haltbarkeit übrig sind.
  Werkzeuge haben eine niedrige Transportpriorität; bei vollen Straßen dauert die Lieferung.
- **Material:** Regler „Werkzeuge“ je Spieler (`PlayerResearch::GetToolMaterialPriorities`, Befehl
  `gc::SetToolMaterialPriorities`). Bestellt wird das vorrätige Werkzeug mit dem höchsten Wert, bei Gleichstand das
  schnellere; 0 = nie. Startwerte seit 2026-11-12 in `distribution.lua` (`rttr:SetToolMaterialPriorities`).
- **Inventar:** Werkzeuge im Gebäude zählen nicht mehr zum Inventar des Spielers (wie verbrauchte Waren). So stimmen
  die Formeln der KI „Werkzeuge + Arbeiter“ weiter.
- **Abriss:** Der Arbeiter nimmt Werkzeuge mit mindestens halber Haltbarkeit und den Ersatz mit ins Lager
  (`nofBuildingWorker::TakeTools`). Abgenutztere gehen verloren. Ohne Arbeiter im Gebäude gehen alle verloren.
- **Fenster:** Gebäudefenster zeigt links die Werkzeuge mit Haltbarkeitsbalken, „+1“ für Ersatz, „...“ für bestellt und
  rechts das Tempo. Verteilungsfenster hat den Reiter „Werkzeuge“.
- **KI:** Stellt zuerst die Werkzeuge her, die ihren Gebäuden fehlen (`AdjustSettings`), und gibt der Schlosserei so
  lange Eisen und Bretter vor der Waffenschmiede. Der Bauplaner zählt Spitzhacken im Lager als mögliche Bergleute und
  Steinmetze. Seit 2026-11-12 zählt sie alle Werkzeuge eines Typs (auch Kupferäxte) und stellt die Werkzeug-Regler
  auch ein, wenn sie nur eine Werkstatt mit `outputChoice = "tools"` hat. Diese baut sie, sobald alle Eingänge im
  Lager sind: 1, dazu 1 je 20 Waren des knappsten Eingangs, höchstens 3. Mögliche Arbeiter einer Lua-Werkstatt unter
  der Reform: Helfer, wenn sie ohne Werkzeug arbeitet, sonst die vorrätigen Werkzeuge ihres ersten Platzes.
- **Spielstand:** Version 22. Ältere Spielstände im Zivilisationsmodus laden; die Gebäude haben dann keine Werkzeuge und
  bestellen sie.

### Startwerte in `tools.lua` (von Claude gesetzt, der Nutzer balanciert)

- Material Eisen, alle 12 Werkzeuge: Haltbarkeit 150 Schritte, Tempo +100. Vorrang in `distribution.lua`.
- `baseSpeed` 0 für alle Gebäude mit Werkzeug, außer der Kette, die neue Werkzeuge ermöglicht: Holzfäller, Sägewerk,
  Steinbruch, alle Minen und Eisenschmelze 25, Schlosserei 50. Sonst gäbe es nach dem Bruch des letzten Werkzeugs nie
  wieder welche.
- Ersatz bei 10 % Resthaltbarkeit.

## Welt: Kartengenerator mit Klima und Zonen (Stufe 1)

Stand 2026-10-27 (Überarbeitung nach Test des Nutzers). Gilt nur für **erzeugte Karten** (Zufallskarte im
Kartenauswahlmenü).

### Ablauf der Erzeugung (`mapGenerator/RandomMap.cpp`)

1. **Klima zuerst:** Temperatur und Feuchtigkeit sind **normalverteilt** um den Mittelwert der Voreinstellung
   (Gemäßigt = 50) mit der Streuung `temperatureSpread`/`humiditySpread`: der größte Teil der Karte ist gemäßigt,
   Extreme sind selten. Das Klima entsteht vor dem Land, weil es bestimmt, wo es weniger Gebirge gibt.
   **Wasser, Land, Gebirge wie im Online-Generator** (`Landscape.cpp`, `CreateLandMasses`): ein fraktales Rauschen
   (wiederholt sich an den Kartenrändern, Zeilen halb so hoch wie Spalten wie im Spiel) wird nach Rang geschnitten:
   die untersten `Wasser %` werden Wasser, die obersten `Gebirge %` des Landes Gebirge. Landmasse bestimmt die Größe
   der Formen (Inseln klein, Kontinente groß), Oberfläche die Feinheit der Küsten und Bergränder.
   `mountainWeights` (Faktor je Temperatur-/Feuchtebereich, `GetMountainFactors`) verringert den Gebirgsanteil dort,
   z. B. halb so viel Gebirge im feucht-warmen Gebiet (Konzept: Minenplätze von Vegetation verdeckt). Der Faktor wird
   über ein paar Knoten geglättet, Bergketten werden also dünner statt an einer Linie abzubrechen. Er nimmt nur
   Gebirge weg; Wasser und das übrige Land bleiben unverändert, der Regler gilt dort also nur verringert. Mit dem
   Gebirge gibt es dort auch weniger Bauflächen und Minen; die Erzmenge je Gebirgsknoten ist überall gleich.
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
   würden sich die Wasserflächen nur an einer Ecke berühren = „Pfützen“). In einer Biegung mit zwei Drehungen in
   dieselbe Richtung würde das äußere Dreieck des mittleren Schritts abstehen (berührt nur seinen Partner); deshalb
   wird nur ein Dreieck gesetzt, das an den vorigen oder nächsten Schritt (an der Mündung: an Wasser) grenzt.
   Der Lauf ist der kürzeste solche Weg in
   einem Korridor von 3 Knoten um den Abflussweg (bergauf teuer, etwas Zufall für natürliche Biegungen). Keine Furten:
   Flüsse trennen das Land wie im Original. Das Flussbett wird gegraben (flussabwärts nie höher), die Ufer bekommen
   keinen Sand und keinen Sumpf. Kürzere als `riverMinLength` Schritte entfallen. Regler „Flüsse“: bei 100 % etwa ein
   Fluss je 2000 Knoten.
4. **Klima nach Höhe und Wasser:** Mit der Höhe wird es kälter (`heightCooling`). An Seen und Flüssen steigt die
   Feuchtigkeit um `waterHumidity` Punkte, alle `waterHumidityStep` Knoten Abstand um `waterHumidityFalloff` Punkte
   weniger, so weit, bis nichts mehr übrig ist (nicht am Meer). Die Abnahme muss größer als 0 sein. Seit 2026-11-10
   begrenzt `nearWaterDistance` das nicht mehr; es gilt nur für die Gelände „near_fresh“, „near_salt“, „near_river“.
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
rttr:SetMapClimate{ heightCooling = 30, temperatureSpread = 20, humiditySpread = 20, nearWaterDistance = 5,
                    lakeMaxSize = 50, waterHumidity = 50, waterHumidityFalloff = 10, waterHumidityStep = 1,
                    riverMinHeight = 30, riverMinLength = 15, mountainBuildSpacing = 6, previewImages = true,
                    mountainWeights = { { temperature = {60, 100}, humidity = {60, 100}, factor = 0.5 } } }
rttr:AddMapZone{ name = "Afrika", title = "Afrika", color = 0xE08030, packages = {
    { name = "regenwald", temperature = {65, 100}, humidity = {55, 100}, height = {0, 40},
      trees = { Palm1 = 2, Pineapple = 1 }, animals = { Deer = 2, Duck = 1 } },
    { trees = { Oak = 1 } } -- ohne Bereiche: gilt überall sonst
} }
rttr:AddMapResource{ name = "Steinhaufen", object = "stones", amount = "stonePiles",
    terrain = { "land", "mountain" }, cluster = 8, density = 80,
    weights = { { temperature = {60, 100}, humidity = {60, 100}, factor = 0.3 } } }
```

- Objekte: `trees`, `stones`, `animals`, `decoration`, `fish`, `groundwater`, `coal`, `iron`, `gold`, `granite`,
  dazu Kartenobjekte (`rttr:AddMapObject`) und Erze (`rttr:AddOre`) des Zivilisationsmodus.
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

- Neue Pflanzen und Tiere (Mahagoni, Schimpanse …) brauchen erst eigene Kartenobjekte und ein Speicherformat
  (das S2-Format kennt nur 9 Baumarten und 6 Tierarten). Bis dahin stehen vorhandene Arten in den Paketen. Lehm und
  Papyrus nutzen schon die Kartenobjekte des Zivilisationsmodus, Kupfer die Erze (siehe oben).
- Die Zonen werden nicht in der Karte gespeichert (Plantagen ignorieren sie ohnehin), Temperatur, Feuchtigkeit und
  Süßwasser/Flüsse schon (siehe „Klima im Spiel“).
- Eisbären lassen sich im Kartenformat nicht speichern.
- Originalkarten werden nicht verändert.

## Militär (Konzept, Stand 2026-11-07)

Im Zivilisationsmodus wird das Militärsystem von S2 ersetzt. Ziel: ein Militär, das auf der Karte präsent ist, auch
außerhalb der Grenzen handelt, Versorgung braucht und verschiedene Einheitentypen hat, ohne RTS-Steuerung einzelner
Einheiten. Noch nichts umgesetzt.

### Entscheidungen

| Datum      | Entscheidung |
|------------|--------------|
| 2026-11-07 | Das alte Militärsystem wird im Zivilisationsmodus **komplett ersetzt** (Angriffsfenster, Militäreinstellungen, Verteidiger aus Nachbargebäuden, Seeangriffe, KI-Angriffslogik). Es bleibt nur: Soldaten besetzen Militärgebäude und halten so das Gebiet. |
| 2026-11-07 | **Verbände:** Gruppen von Soldaten, sichtbar auf der Karte, beliebig groß (die Obergrenze ergibt sich aus der Versorgung). Man steuert sie über **Aufträge**, nicht über einzelne Einheiten. Drei Arten: **einfacher Trupp** (von Anfang an), **stehendes Heer** (erst mit der Forschung zum Armeelager, Ära 2), **Miliz** (mit den Wohnhäusern). |
| 2026-11-07 | **Deaktivieren:** Beim einfachen Trupp heißt Deaktivieren Auflösen, Ränge und Erfahrung gehen verloren. Das stehende Heer kann man deaktivieren: Es behält seine Ränge und **trainiert**, solange es deaktiviert ist. Durch Training sammelt es Erfahrung, die Soldaten steigen im Rang auf. Erfahrung gibt es auch nach Kämpfen. |
| 2026-11-07 | **Verbandsfenster** (Knopf in der Leiste unten am Bildschirmrand), das zentrale Fenster für alle Verbände: neue Verbände erstellen, Art (einfacher Trupp, stehendes Heer, Miliz) und Zusammensetzung aus Einheitentypen festlegen (auch bei bestehenden Verbänden), Liste aller Verbände, Auflösen, Aktivieren/Deaktivieren, Kamera zum Verband, Verband auswählen und ihm Befehle geben. Aus welchen Gebäuden Leute und Ausrüstung kommen, steuert man nicht; das ergibt sich daraus, was wo verfügbar ist. |
| 2026-11-07 | **Befehlsverzögerung:** Ein Befehl wirkt erst nach einer Wartezeit, die mit dem Abstand des Verbands zur eigenen Gebietsgrenze wächst (Timer; im eigenen Gebiet keine Verzögerung). Was der Verband tut, sieht man live. Befehle, die unterwegs sind, lassen sich nicht abbrechen. |
| 2026-11-07 | **Kampf** nur zwischen Verbänden (auch mehrere gegen einen), gleichzeitig auf der Karte, kein eigener Schlachtbildschirm. Das Ergebnis hängt an Stats und Position: Wald, Fluss, Höhe, Befestigung, Flanke, Hinterhalt. Die **Formation** ist wählbar und verändert die Stats. |
| 2026-11-07 | **Befestigungsbonus** gibt es nur für die Soldaten im bzw. auf dem Militärgebäude, nicht im Umkreis. Das ist die Garnison: Soldaten im Gebäude gehören zu keinem Verband, ein Verband betritt kein Militärgebäude. Später gilt das genauso für die besetzbaren Mauern. |
| 2026-11-07 | **Moral** ergibt sich aus Versorgung, eigener Truppenstärke und Stärke im Vergleich zum Gegner. Ein Gefecht endet, wenn die Moral einer Seite unter eine Schwelle fällt, nicht erst, wenn alle tot sind; der Verband zieht sich dann zurück, wenn er kann. Bei Versorgungsmangel sinkt die Moral und die Soldaten heilen nicht. |
| 2026-11-07 | Kann ein geschlagener Verband sich nicht zurückziehen, kämpft er bis zum Ende oder gibt auf. Was davon, hängt von der **Kultur** ab (siehe „Notizen für Kulturtraits“). |
| 2026-11-07 | **Deserteure:** Bleibt die Moral lange unter einer tieferen Schwelle, desertiert der ganze Verband und führt ein Eigenleben: Deserteure ziehen umher, plündern, greifen alle an und laufen evtl. zu einem anderen Spieler über. Zurückgewinnen kann man sie nicht. |
| 2026-11-07 | **Versorgung** (Nahrung, Munition, Ausrüstung usw.) läuft nie von selbst. Ein Verband versorgt sich mit dem, was gerade da ist: Lager, Kartenobjekte (z. B. Beerenbüsche, Wild), Plünderung, Nachschub. Steht er **direkt an der Flagge** eines eigenen Lagers (Lagerhaus, HQ, Hafen), nimmt er sich den Bedarf direkt aus dem Bestand. Befehlseinstellung: „Vorrat reicht nur noch für X Minuten → zum nächsten Lager“. Wird der Vorrat knapp, kommt immer eine Nachricht. |
| 2026-11-07 | **Versorgungsposten** schicken Karawanen (sichtbare, einfache Figuren) zu den Verbänden. Im Versorgungsposten wählt man aus, welche Verbände er versorgt; ein Posten kann mehrere Verbände versorgen, ein Verband kann von mehreren Linien versorgt werden. Das läuft automatisch, die Routen lassen sich über Wegpunkte nachsteuern. Versorgungslinien gibt es erst nach den Lasttieren (`pack_animals`). |
| 2026-11-07 | **Armeelager** (Forschung in Ära 2, mit ihr kommt auch das stehende Heer): Unterkunft für ein stehendes Heer, solange es inaktiv ist; ein Verband im Lager wird direkt versorgt. |
| 2026-11-07 | Auftrag **„Zurück“** ohne Armeelager: Der Verband geht zum nächsten eigenen Lagerhaus, HQ oder Hafen und steht dort bereit. |
| 2026-11-07 | **Auflösen:** Der Verband geht zum nächsten Lagerhaus, legt seine Ausrüstung ab und wird wieder zu Helfern (Miliz: zurück in die Häuser und Berufe). Der Rang geht dabei verloren. |
| 2026-11-07 | **Eroberung** (vorerst): Ein Verband greift ein Militärgebäude direkt an, nach der Übernahme bleibt **1 Soldat** im Gebäude. Mauern und Belagerung kommen später, das Katapult wird dafür umgebaut. |
| 2026-11-07 | **Miliz:** Ausrüstungsplatz in den Wohnhäusern, Einberufen wie in Manor Lords, jeder Milizverband hat ein eigenes Milizlager. |
| 2026-11-07 | **Feldlager** außerhalb der Grenze dienen als Befestigung und evtl. als Kolonie, nicht zur Versorgung. |
| 2026-11-07 | **Schiffe:** Zivile Schiffe bleiben wie bisher. **Kampfschiffe** sind Verbände auf dem Wasser, führen Seekämpfe und nehmen Landverbände auf (Betreten und Verlassen wie in Siedler 4, an jeder Küste). Typen z. B. Kriegsschiff (stark im Seekampf), Landungsboot (schwach, viel Platz für Truppen) und Zwischenstufen. Keine Versorgungslinie, dafür ein großer Vorrat an Bord, aufgefüllt im Hafen. Das Schiff selbst braucht keine Versorgung, nur die Truppen an Bord; ein Schiff ohne Truppen verbraucht nichts. |

### Technische Auslegung (Vorschlag)

- Ein Verband ist **ein Kartenobjekt**, das von Knoten zu Knoten zieht und seine Soldaten in Formation zeichnet. Die
  Soldaten (Typ, Rang, Lebenspunkte, Ausrüstung) sind Daten des Verbands.
- Befehle sind GameCommands. Die Verzögerung wird beim Erteilen aus dem Abstand zum nächsten eigenen Gebietsknoten
  berechnet: **20 GF pro Knoten**, so lange wie ein Läufer für die Strecke braucht (10 Knoten ≈ 10 s bei normaler
  Geschwindigkeit). Befehle treffen in der Reihenfolge ein, in der sie erteilt wurden.
- Drei Ebenen: **Auftrag** (verzögert, als Liste), **Haltung** (aggressiv, standhaft, ausweichen, verborgen),
  **Regeln** (wirken sofort, z. B. Rückzug ab Moral, Verfolgungsweite, Vorrat für weniger als X Minuten → zum Lager).
- Aufträge: Marschieren (Wegpunkte), Halten (Verschanzung wächst mit der Zeit), Patrouille, Erkunden, Angreifen
  (Verband, Gebäude, Gebiet), Gebiet verteidigen, Geleitschutz, Feldlager, Zurück, Auffüllen, Vereinen/Teilen, Auflösen,
  Ein-/Ausschiffen; später Belagern, Plündern.
- Kampftakt etwa 1 s: Abstand ≤ Reichweite → Fernkampf (verbraucht Munition, Höhe gibt Reichweite und Schaden, Wald
  gibt Deckung); Kontakt → Nahkampf, die Breite der Formation bestimmt, wie viele gleichzeitig kämpfen. Schaden trifft
  die Seite, von der er kommt (Blickrichtung, 6 Richtungen). Ob es zum Nahkampf kommt, entscheidet die Haltung des
  Beschossenen. Wasser dazwischen → nur Fernkampf.
- Weil nur Verbände kämpfen, tritt die Garnison eines angegriffenen Militärgebäudes im Gefecht wie ein feststehender
  Verband mit dem Befestigungsbonus an. Im Verbandsfenster erscheint sie nicht. Ihre Größe ist durch das Gebäude begrenzt
  (`NUM_TROOPS`: Baracke 2, Wachstube 3, Wachturm 6, Festung 9).
- Gerechnet wird nur mit Ganzzahlen, damit MSVC und GCC im Mehrspieler dasselbe Ergebnis haben.
- Einheitentypen, Formationen, Geländewerte und der Bedarf je Einheitentyp kommen aus Lua.
- Karawanen nach dem Vorbild von `nofTradeLeader`/`nofTradeDonkey`, die schon heute frei über die Karte laufen.
- Schiffe fahren in RttR heute nur Hafenplätze an. Für Landungen an jeder Küste braucht es eine eigene Wegfindung auf
  dem Wasser.

### Notizen für Kulturtraits

- Verhalten eines geschlagenen Verbands, der sich nicht zurückziehen kann: bis zum Ende kämpfen oder aufgeben.

### Reihenfolge (Nutzer, 2026-11-07)

1. **M1:** Verbandsfenster, Verbände (einfacher Trupp) gründen, auf der Karte bewegen, Aufträge, Befehlsverzögerung,
   KI. Ohne Armeelager und Eroberung.
2. **M3:** Einheitentypen und Ausrüstung über Lua und den Techtree. Die Kampfwerte werden hier angelegt und ab M2
   benutzt.
3. **M4:** Versorgung (Vorrat, Lager, Kartenobjekte, Versorgungsposten, Karawanen), Armeelager und stehendes Heer.
4. **M2:** Gefechte (Gelände, Flanke, Formation, Moral, Fernkampf, schießende Garnisonen) und Eroberung.
5. **M5:** restliche Aufträge.
6. **M6:** Kampfschiffe.

Bis M2 fertig ist, bleibt das alte Militärsystem im Modus aktiv, damit es zwischendurch Krieg gibt und `ai-battle`
testbar bleibt; mit M2 wird es im Modus abgeschaltet (Vorschlag Claude, 2026-11-07).

Später: Miliz (mit den Wohnhäusern), Feldlager und Kolonien, Mauern, Belagerung.

## Geld und Finanzen (Konzept, Stand 2026-11-08)

Geld bekommt in S2 erst Sinn, wenn es Gegenüber mit eigenen Interessen gibt: Haushalte, Soldaten, Händler, andere
Spieler. Das Konzept hängt deshalb eng an den Wohnhäusern (Bedürfnisse wie in Anno, siehe Konzept des Nutzers) und am
Militär (Sold). Noch nichts umgesetzt.

### Entscheidungen

| Datum      | Entscheidung |
|------------|--------------|
| 2026-11-08 | **Marktmodell:** Zwischen Wohnhäusern und Waren steht ein **Markt**. Wohnhäuser bestellen nicht selbst; liegt ein Haus an keinem Markt, wird es nicht versorgt. Jedes Haus gehört immer zum **nächsten Markt**. Der Markt bestellt den Bedarf seiner Häuser per Träger. |
| 2026-11-08 | **Marktgänger:** Jedes Haus schickt regelmäßig einen Marktgänger zum Markt. Er läuft **auf den Straßen** (wie ein Soldat auf dem Weg zum Militärgebäude), bezahlt am Markt mit Münzen oder Tauschware und trägt die Ware heim. |
| 2026-11-08 | **Geld ist physisch:** Die Staatskasse sind die Münzen in den Lagern (plünderbar). Zwischen Gebäuden reist Geld als Ware (Geldkisten zwischen Markt und Lager, Sold per Karawane), innerhalb des Marktumkreises trägt es der Marktgänger. Haushalte haben ein eigenes Vermögen. (Vorschlag Claude, vom Nutzer übernommen.) |
| 2026-11-08 | **Tausch** ist der Normalfall ohne Münzen (Ära 0, Münzmangel): Ware gegen Ware im Verhältnis ihrer Werte. Dieselbe Regel gilt mit fremden Händlern und anderen Spielern. (Vorschlag Claude, vom Nutzer übernommen.) |
| 2026-11-08 | **Preise** und alles andere Finanzielle werden **automatisch** berechnet. Die Hebel des Spielers: Gebäude bauen (damit formt er die Wirtschaft) und der **Steuersatz**. |
| 2026-11-08 | **Lohn pro Arbeitsplatz**, berechnet als **Lohn pro produzierter Ware**: Ein schlecht versorgter Betrieb macht wenig Umsatz, seine Arbeiter können sich weniger leisten → Unzufriedenheit, geringeres Wachstum. Lohn pro Haus ist verworfen. |
| 2026-11-08 | Lohn nach **Wertschöpfung** (Stufe 2, siehe unten). Einen Arbeitsmarkt (Stufe 3) gibt es nicht, er passt nicht zu Siedler 2. |
| 2026-11-08 | **Luxusbedürfnisse** sind der Anreiz, Wohnhäuser zu verbessern: Sie holen Geld in die Staatskasse zurück. |
| 2026-11-08 | **Heimarbeit** gibt es nur über spezielle Forschungen und Hausupgrades, nicht als Standard. Welche Waren (historisch korrekt), recherchiert der Nutzer; Vorschläge siehe unten. |
| 2026-11-08 | **Selbstversorgung:** Manche Gebäude decken Bedürfnisse aus eigener Produktion, z. B. der Bäcker mit Wohnstube sein Brot oder der Bauernhof mit seinen Erträgen. Diese Bedürfnisse kaufen sie nicht am Markt. |
| 2026-11-08 | **Zuhause:** Jeder Arbeiter wohnt in einem Haus. Häuser sind auf **Berufsgruppen** beschränkt (ein Minenarbeiter lebt nicht in einer Luxusvilla). Ein Arbeiter ohne freies Bett wird trotzdem einem Haus zugeordnet, weil er Bedürfnisse hat. |
| 2026-11-08 | **Pendeln** wird nur gerechnet, nicht als Figur gezeigt. Während der Arbeiter pendelt, zeigt das Arbeitsgebäude eine Info. |

### Technische Auslegung (Vorschlag)

- Kassen sind Zähler in Haus, Markt und Lager (in Talern). Eine Münz-Ware steht für einen Beutel mit mehreren Talern, sonst
  müsste die Münzerei Tausende Münzen prägen. Hat ein Markt zu viel Geld, schickt er eine Geldkiste ins Lager; hat er zu
  wenig, bestellt er eine.
- Der Lohn wird beim Markt des Wohnhauses gutgeschrieben, der Marktgänger holt ihn beim nächsten Besuch ab. Steuern werden
  beim Marktbesuch fällig (Münzen oder Naturalien).
- Jeder Markt berechnet alle paar Sekunden seine eigenen Preise je Ware aus der Nachfrage seiner Häuser und der
  Verfügbarkeit der Ware. Sie gleiten zum Gleichgewicht und bleiben in einem Rahmen um den Grundpreis aus Lua.
  Gerechnet wird nur mit Ganzzahlen.
- Lohn = (Preis der Ware − Preis der verbrauchten Eingänge) × Lohnanteil (Wertschöpfung, entschieden). Verworfen:
  reiner Stücklohn ohne Eingänge und ein Arbeitsmarkt je Berufsgruppe.
- Der Lohn wird über einige Minuten geglättet und Haushalte haben Ersparnisse, damit nicht jeder Stau sofort das
  Wachstum trifft. Berufe ohne Ware (Förster, Geologe, Späher, Bibliothekar) bekommen einen Zeitlohn aus dem
  Durchschnittslohn.
- Wohnort als Verweis Arbeiter → Haus, vergeben bei Arbeitsantritt (nächstes Haus der passenden Berufsgruppe), neu bei
  Bau oder Abriss eines Hauses. Die Pendelzeit ergibt sich aus der Weglänge über die Straßen. Der Weg wird einmal
  berechnet und gespeichert und nur neu berechnet, wenn sich das Straßennetz geändert hat (Zähler). Hintergrund: Eine
  Figur auf Straßen rechnet heute an jeder Flagge `FindHumanPathOnRoads` neu (`noFigure::WalkToGoal`).
- Selbstversorgung in Lua: welche Ware welches Bedürfnis deckt und wie viel; der Eigenverbrauch wird vor der Flagge
  abgezweigt. Getreide ist heute keine Nahrung.
- Der Markt bestellt wie die Bibliothek nach Tags, ohne Grenze von 3 Eingängen.
- Geld fließt nur über Marktkäufe und Steuern in die Staatskasse zurück. Sparen die Haushalte, fehlt Geld im Umlauf →
  Nachprägen → steigende Preise. Luxus und Steuern sind die Rückflüsse.

### Heimarbeit: Vorschläge von Claude (vom Nutzer zu prüfen)

- Spinnen: Wolle, Flachs oder Baumwolle → Garn
- Weben: Garn → Tuch (Verlagssystem, ab dem Spätmittelalter)
- Korbflechten: Weide oder Schilf → Körbe
- Töpfern: Ton → einfache Gefäße
- Hausbrauen: Getreide → Bier (im Mittelalter oft Arbeit der Frauen im Haushalt)
- Schnitzen: Holz → Holzgeschirr; Knochen → Knochenschmuck
- Kerzenziehen: Talg → Kerzen (Talg als Nebenprodukt der Fleischerei)
- Nähen: Tuch → Kleidung
- Strohflechten: Stroh → Matten (Stroh als Nebenprodukt des Bauernhofs)
- Später: Klöppeln (Spitze, ab dem 16. Jh.), Nagelschmieden (17./18. Jh.)

### Träger

Siehe Abschnitt „Transport und Logistik“ (Stationsträger, keine Fron, keine Dauerträger).

## Transport und Logistik (Konzept, Stand 2026-11-09)

Im Zivilisationsmodus ersetzt ein System mit mehreren Transportarten den S2-Träger, der dauerhaft auf jedem
Straßenabschnitt steht. Noch nichts umgesetzt.

### Entscheidungen

| Datum      | Entscheidung |
|------------|--------------|
| 2026-11-09 | **Mehrere Transportarten nebeneinander.** Für jede Lieferung wählt das Spiel automatisch die günstigste (geschätzte Zeit plus Frachtkosten). Der Spieler baut Gebäude, Straßen, Stationen und Linien; den Rest regelt das Spiel. |
| 2026-11-09 | **Nahverkehr: Der Käufer holt.** Liegt die Quelle nah, holt ein Arbeiter des bestellenden Gebäudes die Ware selbst über die Straßen (der Bäcker das Mehl beim Müller, der Schmied das Eisen im Lager um die Ecke). Startwert für „nah“: **20 Knoten** (wird noch gebalanct). |
| 2026-11-09 | **Mehrere Arbeiter pro Gebäude** mit verschiedenen Aufgaben, z. B. ein Träger (braucht kein Werkzeug, erhöht die Produktion, wenn er gerade nicht tragen muss) und ein Arbeiter mit Werkzeug, der den eigentlichen Beruf ausübt. |
| 2026-11-09 | **Straßenverkehr über Trägerstationen** (Trägerhaus, später Poststation): Die Träger wohnen in der Station und warten dort. Liegt eine Ware an einer Flagge im Einzugsgebiet, schickt die Station einen freien Träger; er trägt sie über mehrere Abschnitte bis zum Ziel oder zur nächsten Station und kehrt zurück. Bezahlt pro Ware und Strecke. |
| 2026-11-09 | **Die bisherigen Dauerträger** (ein fester Träger pro Straßenabschnitt) **entfallen** im Zivilisationsmodus. **Fron** gibt es nicht. |
| 2026-11-09 | Zum Spielstart braucht es keine Trägerstationen, weil es noch keine weiten Strecken gibt. |
| 2026-11-09 | **Lagerhäuser haben Träger als Arbeiter.** Sie sammeln alle herrenlosen Waren ein und beliefern die Baustellen mit Baumaterial. |
| 2026-11-09 | **Fernverkehr über Umschlagplätze** (Karawanserei, Packstation, Anlegestelle, Bahnhof), die wie Häfen arbeiten: Waren für ein Ziel werden gesammelt, ein Fahrzeug bringt sie gebündelt zur Gegenstation, von dort geht es im Nah- oder Straßenverkehr weiter. Fahrzeuge je nach Forschung, Gelände und Tieren der Zone: Eselskarawane, Ochsenkarren (befestigte Straße), Kamelkarawane (Wüste, auch ohne Straße), Lamas (Gebirge), Flussboot, Schiff, Eisenbahn. Tiere müssen gezüchtet oder in ihrer Zone gefangen werden und fressen. |
| 2026-11-09 | **Keine Händler** als eigene Figuren. **Jeder Markt berechnet seine eigenen Preise** aus der Nachfrage (den Bedürfnissen seiner Häuser) und der Verfügbarkeit der Waren. Preisregionen gibt es nicht. |

### Technische Auslegung (Vorschlag)

- Die Wegsuche kennt heute schon Schiffsetappen zwischen Häfen (`RoadPathDirection::Ship`). Dieses Prinzip wird um weitere
  Etappenarten erweitert: Selbstabholung, Stationsträger, Linie zwischen Umschlagplätzen.
- Nahverkehr: Bis zur Flagge vor dem Gebäude trägt der Arbeiter seine Ware heute schon selbst (`CarryoutWare` in
  `nofBuildingWorker`). Neu läuft der Abholer über die Straßen bis zur Quelle und zurück. Die Höchstentfernung (Start
  20 Knoten) steht in Lua und kann durch Forschungen steigen. Hat das Gebäude keinen eigenen Träger, holt der Arbeiter
  selbst und der Betrieb steht so lange.
- Lagerhausträger: Heute tragen Lagerarbeiter (`nofWarehouseWorker`) Waren nur zwischen Lager und Flagge. Neu laufen
  sie über die Straßen, holen herrenlose Waren (Waren ohne Abnehmer, z. B. Produkte, die niemand bestellt hat) ins Lager
  und bringen Bretter, Steine usw. zu den Baustellen.
- Mehrere Arbeiter pro Gebäude in Lua, z. B. `workers = { { job = "Baker", tool = "Rollingpin" }, { job = "Helper",
  role = "carrier" } }`. Heute hat ein Gebäude genau einen Arbeiter (`nobUsual`).
- Trägerstationen: Straßen im Einzugsgebiet fordern keinen festen Träger mehr an; eine Ware an einer Flagge meldet sich
  bei der nächsten Station. Die Suche nach einem freien Träger ist billig, weil es wenige Stationen gibt.
- Umschlagplätze und Fahrzeuge nach dem Vorbild von Häfen und Schiffen sowie `nofTradeLeader`/`nofTradeDonkey`.
  Fahrzeugtypen in Lua: Kapazität, Tempo, erlaubtes Gelände und Straßenart, benötigtes Tier bzw. Ware, Unterhalt,
  Technologie.
- Frachtkosten fließen in den Preis am Zielort; damit ist Transport Teil der Wertschöpfung.
- Wege werden gespeichert wie beim Pendeln und nur bei geändertem Straßennetz neu berechnet.
- Vorgeschlagene Reihenfolge: Nahverkehr → Umschlagplätze mit Eselskarawane → Trägerstationen → Tiere je Zone und
  Straßenarten → Flussboote → Eisenbahn.

### Historischer Hintergrund (Recherche 2026-11-08)

- Kurze Wege trugen Erzeuger oder Käufer selbst: Der Bauer brachte sein Getreide zum Markt und zur Mühle, der Bäcker kaufte
  Getreide, ließ es mahlen und zahlte dem Müller einen Mahllohn.
- Fernhandel organisierten Kaufleute und Verleger; sie heuerten bezahlte Transporteure an: Schiffer, Fuhrleute, Säumer
  (Genossenschaften mit Monopol auf ihrem Wegabschnitt, Umladen in Susten), Kameltreiber, in Mesoamerika Berufsträger in
  Staffeln, in Indien die Banjaras mit Ochsenkarawanen.
- Strategische Versorgung organisierte der Staat: Annona in Rom, Caoyun auf dem chinesischen Kaiserkanal, die Speicher der
  Inka, die Proviantwidmung für den steirischen Erzberg.
- Landtransport war sehr teuer: Weizen 100 römische Meilen über Land kostete etwa 55 % seines Werts, über See etwa 1,3 %
  (nach Duncan-Jones).

## Gebiet und Klima im Spiel (Notizen des Nutzers, 2026-11-09)

- **Gebietsabhängige Bedürfnisse:** In kalten Gebieten hat jedes Haus das Bedürfnis nach Wärme, und Verbände brauchen
  besondere Versorgungsgüter (z. B. Pelze oder Feuerholz).
- **Gebietsabhängige Laufgeschwindigkeit** für Siedler und Verbände, veränderbar durch besondere Einheiten oder
  Technologien (z. B. Kamele sind in der Wüste schneller, Schlitten in schneereichen Gebieten).
- Technischer Anknüpfungspunkt: Temperatur und Feuchte je Knoten liefert schon `civ::ClimateMap`
  (`GameWorldBase::GetClimate()`); Laufen kostet heute fest 20 GF pro Knoten (`noFigure::StartMoving(dir, 20)`).

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
   WP-Werte …), Kulturen als exklusive Technologien mit stapelnden Traits. Ideen für Traits sammelt u. a. der Abschnitt
   „Militär“ („Notizen für Kulturtraits“).
6. **Emblematische Inhalte** – Kulturgebäude und -soldaten (Grafik der RttR-Völker wiederverwenden).
7. **Gesellschaft** – Civics, Stabilität, Einfluss; Wunder.
8. **Militär** (siehe Abschnitt „Militär“, Reihenfolge M1–M6), **Religion, Diplomatie, Ereignisse**; die KI wird in
   jeder Phase mitgezogen.

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
- Militär: Bleibt die Beförderung mit Goldmünzen aus S2 für die Garnisonen, oder steigen Soldaten nur noch durch
  Erfahrung (Training, Kämpfe) im Rang auf?
- Geld: Welche Folgen hat Überbelegung eines Hauses (Arbeiter ohne freies Bett)?
- Geld: Was passiert, wenn es für eine Berufsgruppe gar kein passendes Haus gibt?
- Transport: Welche Forschung schaltet die Trägerstationen frei?
- Geld: Wofür gilt der Steuersatz (pro Bewohner, Anteil am Lohn, auf Marktkäufe)?

## Neue KI (Konzept, Stand 2026-11-12)

Noch nichts umgesetzt. Ziel: Die KI hat kein Wissen über einzelne Gebäude oder Waren im Code. Sie leitet ihre Wirtschaft
aus den Lua-Daten her und liest aus der Karte ab, was es gibt. Neue Rohstoffe und Warenketten funktionieren ohne
Code-Änderung. Die bisherige KI (AIJH) hat feste Regeln („12 Holzfäller“, „Schlosserei ab …“) und versteht fehlende
Rohstoffe nicht.

### Entscheidungen des Nutzers

- Die KI darf die ganze Karte kennen (kein Nebel, keine Erkundung nötig).
- Keine festen Persönlichkeiten: Die Ausrichtung ergibt sich nur aus Karte und Lage.
- Schwierigkeitsgrade ohne Boni, nur über die Qualität der Planung (z. B. wie oft und wie weit sie plant, Reaktionszeit,
  ob sie Engpässe gezielt sucht).
- Alternativen stehen in den Daten: Baukosten und Eingänge werden über abstrakte Tags definiert (z. B. „Mauerstein“, auf
  den Steine, Lehmziegel und Stampflehm passen). Nur solche Alternativen kann die KI wählen.

### Schichten

1. **Wahrnehmung (was gibt es wo):** Karte in Regionen (z. B. Raster ~10×10 Knoten). Je Region ein Ressourcenprofil:
   Bäume, Steinhaufen, Kartenobjekte aus Lua (Lehm …), Erze im Boden, Fisch, Wasser, Klima/Zone, Bauplätze nach Größe.
   Die KI kennt die ganze Karte, braucht also keinen Geologen und keine Späher für ihr Wissen.
2. **Wirtschaftsmodell (was ist machbar):** Beim Spielstart ein Produktionsgraph aus `civ::Content` und den eingebauten
   Daten: Ware ← Gebäude ← Eingänge/Werkzeuge ← Rohstoffquelle (Kartenobjekt, Erz, Feld …), dazu Baukosten und die
   Technologie, die das Gebäude freischaltet. Jede Kante hat eine Rate (Waren pro Minute und Gebäude aus Arbeitszeit,
   `outputRatio`, Tempo, Werkzeugverschleiß). Abgeleitet: welche Waren auf dieser Karte erreichbar sind, welche erst nach
   Forschung, welche gar nicht.
3. **Ziele (was will ich):** Wenige Oberziele als Zielraten mit Gewichten, z. B. Baumaterial, Nahrung, Werkzeuge,
   Soldaten, Wissenspunkte (LWP/IWP/MWP). Die Gewichte ergeben sich aus der Lage (Bedrohung, Kartenprofil, Forschung,
   Bestände). Utility-Ansatz: Jede mögliche Maßnahme bekommt einen Nutzwert, die besten werden ausgeführt.
4. **Planung (wie erreiche ich sie):** Rückwärtsrechnung durch den Graphen: Zielrate → nötige Gebäude → deren Eingänge
   → Rohstoffe. Mehrere Wege zu einer Ware (über Tags): der günstigste nach Kosten, Verfügbarkeit und Entfernung; ein
   Weg ohne Quelle ist unendlich teuer (keine Steine → Lehmweg). Ergebnis: Bauwünsche und Gebietswünsche.
5. **Ausführung:** Bauplatzsuche, Straßenbau, Material-Reservierung (aus AIJH übernehmbar). Expansion gezielt: Regionen
   mit Wert = Knappheit × Menge der Ressourcen − Entfernung − Gefahr; Militärgebäude als Korridor zur Zielregion statt
   zufälligem Wachstum am Rand.

**Rückkopplung:** geplante gegen gemessene Raten (Statistik, Lagerbestand). Bleibt eine Kette zurück, wird der
Engpass gesucht (Rohstoff, Werkzeug, Arbeiter, Transport) und gezielt behoben, nicht pauschal mehr gebaut.

**Forschung:** Wert einer Technologie = Nutzen der Ketten, die sie freischaltet, unter den aktuellen Zielen und den
Ressourcen der Karte, plus der Wert der Technologien, zu denen sie führt. Kupferwerkzeuge ohne erreichbares Kupfer sind
fast wertlos, Lehmziegel ohne Steine sehr wertvoll.

**Nicht vorgesehen:** GOAP/volle Planungssuche über die Wirtschaft (Kombinationen explodieren), maschinelles Lernen
(keine Trainingsdaten, bei jeder Inhaltsänderung neu, nicht erklärbar), großer Behavior Tree (wird wieder hartcodiert).

**Erklärbarkeit:** Die KI protokolliert ihre Gründe („Ziel Baumaterial 4/min, Weg Lehmziegel, Engpass Ton, Expansion
nach Region 12 wegen Lehm“), später als Fenster im Cheat-Modus. Wichtig fürs Balancing. Messstand bleibt ai-battle.

### Rechenleistung

Die KI läuft nur beim Host (`GameClient`: `AddAIPlayer` nur bei `IsHost()`), ihre Rechnung muss also nicht synchron
sein; nur ihre Befehle (Game Commands) gehen ins Spiel. Ziel: im Mittel deutlich unter 1 ms je KI und GF auf einer
512×512-Karte mit 8 KIs, keine Spitzen, die das Spiel ruckeln lassen. Maßnahmen:

- **Takt je Schicht:** Wirtschaftsmodell einmal beim Spielstart (die Lua-Daten ändern sich im Spiel nicht), Forschung
  nur bei Änderungen. Wahrnehmung ereignisgesteuert, Planung alle paar Sekunden Spielzeit, Ausführung je NWF in kleinen
  Portionen.
- **Wahrnehmung inkrementell:** Regionsprofile einmal beim Start aus der Karte, danach nur geänderte Regionen neu (Baum
  gefällt/gepflanzt, Stein abgebaut, Erz abgebaut, Gebiet gewonnen/verloren) statt die Karte neu zu scannen. Der Graph
  der Regionen (Nachbarschaft, grobe Entfernungen) ersetzt teure Wegsuchen auf Knotenebene für die Bewertung.
- **Planung billig:** Der Produktionsgraph hat Dutzende bis wenige Hundert Knoten. Eine Rückwärtsrechnung mit
  Wegewahl ist linear bis n·log n darin (Mikrosekunden). Zyklen (Werkzeuge für die Werkzeugherstellung) mit begrenzter
  Iteration.
- **Regionsbewertung:** Bei ~2600 Regionen (512×512) ein Skalarprodukt Ressourcenprofil × Knappheitsgewichte je
  Region; neu nur, wenn sich die Gewichte deutlich ändern. Entfernungen per Breitensuche auf dem Regionsgraphen.
- **Teure Schritte mit Budget:** Bauplatzsuche, Wegsuche und Straßenbau als Aufgaben in einer Warteschlange mit festem
  Budget je GF (Anzahl Suchen bzw. Knoten). Große Aufgaben werden über mehrere GFs verteilt.
- **KIs versetzt:** Schwere Schritte (Planung, Regionsbewertung) laufen bei mehreren KIs in verschiedenen GFs.
- **Messen statt mitzählen:** Raten aus den vorhandenen Statistiken und Beständen (gleitende Mittel), keine neuen
  Ereignisse je Ware.
- **Datenstrukturen:** flache Felder je Ware/Gebäude/Region (`EnumArray`, Vektoren), keine Maps und keine
  Speicheranforderungen in häufig laufenden Schleifen.
- **Kein Nebel-Wissen:** Da die KI die Karte kennt, entfällt die Verwaltung des Erkundeten.
- Später möglich: Planung in einem eigenen Thread auf einer Momentaufnahme der Daten (die KI ist nicht synchron), nur
  wenn das Budget sonst nicht reicht.
- Jede Schicht bekommt eine Zeitmessung im ai-battle (Summe und Spitze je KI), damit Regressionen auffallen.

### Schritte

1. Wahrnehmung und Wirtschaftsmodell, nur lesend: Graph und Regionsprofile berechnen und protokollieren, nichts im
   Spiel ändern. Prüfen, ob die KI die Karte versteht; Zeitmessung.
2. Planung ersetzt den Bauplaner (`BuildingPlanner`): Bauwünsche aus der Rückwärtsrechnung.
3. Gezielte Expansion über den Regionswert.
4. Forschung über den Graphen.
5. Ziele aus der Lage (Gewichte), danach Militär.

Jeder Schritt ist für sich testbar und mit der alten KI vergleichbar (ai-battle).

## KI-Bauregeln (Wunsch des Nutzers 2026-10-23, gilt in allen Spielen)

- **Expansion gebremst:** höchstens `MAX_MILITARY_SITES` (derzeit 20) Militär-Baustellen gleichzeitig (auch gerade
  beauftragte), und nur wenn mindestens `MIN_SOLDIERS_PER_MILITARY_SITE` (derzeit 1) freie Soldaten je Militär-Baustelle
  (inkl. der neuen) in den Lagern sind. Sonst wird der Militärauftrag verworfen (die KI plant regelmäßig neue).
- **Material vor jedem Bauauftrag:** Bretter und Steine im echten Lagerbestand minus das, was bestehende und gerade
  beauftragte Baustellen noch brauchen, müssen für das Gebäude reichen. Sonst wartet der Auftrag (Militär: verworfen).
  Gilt auch für das Sägewerk, das die KI an neuen Lagern direkt setzt (sonst normaler Bauauftrag).
- **Wichtige Gebäude zuerst:** `IMPORTANT_BUILDINGS` (Waffenschmiede, Schlosserei, Eisenschmelze, Münzprägerei,
  Kohle-, Eisen-, Goldmine) werden in jeder Runde vor allen anderen Aufträgen bearbeitet. Wartet eines davon auf
  Material, bleibt dieses Material für die anderen Gebäude gesperrt.
- **Grenznähe frei für Militärgebäude (2026-11-09):** Bis `BORDER_RESERVE_RADIUS` = 4 Knoten an einer Landgrenze
  setzt die KI nur Militärgebäude und `BUILDINGS_NEAR_BORDER` (Holzfäller, Steinbruch, Fischer, alle Minen, Hafen,
  Katapult), im Zivilisationsmodus außerdem Werkstätten mit `aiNearBorder = true` in `workshops.lua` (gesetzt bei
  Lehmgrube und Kupfermine). Eine Grenze zählt nur, wenn dahinter innerhalb von 4 Knoten Land liegt, das nicht der KI
  gehört (`AIInterface::IsLandBorder`): Küsten mit nur Wasser davor bleiben normal bebaubar, Land hinter einem
  schmalen Fluss zählt dagegen. Gilt in allen Platzsuchen der KI (`AIPlayerJH::FindPositionForBuildingAround`, auch
  Sammler, Minen und Plantagen aus Lua und das Sägewerk am neuen Lager); bestehende Gebäude bleiben stehen,
  Lua-Bauaufträge mit fester Position (`forced`) sind ausgenommen.
  Vergleich 5 Karten (GreenPlains mit 2 Seeds, Bergstrasse, DerGrosseSee, Hochebene), 2 KI, 2 h Spielzeit: vorher
  standen 273 von 1030 solchen Baustellen in Grenznähe, jetzt 0 von 1150. Land und Gebäude insgesamt eher etwas mehr
  (49 060 statt 44 908 Knoten, 2029 statt 1814 Gebäude; nur ein Lauf je Karte, die Streuung ist groß).
- Einstellbar oben in `libs/s25main/ai/aijh/AIConstruction.cpp`; Prüfung in `BuildJob::CheckLimitsAndMaterial`
  (`Jobs.cpp`).
- Vergleich GreenPlains, 2 KI, 2 h, gleicher Seed: Militärgebäude 23/40 statt 50/61, höchstens 3 statt bis zu 10
  Militär-Baustellen, deutlich mehr Bretter/Steine im Lager. Die Zahl der Industriegebäude hängt weiter an den
  Bauwünschen in `BuildingPlanner.cpp` (z. B. Minen nur mit erreichten Bergen).

## Tests (Stand 2026-11-10)

Alle 20 Testprogramme (`ctest`) laufen grün, sowohl mit den Radien des Nutzers (HQ 10, Militär 10/12/14/16, größere
Sichtweiten) als auch mit den alten Werten (HQ 9, Militär 8–11). Ohne Soundkarte (z. B. auf einem Server) braucht
`Test_drivers` die Umgebungsvariable `SDL_AUDIODRIVER=dummy`.

- Viele Tests stammen aus dem Original (RttR) und prüfen das Grundspiel. Wo der Fork Werte bewusst ändert, leiten die
  Tests ihre Erwartung aus den Konstanten ab statt aus festen Zahlen: Radien (`HQ_RADIUS`, `MILITARY_RADIUS`,
  `SEAATTACK_DISTANCE`, Testweltgrößen), KI-Grenzwerte (`AIConstruction::GetMaxMilitarySites`,
  `GetMinSoldiersPerMilitarySite`, `GetBorderReserve`), Standard „HQ-Abstand Berg“ aus `MapSettings`.
- KI-Tests geben dem Hauptquartier Startwaren (`addStartResources`), weil die KI ohne Bauarbeiter und Material keine
  Baustelle eröffnet. `MilitarySitesNeedSoldiers` schaltet die HQ-Reserve ab (sonst hält das HQ 1 Soldaten je Rang
  zurück).
- `AIBuildsWorkshopsAndLibraries` nennt die Werkstatt `BoneCarver`: Von den Lua-Werkstätten mit Eingängen baut die KI
  nur diese und Werkstätten mit `outputChoice = "tools"` (`BuildingPlanner::UpdateCustomBuildingsWanted`, Test
  `AIBuildsToolWorkshop`).
- Seefahrt: Die Erkundungsfahrt erkundet zuerst Hafen 6, weil Hafen 8 über den Kartenrand nur etwa 16 Knoten vom
  Heimathafen entfernt ist und das Erkundungsschiff (`VISUALRANGE_EXPLORATION_SHIP`) ihn schon von dort sieht.
- **Cheat-Modus (Entscheidung 2026-11-10):** Im Einzelspielerspiel von Anfang an an, im Mehrspielerspiel aus und nicht
  einschaltbar (`Cheats::isCheatModeOn`). Vorher war er auch im Mehrspielerspiel an: Das Cheat-HQ wurde ohne Spielbefehl
  gesetzt (asynchroner Spielstand) und die Spielerauswahl in Gebäude-, Forschungs- und Inventurfenster zeigte die Daten
  der Gegner. Bleibt nach einem Mehrspielerstart nur ein Mensch übrig, schaltet er den Modus wie im Original mit
  „winter“ ein. Die Cheat-Tests beginnen mit ausgeschaltetem Modus; `CheatModeIsOnAtStart` und
  `CheatModeIsNeverOnInMultiplayer` prüfen den Standard.
- Erledigt: `Test_UI/IwMapGenerator` kompilierte nach dem Entfernen der Landschaftsauswahl nicht mehr,
  `GamePlayer::IsBuildingEnabled` stürzte ohne GameInterface ab, `ResearchSuite/DefaultTechTreeIsValid` prüft nur die
  Struktur, keine Balancing-Werte.
