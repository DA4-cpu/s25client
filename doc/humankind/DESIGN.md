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
    requires = { "toolmaking", { "milling", "animal_husbandry" } }, -- Voraussetzungen, VORHER definiert (keine Zyklen);
                                     -- eine Liste in der Liste = mindestens eine davon
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

Ein Replay ohne Grafik abspielen (z. B. um einen Absturz zu finden): `ai-battle --play-replay <datei.rpl>` führt die
aufgezeichneten Befehle aller Spieler GF für GF aus und vergleicht die Prüfsummen (asynchron = andere Spieldaten als im
aufgezeichneten Spiel, die Lua-Dateien müssen dieselben sein). `--replay-ais` lässt dabei die KI-Spieler mitlaufen (ihre
Befehle werden nicht ausgeführt, die aufgezeichneten schon; die KI würfelt anders als im Spiel). Mit einem Build mit
AddressSanitizer (`-fsanitize=address,undefined`) zeigt das Speicherfehler mit Aufrufliste.

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
  512 Waren (`GoodType::Custom0` …), 128 Gebäude (`BuildingType::Custom0` …) und 128 Berufe (`Job::Custom0` …, einer
  pro Werkstatt), seit 2026-11-24 (vorher 64/32/32). Lua belegt sie in der Reihenfolge der Definition. Unbenutzte Plätze
  sind überall ausgeblendet.
- `civ::Content` (`libs/s25main/civ/Content.*`) löst die Lua-Definitionen auf. Der `TechTree` besitzt den Content; im
  laufenden Spiel ist er „aktiv“ (`civ::getActiveContent()`), damit globale Tabellen ohne Zugriff auf die Welt die
  neuen Einträge kennen: `WARE_NAMES`, `BUILDING_NAMES`, `BUILDING_COSTS`, `BUILDING_SIZE`, `BLD_WORK_DESC`,
  `JOB_NAMES`, `JOB_CONSTS`, `JOB_SPRITE_CONSTS`, `STD_TRANSPORT_PRIO` liefern für die Slots die Werte aus Lua.
- **Platzhaltergrafik:** Jede neue Ware, Werkstatt und jeder Arbeiter nennt ein eingebautes Vorbild (`look`), dessen
  Grafiken verwendet werden (`civ::getLook`). Eigene Grafiken folgen später.
- **Werkstätten** sind normale Produktionsgebäude (`nobUsual`) mit dem Arbeiter `nofCustomWorker`: Er holt die
  Eingangswaren, arbeitet `workTime` Sekunden im Gebäude und trägt das Produkt heraus. Die Werkstatt wird
  automatisch als Abnehmer ihrer Eingangswaren eingetragen.
- **Tags:** Jede Ware hat ihren Bezeichner als Name-Tag, dazu optional Warenarten (`kind`, ein Tag oder eine Liste
  beliebig vieler Tags, seit 2026-11-24) und Eigenschaften (`properties`). Eine Tag-Abfrage trifft alle Waren, die alle
  genannten Tags haben. Mit `inheritProperties` bekommt das Produkt die Eigenschaften der verbrauchten Ware
  (Nadelholz → Nadelholzbretter); Warenarten werden nicht weitergegeben.
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
  Aufgelöst in `civ::Content` (`CustomBuilding::productChoice`,
  `productChoices`).
- `outputChoice = "knowledge"`: nach dem Regler „Wissen“; jedes Produkt braucht `knowledge` in `wares.lua` (genau eine
  Art).
- Mehr als ein Produkt ohne `outputChoice`: die Gesamtverteilung wählt (`ProductChoice::Distribution`, Abschnitt
  „Gesamtverteilung“). `outputChoice = "tools"` und eigene Regler (`rttr:AddRegulator`) gibt es seit 2026-11-21 nicht
  mehr.
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
  (Schwert/Schild abwechselnd) und Tempel wählen jede Ware neu. Abbau: je Arbeitsschritt genau 1 Einheit des
  Vorkommens, egal wie viele Waren herauskommen (Nutzer, 2026-11-24; vorher 1 Einheit je Ware): Minen (auch Lua-Minen)
  und Brunnen eine Einheit, Steinmetz ein Stück des Steinhaufens, Sammler und Plantagen ein Stück des Objekts, Fischer
  ein Fisch. Holzfäller, Bauer, Jäger usw.: alle Waren aus demselben Baum, Feld, Tier.
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
  (höchstens 5). Werkzeuge bestellt sie nicht: Die Gesamtverteilung steuert die Werkzeughersteller.
- KI und Regler (Wunsch des Nutzers 2026-10-23): Alle 200 GF (10 Sekunden) stellt die KI den Regler nach den WP ein,
  die ihrer aktuellen Forschung noch fehlen: die am meisten fehlende Art 10, andere fehlende anteilig (mindestens 1),
  nicht benötigte 0. Forscht sie nichts oder fehlen keine WP: Startwerte aus `distribution.lua`. Einstellbar oben in
  `libs/s25main/ai/aijh/AIPlayerJH.cpp` (`KNOWLEDGE_DISTRIBUTION_INTERVAL_GF`, `KNOWLEDGE_WEIGHT_NOT_NEEDED`,
  `KNOWLEDGE_WEIGHT_MOST_NEEDED`), Logik in `AIPlayerJH::AdjustKnowledgeDistribution`.

### Datenformat

Dateien: `data/RTTR/gamedata/civ/wares.lua` und `civ/workshops.lua` (dort ausführlich erklärt, mit Beispiel).

```lua
rttr:AddWare{ name = "NeedleWood", title = "Nadelholz", kind = { "Holz", "Baumaterial" }, properties = { "Nadelholz" }, look = "Wood", transport = "Wood" }
rttr:AddWare{ name = "NeedleBoards", title = "Nadelholzbretter", kind = "Bretter", properties = { "Nadelholz" }, look = "Boards", transport = "Boards" }
rttr:SetWare{ Fish = { kind = { "Nahrung" } } }   -- Tags für eingebaute Waren
rttr:SetWare{ Wood = { name = "PineLog", kind = { "Wood", "Softwood" } } }   -- eingebaute Ware umbenennen

rttr:AddWorkshop{
    name = "Sawmill2", title = "Sägewerk", size = "medium", cost = { Boards = 2, Stones = 2 }, look = "Sawmill",
    worker = { title = "Säger", look = "Carpenter" }, tools = { tools = { "Saw" } },
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
  zurück, ist er wieder Helfer (`ConvertToHelpers`). Arbeiter von Werkstätten brauchen seit 2026-11-24 auch ohne
  Werkzeugreform kein Werkzeug (`worker.tool` gibt es nicht mehr); eine Werkstatt ohne `tools` arbeitet mit Tempo 100.
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
- **KI:** Im Zivilisationsmodus stellt sie keine Werkzeug-Regler ein und bestellt keine Werkzeuge (Gesamtverteilung,
  2026-11-21). Sie gibt der Schlosserei Eisen und Bretter vor der Waffenschmiede (Verteilung der Eingänge). Der Bauplaner zählt Spitzhacken im Lager als mögliche Bergleute und
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

Schritte 1 und 2 umgesetzt (siehe „Stand Schritt 1“ und „Stand Schritt 2“). Ziel: Die KI hat kein Wissen über einzelne Gebäude oder Waren im Code. Sie leitet ihre Wirtschaft
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
   Spiel ändern. Prüfen, ob die KI die Karte versteht; Zeitmessung. *(umgesetzt 2026-11-12, siehe unten)*
2. Planung ersetzt den Bauplaner (`BuildingPlanner`): Prioritätenliste aus gemessenen Zahlen. *(erste Fassung
   2026-11-12 verworfen; Neufassung umgesetzt 2026-11-13, siehe „Stand Schritt 2: Prioritätenliste“)*
3. Gezielte Expansion über den Regionswert.
4. Forschung über den Graphen.
5. Ziele aus der Lage (Gewichte), danach Militär.

Jeder Schritt ist für sich testbar und mit der alten KI vergleichbar (ai-battle).

### Stand Schritt 1 (2026-11-12)

Code in `libs/s25main/ai/civ/`, Namensraum `civai`. Läuft im Zivilisationsmodus in jeder AIJH-KI mit
(`AIPlayerJH::RunGF`), gibt keine Befehle.

- `EconomyGraph`: Rezepte (Gebäude → Eingänge, Produkte, Nebenprodukte, Quelle auf der Karte, nötige Werkzeuge,
  gesperrt durch Forschung, Waren pro Minute). Eingebaute Gebäude aus `BLD_WORK_DESC`/`JOB_CONSTS`; nur ihre Quellen
  (Holzfäller → Bäume, Steinbruch → Steinhaufen, Minen → Erz, Fischer, Brunnen, Jäger, Bauer/Weinberg/Köhler → Land)
  und die Mehrfachprodukte (Schlosserei, Waffenschmiede, Tempel) stehen als Tabelle im Code, weil diese Gebäude eigene
  Figuren haben. Werkstätten aus Lua vollständig aus `civ::Content` (Sammler → Kartenobjekt, Plantage → Anbaufläche,
  Mine → Erz). Gebäude ausgeschalteter Addons fehlen. Rate = Arbeitszyklus (+ geschätzte Wege) × `outputRatio`.
- `MapProfile`: Regionen 8×8 Knoten; je Region Bäume, Steine in Steinhaufen, Tiere, freies Bauland, Bodenschätze je
  Art (Erze, Wasser, Fisch), Stücke je Kartenobjekt, Anbauflächen je pflanzbarem Kartenobjekt; dasselbe nur für das
  eigene Gebiet. Was sich im Spiel nicht ändert (bebaubares Terrain, wo ein Kartenobjekt natürlich wächst), wird einmal
  je Knoten vorberechnet. Danach werden je NWF 16 Regionen neu gelesen (`REGIONS_PER_NWF`), reihum.
- `analyzeEconomy`: Fixpunkt über den Graphen für drei Lagen (eigenes Gebiet + jetzige Gebäude, ganze Karte, ganze
  Karte + alle Forschung) → je Ware herstellbar / nach Expansion / nach Forschung / nicht erreichbar mit Grund („fehlt
  Steinhaufen“, „braucht Lehm“). Baumaterial und Werkzeuge dürfen auch aus dem Lager kommen (einmaliger Bedarf),
  Eingänge nicht. Alle 3000 GF neu (`ANALYSIS_INTERVAL`).
- Log: Bericht beim Start und wenn sich ein Warenzustand ändert (Rohstoffe Gebiet/Karte, Waren je Zustand), alle 36000 GF
  die Zeiten. Im ai-battle braucht es dafür den Ordner `logs` im Arbeitsverzeichnis.
- Zeiten (Debug-Build, DerGrosseSee, 2 KI): Graph ~0,2 ms und Kartenprofil ~13 ms einmal beim Start, Analyse ~0,4 ms
  alle 3000 GF, Aktualisierung ~0,1 ms je NWF. Erste Fassung ohne Vorberechnung lag bei ~0,33 ms je NWF.
- Tests: `CivAISuite` (Graph, fehlende Rohstoffe, Expansion, Forschung, schrittweise Aktualisierung).

### Stand Schritt 2 (2026-11-12) – verworfen

**Vom Nutzer verworfen (2026-11-12):** Die Planung aus geschätztem Bedarf hat schlecht gespielt (z. B. 7 Brunnen ohne
Wasserverbrauch, Brauereien ohne Farm, kein Roden). Sie schätzte Raten (Wirkungsgrad 0,6) und enthielt künstliche
Regeln (Expansionsfluss, Wissensgebäude, Baustellen- und Gehilfengrenzen). Der Code in `ai/civ/ProductionPlanner.*`
ist gelöscht und durch `ai/civ/EconomyPlanner.*` ersetzt. Die Beschreibung bleibt als Beispiel, wie es nicht geht.


Entscheidungen des Nutzers: Der Planer übernimmt nur Gebäude, die Waren herstellen (eingebaute und aus Lua) und den
Förster; Militär, Lager, Hafen, Werft, Katapult, Eselzucht und Bibliothek plant weiter AIJH. Die Ziele kommen bis
Schritt 5 aus dem aktuellen Bedarf plus kleinen Vorräten, die Gewichte stehen im Code (oben in
`ai/civ/ProductionPlanner.cpp`).

- **Bedarf** (`collectDemand`, je Eintrag „eine von diesen Waren“, z. B. ein Kosteneintrag nach Tags):
  - Baustellen: Material, das noch nicht bestellt ist; Werkzeuge für die fertigen Gebäude.
  - Werkzeugreform: leere Werkzeugplätze der Gebäude, dazu je Art Werkzeugplatz 1 Ersatz.
  - Forschung: noch zu zahlende Waren der laufenden Technologie und das fehlende Wissen.
  - Soldaten: fehlende Soldaten in Militärgebäuden (Schwert, Schild, Bier je Soldat, wie
    `nobBaseWarehouse::CanRecruitSoldiers`), Vorrat 20 Soldaten im Lager.
  - Militär: Münzen (wenn Ränge möglich) und Rüstungen (Leder-Addon), die Militärgebäude aufnehmen.
  - Expansion als Fluss: alle 2 Minuten das billigste Militärgebäude und ein Soldat. Der Lagerbestand deckt das nicht ab,
    sonst baut die KI mit den großen Startvorräten lange nichts. Ohne Sägewerk expandiert AIJH nicht.
  - Vorrat: 15 je Kosteneintrag der Militärgebäude.
  - Der Lagerbestand deckt den Bedarf in der Reihenfolge der Arten (`applyStock`). Menge / Horizont (5–20 min je Art) =
    Waren je Minute.
- **Planung** (`planProduction`, alle 200 GF, KIs versetzt):
  1. Kosten je Ware über das billigste jetzt nutzbare Rezept (Bellman-Ford). Ein Rezept ist nutzbar, wenn:
     - das Gebäude erlaubt ist,
     - seine Quelle im Gebiet liegt,
     - Baumaterial und Pflichtwerkzeug im Lager oder herstellbar sind,
     - die Eingänge herstellbar sind.

     Die Kosten sind (1 + Baukosten/10) / Rate + Eingänge. Nebenprodukte (wenn erforscht) zählen als Weg zur Ware.
     Waren im Lager, die nicht herstellbar sind, gelten als Eingang mit festen Kosten (z. B. Knochen für den
     Schnitzer).
  2. Rückwärts durch die Wege: Waren je Minute → Gebäude → Eingänge. Jeder Weg nur so weit, wie seine Quelle reicht
     (Stücke je Gebäude: Steinhaufen 15, Erz 50, Fisch 40, Tiere 8, Ackerland 20, Kartenobjekte und Anbauflächen 10;
     Bäume und Wasser ohne Grenze). Der Rest geht auf den nächsten Weg. Nebenprodukte bereits geplanter Gebäude werden
     angerechnet.
  3. Förster nach verbrauchten Bäumen, halb so viele bei vielen Bäumen im Gebiet.
  4. Wissen für die laufende Forschung: Fertige Gebäude geben Punkte (`knowledge.lua`). Baustellen und vom Plan
     gewollte Gebäude zählen schon. Für den Rest kommen Gebäude mit dem billigsten Punkt dazu (Baukosten je Punkt, halb
     für Gebäude ohne Eingänge oder mit Bedarf). Was so nicht reicht, kommt über Wissensträger (Schnitzer usw.).
  5. Material und Werkzeuge der neuen Gebäude und die Wissensträger als zweiter Durchlauf.
  6. Höchstens 2 Baustellen je Typ. Neue Gebäude insgesamt höchstens Gehilfen im Lager + 2, reihum je Typ eins, damit
     jede Stufe einer Kette anfängt.
- AIJH übernimmt `wanted` für alle Typen des Plans (`BuildingPlanner::UpdateBuildingsWanted`) und baut auch die
  eingebauten Gebäude, die in seiner festen Liste fehlen (Brunnen, Tempel …). Gebäude mit Quelle setzt es auch an
  Militärgebäuden.
- Log: alle 6000 GF der Plan (Bedarf je Art, Waren direkt/gesamt, Gebäude gewollt mit Grund, nicht machbar), alle
  36000 GF die Zeiten.
- Zeit: Plan im Debug-Build ~0,3–0,8 ms alle 200 GF, Spitzen bis ~5 ms.
- Vergleich mit der alten Planung (7 Karten, 2 KI, 2 h; je Karte ein Lauf, große Streuung):

  | | alt | neu |
  |---|---|---|
  | Land | 82 334 | 75 067 |
  | Gebäude | 2724 | 1402 |
  | Militärgebäude | 1546 | 1193 |
  | Forschung (Ø Technologien) | 15,5 | 14,6 |

  Die neue KI baut nur, was gebraucht wird, also etwa halb so viele Gebäude. Die alte baut z. B. 23 Holzfäller und 15
  Bauernhöfe auf Vorrat. Die Spiele sind ausgeglichener: Bei der alten KI hatte oft eine Seite 5× so viel Land.
- Bekannte Grenzen:
  - Die Raten sind geschätzt: Wirkungsgrad pauschal 0,6, keine gemessene Rückkopplung.
  - Die Quellgrenzen sind grobe Stückzahlen je Gebäude.
  - Gebäude, die nur für Wissen gebaut werden, arbeiten oft nicht.
  - Die Werkzeugwahl der Schlosserei stellt weiter `AdjustSettings`.
  - AIJH-Regeln, die Gebäude abreißen (Sägewerke, Holzfäller, Förster, Granitminen), laufen weiter.
- Tests: `CivAISuite` (Kette rückwärts, keine Quelle, Weg nach Quelle und Kapazität, Nebenprodukt, Baustelle, Lager nach
  Priorität, Förster, Wissen über Gebäude, Plan im Takt). Die KI-Tests in `CivContentSuite`, `CivOutputChoiceSuite`
  und `CivPlantsAndOresSuite` geben der KI jetzt einen Bedarf (Forschung), statt dass sie Werkstätten pauschal baut.

### Neufassung der Planung (Entscheidungen des Nutzers 2026-11-13)

Grundsätze:
- **Maximale Ausbeutung und Expansion.** Es gewinnt, wer die Rohstoffe am besten ausnutzt.
- **Nur natürliche Grenzen**, keine künstlichen Regeln oder Timer. Bei fehlenden Spielregeln den Nutzer fragen.
- **Exakte gemessene Zahlen statt Schätzungen.**

**Baustein A – Statistik** (umgesetzt, siehe unten): Bestand, Produktion, Verbrauch je Verwendung, Bilanz, Gehilfen,
offene Nachfrage, Gebäudezustände.

**Baustein B – Wahrnehmung:**
- Kartenprofil und Wirtschaftsgraph ohne geschätzte Raten.
- Keine Regel „freie Vorkommen“: Mehrere Gebäude dürfen eng an einem Vorkommen stehen.
- Natürliche Bedingung: Am Bauplatz muss etwas in Reichweite sein (Bauplatzsuche).
- Lagerkapazität (geplant): Jedes Lager fasst eine Gesamtzahl Waren. Ist kein Platz mehr, gibt es nur noch benötigte
  Aufträge.

**Warenzustand.** Bilanz und Verbrauch beziehen sich auf die letzten 5 Minuten; die Länge ist eine Variable zum
Ausprobieren. Schwelle `S = max(8, 10 × Verbrauch/min)` (8 ist der Mindestbestand):
- **Mangel:** Bilanz < 0, oder offene Nachfrage größer als der Bestand. Folge: benötigter Auftrag für Erzeuger.
- **Überschuss:** Bilanz ≥ 0 und Bestand > S. Folge: benötigter Auftrag für Verbraucher. Liegengebliebene Waren
  belegen Lager und haben Arbeit gekostet; die Verarbeitung ist dann der Engpass.
- **Normal:** Bilanz ≥ 0 und Bestand ≤ S. Folge: expandierender Auftrag, nur für Gebäude der Expansionsliste.

**Reicht ein Eingang für einen weiteren Verbraucher?**
- Bilanz > 0: ja, der Bestand ist egal.
- Bilanz = 0: ja, wenn Bestand ≥ `max(8, 10 × (gemessener Verbrauch + Verbrauch des neuen Gebäudes laut Arbeitszeit))`.
- Bilanz < 0: nein.

**Benötigt / expandierend:**
- **Benötigt:** gleicht einen Mangel aus oder stabilisiert die Wirtschaft.
- **Expandierend:** macht die Wirtschaft größer.
- Nur Gebäude der Liste in `ai.lua` dürfen expandierend gebaut werden: Waffenschmiede, Münzprägerei, Schlosserei,
  Kupferwerk, Brauerei, Holzfäller, Steinbruch, Lehmgrube, Fischer, Knochenschnitzer, Gold-, Kohle-, Eisen- und
  Kupfermine, Lederei, Lagerhaus, Hafen, Papyrusplantage, Köhler, Baracke, Wachstube, Wachturm, Festung, Sägewerk,
  Jäger. Begründung: Expansion über Endproduzenten stützt die Wirtschaft gezielter.
- Förster, Brunnen, Werft und Katapult sind nie expandierend.

**Prioritätenliste:**
- **Einträge:** einzelne Gebäudeaufträge mit der Ware, für die sie gedacht sind, und dem Auftrag darüber (`parent`),
  nicht der ganzen Kette.
- **Kind-Aufträge** kommen direkt über ihren Eltern-Auftrag. Beispiel: Die Brauerei braucht Getreide, also kommt die
  Farm darüber; fehlen Sensen, kommt deren Auftrag über die Farm.
- **Reihenfolge:**
  1. Mangel, nach „Minuten bis leer“ (offene Nachfrage zählt als 0 Minuten),
  2. Überschuss,
  3. ganz unten die expandierenden Aufträge.
- **Abarbeitung:** Der oberste Eintrag muss platziert oder als nicht baubar erkannt sein, bevor der nächste drankommt.
  - Benötigte Aufträge: Bei jeder unerfüllten Regel wird zuerst versucht, sie zu lösen.
  - Expandierende Aufträge: Ist eine Regel unerfüllt, wandert der Auftrag nach unten.
- **Regeln** sind einzelne, leicht ergänzbare Funktionen: erforscht, Baumaterial, Werkzeug, Arbeiter/Gehilfen
  (Helfer-Bilanz), Eingänge reichen, Bauplatz mit Vorkommen in Reichweite, Lagerplatz (nur expandierend),
  Expansionsliste.
- **Anlaufzeit:** Nach dem Platzieren eines Erzeugers bzw. Verbrauchers einer Ware gibt es keinen weiteren, bis er eine
  Bilanzlänge gearbeitet hat.

**Vorgehen für eine gefragte Ware:**
1. Erzeuger ansehen.
2. Produktivität unter 80 %: Grund suchen (Eingang, Werkzeug, Arbeiter, Platzierung, Wachstum) und diesen angehen.
3. Produktivität ab 80 % oder noch kein Erzeuger: dessen Eingänge prüfen.
   - Reichen alle, wird der Erzeuger ein Auftrag.
   - Sonst dasselbe Vorgehen mit jedem fehlenden Eingang. Der Erzeuger wartet so lange.
4. Schleifen: Waren, die in der Elternkette schon vorkommen, werden übersprungen.

Verbraucher ohne Gebäude (Rekrutierung, Bau, Forschung, Militär, Expeditionen) werden wie Gebäude behandelt: Ihre
übrigen Eingänge werden geprüft. Liegen z. B. Schwerter im Überschuss, weil Bier fehlt, stößt das Bier an. Münzen fragt
nur das Militärgebäude an, das Münzen erlaubt.

**Gescheiterte Aufträge:**
- Schleife erkennen: Kommt die Ware oder das Gebäude in der Elternkette schon vor, ist dieser Lösungsweg unmöglich.
- Sonst die nächste Alternative für die Ware versuchen; gescheiterte Wege merkt sich der Auftrag.
- Keine Alternative mehr: Der Auftrag wird blockiert und speichert eine prüfbare Bedingung (z. B. „freie Gehilfen ≥ 1“,
  „Steine ≥ 2“, „Vorkommen im Gebiet“, „Technologie erforscht“). Nur diese Bedingungen werden geprüft; ist eine
  erfüllt, wird der Auftrag neu versucht.
- Beispiel: Ein Steinbruch braucht Gehilfen. Die Lösung „Lagerhaus“ bräuchte Steine, das ist eine Schleife. Also
  wartet der Steinbruch auf Gehilfen.

**Wahl zwischen Alternativen** (z. B. Kohlemine oder Köhler, Nahrungsketten): ein Probelauf je Weg ohne zu platzieren.
Gewählt wird nach dieser Reihenfolge:
1. nicht blockiert,
2. die wenigsten neuen Gebäude,
3. die höchste gemessene Produktivität der vorhandenen Erzeuger,
4. die geringeren Baukosten.

Der Aufwand wächst nur mit der Zahl der Waren, weil die KI Ergebnisse je Ware merkt.

**Holzfäller und Förster** (Radien aus dem Spiel, Standard je 6):
- **Rodender Holzfäller:**
  - Der Auftrag entsteht, wenn die Walddichte im Radius des Holzfällers mindestens 8 Bäume erreicht.
  - Abstand zu jedem Förster mindestens Försterradius + Holzfällerradius.
  - Kein Förster; Abriss, wenn keine Bäume mehr in Reichweite sind.
- **Dauerhafter Holzfäller:**
  - Der Auftrag entsteht aus Holzbedarf.
  - Fällt die Dichte in seinem Bereich unter 5 Bäume, wird ein Förster benötigt.
- **Förster:**
  - Nicht im Radius eines rodenden Holzfällers + Försterradius.
  - Sonst gelten die alten Platzierungsregeln.
  - „2 Holzfäller je Förster“ entfällt.
- Steinbruch: die alten Platzierungsregeln.
- Entfällt: der automatische Brunnen und die Folgeaufträge der alten KI (Farm → Mühle, Eisenmine → Schmelze …).
- Abrissregeln werden an die neuen Regeln angepasst.

**Baustein D – Ausführung:**
- Bauplatzsuche, Straßen und Militär bleiben aus AIJH.
- Der Takt wird zur Reaktionszeit je Schwierigkeitsgrad (Vorschlag: Schwer 1 s, Mittel 5 s, Leicht 20 s). Gesucht wird
  nur für die obersten Aufträge.
- Wo die KI Lagerhäuser hinsetzt, bleibt unverändert.

**Spielstart:** keine vorgefertigte Liste; sie ergibt sich aus den Regeln. Jeder Verbrauch zählt, nicht nur der von
Militärbaustellen.

### Stand Baustein A: Wirtschaftsstatistik (2026-11-13)

Code: `EconomyStatistics.*` (Zähler, im Spielstand, Version 26) und `EconomySnapshot.*` (Momentaufnahme). Die
Statistik läuft in allen Spielen mit, nicht nur im Zivilisationsmodus.

- **Jede Änderung des Warenbestands nennt ihren Grund.**
  - `GamePlayer::IncreaseInventoryWare(Ware, Anzahl, WareSource)`. Quellen: Produktion (auch Nebenprodukte), Rückgabe
    (Abriss, abgebrochene Forschung), Werkzeug zurück, erobert, Handel, hinzugefügt (Startwaren, Lua, Cheats).
  - `DecreaseInventoryWare(Ware, Anzahl, WareUse)`. Verwendungen: Produktion (Eingang je Arbeitsschritt), Bau (an der
    Baustelle abgeliefert, auch Expeditions-Hafen), Werkzeug (Arbeiter rekrutiert oder in einen Werkzeugplatz
    gesteckt), Soldaten (Rekrutierung), Militär (Münze für Beförderung, Rüstung angelegt), Forschung, Handel, Verlust
    (verloren, verbrannt, erobert).
  - Da beide Parameter Pflicht sind, ist die Zählung vollständig. Es gilt immer: Bestand = Summe der Zugänge −
    Summe der Verwendungen (Test und 2-h-ai-battle: 0 Abweichungen).
- **Verlauf:** Je Spielminute die Zähler der letzten 240 Minuten (4 h), danach abgeschnitten; dazu Summen seit
  Spielstart.
  - „Die letzten n Minuten“ sind die laufende Minute und die n−1 davor.
  - `GetBalance` = Produktion − Verbrauch (alle Verwendungen); andere Zugänge zählen nicht.
  - `GetConsumptionPerMinute` teilt durch die tatsächlich vergangene Zeit, am Spielstart also weniger als n Minuten.
  - Im Spielstand stehen nur Werte ungleich 0.
- **Gehilfen:** von Lagern erzeugt, und verbraucht (zu Arbeitern oder Soldaten geworden oder als Träger auf eine
  Straße gegangen; Nutzer 2026-11-13: Träger zählen mit), je Minute. Davon zu Arbeitern geworden: `helpersForJobs`
  (Spielstand 29).
- **Je Gebäudetyp** (2026-11-17, Spielstand 29): was die Arbeiter eines Gebäudetyps an der Fahne abgelegt haben
  (Produkte und Nebenprodukte) und was sie als Eingang genommen haben, je Minute (`GetProducedBy`, `GetUsedBy`).
- Freie Gehilfen und Arbeiter liefert die Momentaufnahme.
- **Expansionsliste:** `data/RTTR/gamedata/civ/ai.lua` mit `rttr:SetAIExpandingBuildings{ ... }` (Liste von
  Gebäudenamen, auch Werkstätten aus Lua), abfragbar mit `civ::TechTree::IsAIExpanding`.
- **Momentaufnahme** (`takeEconomySnapshot`): offene Nachfrage (angefragt, aber noch nicht geschickt). Sie umfasst:
  - Baustellen,
  - Eingänge der Gebäude, bei „eins von“ als ein Eintrag,
  - Werkzeugplätze,
  - Werkzeuge zum Rekrutieren fehlender Arbeiter,
  - Münzen nur bei Gebäuden mit erlaubten Münzen,
  - Rüstungen,
  - Expeditionen,
  - laufende Forschung,
  - fehlende Soldaten.

  Dazu Gebäudezustände je Typ: Anzahl, Baustellen, ohne Arbeiter, gestoppt, ohne Werkzeug, wartet auf Ware X,
  Produktivität. Außerdem Waren und Personen in den Lagern.
- Tests: `EconomyStatisticsSuite`.

### Stand Schritt 2: Prioritätenliste (Neufassung 2026-11-15 nach Prüfung mit dem Nutzer)

Code: `ai/civ/EconomyPlanner.*`, Tests `EconomyPlannerSuite`. Der Planer läuft im Zivilisationsmodus in jeder
AIJH-KI. Diese Fassung ersetzt die vom 13./14.11.: Die Prüfung am 14.11. fand Fehler gegen das Konzept, die Regeln
unten sind die Entscheidungen des Nutzers vom 14.11. abends. Überarbeitet am 17.11. nach der Prüfung vom 15.11. abends
(Entscheidungen des Nutzers vom 15.11. nachts): Produktivitätsregel entfällt, jede Regel gilt für jeden Auftrag,
Werkzeuge und Gehilfen über die Prognose.

**Grundsatz (Nutzer, 2026-11-15):** Jeder Auftrag folgt allen Regeln und Sperren, egal wie er entstanden ist. Keine
Regel wird übersprungen, außer eine Alternativregel ersetzt sie ausdrücklich (bisher nur eine: der Abbauer neben seinem
Anbauer, dort zählt die Menge in Reichweite nicht). Jeder Eingang und jeder Ausgang jedes Gebäudes steht in der
Prognose, auch bei Werkzeugherstellern.

**Einstellungen:**
- In `civ/EconomyForecast.h` (seit 2026-11-21, dieselbe Rechnung für Bauplaner und Gesamtverteilung):
  - Bilanzlänge `BALANCE_MINUTES` = 10 (Wert des Nutzers, 2026-11-21, vorher 5; auch die Zeit, in der ein neues
    Gebäude als kommend zählt).
  - Schwelle: `SURPLUS_FACTOR` = 10 × Verbrauch/min, mindestens `MIN_STOCK` = 8.
  - Werkzeugvorrat `TOOLS_IN_STOCK` = 2 je Werkzeugart.
- In `ai/civ/EconomyPlanner.h`:
  - Rodende Holzfäller ab `CLEARING_MIN_TREES` = 8 Bäumen, Sammler anderer Vorkommen (Steinhaufen, Kartenobjekte aus
    Lua) ab `MIN_DEPOSITS_IN_REACH` = 2 Vorkommen (Wert des Nutzers, 2026-11-18, vorher 1).
  - Förster, wenn weniger als `RENEW_BELOW` = 7 Bäume in Reichweite eines dauerhaften Holzfällers stehen (Wert des
    Nutzers).
  - Faktor Baumaterial `BUILDING_MATERIAL_FACTOR` = 0,5 (Startwert des Nutzers, 2026-11-21): Anteil aller Baukosten in
    der Auswertung des Mangels (siehe „Prognose“).
  - Höchstens `MAX_ORDERS_PER_TYPE` = 5 Aufträge je Gebäudetyp gleichzeitig (auch platzierte, bis das Gebäude fertig
    ist; Nutzer, 2026-11-15).
  - Bauernhöfe und Plantagen brauchen `MIN_FIELD_NODES` = 4 Feldknoten in Reichweite (Nutzer, 2026-11-15).
  - Vorkommen und Felder um ein Gebäude werden alle `SOURCE_CHECK_GFS` = 400 GF neu gezählt.
  - Kette steckt fest nach `STUCK_MINUTES` = 9 Minuten ohne Herstellung (Wert des Nutzers, vorher 3).
  - Die Produktivitätsregel (`HIGH_PRODUCTIVITY`) entfällt (Nutzer, 2026-11-15): Die Produktivität ist ein Symptom,
    keine Diagnose.
- In `data/RTTR/gamedata/civ/ai.lua`: Expansionsliste, Grundwirtschaft, Startvorräte ohne Überschuss, Höchstzahl je
  Gebäudetyp, Lager voll.

**Benötigt und expandierend** hängen allein vom Anlass ab, nie vom Gebäudetyp:
- **Benötigt:** Es gibt ein gemessenes Problem, und das Gebäude ist seine Lösung. Jedes Gebäude kann benötigt sein.
  Jeder Auftrag nennt seinen Anlass (`OrderCause`, im Log nach „benötigt:“):
  - Mangel einer Warengruppe (Prognose unter 0): Erzeuger, auch als Verbraucher eines Überschusses, wenn dessen
    Produkt im Mangel ist.
  - Eingang, der einem benötigten Auftrag fehlt (Kind-Auftrag, steht über ihm). Für Baumaterial gibt es keine
    Kind-Aufträge (Nutzer, 2026-11-17): Seine Erzeuger kommen nur über die Prognose (die Baukosten der Aufträge stehen
    unter „angefragt“). Die „Grundausstattung“ (ein Erzeuger je Baumaterialgruppe) war kurz umgesetzt und ist wieder
    entfallen (Nutzer, 2026-11-17).
  - Militärgebäude (Anlass „Gebiet“, Schritt 3): siehe „Militär im Planer“.
  - Soldaten fehlen: die Waren zum Rekrutieren.
  - Gehilfen fehlen laut Prognose, Lager voll: ein Lager.
  - Der Anlass „steht still“ entfällt (Nutzer, 2026-11-15): Der Verbrauch wartender Gebäude steht in der Prognose, der
    Mangel macht die Aufträge.
  - Wenige Bäume am dauerhaften Holzfäller: ein Förster.
- **Expandierend:** Es gibt kein Problem. Nur Gebäude der Expansionsliste (eine Erlaubnis), für eine Ware im Zustand
  „normal“ oder als Verbraucher eines Überschusses mit normalem Produkt. Dazu unabhängig von der Liste die Sammler von
  Vorkommen (Holzfäller, Steinbruch, Sammler aus Lua), sobald genug Vorkommen im Gebiet sind.
- Ein expandierender Auftrag löst nie ein Problem und hat keine Rechte: kein Vorrang, kein vorbehaltenes Material, kein
  vorbehaltener Eingang, kein Abbruch von Baustellen. Fehlt ihm etwas, wandert er ans Ende.
- **Expandierende Lösungsaufträge** (Nutzer, 2026-11-15): Ein expandierender Auftrag darf Aufträge für seine eigenen
  Probleme anlegen (Eingang, Gehilfen; Baumaterial nicht mehr, Nutzer 2026-11-17). Sie gelten selbst als expandierend
  (keine Rechte, Höchstzahl gilt) und dürfen auch Gebäude außerhalb der Expansionsliste sein (z. B. ein Brunnen für eine
  expandierende Brauerei).
  Baukosten, Werkzeuge und Arbeiter expandierender Aufträge stehen unter „angefragt“, solange sie nicht blockiert sind:
  Nur darüber entstehen aus ihnen benötigte Aufträge (z. B. Brettermangel → benötigtes Sägewerk).
- **Blockierte Aufträge zählen zu nichts** (Nutzer, 2026-11-17): Ein blockierter Auftrag existiert für keine Rechnung:
  nicht in „geplant“ (Baukosten, Werkzeuge, Arbeiter, Gehilfen), nicht bei der Anzahl als anderer Auftrag, nicht bei
  der Grenze der Bauarbeiter, nicht bei „für Aufträge darüber“ der Werkzeuge und Gehilfen. Ob der Auftrag darüber
  blockiert ist, spielt keine Rolle (vorher zählte ein benötigter Auftrag nicht, wenn einer darüber blockiert war).
  Benötigte und expandierende gleich; die frühere Grenze „expandierende nur so viele, wie Bauarbeiter da sind“ entfällt.
  - **Blockiert** (Lesart von Claude, vom Nutzer bestätigt 2026-11-18): kann nicht gebaut werden, bis sich die Lage
    ändert: nicht erforscht, kein Weg, kein Bauplatz, Höchstzahl des Typs, Lager voll, Eingang fehlt oder ist für einen
    Auftrag darüber vorbehalten (expandierend).
  - **Wartet auf Ressourcen** (kein Block, im Bericht „wartet“): Baumaterial, Bauarbeiter, Werkzeug, Gehilfen, Soldaten.
    Er wird gebaut, sobald sie da sind, und zählt wie ein wartender Auftrag. Grund: Zählen auch diese nicht (wörtliche
    Lesart), sieht die Prognose den Bedarf an Baumaterial nicht mehr, sobald alle Aufträge darauf warten; es kommt kein
    benötigter Holzfäller oder Steinbruch mehr, und die Wirtschaft bleibt stehen (Messung 2026-11-18: Land 10 082 /
    10 044 statt 13 116 / 17 560 vorher, Gebäude 126 / 93 statt 240 / 319, Zufallskarte 512, 4 KI, 2 h 50).
  - Es gilt der Zustand vom vorigen Schritt; am Ende jedes Schritts wird die Prognose mit den neuen Zuständen neu
    gerechnet (der Bericht passt zur Liste). Aufträge, die beim Abarbeiten entstehen (Erzeuger eines Eingangs), werden im
    selben Schritt abgearbeitet (bis zu `MAX_LIST_PASSES` = 4 Durchgänge).
  - Weiter gelten für blockierte Aufträge nur: Ein Auftrag zählt für seinen eigenen Bedarf (sonst käme jeden Schritt
    derselbe dazu) und für die Höchstzahl je Gebäudetyp. Ein benötigter Auftrag, dem Baumaterial fehlt, behält das
    vorhandene (Regel vom 15.11.; er wartet auf Ressourcen, ist also nicht blockiert).
- **Überschreiben:** Ein benötigter Auftrag ersetzt den gleichen expandierenden (gleiches Gebäude, gleiche Ware).
- **Eine Lösung für mehrere** (Nutzer, 2026-11-15): Fehlt einem Auftrag ein Eingang und gibt es für ihn schon einen benötigten Auftrag desselben Gebäudes (für einen anderen Auftrag, aus der Statistik oder schon
  platziert), ist dieser die Lösung für beide; es kommt keiner dazu. Er steht über allen, für die er gilt (Log:
  „Woodcutter (benötigt: Eingang, für Wood für Sawmill, Charburner)“). Ersetzt die frühere Übernahme des Auftrags aus der
  Statistik. Auch ein blockierter Auftrag ohne Weg gilt für alle Bedarfe derselben Ware mit demselben Gebäude.

**Warengruppen („Bedarfsgruppen“, statt Sonderregeln für „eins von“):**
- Eine Gruppe sind die Waren, die ein Abnehmer gleichwertig nimmt: die Eingänge „eins von“ (Nahrung der Minen), ein
  Baukosten-Eintrag nach Tags, die Werkzeuge eines Platzes und einer Art (Eisen- und Kupferaxt), dazu jede Ware allein.
  Die Gruppen kommen aus den Spieldaten und aus dem, was das Spiel anfragt; im Code ist keine Ware genannt.
- **Zuteilung der Einträge** (Fehler gefunden vom Nutzer 2026-11-17, ersetzt die Vereinigungen vom selben Tag): Vorher
  zählte ein Eintrag für jede Gruppe, die alle seine Waren enthält, nicht aber für die einzelnen Waren. Holz allein sah
  das Bauen nicht und stand im Überschuss (Prognose 75), während die Baugruppe mit Holz im Mangel war; die Vereinigungen
  (z. B. Wood/Boards/Stones/Thatch/Adobe/LoamBrick) waren Gruppen, die kein Abnehmer so nimmt. Jetzt:
  - Jede Anfrage ist ein Eintrag: Waren (eine davon reicht) und Menge. Offen: was das Spiel gerade anfragt. Verbrauch:
    was die Gebäude in der Bilanzlänge verbrauchen (eine Ware oder „eins von“). Geplant: was kommende Gebäude und die
    Aufträge der Liste anfragen werden, der Werkzeugvorrat.
  - Jede Ware hat ihren Bestand und ihre Herstellung in der Bilanzlänge. Die Einträge werden nacheinander auf ihre Waren
    verteilt: zuerst die mit den wenigsten Waren (eine Ware allein kann von nirgends sonst kommen), bei gleicher Zahl
    offen vor Verbrauch vor geplant, geplant in der Reihenfolge der Liste. In einem Eintrag zuerst die Waren, die
    weniger andere Einträge nehmen (Adobe vor Holz für die Mauern), bei Gleichstand die, von der mehr übrig ist. Jede
    Ware zählt nur einmal.
  - Eine Ware: ihr Bestand, ihre Herstellung und was ihr zugeteilt wurde (von allen Einträgen); was ihren eigenen
    Einträgen fehlt, macht ihre Prognose negativ.
  - Eine Gruppe: Bestand und Herstellung ihrer Waren, was ihren Waren zugeteilt wurde (von allen Einträgen, auch
    anderer Gruppen), und was den Einträgen genau dieser Gruppe von keiner Ware zugeteilt werden konnte (Bericht:
    „davon bekommt keine Ware“). Ein Mangel einer kleineren Gruppe in ihr zählt nicht: Ihre anderen Waren hätten den
    Einträgen dieser Gruppe geholfen.
  - Folge: Eine Ware im Überschuss hat wirklich etwas übrig; eine Gruppe mit ihr kann dann nur im Mangel sein, wenn
    deren Einträge die Ware nicht nehmen. Es gibt nur Gruppen, die ein Abnehmer so nimmt.
- Die Materialprüfung vor dem Platzieren (`DescribeMissingMaterial`, alte KI `HasMaterialFor`) zieht die Einträge
  nacheinander vom freien Material ab: zuerst die Einträge mit den wenigsten Waren, in jedem Eintrag zuerst die Waren,
  die weniger andere Einträge nehmen (Adobe vor Holz für die Mauern). Vorher prüfte jeder Eintrag für sich: Mit 4 Holz
  galten Mauern (4) und Dach (2) als vorhanden. Ein Auftrag löst sein Problem selbst, wenn seine Kette irgendeine Ware
  eines fehlenden Eintrags macht (ein Steinbruch für fehlende Mauern), nicht nur die erste.
- Über Aufträge entscheiden nur Gruppen, die ein Abnehmer genau so nimmt („demanded“). Die Ware einer Gruppe: die
  vorher gewählte, solange sie herstellbar ist, sonst die beste (bei Werkzeugen das bessere: Tempo und Ertrag, dann
  Haltbarkeit aus `tools.lua`).

**Soldaten als Ware:** Bestand = Soldaten in den Lagern, hergestellt = rekrutiert, verbraucht = ein Soldat verlässt ein
Lager zu einem Militärgebäude (Statistik `soldiersCreated/Used`, Spielstand 28), angefragt = fehlende Soldaten der
Militärgebäude. Fehlen Soldaten, werden die Rekrutierungswaren (`SOLDIER_RECRUIT_WARES` aus den Spieldaten: Schwert,
Schild, Bier) bestellt, soweit sie nicht reichen, und ein Lager für Gehilfen, wenn keine frei sind.

**Prognose** (Nutzer, 2026-11-15; ersetzt „Bilanz negativ = Mangel“): Jede Ware, Gruppe, die Soldaten und die Gehilfen
haben einen Wert für die nächste Bilanzlänge (`BALANCE_MINUTES`, 5 Minuten):

**Prognose = (Bestand − Angefragt) + voraussichtliche Herstellung − voraussichtlicher Verbrauch**

Beträge unter 0,005 gelten als 0 (2026-11-18): Rundungsfehler der Anteile und der Herstellung je Bilanzlänge
(z. B. −30 + 29,99999) sind kein Mangel, der Bericht zeigte sie als „Mangel … Prognose 0“.

- Angefragt = offen + geplant (Korrektur des Nutzers, 2026-11-17):
  - Offen: was das Spiel anfragt und kein Lager schicken konnte (Baukosten der Baustellen ohne verbaute, gelieferte und
    bestellte Waren, Eingänge stehender Gebäude, Werkzeugplätze, Münzen, Rüstung, Forschung, Expeditionen).
  - Geplant: jede Baustelle ihre Werkzeuge und ihr Arbeiter; jeder abgeschickte Baubefehl, dessen Baustelle noch nicht
    auf der Karte steht, volle Baukosten, Werkzeuge und Arbeiter; jeder Auftrag der Liste, der nicht platziert und
    nicht blockiert ist, Baukosten, Werkzeuge und Arbeiter (benötigte und expandierende gleich); der Werkzeugvorrat.
  - Keine Eingangsplätze (Nutzer, 2026-11-17): Was ein Gebäude verbrauchen wird, steht als Verbrauch des kommenden
    Gebäudes in der Prognose (Wert je Gebäude, sonst Arbeitszeit). Vorher zählten die Eingangsplätze der Baustellen und
    Baubefehle (ein Sägewerk 6 Holz) zusätzlich.
- Herstellung = gemessen in der letzten Bilanzlänge (was die Arbeiter der Gebäude hergestellt und an die Fahne ihres
  Gebäudes gelegt haben, auch von inzwischen abgerissenen Gebäuden; nicht, was Träger an Fahnen ablegen) + kommende
  Gebäude (Baustellen, abgeschickte Baubefehle, deren Baustelle noch nicht auf der Karte steht, Gebäude, die vor weniger
  als einer Bilanzlänge fertig wurden, anteilig; auch expandierende) voll mit dem **gemessenen Wert je Gebäude** ihres
  Typs. Aufträge der Liste stellen nichts her. Stehende Gebäude stellen nur her, was gemessen ist (kein Zuschlag für
  einen gelösten Grund).
  **Angehaltene Gebäude** (Nutzer, 2026-11-22; vom Spieler „Produktion stoppen“, auch von der alten KI, oder von der
  Gesamtverteilung, automatisch oder manuell, kein Unterschied) zählen mit ihrer vollen Arbeitskraft: In den Minuten
  der Bilanzlänge, in denen sie angehalten waren, stellen sie den Wert je Gebäude her (hochgerechnet wie die gemessene
  Herstellung, auch abgerissene). Ihr Verbrauch zählt dafür nicht: Eingänge nehmen sie erst, wenn sie wieder arbeiten.
  Beispiel (Testspiel 21.11., Seed 5, Spieler 2, 1:15): 7 Kupfersammler angehalten (Überschuss: Warenzahl 10 > 8),
  Kupfererz Prognose −2 = frei 10 + Herstellung 0,0 − Verbrauch 12,0 (neuer Bronzeschmelzer) → benötigte Kupfermine.
  Neu: mit angenommen 0,5 Erz/min je Sammler +35, Prognose 33, keine Mine. Eine Sperre „kein weiterer Auftrag eines
  Typs, solange eines angehalten ist“ gibt es nicht (Nutzer: „Overkill“).
- Wert je Gebäude (Nutzer, 2026-11-15): was alle Gebäude des Typs in der Bilanzlänge hergestellt bzw. verbraucht haben
  ÷ die Minuten, die sie in dieser Zeit fertig und nicht angehalten standen (Nutzer, 2026-11-22: angehaltene Minuten
  sagen nichts darüber, was ein Gebäude herstellt). Gebäude mit Problemen zählen mit (ein neues trifft
  wahrscheinlich auf ähnliche Probleme), auch das Warten auf Eingang, Werkzeug oder Arbeiter. Hat noch keines
  gearbeitet oder standen sie (fast) die ganze Zeit angehalten (weniger als eine Minute übrig), gilt die Arbeitszeit.
  Beispiel: 7 Sammler, 3 davon arbeiteten je 4 Minuten und machten 6 Erz: bisher 6 ÷ 70 = 0,09/min, neu 6 ÷ 12 =
  0,5/min. Statistik je Gebäudetyp: `EconomyStatistics::GetProducedBy/GetUsedBy` (Spielstand 29), angehaltene GF je
  Typ `GetStoppedBy` (Spielstand 31; gezählt bei jeder Rechnung der Gesamtverteilung, alle 200 GF je Spieler, mit dem
  Zustand in diesem Moment, `civ::recordStoppedBuildings`).
- Verbrauch = gemessener Verbrauch als Eingang von Gebäuden + kommende Gebäude mit dem gemessenen Wert je Gebäude +
  **jedes stehende Gebäude mit einem Problem** (Eingang, Werkzeug, Arbeiter, Fahne, Straße, Felder, Vorkommen weit,
  schlechtes Werkzeug) mit dem Teil, den es laut Arbeitszeit mehr verbrauchen würde (1 − Produktivität; Nutzer,
  2026-11-15: der Verbrauch zählt immer). Gebäude ohne Problem (die Diagnose findet nichts) stehen mit ihrem gemessenen
  Verbrauch drin; vom Spieler gestoppte und erschöpfte zählen nichts dazu. Was für Bau, Werkzeug, Soldaten,
  Forschung verbraucht wird, zählt hier nicht: Es wird vorher angefragt und steht in „Angefragt“ (sonst doppelt).
- Werkzeughersteller werden gemessen wie alle Gebäude (seit 2026-11-21: Die Gesamtverteilung steuert sie, es gibt keine
  Werkzeugbestellungen der KI mehr).
- Reserviert (Nutzer, 2026-11-21): Waren unterwegs (Straßen, Fahnen) zu einem Lager oder ohne Ziel zählen zum Bestand.
  Waren unterwegs zu einem Abnehmer zählen nicht, ihre Anfrage ist nicht mehr offen.
- Gehilfen: Bestand in den Lagern, angefragt = gesuchte Arbeiter + Arbeiter der Baustellen und Aufträge, deren Beruf
  nicht frei im Lager steht, Herstellung = von den Lagern erzeugt, Verbrauch = zu Soldaten oder Trägern geworden
  (Arbeiter stehen in „Angefragt“). Statistik `helpersForJobs` (Spielstand 29).
- Weniger als eine Minute gemessen (Spielstart): Die Zahlen werden wie eine Minute hochgerechnet, nicht die ersten
  Sekunden auf die ganze Bilanzlänge.
- **Mangel:** Prognose unter 0. **Überschuss:** Prognose über der Schwelle (10 × Verbrauch/min, mindestens 8, ohne
  unverbrauchten Startvorrat). Sonst normal.
- **Auswertung des Mangels mit dem Faktor Baumaterial** (Nutzer, 2026-11-21, `BUILDING_MATERIAL_FACTOR` = 0,5): Ob eine
  Warengruppe für benötigte Erzeuger im Mangel ist, wie viel pro Minute fehlt (Anzahl, auch Regel A) und ob ein
  Mangel-Auftrag noch nötig ist, entscheidet die **bewertete Prognose**: dieselbe Rechnung, aber alle Baukosten (offen
  bei den Baustellen und Expeditionen, geplant bei den Aufträgen und abgeschickten Baubefehlen, für alle Gebäude) zählen
  mal Faktor (0 = gar nicht, 1 = voll, 0,5 = zur Hälfte).
  - Grund: Ein Erzeuger von Baumaterial verbraucht erst Baumaterial, bevor er welches liefert. Voll gezählt vergrößern
    seine Baukosten den Mangel, es kommen weitere Erzeuger, deren Kosten ihn wieder vergrößern (Spirale). Die Kette
    bricht ab, wenn Faktor × Baukosten kleiner ist als das, was der Erzeuger in einer Bilanzlänge liefert (Holzfäller:
    4–6 Baumaterial der Gruppe, gemessen 0,5–0,9 Holz/min, also 5–9 in 10 Minuten; Faktor 1 kippt bei langsamen
    Holzfällern, 0,5 nicht). Da auch die offenen Baukosten mal Faktor zählen, verschlechtert keine Baustelle die
    Auswertung (auch keine Förster-Baustelle).
  - Der Faktor bestimmt auch, wie schnell der Rückstau wartender Aufträge abgebaut werden soll: Erzeuger kommen, bis
    die Herstellung einer Bilanzlänge Faktor × Baukosten − Bestand deckt, bei 0,5 also für etwa 20 statt 10 Minuten.
    Beispiel (Seed 9, Spieler 2, 30 min): Gruppe Holz/Bretter/Steine/Adobe Prognose −44,3 bei Bestand 8, geplant 99,9,
    Herstellung 45,5; bewertet −44,3 + 0,5 × 99,9 = +5,7, kein Mangel.
  - Nur dort. Die Prognose selbst zählt alles voll: Bericht (zusätzlich „bewertet …“), Reihenfolge der Liste, Regeln
    der Liste (Eingänge, Werkzeuge, Gehilfen; die Baumaterialregel prüft das freie Material im Lager), Zustand normal
    und Überschuss (expandierende Aufträge), Eingang eines Auftrags (sonst wartete er auf einen Eingang, für den kein
    Erzeuger mehr kommt), Gesamtverteilung („die Baukosten sollen ganz normal weiter so funktionieren“, „soll keinen
    Einfluss auf die Prioritätenliste haben“: Gälte der Faktor dort, hielte die KI mehr Gebäude für baubar, deren
    Kosten den geplanten Wert wieder hochtreiben).
  - Code: `EconomyForecast::Extra::constructionShare` (Anteil in 1/10 000), `WareStatus::evaluated`,
    `evaluatedShortage`, `evaluatedMissingPerMinute`. Bewertete und volle Prognose aus einer Rechnung
    (`RecomputeStatuses` mit denselben Messwerten). Test `ShortagesAreEvaluatedWithTheBuildingMaterialFactor`.
  - Sägewerk: eine richtige Lösung für einen Baumaterialmangel (Nutzer, 2026-11-21): Die Säge hat einen Ertragsbonus
    (`outputRatio` 0,4–0,7), aus 1 Holz werden 1,4–1,7 Bretter.
  - Testspiele (Zufallskarte 512, 4 KI, 3 h, je ein Spiel; Land und Gebäude als Summe der 4 Spieler): Faktor 1 Seed 5
    Land 39 654, Gebäude 1 381, Militärstärke wächst bei 1 Spieler ab 2:30; Seed 9 Land 39 749, Gebäude 1 232, 2
    Spieler ab 2:50. Faktor 0,75: Seed 5 36 104 / 1 278, 2 Spieler ab 2:10 und 2:30; Seed 9 43 446 / 1 193, 1 Spieler
    ab 2:00. Faktor 0,5: Seed 5 30 359 / 1 182, 1 Spieler ab 2:00; Seed 9 36 984 / 1 270, 2 Spieler ab 2:50 und 3:00.
    Benötigte Holzfäller wegen Mangel (Minutenberichte) bei 1 → 0,5: 381 → 78 und 458 → 88; auf Baumaterial wartende
    Aufträge +10 bis +13 %.
- **Eingang reicht** für einen weiteren Verbraucher: Überschuss, oder die Prognose bleibt mit seinem Verbrauch (Wert je
  Gebäude) bei 0 oder darüber. Gilt für jedes Gebäude, auch Werkzeughersteller.
- Wird in einem Schritt ein Gebäude platziert, wird die ganze Prognose neu gerechnet (die Aufträge darunter sehen es).

**Anzahl** (Nutzer, 2026-11-15: mindestens ein Auftrag, so viele, bis die Prognose 0 oder positiv ist), für jeden
Anlass: Erzeuger = ⌈fehlende Menge pro Minute ÷ Wert je Gebäude⌉, jeden Schritt neu (sinkt auch). Die Prognose enthält
die kommenden und stehenden Gebäude schon.
- Fehlend: Mangel = −Prognose ÷ Bilanzlänge; Eingang eines Auftrags = sein Verbrauch (Wert je Gebäude) − Prognose ÷
  Bilanzlänge.
- Alle nicht platzierten und nicht blockierten Aufträge für Gebäude, die eine Ware der Gruppe machen, zählen mit, auch für ein anderes Produkt
  (bei einem expandierenden Bedarf auch die expandierenden). Kleinere Gruppen werden zuerst bearbeitet; ihre Aufträge
  zählen für die größeren Gruppen mit ihren Waren (sonst bestellen „Fisch/Brot/Fleisch“ und „Brot/Fleisch“ getrennt).
- Die frühere Grenze „für einen anderen Auftrag höchstens einer je Ware“ entfällt: Auch für einen Eingang kommen so
  viele, bis die Prognose 0 erreicht.
- Grenzen: Reichen die Eingänge eines Erzeugers nicht oder lassen die Prognosen seiner Werkzeuge oder der Gehilfen keinen
  weiteren zu, wartet einer; ein Gebäude des Typs, das auf einen Eingang, ein Werkzeug oder einen Arbeiter wartet, zählt
  als dieser eine. Höchstens `MAX_ORDERS_PER_TYPE` je Gebäudetyp (ein benötigter Auftrag ersetzt den gleichen
  expandierenden, der zählt dafür nicht mit). Nie mehr wartende (nicht blockierte) benötigte Aufträge als Bauarbeiter
  (über alle Anlässe gezählt; ein Auftrag je Bedarf kommt immer, er zeigt, was fehlt).
- Ein Auftrag ist erledigt, sobald sein Gebäude fertig ist. Nur ein Förster bleibt bis nach seiner Anlaufzeit.

**Diagnose jedes Gebäudes** (Nutzer, 2026-11-15; ersetzt „Gebäude unter 80 % Produktivität“): jedes Gebäude, jeder
Schritt, unabhängig von seiner Produktivität. Das erste gefundene Problem:
- gestoppt (vom Spieler, nicht untersucht),
- keine Straße an der Fahne (nur gemeldet, Straßenbau kommt später),
- kein Arbeiter (kein Lager kann einen schicken) / Arbeiter unterwegs,
- kein Werkzeug: das Gebäude arbeitet ohne Werkzeug nicht (`baseSpeed` = 0) und hat keines, das Tempo gibt (Tempo 0);
  es wartet (Nutzer, 2026-11-18),
- Eingang fehlt,
- Fahne voll (nur gemeldet),
- nichts mehr in Reichweite (erschöpft),
- Felder fehlen: Bauernhof oder Plantage mit weniger als 4 Feldknoten in Reichweite,
- Vorkommen weit entfernt: nichts in der nahen Hälfte der Reichweite (nur gemeldet),
- Werkzeugplatz leer: ein Platz ist leer, das Gebäude arbeitet aber ohne dieses Werkzeug (langsam). Nur gemeldet, es
  zählt als arbeitend (nicht als wartendes Gebäude seines Typs, als Werkzeughersteller mit Kapazität); der Platz bestellt
  sein Werkzeug selbst (Nutzer, 2026-11-18),
- schlechtes Werkzeug: ein Werkzeug ist schlechter als das beste seines Platzes.

Reaktion: Eingang, Werkzeug, Arbeiter über die Prognose (der Verbrauch stehender Gebäude zählt, ihre Werkzeuge und
Arbeiter sind angefragt). Schlechtes Werkzeug: Das bessere wird zusätzlich angefragt, wenn die Prognose seiner Art einen
weiteren zulässt; das Gebäude nimmt es beim Bruch des alten aus dem Lager (Materialpriorität des Spiels). Felder fehlen:
Ein neuer Bauernhof bzw. eine neue Plantage kommt nur an einen Platz mit mindestens 4 Feldknoten. Im Bericht steht je
Gebäudetyp, wie viele Gebäude welches Problem haben.

**Werkzeuge** (Nutzer, 2026-11-15: mit der Wirtschaft verknüpft; jedes Gebäude mit Werkzeugplatz, unabhängig von
`baseSpeed`):
- Werkzeuge sind Waren mit Prognose. Gruppen: alle Werkzeuge einer Art (Eisen- und Kupferaxt), eines Werkzeugplatzes,
  ein einzelnes Werkzeug (z. B. als Forschungskosten).
- Angefragt zählt auch, was kommende Gebäude und alle Aufträge der Liste brauchen werden (die Werkzeuge der Plätze; das
  Werkzeug zum Rekrutieren des Arbeiters nur, wo der Beruf noch eines braucht), dazu der Vorrat `TOOLS_IN_STOCK` je Art.
  So entsteht der Mangel, bevor das Gebäude fertig ist.
- **Regel für jeden Auftrag:** Die Prognose jedes Werkzeugs, das das Gebäude zum Arbeiten braucht, muss mit ihm (und
  allen geplanten Werkzeugen der Baustellen und der Aufträge darüber) 0 oder mehr bleiben. Der Vorrat je Art ist für
  neue Gebäude da und sperrt sie nicht. Ein Werkzeughersteller rechnet die Werkzeuge mit, die er selbst machen wird.
  Sonst wartet der Auftrag („Werkzeug fehlt: Saw (Prognose …)“); der Werkzeugmangel bestellt die Werkzeuge und, wenn
  die Hersteller es nicht schaffen, weitere Hersteller.
- **Zum Arbeiten gebraucht** (Nutzer, 2026-11-18; Werkzeuge sind sonst nur ein Bonus): das Werkzeug zum Rekrutieren des
  Arbeiters (wo der Beruf eines braucht); bei der Werkzeugreform nur, wenn das Gebäude ohne Werkzeug nicht arbeitet
  (`baseSpeed` = 0), und dann ein Werkzeug irgendeines Platzes, das Tempo gibt (`speed` > 0; Tempo = `baseSpeed` +
  `speed` der Werkzeuge). Bei mehreren Plätzen: „Werkzeug fehlt: PickAxe (…) oder WoodenBucket (…)“. Ein Gebäude mit
  `baseSpeed` > 0 wartet nie auf Werkzeug. Die übrigen Werkzeuge (alle Plätze, auch die von Gebäuden mit
  `baseSpeed` > 0) zählen weiter als geplant und angefragt, ihre Hersteller kommen über die Prognose
  (`EconomyGraph::GetWorkTools`, `CheckToolsAndHelpers`). Dieselbe Regel gilt für „kann fertig arbeiten“ (höchstens ein
  wartender Auftrag je Typ) und für die Erreichbarkeit der Waren (`CivAI`: ein Platz genügt, vorher alle).
- Werkzeuge werden nicht bestellt (seit 2026-11-21): Ein Werkzeugmangel ist ein Mangel wie jeder andere, seine
  Hersteller kommen über „Anzahl“. Gebäude mit mehreren Produkten zählen nach Regel A (Abschnitt „Gesamtverteilung“).
- Gebäude mit mehreren Produkten dürfen expandierend kommen (Expansionsliste), aber nicht, solange ihre Gebäude
  schaffen, was alle ihre Produkte brauchen (Regel A ohne Mangel).

**Sortierung der Liste:**
1. Benötigte, die ihr eigenes Problem lösen, dann alle anderen benötigten, dann expandierende.
2. Benötigte: die Stufen der Grundwirtschaft (`ai.lua`), dann alle anderen. Darin Werkzeughersteller, Lager für
   Gehilfen und bei vollem Lager zuerst, dann nach der Prognose dessen, wofür sie sind (die niedrigste zuerst).
3. Expandierende (Nutzer, 2026-11-15): nur nach der Wirtschaft, keine Stufen, keine andere Regel. Wert = Prognose des
   Produkts; bei Verbrauchern (Prognose des Produkts − Prognose der Eingänge) ÷ 2, in Stück. Der kleinste Wert zuerst:
   Ein schwaches Produkt und ein Überschuss am Eingang schieben nach oben, ein knapper Eingang nach unten.
Ein Auftrag für einen anderen steht nie unter ihm.

**Abarbeitung:**
- Ein benötigter Auftrag, dem Baumaterial fehlt, behält das vorhandene in jedem Schritt (nicht nur im ersten). Er bearbeitet
  seine unerfüllte Regel in jedem Schritt (Bauarbeiter, Gehilfen): Ist seine Lösung weggefallen, wird sie neu bestellt.
  Für fehlendes Baumaterial bestellt er nichts (Nutzer, 2026-11-17).
- Löst er sein eigenes Problem, bekommt er das Material vor allen anderen; steckt die Kette 3 Minuten fest, werden
  Baustellen außerhalb der Kette ohne verbautes Material abgebrochen, nicht aber Baustellen benötigter Aufträge.
- Vorbehaltene Eingänge, Kind-Aufträge nur wenn alle Eingänge herstellbar, Höchstzahl je Gebäudetyp nur für
  expandierende Aufträge (benötigte ignorieren sie).
- Kein Bauplatz für einen Erzeuger: Stehen schon welche, ist der Weg nicht gesperrt (sie machen die Ware), es kommt nur
  keiner dazu. Wieder versucht, wenn sich das Gebiet ändert oder etwas erforscht wird.
- Blockierte Aufträge: nicht erforscht → bei neuer Forschung; kein Weg → jeden Schritt geprüft; kein Bauplatz → bei
  mehr Land oder weniger Gebäuden.

**Holzfäller und Förster:** Rodende Holzfäller und Förster stehen nie so, dass sich ihre Radien überschneiden (Abstand
größer als Radius des rodenden Holzfällers + Radius des Försters, in beide Richtungen): Der Förster würde den Wald
nachpflanzen, und der rodende Holzfäller würde nie abgerissen. Dauerhafte Holzfäller dürfen überall stehen, auch im
Bereich rodender; ihr Förster darf dort nicht hin, sie stehen dann still, bis der rodende abgerissen ist (Absicht des
Nutzers). Ein Förster ist benötigt, wenn weniger als 7 Bäume in Reichweite eines dauerhaften Holzfällers stehen. Einer
je Holzfäller zur Zeit; lassen die Prognosen seiner Werkzeuge oder der Gehilfen keinen weiteren zu, wartet einer, sonst
gilt die Grenze je Gebäudetyp.

**Rodender Holzfäller wird dauerhaft** (Nutzer, 2026-11-18): Hat ein fertiger rodender Holzfäller weniger als 7 Bäume
in Reichweite (`CLEARING_UPGRADE_BELOW`, Wert des Nutzers, 2026-11-18, vorher 5) und steht in der Liste ein benötigter Förster für einen Holzfäller (Anlass
„erneuert“, noch nicht platziert, nicht durch Forschung blockiert), wird dieser rodende Holzfäller dauerhaft (Befehl
„rodend“ aus) und bekommt den Förster. Von mehreren rodenden bekommt ihn der nächste zum ursprünglichen Holzfäller des
Försters; hat der Förster noch keinen Holzfäller mit Bauplatz, der nächste zu einem Lager, und der Auftrag dieses
Holzfällers ohne Bauplatz entfällt. Der Förster steht danach neben dem umgewandelten Holzfäller (Anlass „für den
Holzfäller (x,y) (rodend, jetzt dauerhaft)“), der ursprüngliche Holzfäller wartet auf den nächsten Förster. Logzeile
„rodender Holzfäller (x,y) wird dauerhaft, bekommt den Förster …“. Bis der Befehl ausgeführt ist, zählt der Holzfäller
schon als dauerhaft.

- **Offen (Nutzer, 2026-11-18, noch nicht umgesetzt):** Ein rodender Holzfäller wird nur dauerhaft, wenn sich ein
  Förster innerhalb seiner Reichweite platzieren lässt. Lässt sich keiner platzieren, bleibt er rodend.

**Eltern-Auftrag folgt sofort** (Nutzer, 2026-11-15; ohne eine Regel zu überspringen): Steht die Baustelle eines für
ihn platzierten Eingangs-Erzeugers, zählt sie voll in der Prognose, und der Auftrag wird im selben Schritt neu geprüft.
Reicht die Prognose seines Eingangs, folgt er sofort, möglichst nah an der Baustelle; reicht sie nicht, kommen weitere
Erzeuger, bis die Prognose 0 erreicht. Weitere Eingänge desselben Auftrags kommen nah an den ersten. Solange ein Auftrag
auf eine solche Baustelle wartet, plant die KI alle 20 GF (`FOLLOW_UP_GFS`), unabhängig vom Schwierigkeitsgrad.

**Platzwahl** (Nutzer, 2026-11-17: die Regeln vom 15.11., besser umgesetzt; die Plätze der alten KI gelten für den
Planer nicht mehr, nur die unten genannten Sperren bleiben). Gründe des Fehlschlags vom 15.11.: Der höchste Punktwert über
bis zu 8 Suchzentren gewann, größere Vorkommen (bis 40 Punkte) schlugen die Nähe (30 bzw. 20); Förster kamen mit dem
Code der alten KI an das Lager statt an den Holzfäller; Lager hatten keine Abstandsregel.
- **Anker**, zuerst in dieser Reihenfolge, je im Radius 15 (`SEARCH_RADIUS`): der vorgegebene Punkt (Förster an
  seinem Holzfäller, Abbauer neben seinem Anbauer), die Baustelle eines für den Auftrag platzierten Eingangs, bei
  verarbeitenden Gebäuden die drei Partner, die am nächsten an einem Lager stehen (Erzeuger eines Eingangs, Abnehmer des
  Produkts, Gebäude und Baustellen). Baumaterial-Erzeuger (Produkt steht in Baukosten) ankern nicht an Partnern.
  Danach die Gebäude mit Gebiet (Hauptquartier, Häfen, Militärgebäude), jedes in seinem Militärradius: zusammen
  decken sie das ganze Gebiet ab, für alle Gebäudetypen (Nutzer, 2026-11-18; vorher die Lager im Radius 15, Lager im
  Radius 25, Militärgebäude nur für Abbauer und Lager; Testpartie des Nutzers vom 18.11.: 7 gültige Bauplätze für einen
  Bauernhof lagen 21–28 vom nächsten Lager). Das nächste an einem Lager zuerst (kurze Wege; Claude, vorher reihum ab
  wechselndem Anker: im Vergleich mehr Land und früher Militärstärke). Bis zu 8 Anker je Schritt
  (`SEARCHES_PER_STEP`), die Suche geht im nächsten Schritt beim nächsten Anker weiter.
- **Nähe geht vor:** Gesucht wird vom Anker nach außen. Der nächste Platz, der alle Regeln erfüllt, legt die Entfernung
  fest; nur Plätze bis `PLACE_BAND` = 2 weiter kommen in Frage. Unter ihnen entscheiden die Vorlieben des Nutzers:
  Vorkommen in Reichweite (Abbauer, bis 40), −10 je Gebäude gleichen Typs in der Nähe (Abbauer doppelte Reichweite,
  sonst 8), −0,5 je Feld Abstand zum nächsten Partner (verarbeitende Gebäude). Gleich: der nächste am Anker.
- **Regeln für jeden Platz:** erreichbar, eigenes Gebiet, Bauplatzgröße, Abstand zur Grenze außer Militär (11.11.),
  nicht an einem Hafenplatz, nicht auf Feldern und möglichen Feldplätzen von Plantagen; Mindestmenge in Reichweite
  (rodender Holzfäller 8 Bäume, dauerhafter Abbauer einer Quelle mit Erneuerer `RENEW_BELOW`, sonst 1); Bauernhof und
  Plantage mit mindestens 4 Feldknoten; rodender Holzfäller nicht im Bereich eines Försters; Köhler nicht im Bereich
  eines Försters (Radius Köhler + Förster, in beide Richtungen, wie bei rodenden Holzfällern; Nutzer 2026-11-17).
- **Sperren der alten KI, die bleiben** (`AIPlayerJH::IsPlaceAllowed`): Jäger erreicht mindestens 9 Tiere (ohne die
  Sperre „kein Jäger im Radius 14“); Steinbruch und Fischer erreichen ihre Steine und Fische; Lager 15 von anderen
  Lagern entfernt; Bauernhof Pflanzfläche ≥ 85, Köhler ≥ 86 und genug Bauplätze ringsum.
- **Entfallen** (Nutzer, 2026-11-17): höchstens 2 Holzfäller je Förster, Walddichte 8 % für Holzfäller ohne Förster (es
  gilt die Mindestmenge), Förster-Regeln der alten KI (die ersten 4 ohne Wald, ab dem 5. kein Förster/Köhler im Radius 8,
  Sägewerk und Lager im Radius 8), alle Sägewerk-Regeln, der steigende Mindestwert für Steinbrüche.
- Förster für einen Holzfäller: in dessen Reichweite, beste Pflanzfläche, nicht im Bereich rodender Holzfäller und
  Köhler (`AIPlayerJH::FindForesterPosition`).

**Militär im Planer** (Schritt 3, Nutzer 2026-11-17): Der Planer entscheidet, wann, wo und welche Militärgebäude gebaut
werden. Truppen, Angriffe, Münzen und Abriss bleiben bei der alten KI; sie baut im Zivilisationsmodus keine
Militärgebäude mehr (`AIPlayerJH::AddMilitaryBuildJob`).
- **Wann** (Anlass „Gebiet“, `OrderCause::Territory`):
  - Ein Auftrag findet keinen Bauplatz: ein Militärgebäude für ihn, benötigt für einen benötigten, expandierend für
    einen expandierenden. Für einen Weg ohne Bauplatz (der Auftrag geht, der nächste Weg wird versucht) gilt es, solange
    der Weg gesperrt ist.
  - Die Quelle eines Wegs liegt nur außerhalb des Gebiets (auf der Karte vorhanden, z. B. „nicht im Gebiet: Lehm“):
    benötigt, wenn der Bedarf benötigt ist, sonst expandierend. Gilt, solange der Bedarf besteht und die Quelle nicht im
    Gebiet ist.
  - Sonst ein expandierendes Militärgebäude (ohne Rechte, sortiert wie expandierende Aufträge nach der Prognose der
    Ware, für die seine Zielregion am meisten zählt). Immer nur eines, das noch nicht platziert ist.
  - Platzierte Militäraufträge bleiben bis zum fertigen Gebäude in der Liste: höchstens `MAX_ORDERS_PER_TYPE` je Typ
    mit ihren Baustellen. Die Grenze der alten KI von 20 Baustellen und das Freihalten von Material fürs Militär
    entfallen; es gelten dieselben Regeln wie für jeden Auftrag (Material, Bauarbeiter). Dazu die Regel der alten KI:
    je Militärbaustelle (mit der neuen) ein Soldat frei in den Lagern („keine Soldaten frei“).
- **Wo:** Regionen der Karte (8 × 8), deren Mitte höchstens `MAX_TERRITORY_DISTANCE` = 24 von einem eigenen
  Militärgebäude oder Lager liegt und keinem anderen Spieler gehört. Wert = Σ Knappheit × Menge außerhalb des Gebiets
  (je Quelle auf die größte Menge normiert) + Land (1 für ein Gebäude ohne Bauplatz, 0,25 beim Expandieren) − 0,5 ×
  Entfernung ÷ 24 − 1 je feindliches Militärgebäude in der Nähe. Knappheit aus der Prognose (Nutzer: niedriger
  Prognosewert = hohe Knappheit): (8 − Prognose) ÷ 8, ab 8 null. Für einen Bedarf zählt nur seine Quelle (ohne Quelle:
  Land). Die besten Regionen der Reihe nach (bis 4 je Suche): über Land erreichbar, Platz an der Grenze am nächsten
  Punkt des Gebiets (`FindPositionForBuildingAround` der alten KI, Grenzland).
- **Welcher Typ:** wie die alte KI an diesem Punkt (`AIConstruction::ChooseMilitaryBuilding`), der kleinste, wenn dafür
  kein Platz oder kein Material da ist. Geplant werden die Kosten des kleinsten.
- Bericht: „Barracks (benötigt: Gebiet für Storehouse, kein Bauplatz, Ziel (x,y))“, „(expandierend: Gebiet, für Wood,
  Ziel …)“, „… Lehm nur außerhalb“.

**Vergleich Schritt 3** (Zufallskarte 512 × 512 des Kartengenerators, 4 KI, 3 h, `ai-battle --random-map 512
--map-seed 5|9`; vorher = `5f33424b`, Militär der alten KI). Kleine Karten und 2 h sagen wenig (Nutzer, 2026-11-17: die KI
lebt so lange vom Startvorrat oder kämpft schon), wichtig ist, ab wann die Militärstärke wächst und um wie viel.

| Kartenzufall 5 / 9 | vorher | Schritt 3 |
|---|---|---|
| Land nach 30 min | 9 752 / 7 393 | 4 719 / 4 726 |
| Land nach 1 h | 13 611 / 7 966 | 6 793 / 4 726 |
| Land nach 2 h | 26 385 / 25 796 | 13 070 / 8 801 |
| Land nach 3 h | 34 790 / 38 888 | 23 592 / 22 707 |
| Gebäude / Militärgebäude nach 3 h | 488 / 181, 563 / 201 | 368 / 133, 392 / 136 |
| Förster nach 3 h | 22 / 30 | 24 / 24 |
| Militärstärke (Statistik) | 52 bei allen, 3 h lang | 52 bei allen, 3 h lang |
| Rechenzeit je Schritt (Mittel / Spitze) | 2,1 ms / 16–99 ms | 2,8–3,1 ms / 31–36 ms |

- Mit denselben Regeln für alle Aufträge wächst das Gebiet nur halb so schnell: Die expandierenden Militäraufträge
  stehen am Ende der Liste, die benötigten Aufträge behalten das wenige Baumaterial (z. B. 5 im Lager, 15 benötigte
  Aufträge, keiner vollständig). Weniger Land heißt weniger Bäume und Steine. Die alte KI hielt Material für das nächste
  Militärgebäude vor den expandierenden Aufträgen frei.
- Die Militärstärke wächst in keiner Fassung: Die Soldaten aus dem Startvorrat reichen 3 h für alle Besatzungen, die
  Soldaten sind nie knapp, also wird nie rekrutiert.

**Vergleich Korrektur der Rechnung** (2026-11-18: Zuteilung der Einträge, keine Eingangsplätze, blockierte Aufträge
zählen nicht; Zufallskarte 512 × 512, 4 KI, `--random_init 7`, Summe der 4 Spieler; vorher = `27d993dd`):

| Kartenzufall 5 / 9 | vorher | neu |
|---|---|---|
| Land nach 30 min | 4 637 / 4 891 | 5 903 / 5 233 |
| Land nach 1 h | 5 825 / 4 891 | 7 380 / 5 401 |
| Land nach 2 h | 9 629 / 9 140 | 12 885 / 11 030 |
| Land nach 2 h 50 | 13 116 / 17 560 | 20 224 / 17 886 |
| Gebäude nach 2 h 50 | 240 / 319 | 373 / 335 |
| Militärstärke | 208 durchgehend | 208 durchgehend (207 am Ende bei 5) |

- Wörtliche Lesart (auch auf Material wartende Aufträge zählen nicht): 10 082 / 10 044 Land, 126 / 93 Gebäude.
- `tools/checkEconomyLog.py`: im Log des Nutzers vom 17.11. 178 Überschüsse in einer Mangelgruppe, in den neuen Läufen
  keine Widersprüche.
- Die Militärstärke wächst weiter nicht (keine Rekrutierung, Soldaten nie knapp): offene Frage an den Nutzer.

**Vergleich Abbauer je Vorkommen und rodende Holzfäller** (2026-11-18; gleiche Karten und Einstellungen; vorher =
`8acc348`):

| Kartenzufall 5 / 9 | vorher | neu |
|---|---|---|
| Land nach 30 min | 5 903 / 5 233 | 6 152 / 5 362 |
| Land nach 1 h | 7 380 / 5 401 | 10 424 / 8 455 |
| Land nach 2 h | 12 885 / 11 030 | 25 534 / 21 618 |
| Land nach 2 h 50 | 20 224 / 17 886 | 34 793 / 31 073 |
| Gebäude nach 2 h 50 | 373 / 335 | 539 / 496 |
| Abbauer, nach weniger als 3 min abgerissen | 27 / 11 | 0 / 2 |
| Militärstärke | 208 (207 am Ende bei 5) | 208 durchgehend |

- Rechenzeit je Schritt im Mittel 2,9 → 3,6 ms. Die Spitzen (bis etwa 190 ms) kommen aus der Platzsuche der alten KI
  für den Jäger (`AnimalsInRange` mit Wegsuche je Tier, schon vorher so); sie zählt jetzt zuerst die Tiere und sucht
  Wege nur, wenn genug da sind (gleiches Ergebnis).
- `tools/checkEconomyLog.py`: keine Widersprüche.

**Vergleich Werkzeug nur als Bonus** (2026-11-18: ein Gebäude wartet nur auf Werkzeug, wenn es ohne nicht arbeitet;
Zufallskarte 512 × 512, 4 KI, 3 h, Startwaren „viel“ (`--wares alot`), `--random_init 7`, Spieldaten und Konstanten des
Nutzers vom 18.11.; vorher = `390ac0e`; neu mit der Rundung der Prognose):

| Kartenzufall 5 / 9 | vorher | neu |
|---|---|---|
| Land nach 30 min | 9 473 / 9 001 | 6 700 / 7 314 |
| Land nach 1 h | 15 479 / 16 922 | 13 371 / 12 695 |
| Land nach 2 h | 41 340 / 44 870 | 25 588 / 23 830 |
| Land nach 3 h | 56 051 / 63 302 | 43 049 / 55 365 |
| Gebäude nach 3 h | 870 / 966 | 1 347 / 1 560 |
| Militärstärke nach 3 h (Start 416) | 349 / 416 | 607 / 494 |
| Militärstärke wächst ab | nie / nie | 1 h 30 / 1 h 50 |
| Berichte „wartet: Werkzeug fehlt“ | 5 234 / 5 473 | 224 / 309 |
| Rechenzeit je Schritt im Mittel | 7,1 / 8,7 ms | 6,4 / 6,5 ms |

- Vorher warteten vor allem LoamPit und die Sammler (WoodenBucket), SpinningMill (WoodenSpindle), Förster und
  Holzfäller, alle mit `baseSpeed` 25. Jetzt warten nur noch Gebäude mit `baseSpeed` 0
  (Farm, Hunter, TinCollector, Goldmine, Slaughterhouse, Mint).
- Bis 1 h im Mittel weniger Militärbaustellen (2,7 / 2,4 statt 3,1 / 3,4): Die Wirtschaftsaufträge, die nicht mehr
  warten, stehen in der Liste über den Militäraufträgen und nehmen das Baumaterial. Daher bis 2 h weniger Land.

**Vergleich Platzsuche im ganzen Gebiet** (2026-11-18; Spieldaten und Konstanten des Nutzers vom 18.11. abends, u. a.
`UNITS_PER_HARVESTER` 32; vorher = `30efd3d`):

| Zufallskarte 512, 4 KI, `--wares alot`, Kartenzufall 5 / 9 | vorher | neu |
|---|---|---|
| Land nach 1 h | 14 467 / 15 503 | 12 532 / 12 561 |
| Land nach 2 h | 32 778 / 39 564 | 28 904 / 35 666 |
| Land nach 3 h | 63 062 / 63 811 | 54 425 / 59 570 |
| Gebäude nach 3 h | 1 620 / 1 771 | 1 598 / 1 746 |
| Militärstärke nach 3 h (Start 416) | 541 / 441 | 546 / 511 |
| Militärstärke wächst ab | 2 h 10 / 2 h 40 | 1 h 10 / 2 h |

| Karte der Testpartie des Nutzers, 8 KI, nach 1 h / 1 h 30 | vorher | neu |
|---|---|---|
| Land | 29 126 / 47 554 | 24 864 / 38 433 |
| Bauernhöfe | 51 / 110 | 80 / 119 |
| Köhler | 2 / 11 | 7 / 27 |
| Schlossereien | 0 / 0 | 0 / 7 |

- Reihum ab wechselndem Anker statt „nächstes am Lager zuerst“: Land nach 3 h 49 980 / 59 476, Militärstärke wächst
  ab 2 h / 2 h 20.
- Weniger Land: Die Wirtschaftsaufträge finden öfter einen Platz und nehmen das Baumaterial vor den Militäraufträgen.
- Rechenzeit je Schritt im Mittel 7,0 / 7,3 → 6,2 / 7,1 ms.

**Anbauer und Abbauer** (Nutzer, 2026-11-15): Findet ein benötigter dauerhafter Abbauer keinen Platz mit genug Vorkommen
in Reichweite, wird zuerst ein Anbauer für ihn beauftragt (Erneuerer der Quelle: Förster, Plantage aus Lua ohne Produkt;
Anlass „erneuert“, Kind des Abbauers, steht über ihm). Steht dessen Baustelle, kommt der Abbauer direkt daneben (nächster
Platz in Reichweite des Anbauers, Log „neben (x,y)“). Dort zählt die Menge in Reichweite nicht, alle anderen Regeln gelten
(Material, Bauarbeiter, Grenze). Der Anbauer erneuert danach um diesen Abbauer
(kein zweiter während seiner Anlaufzeit). Findet auch der Anbauer keinen Platz: kein Bauplatz für diesen Weg.

**Höchstzahl Abbauer je Vorkommenmenge** (Nutzer, 2026-11-18; gilt für alle Ressourcen auf der Karte: Bäume,
Steinhaufen, Kartenobjekte wie Kupfer, Zinn, Obsidian, Torf, Lehm, Erze und Fische): Jeder Abbauer bekommt höchstens
16 Einheiten (`UNITS_PER_HARVESTER`, für alle Quellen gleich; Wert des Nutzers, vorher 8) der Vorkommen in seiner Reichweite. Ein Knoten, den k andere
Abbauer derselben Quelle erreichen (stehende, Baustellen und im selben Schritt platzierte), zählt mit Menge / (k + 1).
Ein Platz ist nur gültig, wenn die so geteilte Menge mindestens 16 erreicht; teilt kein anderer Abbauer die Knoten,
genügt eine Einheit. Bäume, Steinhaufen und Kartenobjekte zählen nur, wenn der Arbeiter sie vom Platz aus zu Fuß
erreicht (Weg höchstens 20 Schritte, wie beim Arbeiter). Mengen: Baum 1, Steinhaufen und Kartenobjekte Größe + 1, Erz
und Fisch die Menge auf dem Knoten. Neben seinem Erneuerer (Anlass „neben (x,y)“) gilt die Regel nicht.

Ursache der Abbauer, die nie arbeiteten (Replay des Nutzers vom 18.11.: 16 Sammler, meist CopperCollector, ohne
einen Arbeitsgang nach etwa 5 min abgerissen): Bei den meisten hatten andere Sammler das Vorkommen während des Baus
abgebaut (im Mittel 2,1 Sammler derselben Art in Reichweite), bei einem war das Vorkommen nicht erreichbar (von
Granit umschlossen). Ein Spiel mit 8 KI auf der Karte und mit den Spieldaten des Nutzers (1 h 40) danach: kein Sammler
ohne Arbeitsgang, im Mittel 1,5 Sammler derselben Art in Reichweite.

**Bericht** (Log „ECONOMY“, jede Spielminute):
- „Mangel ohne eigenen Auftrag“: warum ein Mangel keinen Auftrag hat, z. B. „gedeckt: fehlen 1,8/min,
  andere Aufträge 1 (je 1,3/min)“ oder „Woodcutter: kein Bauplatz, es kommt keiner dazu (4 stehen)“.
- Am Auftrag: Eingänge ohne eigenen Auftrag und warum (Nutzer, 2026-11-18), z. B. „Charburner … wartet: Eingang fehlt:
  Grain – ohne eigenen Auftrag: Grain: Farm: kein Bauplatz, es kommt keiner dazu (5 stehen)“. Ein Eingangsauftrag ohne
  Bauplatz wird entfernt, sein Weg ist gesperrt, bis sich Gebiet oder Forschung ändern; der Grund stand vorher nur in
  „Mangel ohne eigenen Auftrag“ (Mangel der Ware), nicht am wartenden Auftrag.
- Jede Ware im Mangel und Überschuss mit ihrer Prognose: „Prognose −21 = frei −27 + Herstellung 15,0 − Verbrauch 8,9“.
  Zugeteilte Teile von Einträgen mehrerer Waren sind keine ganzen Zahlen (eine Nachkommastelle). Bei Gruppen „davon
  bekommt keine Ware X“: was den Einträgen genau dieser Gruppe fehlt.
- Prüfung der Logs: `tools/checkEconomyLog.py <log>` sucht Widersprüche (Formel, frei, Überschuss einer Ware in einer
  Mangelgruppe).
  Eigener Abschnitt „Werkzeugmangel“ je Werkzeuggruppe.
- „Zuletzt abgerissen“: Abbauer, die die alte KI wegen „nichts mehr in Reichweite“ abreißt, mit Anlass, Vorkommen beim
  Platzieren und Zeit mit Arbeiter (auch als eigene Logzeile).
- „Gehilfen“: die Prognose der Gehilfen wie bei den Soldaten.
- „Baustellen“: Militär (alte KI) und Wirtschaft mit dem Material, das sie noch brauchen, ohne Straße, ohne Material
  unterwegs.
- „Probleme“: je Gebäudetyp die Zahl der Gebäude mit jedem Problem der Diagnose.
- „offen: neu, im nächsten Schritt“: im Schritt entstanden (z. B. ein Kind-Auftrag). „offen: suche Bauplatz“: die Suche
  (8 Lager/Militärgebäude je Schritt) geht im nächsten Schritt weiter.
- Ein „eins von“-Eingang nennt die ganze Gruppe („Eingang fehlt: Fish/Meat/Bread“).

**Vergleich zur Prognose** (Karte und Einstellungen aus dem Replay des Nutzers vom 15.11., 8 KI; vorher = `4623c4e`):

| | vorher | neu |
|---|---|---|
| Berichte mit Holzmangel, 40 min, Zufallswert 7 / 11 | 205 / 180 | 24 / 46 |
| davon ohne benötigten Holzfäller | 53 / 43 | 4 / 6 |
| Mangel-Einträge mit über 100 im Lager, 2 h | 243 | 0 |
| Berichte „Förster blockiert: Werkzeug fehlt“, 2 h | 3 604 | 657 |
| Berichte „Werkzeug fehlt“ insgesamt, 2 h | 5 401 | 2 098 |
| Abgerissene Abbauer, 2 h | 1 091 | 632 |
| Land nach 40 min (Summe 8 Spieler), Zufallswert 7 / 11 | 35 417 / 34 340 | 33 331 / 33 260 |
| Gebäude nach 40 min | 677 / 683 | 657 / 694 |
| Land / Gebäude nach 2 h (Zufallswert 7) | 55 908 / 1 803 | 55 050 / 2 427 |
| Forschung nach 2 h (Ø Technologien) | 22,8 | 22,8 |
| Rechenzeit je Schritt (Mittel, 2 h) | 2,0 ms | 1,8 ms |

- Land und Gebäude streuen stark (ein bis zwei Läufe, Krieg der alten KI).
- Seltene Spitzen bis 45 ms (Platzsuche mit Bewertung über 8 Mittelpunkte).

**Vergleich zur Fassung vom 17.11.** (7 Karten wie unten, je 2 KI, 3 Zufallswerte = 42 Spieler, 2 h; Spieldaten und
Konstanten des Nutzers vom 15.11. in beiden Fassungen; vorher = `a4a1803`):

| | vorher | neu |
|---|---|---|
| Land nach 1 h / 2 h | 156 674 / 179 005 | 154 353 / 175 398 |
| Gebäude nach 1 h / 2 h | 4 143 / 6 532 | 4 149 / 5 533 |
| Militärgebäude nach 2 h | 2 090 | 2 059 |
| Spieler unter 1000 Land nach 2 h | 4 | 1 |
| Forschung (Ø Technologien) | 15,4 | 15,7 |
| Schlossereien nach 2 h (je Spieler) | 488 (11,6) | 115 (2,7) |
| Eisenschmelzen / Kohleminen / Eisenminen nach 2 h | 335 / 387 / 240 | 142 / 191 / 118 |
| Mühlen / Waffenschmieden nach 2 h | 335 / 82 | 123 / 43 |
| Holzfäller / Förster / Sägewerke nach 2 h | 679 / 343 / 262 | 731 / 472 / 296 |
| Bauernhöfe / Steinbrüche nach 2 h | 317 / 288 | 405 / 325 |

- Neue Fassung, je Bericht (Spieler und Minute): Sägewerke ohne Säge 0,18, Holzfäller ohne Axt 0,52, Eisenschmelzen ohne
  Tiegel 0,07; Bauernhöfe mit zu wenig Feldern 1,8.
- Mit den Spieldaten vom 17.11. (`cb4411e`) läuft der Planer fehlerfrei; die Wirtschaft ist kleiner (längere
  Arbeitszeiten). Beobachtet: Die Baustellen der Militärgebäude der alten KI belegen zeitweise fast das ganze Baumaterial
  (51 von 52); expandierende Aufträge (rodende Holzfäller) kommen dann lange nicht dran. Klärt sich mit Schritt 3.

**Prüfen auf der Karte eines Spiels:** `ai-battle --from-replay <datei.rpl> --ai aijh ...` nimmt Karte und Einstellungen
(Addons, Ziel, Startwaren) aus einer Aufzeichnung und spielt sie mit KI-Spielern. Ein Replay lässt sich nicht genau
nachspielen: Die alte KI würfelt mit echtem Zufall. Das Addon „Gebäude ohne Rohstoffe abreißen“ wird in `ai-battle`
abgeschaltet (es gilt nur für den lokalen Spieler, den es dort nicht gibt).

**Vergleich** (7 Karten, je 2 KI, je 3 Zufallswerte = 42 Spieler, gegen die Fassung vom 14.11., `7f59ed0`):

| | vorige Fassung | neu |
|---|---|---|
| Land nach 1 h | 161 762 | 164 799 |
| Gebäude nach 1 h | 3 684 | 4 147 |
| Militärgebäude nach 1 h | 1 234 | 1 222 |
| Forschung nach 1 h (Ø Technologien) | 11,6 | 12,8 |
| Land nach 2 h | 182 223 | 184 749 |
| Gebäude nach 2 h | 5 088 | 4 895 |
| Militärgebäude nach 2 h | 2 642 | 1 982 |
| Spieler unter 1000 Land nach 2 h | 6 | 10 |

- Karten: GreenPlains, Bergstrasse, DerGrosseSee, Hochebene, AufBreiter, TueranTuer, Landstr; Zufallswerte 7, 11, 23;
  Addon nur Zivilisationsmodus. Nach 2 h entscheidet oft der Krieg (Vorherrschaft): Die Ergebnisse streuen stark.
- Weniger Militär nach 2 h: Das Militär baut noch die alte KI; der Planer hält ihr Material nur vor den expandierenden
  Aufträgen frei, die benötigten (jetzt mehr) kommen davor.
- Rechenzeit (Release): im Mittel unter 1,5 ms je Schritt, Spitzen bis 26 ms; Liste höchstens 67 Einträge.

**Notprogramm** (Nutzer, 2026-11-17): Im Zivilisationsmodus gibt es kein Notprogramm. Es lieferte Bretter und Steine nur an
Holzfäller und Sägewerke, sobald Bretter oder Steine ≤ 10 waren und kein Sägewerk stand; mit den Baukosten nach Tags
(Holz, Lehmziegel, Stroh) war es fast immer aktiv und hielt alle anderen Baustellen an. Außerdem versucht eine
Baustelle mit „eins von“-Kosten jetzt die anderen Waren, wenn eine nicht geschickt werden kann
(`GamePlayer::OrderWare`).

**Offen:**
- Das Militär der alten KI verbrauchte das Baumaterial (bis 20 Baustellen, Material vor den expandierenden Aufträgen
  freigehalten). Mit Schritt 3 entschieden; Ergebnis siehe „Vergleich Schritt 3“.
- Förster: In den Testspielen vom 17.11. hatte kein Spieler einen Förster. Förster kommen nur für dauerhafte
  Holzfäller, die nur bei Holzmangel benötigt werden; mit den Baukosten nach Tags deckt meist ein Steinbruch die
  Baumaterialgruppe, die Holzfäller sind rodend.
- Forschung (Schritt 4, nach dem Militär): wählt noch das Spiel für die KI (`civ::chooseResearchForAI`).
- Mangel eines Zwischenprodukts, dessen Endprodukt im Überschuss liegt: bewusst keine Regel (Nutzer, 2026-11-15).
- Umbau der Eingänge nach Tags (ein Eingang je Tag, mehr als 3, Mischen).

## Notizen aus den Lua-Dateien, Schritt 1 (Stand 2026-11-24)

Sammlung aller Notizen und Antworten des Nutzers: Projektdokument `claude/notizen-2026-11-22-lua-kommentare.md`,
Antworten im Dokument „S2 Modding – Offene Fragen zu den Lua-Notizen“ (Claude Docs). Umgesetzt in Schritt 1:

- **`kind` als Liste (A2):** `kind = "Ore"` oder `kind = { "Fibers", "BasicRoofMaterial" }` in `rttr:AddWare` und
  `rttr:SetWare`. `properties` bleibt unverändert (nur sie gibt `inheritProperties` weiter). Code:
  `WareDesc::kind`, `BuiltinWareDesc::kind` als `std::vector<std::string>` (gelesen mit `parseTagQuery`),
  `Content::SetTags`. Daten: „BasicRoofMaterial“ bei Holz, Brettern, Steinen und Thatch von `properties` nach `kind`
  (F-31), LoamBrick `kind = { "Stones", "BasicRoofMaterial" }` (F-25).
- **`worker.tool` entfernt (C4):** Der Arbeiter einer Werkstatt hat kein Werkzeug mehr; das Feld `tool` ist ein
  Ladefehler. Jeder Helfer wird ohne Werkzeug Arbeiter einer Werkstatt, auch ohne Werkzeugreform
  (`JOB_CONSTS[custom].tool` = `GoodType::Nothing`). Werkzeuge einer Werkstatt gibt es nur über `tools`; ohne `tools`
  braucht sie kein Werkzeug und arbeitet mit Tempo 100 (`makeDefaultTools(GoodType::Nothing)`). Die ObsidianWorkshop
  hatte `tools = { baseSpeed = 25 }` ohne Werkzeugtypen (immer 25 %); laut Nutzer (F-32) soll sie kein Werkzeug haben,
  die Zeile ist entfernt (Tempo 100).
- **Oder-Voraussetzung (D1):** `requires = { "storage", { "milling", "animal_husbandry" } }`: eine Liste in der Liste
  heißt „mindestens eine davon“, sie braucht mindestens 2 Einträge; jede Technologie darf nur einmal vorkommen.
  `TechnologyDesc::requirements` und `Technology::requirements` sind Gruppen (`std::vector<std::vector<…>>`),
  `civ::getTechState` prüft je Gruppe, das Forschungsfenster zeigt „A oder B“. Daten: „UndergroundMining“ verlangt
  `{ "milling", "animal_husbandry" }` statt „agriculture“ (beide setzen Ackerbau voraus).
- **Startvorrat der KI nach Tags (B1):** Jeder Eintrag von `rttr:SetAIStartStockNoSurplus` ist eine Tag-Abfrage, jede
  passende Ware bekommt die Regel. Gerechnet wird wie bisher im Wirtschaftsplaner: Startvorrat übrig je Ware, für eine
  Warengruppe die Summe ihrer Waren (Antwort F-7). Daten: `{ "BasicBuildingMaterial", "Fish", "Bread", "Meat" }`.
- **Gebäudegrenze der KI (B2):** `rttr:SetAIBuildingLimit{ base = 5, perArea = 1, areaSize = 331 }`, alle drei Pflicht.
  Grenze = base + perArea × ⌊Land ÷ areaSize⌋, ohne „mindestens 1 Gebiet“ (F-8). `perStartArea` gibt es nicht mehr.
- **Aufräumen:** erledigte Notizen entfernt, jede Ware und jedes Kartenobjekt in einer Zeile, keine
  „PLATZHALTER“-Markierungen mehr (Projektregel; in `world.lua` bleibt der Originalwert als „Original: …“ stehen,
  `platzhalterPakete` heißt `standardPakete`), Beschreibung der Technologie „Well“ (war der Text der
  Knochenschnitzerei), WP-Summen im Kopf von `techtree.lua`.
- **Eingebaute Waren ändern (A3, Antworten des Nutzers 2026-11-24):** `rttr:SetWare` ersetzt `rttr:SetWareTags`:
  `rttr:SetWare{ Wood = { name = "PineLog", kind = { "Wood", "Softwood" }, properties = { ... } } }`, alle Felder
  optional. `name` ist der neue Bezeichner und zugleich Anzeigename und Name-Tag (kein eigenes `title`). Danach heißt die
  Ware in allen Lua-Dateien so (`Content::FindGood` findet sie nur noch unter dem neuen Namen); der alte Name trifft sie
  nur noch als Tag, wenn er in `kind` steht. `look` und `transport` nennen weiter die Originalnamen (Originalgrafik,
  Original-Transportpriorität); die Originalnamen bleiben deshalb für neue Waren gesperrt. Ein umbenanntes eingebautes
  Werkzeug bleibt sein Werkzeugtyp (`Content::GetToolTypeOf`). Verhalten im Spiel unverändert (der Holzfäller legt
  weiter `GoodType::Wood` ab). Code: `BuiltinWareDesc`, `parseBuiltinWare` (`CivContentDesc.*`),
  `WorldDescription::builtinWares`, `GameDataLoader::SetWare`, `Content::GetBuiltinName` und `builtinNames_`,
  `WARE_NAMES` zeigt den neuen Namen. Kein neuer Spielstand nötig (gespeichert wird der Warentyp, nicht der Name).

## Notizen aus den Lua-Dateien, Schritt 2: Warengrenze (Stand 2026-11-24)

Punkt A1 der Notizensammlung (wares.lua „Das Warenlimit muss dringend aufgehoben oder auf über 500 erhöht werden“,
Antwort des Nutzers F-1: 512 Plätze, mehr später durch eine Zahl im Code).

- **Plätze:** `GoodType` hat 2 Byte (`uint16_t`) und 512 freie Plätze (`CustomLast = Custom0 + 511`, Werte 40–551,
  `Nothing` = 552). Werkstätten (`BuildingType`) und Berufe (`Job`) haben je 128 freie Plätze (Werte 40–167 bzw.
  38–165) und bleiben 1 Byte groß. Mehr Plätze: nur `CustomLast` ändern (bei Waren bis 65 494 freie Plätze möglich,
  bei Werkstätten und Berufen höchstens 215 bzw. 217, sonst muss auch dort der Typ 2 Byte bekommen). Jeder Platz kostet
  auch leer Speicher und Rechenzeit (Tabellen je Ware, Schleifen über alle Waren), deshalb nicht unnötig groß wählen.
- **Speichern und Netzwerk:** Jede Ware wird mit 2 Byte geschrieben (`helpers::pushEnum<uint16_t>`, gelesen mit
  `popEnum<GoodType>`, das den Typ des Enums nimmt). Die Liste der Waren einer geplanten Anfrage (`SetPlannedDemand`,
  `WareDistribution::planned_`) hat ihre Länge jetzt mit 2 Byte bzw. variabler Länge statt 1 Byte, damit eine
  Tag-Gruppe mehr als 255 Waren haben kann.
- **Versionen:** Spielstand-Daten 32 (`SerializedGameData.cpp`), ältere Spielstände laden nicht mehr (Fehlermeldung
  „Game data version … is too old“, Projektregel: keine Kompatibilität mit alten Spielständen). Spielbefehle Version 4
  (`GameCommand.cpp`), ältere Replays werden abgelehnt (`Replay::LoadHeader`), weil ihre Waren 1 Byte haben.
- **Fenster:** Lager, Warenübersicht (`iwWares`, `iwBaseWarehouse`) und Gesamtverteilung (`iwWareDistribution`)
  hatten schon Seiten, die mit der Zahl der benutzten Waren wachsen; leere Plätze werden nicht gezeigt. Das Baumenü
  (`iwAction`) zeigt alle freigeschalteten Gebäude einer Größe untereinander (5 je Zeile); bei sehr vielen Werkstätten
  einer Größe wird es entsprechend hoch.
- **Rechenzeit:** Schleifen über alle Waren, die oft laufen (Prognose `civ::EconomyForecast`, Wirtschaftsplaner,
  Gesamtverteilung, `takeEconomySnapshot`), laufen nur über die vorhandenen Waren: `civ::Content::GetGoods()` liefert
  `GoodType(0)` bis `GoodType(GetNumGoods() - 1)`, die eingebauten Waren und die belegten Plätze (Lua belegt die Plätze
  lückenlos ab `Custom0`). `Content::GetToolTypeOf` baut die Namen der eingebauten Werkzeuge nur einmal.
  `EconomyPlanner::GetWareStatus` liefert für unbenutzte Plätze einen leeren Stand (sie haben keine Gruppe mehr). Ohne
  diese Änderungen dauerte ein KI-Testspiel (512 × 512, 4 KI, 3 h) rund 20 % länger; mit ihnen gleich lang wie mit 64
  Plätzen.
- **Stapelspeicher:** Tabellen je Ware sind 5-mal so groß. Unter Windows hat das Hauptprogramm 1 MB Stapelspeicher.
  Deshalb liegt die Prognose in `WareDistribution::Update` auf dem Heap, `GamePlayer::SuggestPact` kopiert den
  Zielspieler nicht mehr (das kopierte den ganzen Spieler, rund 180 KB) und `CreatePlayers` baut die Spieler direkt im
  Vektor.
- **Tests:** `CivContentSuite/AllSlotsCanBeUsed` (512 Waren und 128 Werkstätten laden, eine mehr ist ein Ladefehler),
  `CivContentSuite/WaresInHighSlotsAreSaved` (Waren auf Plätzen über 255 überstehen Speichern und Laden).
- **Freigeschaltete Waren (eigener Commit):** Die Waren, die der Nutzer wegen der Grenze auskommentiert hatte, sind
  aktiv: Reed, Hemp, Flax, LindenBast, BirchBark, Beeswax, Honey, Resin, Pitch, Tar, Bitumen, Asphalt, Firewood
  (wares.lua, „Warenlimit von 64 erreicht, deswegen werden diese Waren ausgeblendet“) und WoodenHammer (wares.lua,
  Werkzeug in tools.lua, Produkt der WoodWorkshop laut Notiz „WoodenHammer erst wenn Warengesamtzahl erhöht wurde“).
  Beeswax hatte `look = "Coin"`, der eingebaute Name ist „Coins“ (sonst Ladefehler). Außer WoodenHammer stellt noch
  kein Gebäude diese Waren her. Weiter auskommentiert sind die Werkstätten AsphaltDrawer, BitumenProducer und
  FirewoodSplitter (workshops.lua): AsphaltDrawer hat `worker.look = "AsphaltDrawer"` (kein eingebauter Beruf) und
  verbraucht den Tag „Bucket“ (ein Werkzeug) als Eingang, BitumenProducer hat `look = "Leatherworks"` (eingebaut heißt
  es „LeatherWorks“), und keine der drei steht im Technologiebaum.

## Gesamtverteilung (Stand 2026-11-21)

Entscheidungen des Nutzers vom 18. bis 21.11.2026 (Chats „Werkzeug-Prüfung für Gebäude“ und „Gesamtverteilung“).
Code: `civ/EconomyForecast.*` (Rechnung), `civ/WareDistribution.*` (Spielregel), `ingameWindows/iwWareDistribution.*`
(Fenster), Befehle `gc::SetWareDistribution`, `gc::ChangeWareOrders`, `gc::SetPlannedDemand`.

### Ziel

Gebäude arbeiten von selbst, kein Spieler soll Regler nachjustieren. Vorbild ist das Werkzeugfenster von Siedler 2:
Dort steht je Werkzeug ein Regler, jede Schlosserei wählt bei jedem Arbeitsschritt gewichtet nach den Reglern, und ein
Regler steuert alle Schlossereien. Die Gesamtverteilung ist dasselbe für alle Waren, nur stellen sich die Regler selbst
ein: „Die Gesamtverteilung ist der Dirigent, die Gebäude sind nur die Marionetten.“ Später wird sie die Grundlage für
lokale Warenwerte (Märkte).

Kommentar des Nutzers in `distribution.lua` (Stand vor der Umsetzung, wörtlich): „Es sollte für alle Werkzeuge und
Ausrüstung eine einzige gesammelte Bestellungliste und Verteilungsliste geben. Und die Verteilung soll sich automatisch
nach dem aktuellen Bedarf einstellen. Diese GesamtVerteilung steuert dann automatisch alle Gebäude, die die Waren in der
Liste herstellen. Die Gebäude stellen dann unter Berücksichtigung der Verteilung die Waren her, die sie herstellen
können. In dieser Liste sind alle die Verteilungen für alle Gebäudeoutputs festgehalten (auch von Monooutput Gebäuden)
und deren Mengenverhältnis zueinander. Damit soll das ständige Nachjustieren von Reglereinstellungen sollte nicht in so
einem Spiel existieren. Alle Gebäude funktionieren automatisch, warum sollte man dann manche Gebäude manuell steuern
können? Das passt nicht zum Spiel.“

### Regeln

1. **Spielregel für alle Spieler** (Variante 1): Jeder Rechner rechnet für jeden Spieler, nur mit ganzen Zahlen
   (Hundertstel einer Ware, `civ::Amount`), nur aus dem Spielzustand. Nur im Zivilisationsmodus.
2. **Waren:** alle Waren außer den Wissensmedien (der Regler „Wissen“ bleibt ein eigener Regler). Der Regler
   „Werkzeuge“ (Materialvorrang) bleibt und spielt für die Gesamtverteilung keine Rolle.
3. **Zahl je Ware**, je Spieler umschaltbar:
   - Prognose = (Lagerbestand + reserviert − offen − unterwegs zu Abnehmern − geplant) + Herstellung − Verbrauch, für
     die Bilanzlänge. Dieselbe Rechnung wie im Bauplaner (Abschnitt „Prognose“). Reserviert zählt nicht zusätzlich: Die
     Herstellung der nächsten Bilanzlänge steckt schon in der Prognose.
   - Geplant im Spiel: Werkzeuge der Baustellen und Werkzeugvorrat (rechnet das Spiel selbst) und bei einer KI ihr
     geplanter Bedarf (Entscheidung des Nutzers, 2026-11-21): Baukosten und Werkzeuge der Aufträge ihrer Liste, die
     nicht platziert und nicht blockiert sind, in der Reihenfolge der Liste, immer voll (ohne den Faktor für Baumaterial
     des Bauplaners). Die KI schickt ihn nach jedem Planungsschritt als Befehl `gc::SetPlannedDemand`, wenn er sich
     geändert hat (Spielzustand, `WareDistribution::GetPlanned`). Ihre abgeschickten Baubefehle schickt sie nicht mit:
     Der Befehl geht mit ihnen ab, beim Ausführen stehen die Baustellen auf der Karte und ihre Baukosten sind offen.
     Damit ist die Prognose der Gesamtverteilung die des Bauplaners (Test `PlannedDemandGoesToTheGame`). Ein Mensch hat
     keinen geplanten Bedarf.
   - Warenzahl = Lagerbestand + reserviert − offen − unterwegs zu Abnehmern.
   - Reserviert = jede Ware, die das Spiel als unterwegs führt (`GamePlayer::GetWares`, Straßen und Fahnen), bis sie in
     ein Gebäude kommt. Unterwegs zu Abnehmern (`EconomySnapshot::inTransit`) wird abgezogen: Ihre Anfrage ist nicht
     mehr offen, sonst zählte die Ware doppelt. Beispiel: 5 Bretter im Lager, eine Baustelle fragt 2 an, 3 sind auf dem
     Weg ins Lager: vorher 5 + 3 − 2 − 0 = 6; das Lager schickt die 2 los: 3 + 5 − 0 − 2 = 6; alle angekommen: 6.
   - Gruppen: Anfragen, die mehrere Waren nehmen (Werkzeugplatz „Axe“, Baukosten nach Tags, Nahrung der Bergwerke),
     werden wie im Bauplaner zugeteilt (Abschnitt „Warengruppen“). Was keine Ware der Gruppe hat, steht nur bei der
     Gruppe. Die Zahl einer Ware ist deshalb die niedrigere aus ihrer eigenen Zahl und den Zahlen aller Gruppen, die sie
     enthalten und die ein Abnehmer genau so anfragt. Beispiel: Eisenaxt Vorrat 3, Kupferaxt 0, Obsidianaxt 1; eine
     Forschung will 1 Eisenaxt, 6 Plätze „Axe“ sind leer, Vorrat 2 Äxte geplant: Einzeln stehen alle drei Äxte bei 0,
     die Gruppe bei −5. Alle drei bekommen −5, also sehen Schlosserei, Kupferwerk und Obsidianwerkstatt die Axt als
     knappstes Produkt; sobald eine davon eine Axt herstellt, steigt die Gruppenzahl für alle drei.
4. **Überschuss:** Warenzahl > max(8, 10 × Verbrauch pro Minute in der Bilanzlänge), in beiden Modi (Entscheidung
   des Nutzers, 2026-11-21: Im Prognose-Modus hielte ein Gebäude sonst wegen seiner eigenen Herstellung an, z. B. Holz
   Lager 3 + Herstellung 13 = Prognose 16 > 8). Die Werte kommen weiter aus der Zahl des eingestellten Modus. Für die
   Warenzahl gilt dieselbe Gruppenregel wie in Regel 3. Der Startvorrat zählt mit (anders als im Bauplaner,
   Entscheidung des Nutzers). Der Verbrauch der Schwelle ist der gemessene, nicht der der Prognose (Nutzer,
   2026-11-22): Angehaltene Gebäude laufen wieder an, wenn die Warenzahl auf die Schwelle fällt oder der gemessene
   Verbrauch die Schwelle hebt.
5. **Wertung:** Darin steht jede Ware, die ein fertiges Gebäude des Spielers als Produkt herstellt (nicht nur als
   Nebenprodukt), die nicht unerwünscht und kein Wissensmedium ist. Wert = 1 000 000 × (höchste Zahl − Zahl) ÷ (höchste
   Zahl − niedrigste Zahl): Der größte Mangel bekommt den höchsten Wert, die höchste Zahl 0. Alle gleich: alle den
   höchsten Wert. Angezeigt wird 0–1000. Manuelle Werte bleiben, wie der Spieler sie stellt; die Spanne ergibt sich aus
   den Zahlen aller Waren der Wertung.
6. **Gebäude mit mehreren Produkten** (Lua-Werkstätten mit mehreren Produkten, Schlosserei, Waffenschmiede) wählen bei
   jedem Arbeitsbeginn (Waffenschmiede beim Ablegen):
   - zuerst ein bestelltes Produkt (die Bestellung gilt für alle Gebäude, wer zuerst anfängt, erfüllt sie);
   - sonst unter den erwünschten Produkten: Gebäudewert = (Wert − niedrigster Wert der eigenen Produkte) ÷ (höchster −
     niedrigster), höchstes 100 %, niedrigstes 0 %; Wahrscheinlichkeit = Gebäudewert ÷ Summe. Beispiel Schlosserei:
     Axt −1, Sense 1, Säge 3, Hammer 9 (Überschuss, fällt heraus) → Axt 100 %, Sense 50 %, Säge 0 % → 2/3 Äxte, 1/3
     Sensen. Alle gleich: gleich verteilt;
   - sind alle Produkte unerwünscht, aber ein Nebenprodukt gebraucht: gleich verteilt unter allen Produkten.
   - Bei Ertrag über 1 wird jede weitere Ware neu gewählt.
7. **Anhalten:** Eine Ware ist unerwünscht, wenn sie automatisch angehalten wird und Überschuss ist, oder wenn der
   Spieler sie angehalten hat. Ein Gebäude hält an, wenn alle seine Produkte und alle erforschten Nebenprodukte
   unerwünscht sind und keines bestellt ist (Bauernhof: Getreide und Stroh). Angehalten heißt wie „Produktion
   stoppen“: kein Arbeitsschritt, keine Eingänge und Werkzeuge bestellen (`nobUsual::IsProductionStopped`). Was von
   selbst anhält, läuft von selbst wieder an; was der Spieler anhält, bleibt angehalten. Ein Gebäude speichert keinen
   eigenen Zustand dafür; ändert er sich, wird der Arbeiter geweckt bzw. gestoppt.
8. **Frequenz:** alle 200 GF (10 Sekunden), versetzt: Spieler p rechnet bei GF mod 200 = p × 200 ÷ Spielerzahl. Eine
   geänderte Einstellung wirkt sofort.
9. **Fenster** (statt des Werkzeugfensters, Knopf „Werkzeuge“ im Hauptmenü): eine Zeile je Ware unter Überschriften
   (Werkzeugart, sonst Warenart aus Lua, sonst „Sonstige“), 12 Zeilen je Seite. Je Zeile: Name mit Zahl, Regler 0–1000
   (verschieben = manuell), A/M für den Wert, Bestellungen mit +/−, Anhalten (A: automatisch, „läuft“, „angehalten“).
   Oben: Werte aus Prognose oder Warenzahlen.
10. **Waffenschmiede:** gehört dazu; das Addon „halbe Kosten für Militärausrüstung“ wirkt im Zivilisationsmodus nicht
    (es setzt das Abwechseln voraus). Schilde aller Völker zählen als eine Ware.
11. **Lua:** `outputChoice` gibt es nur noch als `"knowledge"`. `rttr:AddRegulator`, die Regler `Equipment`,
    `WoodenTools`, `ObsidianTools` und der Werkzeugregler je Art entfallen.
12. **KI:** Sie stellt keine Werkzeug-Regler mehr ein und bestellt keine Werkzeuge (`OrderTools`, `ResolveTools`,
    `ComputeToolFlows` entfallen, Werkzeughersteller werden gemessen wie alle Gebäude). Ihr Planer entscheidet nur, wie
    viele Gebäude es gibt. Angehaltene Gebäude (automatisch, manuell oder vom Spieler gestoppt) zählen mit ihrer vollen
    Arbeitskraft (Abschnitt „Prognose“, Herstellung; Nutzer, 2026-11-22).
    **Regel A** (Gebäude mit mehreren Produkten): Ihre Kapazität zählt einmal über alle Produkte. Was ihre Gebäude in der
    Bilanzlänge liefern müssen: je Produkt was sie davon jetzt herstellen, plus was fehlt, minus was übrig ist
    (höchstens 0 abgezogen); ein Gruppenmangel zählt nur beim Herstellertyp, den der Planer für die Gruppe wählt. Fehlt
    nach der Kapazität (Gebäude, die arbeiten können, angehaltene und kommende × Waren je Minute, während sie arbeiten ×
    Bilanzlänge) etwas, kommen so viele neue, bis es reicht. „Was sie jetzt herstellen“ enthält die volle Arbeitskraft
    der angehaltenen Minuten, wie die Prognose. Beispiel: 1 Schlosserei, 2 Werkzeuge/min, in 5 min 5 Äxte
    und 5 Sägen, Axt −8, Säge +3: Äxte 5 + 8 = 13, Sägen 5 − 3 = 2, zusammen 15, Kapazität 10, es fehlen 5 → 1 neue
    Schlosserei (je Produkt gerechnet wären es 2).

### Gemeinsame Rechnung

`civ::EconomyForecast` ist die Rechnung des Bauplaners (Prognose, Zuteilung, Diagnose, Werte je Gebäude), verlegt ins
Spiel und auf ganze Zahlen umgestellt. Der Bauplaner nutzt sie und gibt seine abgeschickten Baubefehle und seine
Auftragsliste dazu (`Extra`); die Gesamtverteilung gibt den geplanten Bedarf der KI dazu (Regel 3). Gemessen wird mit dem Spielzustand: Fertigstellung und Arbeiter seit (`nobUsual`,
Spielstand 30), abgerissene Gebäude der Bilanzlänge (`GamePlayer::GetGoneBuildings`).

### Offen

- Erstes Testspiel (Zufallskarte 512, 4 KI, „a lot“, 40 min): Land 1 432–1 646 statt 1 784–2 626, Gebäude 41–45 statt
  47–57. Holz: Lager 3, Herstellung 13 → Prognose 16 > 8 → Holzfäller angehalten, während die Gruppe Baumaterial im
  Bauplaner bei −80 steht: Die Aufträge der KI, die auf Material warten, sind keine Baustellen und fehlen in der
  Rechnung des Spiels. Vergleich über 3 h (Seed 5, `db898ba` gegen `607d399`): Land 2 382–3 861 statt 2 122–10 456,
  Gebäude 107–147 statt 72–256, Militärstärke wächst bei keinem Spieler (alt ab 2:00). Abhilfe: der geplante Bedarf
  der KI (Regel 3) und dem Überschuss nach der Warenzahl (Regel 4). Danach (`01c4ca7` gegen `607d399`, 3 h, Karte 512,
  4 KI): Seed 5 Land 8 460–11 401 statt 2 122–10 456, Gebäude 326–385 statt 72–256, Militärstärke wächst bei Spieler 3
  ab 2:30 (alt Spieler 1 ab 1:50, Spieler 3 ab 2:30); Seed 9 Land 7 639–12 312 statt 7 574–14 182, Gebäude 267–332
  statt 222–355, Militärstärke wächst bei Spieler 0 und 2 ab 2:50 (alt Spieler 3 ab 3:00). Etwa gleichauf mit dem alten
  Stand.

## Lagerkapazität (Stand 2026-11-14)

Entscheidungen des Nutzers (2026-11-14). Nur im Zivilisationsmodus; Einstellungen in
`data/RTTR/gamedata/civ/storage.lua`. Tests: `CivStorageSuite`.

- **Kapazität:** `rttr:SetWarehouseCapacity{ Headquarters = 500, HarborBuilding = 300, Storehouse = 200 }`.
  - Gezählt werden nur Waren, keine Personen (`nobBaseWarehouse::GetLoad`).
  - Waren auf dem Weg zum Lager zählen schon mit.
  - Lager ohne Eintrag fassen beliebig viel.
- **Volles Lager:** Es nimmt keine Waren mehr an (`nobBaseWarehouse::CanStoreWare`, geprüft in den
  Lagersuchbedingungen `FW::AcceptsWare`, `FW::CollectsWare`).
- **Ausnahme:** Was im Lager selbst entsteht (Startvorrat, Werkzeuge zurückkehrender Arbeiter, Lua, Cheats), wird immer
  eingelagert, auch über die Grenze.
- **Alle Lager voll:** Die Ware bleibt ohne Ziel an der Flagge ihres Gebäudes liegen.
  - Alle 100 GF (`civ::WAITING_WARES_INTERVAL`) sucht sie erneut: zuerst einen Abnehmer (Baustelle, Werkstatt), sonst
    ein Lager mit Platz (`GamePlayer::FindGoalsForWaitingWares`).
  - Im Original suchen Waren ohne Ziel nur Lager; ohne die Abnehmersuche blieben Bretter liegen, während Baustellen
    warten.
- **Arbeitsstopp an der Flagge:** `rttr:SetFlagWorkLimit(7)`. Ein Arbeiter legt sein Produkt nur ab, wenn danach
  höchstens 7 Waren an der Flagge liegen; sonst wartet er im Gebäude. So bleibt ein Platz für durchlaufendes
  Baumaterial frei.
- **Anzeige:** Die Belegung steht im Titel des Lagerfensters, z. B. „Lagerhaus 120/200“.
- **Lager aus Lua:** `rttr:AddWorkshop{ ..., warehouse = { wares = { "Wood", "Stones" }, capacity = 100 } }`.
  - Es nimmt nur die genannten Waren auf (Tag-Abfragen) und keine Personen, erzeugt keine Gehilfen, hat keinen
    Arbeiter.
  - Technisch ist es ein `nobStorehouse` mit eigenem Gebäudetyp; `BuildingProperties::IsWareHouse` gilt auch dafür.
  - Der Schlüssel heißt `warehouse`, weil `storage` schon die Eingangsplätze einer Werkstatt meint.
  - Die KI baut solche Lager noch nicht.
- **KI:** `rttr:SetAIStorageFull(90)` in `ai.lua`. Ab 90 % Belegung aller Lager zusammen gibt es nur noch benötigte
  Aufträge, und ein benötigter Auftrag „Lagerhaus“ entsteht (einer zur Zeit, auch direkt neben einem anderen Lager).
- **Vergleich** (7 Karten, 2 KI, 2 h): Land 74 238 (ohne Kapazität 74 559). Die KI erreicht die 90 % selten, weil die
  alte KI weiter eigene Lagerhäuser baut. Das Hauptquartier startet mit 308 von 500 Waren.

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
