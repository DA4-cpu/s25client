-- Copyright (C) 2026 DA4-cpu
--
-- SPDX-License-Identifier: GPL-2.0-or-later

--[[
Gebäude im Zivilisationsmodus.

  rttr:SetBuilding{...}  stellt ein eingebautes Gebäude ein (Holzfäller, Bauernhof, Militärgebäude, Hauptquartier ...).
                         Die Einträge stehen unten im Abschnitt "Eingebaute Gebäude".
  rttr:AddWorkshop{...}  definiert ein neues Gebäude: Werkstatt, Sammler, Plantage, Mine, Bibliothek, Lager oder
                         Militärgebäude. Die Einträge stehen unten im Abschnitt "Werkstätten".
Beide Funktionen verwenden dieselben Felder mit derselben Bedeutung (beschrieben bei rttr:AddWorkshop). Welche Felder
ein eingebautes Gebäude hat, steht bei rttr:SetBuilding.
Änderungen gelten ab dem nächsten Spielstart, Neukompilieren ist nicht nötig.

Zeiten: workTime, waitTime und waitAfterTime zählen in Frames (Spielschritten) wie im Originalspiel. 1 Frame dauert
bei normaler Geschwindigkeit 50 ms, 20 Frames sind 1 Sekunde (workTime = 600 sind 30 Sekunden). Höchstens 65535.
Ablauf eines Arbeitsschritts:
  Arbeiter, die im Gebäude arbeiten (Werkstätten, Minen, Sägewerk, Mühle, Brunnen ...):
    waitTime: er wartet im Gebäude -> workTime: er arbeitet im Gebäude (dabei werden die Eingänge verbraucht) ->
    waitAfterTime: er wartet noch (0 = 1 Frame) -> er trägt das Produkt zur Fahne.
  Sammler und Plantagen (Holzfäller, Steinbruch, Fischer, Förster, Bauernhof, Weinberg, Köhler, Werkstätten mit
  collect oder plant):
    waitTime: er wartet im Gebäude -> er geht zum Objekt -> workTime: er arbeitet am Objekt -> er geht zurück und
    trägt das Produkt zur Fahne. Sie haben kein waitAfterTime.
  Jäger: waitTime, dann die Jagd (Gehen, Schießen und Zerlegen dauern immer gleich lang). Er hat kein workTime und
  kein waitAfterTime.
Das Arbeitstempo (Werkzeugreform, tools.lua) teilt die Zeiten: Tempo 50 = doppelt so lang, Tempo 200 = halb so lang.
Die Arbeit am Objekt draußen dauert immer workTime (sie passt zu den Animationen); dafür ändert sich die Wartezeit
davor so, dass waitTime + workTime zusammen im Tempo länger oder kürzer werden.

===================================================================================================================
rttr:SetBuilding{...}: stellt ein eingebautes Gebäude ein
===================================================================================================================
  name   Bezeichner des eingebauten Gebäudes (Liste unten).
  Alle anderen Felder sind optional; was fehlt, bleibt wie im Originalspiel. Jedes Gebäude darf nur einmal vorkommen.
  Ein Feld, das das Gebäude nicht hat, ist ein Ladefehler (mit Meldung beim Spielstart).
  Die Einträge unten enthalten alle Werte, die zurzeit gelten (die Originalwerte, wo nichts geändert wurde).

  Felder je Gebäude:
    Hauptquartier (Headquarters): nur knowledge und warehouse.
    Hafen (HarborBuilding): nur knowledge, warehouse und aiNearBorder. Der Hafen kostet immer 4 Bretter und 6 Steine
      (Expeditionen bringen genau diese Waren mit), seine Baukosten sind deshalb nicht einstellbar.
    Lagerhaus (Storehouse): title, description, size, cost, look, knowledge, aiNearBorder, warehouse.
    Militärgebäude (Barracks, Guardhouse, Watchtower, Fortress): title, description, size, cost, look, knowledge,
      military. look muss eines dieser vier sein.
    Eselzucht (DonkeyBreeder), Werft (Shipyard), Katapult (Catapult), Spähturm (LookoutTower), Tempel (Temple):
      nur cost, knowledge, tools, outputRatio, aiNearBorder. Ihre Arbeitsweise, Eingänge und Produkte bleiben vorerst
      wie im Originalspiel.
    Alle anderen (Produktionsgebäude): title, description, size, cost, look, worker, inputs, storage, outputs,
      inheritProperties, outputChoice, knowledge, tools, outputRatio, workTime, waitTime, waitAfterTime, aiNearBorder
      und je nach Gebäude collect, plant oder mine.
      Ausnahmen: Der Brunnen (Well) hat kein worker (sein Arbeiter ist der Gehilfe). Der Jäger (Hunter) hat kein
      workTime. Sammler und Plantagen haben kein waitAfterTime. storage haben nur Gebäude mit Eingängen. Sammler
      (Holzfäller, Steinbruch, Fischer, Jäger) haben keine inputs. Der Förster (Forester) hat keine outputs,
      inheritProperties und outputChoice (er pflanzt nur).

  Felder, die es nur hier gibt oder die hier anders sind:
    worker     stellt den Beruf des Gebäudes ein: worker = { title = "Holzfäller", look = "Woodcutter" }.
               title: Anzeigename des Berufs. look: Grafik, ein eingebauter Beruf (Bezeichner wie bei
               rttr:AddWorkshop). Ein Beruf ist in allen Gebäuden derselbe: Alle vier Minen haben den Beruf Miner.
               Geben zwei Gebäude desselben Berufs verschiedene title oder look an, ist das ein Ladefehler.
    title      Ein englischer Originalname (z.B. "Woodcutter", "Granite mine") wird wie bisher in die Sprache des
               Spiels übersetzt, jeder andere Name steht so da. Das gilt auch für worker.title ("Woodchopper").
    description Ohne description bleibt der übersetzte Hilfetext des Originalspiels.
    size       "small", "medium", "large" oder "mine". "mine" nur bei Minen, Minen nur mit "mine".
    look       Grafik: ein eingebautes Gebäude. Ein Militärgebäude bekommt von look auch die Positionen der Fahnen.
    collect    Holzfäller (Woodcutter), Steinbruch (Quarry), Fischer (Fishery), Jäger (Hunter): collect = { radius = N }
               Umkreis in Knoten (1 bis 20), in dem der Arbeiter seine Bäume, Steine, Fische bzw. Tiere sucht.
    plant      Förster (Forester), Bauernhof (Farm), Weinberg (Vineyard), Köhler (Charburner): plant = { radius = N }
               Umkreis in Knoten (1 bis 20), in dem der Arbeiter pflanzt (Köhler: Meiler anlegt) und erntet.
               Beim Bauernhof gilt der Umkreis auch für den Förster: Er pflanzt keine Bäume auf Plätze, die im
               Umkreis eines eigenen Bauernhofs Felder werden können.
    mine       Minen (GraniteMine, CoalMine, IronMine, GoldMine): mine = { resource = "Iron" }, das Erz, das die Mine
               abbaut: "Granite", "Coal", "Iron", "Gold" oder ein Erz aus mapobjects.lua (rttr:AddOre). Das Produkt
               steht in outputs: Eine Kohlemine mit resource = "Iron" und outputs = { "Coal" } baut Eisenerz ab und
               bringt Kohle.
    inputs     Eingänge wie bei rttr:AddWorkshop, z.B. inputs = { "Wood" } oder inputs = { { tags = "Food", storage = 6 } }.
               Ersetzen die Eingänge des Originalspiels. Ohne inputs bleiben die des Originalspiels. inputs = { }
               heißt: keine Eingänge. Jeder Arbeitsschritt braucht von jedem Eingang seine Menge (amount, mit dem
               inputRatio der Werkzeuge), auch in den Minen. Wann verbraucht wird:
                 Arbeiter im Gebäude (Sägewerk, Minen, Brunnen ...): beim Beginn der Arbeit im Gebäude.
                 Förster, Bauernhof, Weinberg: bei jedem Pflanzen (Ernten braucht nichts).
                 Köhler: bei jedem Gang, der einen Meiler anlegt oder weiter aufschichtet (im Original abwechselnd mit
                 Holz und mit Getreide; jeder dieser Gänge verbraucht alle Eingänge einmal), nicht beim Abdecken und
                 beim Ernten der Kohle. Die Stufen des Meilers bleiben, wie sie sind.
    storage    Plätze je Eingang (Standard wie im Originalspiel: 6, Abdecker 3). Ein Eingang mit eigenem storage
               (inputs = { { tags = "Food", storage = 6 } }) hat seine eigene Zahl.
    outputs    Produkte wie bei rttr:AddWorkshop (mit steps je Produkt), z.B. outputs = { "Boards" }. Ersetzen die
               Produkte des Originalspiels. Ohne outputs bleiben die des Originalspiels. Sammler, Plantagen und Minen
               haben genau ein Produkt. Mehrere Produkte (z.B. Schlosserei, Waffenschmiede) wählt die
               Gesamtverteilung, mit outputChoice = "knowledge" der Regler "Wissen". "IronShield" ergibt das Schild des
               eigenen Volkes. Ein anderes Produkt als das des Originalspiels trägt der Arbeiter wie ein Träger (die
               Grafik der Ware).
               Wichtig: Jeder Eintrag ist eine Tag-Abfrage. Deshalb tragen die eingebauten Waren, deren Name auch eine
               Warenart ist, eigene Namen (wares.lua): Cobblestones (Tag Stones), IronAxe, IronSaw usw. (Tags Axe,
               Saw ...), IronSword (Sword), IronShield (Shield), LeatherArmor (Armor). "Stones" träfe auch LoamBrick,
               "Axe" auch die Kupfer-, Bronze- und Obsidianäxte.
    inheritProperties, outputChoice  wie bei rttr:AddWorkshop, nur zusammen mit outputs im selben Eintrag.
    outputRatio Standard 1. Bedeutung wie bei rttr:AddWorkshop. Ein Holzfäller mit 2 bringt 2 Holz je Baum. Minen und
               Sammler nehmen je Arbeitsschritt genau 1 Einheit ihres Vorkommens, egal wie viele Waren herauskommen:
               Ein Steinbruch mit 2 nimmt 1 Stück vom Steinhaufen und bringt 2 Steine, ein Fischer mit 2 fängt 1 Fisch
               und bringt 2.
    tools      wie bei rttr:AddWorkshop. Ohne tools braucht das Gebäude das Werkzeug, das sein Arbeiter im
               Originalspiel hat (z.B. die Axt), mit baseSpeed = 0; hatte der Arbeiter keins (Mühle, Brunnen,
               Brauerei ...), arbeitet es wie bisher mit Tempo 100.
    aiNearBorder Standard wie im Originalspiel: true bei Holzfäller, Steinbruch, Fischer, Minen, Hafen und Katapult,
               sonst false. Militärgebäude darf die KI immer an der Grenze bauen.
    warehouse  nur Lager: warehouse = { capacity = N }, Höchstzahl aller Waren zusammen (1 bis 100000), Bedeutung
               wie bei rttr:AddWorkshop. Ohne warehouse fasst das Lager beliebig viel. Die aufgenommenen Waren bleiben
               wie im Originalspiel (alle Waren und Personen).
  Im Zivilisationsmodus wirkungslos: die Addons für die Reichweite von Holzfäller, Förster und Steinmetz (dafür gibt es
  collect und plant).
  Noch nicht einstellbar: eine andere Arbeitsweise (z.B. ein Steinbruch, der Lehm sammelt). Das kommt im nächsten
  Schritt.

  Bezeichner der eingebauten Gebäude: Headquarters, Storehouse, HarborBuilding, Barracks, Guardhouse, Watchtower,
  Fortress, LookoutTower, Catapult, Woodcutter, Forester, Sawmill, Quarry, GraniteMine, CoalMine, IronMine, GoldMine,
  Charburner (Köhlerei-Addon), Fishery, Hunter, Well, Farm, Mill, Bakery, PigFarm, Slaughterhouse, Brewery,
  DonkeyBreeder, Vineyard, Winery, Temple (Wein-Addon), Ironsmelter, Metalworks, Armory, Mint, Shipyard, Skinner,
  Tannery, LeatherWorks (Leder-Addon).
  Bezeichner der eingebauten Berufe (für worker.look): Helper, Woodcutter, Fisher, Forester, Carpenter, Stonemason,
  Hunter, Farmer, Miller, Baker, Butcher, Miner, Brewer, PigBreeder, DonkeyBreeder, IronFounder, Minter, Metalworker,
  Armorer, Builder, Planer, Geologist, Shipwright, Scout, CharBurner, Winegrower, Vintner, TempleServant, Skinner,
  Tanner, LeatherWorker.

===================================================================================================================
rttr:AddWorkshop{...}: definiert ein neues Gebäude
===================================================================================================================
  name        Bezeichner. Nur A-Z, a-z, 0-9 und _. Wird in techtree.lua (unlocks), ai.lua und in Spielständen
              verwendet.
  title       Anzeigename
  description optional: Hilfetext
  size        "small", "medium", "large" oder "mine" (Minenplatz im Gebirge, nur zusammen mit mine)
  cost        Baukosten (Pflicht), zwei Formen:
              kurz: cost = { Boards = 2, Stones = 1 }: je Eintrag ein Tag und die Anzahl. Verbaut wird in
                    alphabetischer Reihenfolge der Tags (Lua-Tabellen haben keine Reihenfolge).
              lang: cost = { { tags = "Boards", amount = 2 }, { tags = { "Bretter", "Nadelholz" }, amount = 1 } }:
                    verbaut wird in dieser Reihenfolge. tags ist eine Tag-Abfrage wie bei den Eingängen.
              Jede Ware, die alle Tags eines Eintrags hat, kann dafür geliefert werden, auch gemischt (z.B. Laub- und
              Nadelholzbretter für "Bretter"). Die Baustelle bestellt aus dem nächsten Lager, das irgendeine passende
              Ware hat. Höchstens 4 Einträge und höchstens 31 Waren insgesamt je Gebäude.
              Der Bauarbeiter verbaut die Einträge nacheinander. Die erste Hälfte des Baufortschritts zeigt das Gerüst,
              die zweite die Mauern.
              "Boards" ist der Bezeichner der eingebauten Bretter (zugleich ihr Tag). "Stones" ist ein Tag der
              eingebauten Steine (Cobblestones, wares.lua) und jeder Ware mit kind = "Stones" (z.B. LoamBrick). Neue
              Waren zählen dazu, wenn sie diesen Tag haben (z.B. kind = "Boards"), oder man gibt den eingebauten Waren
              mit rttr:SetWare (wares.lua) eine Warenart und fragt nach ihr.
              Beim Abriss mit Rückerstattung (Addon) gibt es von jedem Eintrag die erste passende Ware zurück.
  look        Platzhaltergrafik: Das Gebäude sieht aus wie dieses eingebaute Gebäude (z.B. "Sawmill")
  worker      Arbeiter: { title = "...", look = "Carpenter" }
              look: Platzhaltergrafik (eingebauter Beruf). Jeder Helfer kann ohne Werkzeug Arbeiter einer Werkstatt
              werden. Welche Werkzeuge in der Werkstatt liegen, steht unter tools.
              Eigene Bilder für Gebäude und Arbeiter: Ordner data/RTTR/assets/civ/<name>/,
              siehe data/RTTR/assets/civ/LIESMICH.txt
  inputs      optional: bis zu 10 Eingänge. Jeder Eingang ist ein Platz für alle Waren einer Tag-Abfrage, auch gemischt:
              inputs = { "Wood", { tags = "Food", amount = 1, storage = 6 }, { tags = { "Bretter", "Nadelholz" } } }
              Kurzform: eine Tag-Abfrage ("Wood" oder { "Bretter", "Nadelholz" }) = ein Platz mit amount 1.
              tags     Tag-Abfrage: jede Ware mit allen diesen Tags kann geliefert werden (z.B. "Food": Fisch, Fleisch,
                       Brot, Honig)
              amount   optional (Standard 1): Waren je Arbeitsschritt, auch Kommazahlen. 2 = 2 Waren je Schritt, 0.5 = 1
                       Ware jeden 2. Schritt (der Schritt, der eine Ware anbricht, nimmt sie ganz, der Rest zählt für die
                       nächsten Schritte), 1.5 = abwechselnd 2 und 1. Der Verbrauch hängt nur vom Arbeitsschritt ab, nicht
                       davon, wie viele Waren herauskommen. Werkzeuge in Gebrauch ändern amount mit ihrem inputRatio
                       (tools.lua).
              storage  optional (Standard: storage der Werkstatt): Plätze dieses Eingangs
              Ein Arbeitsschritt beginnt erst, wenn jeder Eingang die Waren für diesen Schritt hat. Aus einem gemischten
              Platz wird die Ware genommen, von der am meisten dort liegt (gleich viele: die zuerst definierte).
              Bestellt wird jede passende Ware aus dem nächsten Lager, das eine davon hat. Eine Ware, die zu mehreren
              Eingängen passt, kommt in den ersten, der noch Platz hat.
  outputs     Liste der Produkte. Jeder Eintrag ist eine Tag-Abfrage (ein Text = ein Tag, eine Liste = alle diese
              Tags), jede Ware, die sie trifft, ist ein Produkt:
              outputs = { "CopperAxe", { "Hammer", "Copper" } }: die Kupferaxt und der Hammer mit dem Tag Copper.
              outputs = { "GeritzterKnochen" }: alle geritzten Knochen (mehrere Produkte mit einem Eintrag).
              Eine Ware darf nur einmal vorkommen. Sammler, Plantagen und Minen haben genau ein Produkt. Ohne
              outputs: nur eine Bibliothek oder eine Plantage, die nur pflanzt. (Das frühere Feld output gibt es
              nicht mehr: output = "Clay" heißt jetzt outputs = { "Clay" }.)
              Arbeitsschritte je Produkt: { "CopperSword", steps = 1.5 } (Standard 1, mehr als 0 bis 99, auch
              Kommazahlen; trifft der Eintrag mehrere Waren, gilt es für jede). Ein Produkt kostet so viele für dieses Produkt angesparte Arbeitsschritte (siehe
              outputRatio), z.B. outputs = { "CopperSpear", { "CopperSword", steps = 1.5 } }: ein Speer kostet 1
              Schritt, ein Schwert 1.5.
  outputRatio optional (Standard 1): Waren pro Arbeitsschritt, 0 bis 8, auch Kommazahlen. 0.5 = eine Ware jeden 2.
              Schritt, 2 = zwei Waren je Schritt, 1.5 = abwechselnd 1 und 2.
              Genau: Jedes Produkt hat einen eigenen Sparwert. Zu Beginn jedes Arbeitsschritts wird das Produkt des
              Schritts gewählt (die Sparwerte spielen dabei keine Rolle). Am Ende des Schritts kommt outputRatio auf
              den Sparwert dieses Produkts. Erreicht er die Arbeitsschritte des Produkts (steps in outputs, sonst 1),
              entsteht es, so oft der Sparwert reicht, und die Schritte werden abgezogen. Der Rest bleibt bei diesem
              Produkt, die Sparwerte der anderen Produkte bleiben, wie sie sind.
              Beispiel outputRatio = 1, Speer 1 Schritt, Schwert 1.5: Schritt 1 Schwert gewählt -> Schwert hat 1
              gespart, nichts entsteht. Schritt 2 Speer gewählt -> Speer hat 1, ein Speer entsteht, Speer wieder 0,
              Schwert weiter 1. Schritt 3 Schwert gewählt -> Schwert hat 2, ein Schwert entsteht, 0.5 bleiben beim
              Schwert. Mit outputRatio 2.5 und Speer gewählt: 2 Speere, 0.5 bleiben beim Speer.
              Eine Bestellung (Fenster "Gesamtverteilung") wird erst abgehakt, wenn das Produkt entsteht; Schritte,
              die nur dafür sparen, lassen sie offen.
              Die Eingänge werden in jedem Schritt verbraucht (nur nach ihrem amount, nicht nach dem Ertrag).
              Die Werkzeuge in Gebrauch zählen ihr outputRatio aus tools.lua dazu (0.5 + Werkzeug 1 = 1.5), auch je
              Produkt (es zählt der Bonus für das gewählte Produkt), höchstens 8 insgesamt, mindestens 0. Ein Produkt
              mit Summe 0 wird nicht gewählt; ist die Summe für alle Produkte 0, arbeitet die Werkstatt nicht.
              Nebenprodukte (byproducts.lua) gibt es je Ware. Minen, Sammler und Plantagen nehmen je Arbeitsschritt
              genau 1 Einheit ihres Erzes bzw. Objekts, egal wie viele Waren herauskommen. Der Arbeiter legt alle
              zusammen an die Fahne, mehr als 8 nacheinander.
              Eingebaute Gebäude: outputRatio in rttr:SetBuilding.
  outputChoice optional, nur "knowledge": Die Produkte (mehrere) werden nach dem Regler "Wissen" im Fenster
              "Verteilung" gewählt (z.B. geritzte Knochen für LWP, IWP und MWP). Jedes Produkt braucht knowledge in
              wares.lua (genau eine Art).
              Ohne outputChoice: Hat die Werkstatt mehrere Produkte, steuert sie die Gesamtverteilung (Fenster
              "Gesamtverteilung"): Zu Beginn jedes Arbeitsschritts wählt sie gewichtet nach den Werten ihrer Produkte,
              Bestelltes zuerst (siehe outputRatio).
  inheritProperties  optional (Standard false): Das Produkt hat dieselben Eigenschaften wie die Ware, die zuletzt aus dem
              ersten Eingang verbraucht wurde (z.B. Nadelholz -> Nadelholzbretter). Zu jeder Ware des ersten Eingangs
              muss es genau ein passendes Produkt geben.
  storage     optional (Standard 6): Lagerplätze je Eingang (ein Eingang kann sein eigenes storage haben, siehe inputs)
  library     optional: macht die Werkstatt zur Bibliothek, z.B. library = { capacity = 20 }.
              Eine Bibliothek bestellt alle Wissensmedien (Waren mit knowledge in wares.lua) per Träger, bis capacity
              erreicht ist, und stellt nichts her. Darin zählen die WP der Medien voll (in Lagern nur halb). Wird sie
              zerstört, sind die Medien verloren. Ohne inputs, outputs, workTime, waitTime und waitAfterTime.
  collect     optional: macht die Werkstatt zum Sammler wie den Steinbruch. Der Arbeiter geht zu Objekten auf der Karte
              (mapobjects.lua) im Umkreis radius, arbeitet dort workTime Frames und bringt sein Produkt (outputs) nach Hause:
              collect = { object = "LoamDeposit", radius = 8 }. Ohne inputs. Man sollte auch mehrere objects zum Collecten angeben können  
              Arbeitsanimation je nach worker.look: "Planer" gräbt mit der Schaufel, "Woodcutter" hackt,
              "Farmer" mäht mit der Sense, sonst klopft er wie der Steinmetz.
              Noch wachsende Objekte (von einer Plantage gepflanzt) sammelt er erst, wenn sie ausgewachsen sind.
  plant       optional: macht die Werkstatt zur Plantage wie den Bauernhof, den Förster oder den Köhler. Der Arbeiter
              pflanzt ein Kartenobjekt (mapobjects.lua, braucht dort growTime) im Umkreis radius:
              plant = { object = "PapyrusDeposit", radius = 4 }.
              Gepflanzt wird nur dort, wo das Objekt auch natürlich wächst (Regeln in world/mapgen.lua, aber ohne die
              Zonen), auf freien Plätzen ohne Straße und nicht direkt neben Gebäuden, Feldern und anderen Objekten
              (damit Wege frei bleiben). Fahnen dürfen daneben stehen wie bei Steinen.
              Jedes Pflanzen verbraucht die Eingänge (inputs wie oben), z.B. inputs = { "Water" } zum Gießen.
              Ohne inputs braucht er dafür nichts.
              Mit outputs erntet er außerdem selbst wie der Bauer: ausgewachsene Objekte dieser Art im Umkreis (auch
              natürlich gewachsene), eine Ware pro Gang; Ernten geht vor Pflanzen. Ohne outputs pflanzt er nur (wie der
              Förster), ernten kann dann z.B. ein Sammler (collect).
              Arbeitsanimation beim Pflanzen: worker.look "Farmer" sät, sonst pflanzt er wie der Förster.
              Beim Ernten wie beim Sammler (siehe collect).
              Eigene Bilder fürs Pflanzen: Arbeiter_Pflanzen_<n> (siehe data/RTTR/assets/civ/LIESMICH.txt).
  mine        optional: macht die Werkstatt zur Mine für ein Erz aus mapobjects.lua (rttr:AddOre):
              mine = { resource = "Copper" }. Nur mit size = "mine". Der Arbeiter baut wie in den eingebauten Minen
              je Arbeitsschritt eine Einheit des Erzes im Umkreis von 2 Knoten ab. Ist nichts mehr da, kommt die Meldung
              "Diese Mine ist erschöpft". Eingänge wie bei jeder Werkstatt, z.B. Nahrung:
              inputs = { { tags = "Food", storage = 6 } }.
  warehouse   optional: macht das Gebäude zum Lager statt zur Werkstatt:
              warehouse = { wares = { "Wood", "Stones", "Lehm" }, capacity = 100 }.
              wares = die Waren, die es aufnimmt (Tag-Abfragen wie bei inputs). Es nimmt nur diese Waren auf und keine
              Personen, erzeugt keine Gehilfen und hat keinen Arbeiter. Baustellen und Werkstätten holen ihre Waren
              auch von dort.
              capacity = so viele Waren fasst das Lager höchstens (nur Waren, keine Personen). Waren auf dem Weg zum
              Lager zählen schon mit. Ist ein Lager voll, nimmt es keine Waren mehr an; sind alle voll, bleiben die
              Waren an der Fahne ihres Gebäudes liegen, bis ein Abnehmer sie braucht oder wieder Platz ist. Was im
              Lager selbst entsteht (Startvorrat, Werkzeuge zurückkehrender Arbeiter), wird immer eingelagert, auch
              über die Grenze hinaus. Wann eine Fahne voll ist, steht in storage.lua (rttr:SetFlagWorkLimit).
              Ein Lager hat nur name, title, description, size, cost, look, knowledge und aiNearBorder; worker, inputs,
              outputs, workTime usw. sind nicht erlaubt. Die KI baut solche Lager noch nicht.
              Beispiel:
                rttr:AddWorkshop{ name = "WoodYard", title = "Holzlager", size = "medium", cost = { Boards = 3 },
                    look = "Storehouse", warehouse = { wares = { "Wood", "Boards" }, capacity = 100 } }
  military    optional: macht das Gebäude zum Militärgebäude statt zur Werkstatt. Es verhält sich wie die eingebauten
              Militärgebäude: Soldaten besetzen es, es hält das Land im Umkreis radius, es kann angegriffen und erobert
              werden, die Besatzungsregler gelten, Soldaten werden mit Goldmünzen befördert und tragen mit dem
              Leder-Addon Rüstung. Wissenspunkte (knowledge) zählen ab Fertigstellung wie bei allen Gebäuden.
              military = { soldiers = 3, radius = 9, vision = 14, coins = 1, armor = 1 }
              soldiers  größte Besatzung (1 bis 255)
              radius    Landradius in Knoten (1 bis 255)
              vision    optional (Standard radius + 5): Sichtweite in Knoten, vom Gebäude aus gemessen (1 bis 255)
              coins     Plätze für Goldmünzen (0 bis 255; 0 = keine Beförderung in diesem Gebäude)
              armor     optional (Standard wie coins): Plätze für Rüstungen (0 bis 255)
              Die Werte der eingebauten Militärgebäude stehen unten bei rttr:SetBuilding (Barracks, Guardhouse,
              Watchtower, Fortress).
              look muss ein eingebautes Militärgebäude sein (Barracks, Guardhouse, Watchtower oder Fortress): davon
              kommen die Grafik und die Positionen der Fahnen. size "small", "medium" oder "large", nicht "mine".
              Ein Militärgebäude hat nur name, title, description, size, cost, look, knowledge und military; worker,
              inputs, outputs, workTime, waitTime, waitAfterTime, storage, tools, outputRatio, library, collect, plant,
              mine, warehouse und aiNearBorder sind nicht erlaubt (Militärgebäude darf die KI immer an der Grenze
              bauen).
              Im Baumenü steht es je nach size im passenden Reiter und ist nur dort wählbar, wo auch die eingebauten
              Militärgebäude erlaubt sind. Freischalten wie jede Werkstatt über unlocks in techtree.lua.
              KI: Alle Militärgebäude des Spiels (eingebaute und diese) sind nach soldiers sortiert, bei Gleichstand
              nach radius, dann nach der Reihenfolge der Definition (eingebaute zuerst). Die alte KI nimmt ihre Rollen
              aus dieser Reihenfolge: das 1. ist das kleinste, das 2. hat die Rolle der Wachstube, das vorletzte die
              des Wachturms, das letzte ist das größte. Ist das Gebäude einer Rolle noch nicht erforscht, weicht sie
              aus wie bei den eingebauten (z.B. auf das größte erlaubte).
              Beispiel:
                rttr:AddWorkshop{ name = "Palisade", title = "Palisade", size = "small",
                    cost = { BasicBuildingMaterial = 3 }, look = "Barracks", knowledge = { MWP = 1 },
                    military = { soldiers = 3, radius = 9, vision = 14, coins = 1, armor = 1 } }
                Reihenfolge damit: Barracks (2), Palisade (3, Radius 9), Guardhouse (3, Radius 12), Watchtower (6),
                Fortress (9). Die Palisade hat die Rolle der Wachstube. Eine Wachstube baut die alte KI dann nur noch,
                wenn sie an einem Platz, um den fast nichts bebaubar ist, auf das nächstgrößere Gebäude (size)
                umsteigt: das erste in der Reihenfolge, das größer ist (ai/aijh/Jobs.cpp).
  workTime    Arbeitszeit je Arbeitsschritt in Frames (Sammler und Plantagen: am Objekt), 1 bis 65535. Pflicht außer
              bei Bibliothek, Lager und Militärgebäude. Ablauf siehe "Zeiten" oben.
  waitTime    optional (Standard 0, gilt als 1): Wartezeit vor der Arbeit in Frames, 0 bis 65535.
  waitAfterTime optional (Standard 5): Wartezeit nach der Arbeit in Frames, bevor der Arbeiter das Produkt
              herausträgt, 0 bis 65535 (0 = 1 Frame). Nicht bei Sammlern und Plantagen.
  knowledge   optional: Wissenspunkte (WP) des fertigen Gebäudes, z.B. knowledge = { LWP = 1, IWP = 2 }.
              Arten: LWP = Landwirtschaft, IWP = Industrie, MWP = Militär. Fehlende Arten zählen 0, negative Werte sind
              erlaubt (das Katapult hat MWP = -5). Jedes FERTIGE Gebäude bringt seinem Besitzer diese Punkte, solange
              es steht (Baustellen zählen nicht). Gebäude ohne knowledge bringen keine WP.
              Die WP-Kosten der Technologien stehen in techtree.lua unter "knowledge", die Startwerte des Reglers
              "Wissen" in distribution.lua.
  tools       optional, nur mit der Werkzeugreform (tools.lua): Werkzeuge der Werkstatt und ihr Tempo ohne Werkzeug,
              z.B. tools = { baseSpeed = 25, tools = { "Scythe", "Plough" } }. baseSpeed in Prozent (Standard 0 = ohne
              Werkzeug keine Arbeit), tools = Werkzeugtypen als Tag-Abfragen (höchstens 4). Von jedem Typ liegt
              höchstens ein Werkzeug in Gebrauch im Gebäude, dazu höchstens ein Ersatz.
              Ohne diese Angabe braucht die Werkstatt kein Werkzeug und arbeitet mit Tempo 100.
  aiNearBorder optional (Standard false): true erlaubt der KI, die Werkstatt in Grenznähe zu bauen (höchstens 4 Knoten
              von einer Landgrenze entfernt). Sonst hält die KI diesen Platz für Militärgebäude frei
              (ai/aijh/AIConstruction.cpp, BORDER_RESERVE_RADIUS). Die eingebauten Gebäude haben aiNearBorder in
              rttr:SetBuilding.

Man sollte hier auch die im Spiel integrierten Gebäude anpassen können bzw. alle Gebäude sollten hier als
lua Werkstatt stehen oder zumdindest von hier aus ansteuerbar sein

Freischalten: In techtree.lua unter unlocks = { buildings = { "Name" } } eintragen. Wird eine Werkstatt von keiner
Technologie freigeschaltet, ist sie von Anfang an baubar.

WICHTIG für Spielstände: Neue Werkstätten immer hinten anfügen, bestehende nicht löschen, umbenennen oder umsortieren.
Es sind höchstens 128 Werkstätten möglich.

Beispiel (auskommentiert, nur zur Erklärung des Formats, braucht die Beispielwaren aus wares.lua):

rttr:AddWorkshop{
    name = "ExampleWorkshop",
    title = "Beispiel-Werkstatt",
    size = "small",
    cost = { Boards = 2 },
    look = "Woodcutter",
    worker = { title = "Beispiel-Arbeiter", look = "Carpenter" },
    inputs = { "ExampleRaw" },
    outputs = { "ExampleProduct" },
    workTime = 400,
}
]]

-------------------------------------------------------------------------------------------------------------------
-- Eingebaute Gebäude (rttr:SetBuilding)
--
-- Alles, was man braucht, um neue Werkzeuge herzustellen (Holz, Bretter, Stein, Bergbau, Eisenschmelze, Schlosserei),
-- arbeitet auch ohne Werkzeug langsam weiter (baseSpeed = 25). Sonst gäbe es nach dem Bruch des letzten Werkzeugs nie
-- wieder welche.
-------------------------------------------------------------------------------------------------------------------

-------------------------------------------------------------------------------------------------------------------
-- Lager
-------------------------------------------------------------------------------------------------------------------

rttr:SetBuilding{
    name = "Headquarters", -- Hauptquartier
    knowledge = { LWP = 7, IWP = 7, MWP = 7 },
    warehouse = { capacity = 700 },
}

rttr:SetBuilding{
    name = "Storehouse", -- Lagerhaus
    title = "Storehouse",
    size = "medium",
    cost = { BasicBuildingMaterial = 7, BasicRoofMaterial = 7 },
    look = "Storehouse",
    aiNearBorder = false,
    knowledge = { LWP = 1, IWP = 1, MWP = 1 },
    warehouse = { capacity = 400 },
}

rttr:SetBuilding{
    name = "HarborBuilding", -- Hafen
    aiNearBorder = true,
    knowledge = { LWP = 1, IWP = 1, MWP = 1 },
    warehouse = { capacity = 500 },
}

-------------------------------------------------------------------------------------------------------------------
-- Militär
-------------------------------------------------------------------------------------------------------------------

rttr:SetBuilding{
    name = "Barracks", -- Baracke
    title = "Barracks",
    size = "small",
    cost = { { tags = "BasicBuildingMaterial", amount = 4 }, { tags = "BasicRoofMaterial", amount = 2 } },
    look = "Barracks",
    knowledge = { MWP = 1 },
    military = { soldiers = 2, radius = 10, vision = 15, coins = 1, armor = 1 },
}

rttr:SetBuilding{
    name = "Guardhouse", -- Wachstube
    title = "Guardhouse",
    size = "small",
    cost = { { tags = "BasicBuildingMaterial", amount = 6 }, { tags = "BasicRoofMaterial", amount = 3 } },
    look = "Guardhouse",
    knowledge = { MWP = 2 },
    military = { soldiers = 3, radius = 12, vision = 17, coins = 2, armor = 2 },
}

rttr:SetBuilding{
    name = "Watchtower", -- Wachturm
    title = "Watchtower",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, Stones = 4, BasicRoofMaterial = 5 },
    look = "Watchtower",
    knowledge = { MWP = 3 },
    military = { soldiers = 6, radius = 14, vision = 19, coins = 4, armor = 4 },
}

rttr:SetBuilding{
    name = "Fortress", -- Festung
    title = "Fortress",
    size = "large",
    cost = { BasicBuildingMaterial = 6, Stones = 8, BasicRoofMaterial = 6 },
    look = "Fortress",
    knowledge = { MWP = 4 },
    military = { soldiers = 9, radius = 16, vision = 21, coins = 6, armor = 6 },
}

rttr:SetBuilding{
    name = "LookoutTower", -- Spähturm
    cost = { BasicBuildingMaterial = 5 },
    aiNearBorder = false,
    knowledge = { MWP = 1 },
}

rttr:SetBuilding{
    name = "Catapult", -- Katapult
    cost = { BasicBuildingMaterial = 5 },
    aiNearBorder = true,
    knowledge = { MWP = -5 },
}

-------------------------------------------------------------------------------------------------------------------
-- Holz, Stein, Bergbau
-------------------------------------------------------------------------------------------------------------------

rttr:SetBuilding{
    name = "Woodcutter", -- Holzfäller
    title = "Woodcutter",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },
    look = "Woodcutter",
    worker = { title = "Woodchopper", look = "Woodcutter" },
    collect = { radius = 6 },
    outputs = { "Wood" },
    outputRatio = 1,
    tools = { baseSpeed = 25, tools = { "Axe" } },
    workTime = 148,
    waitTime = 989,
    aiNearBorder = true,
    knowledge = { IWP = 2 },
}

rttr:SetBuilding{
    name = "Forester", -- Förster
    title = "Forester",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },
    look = "Forester",
    worker = { title = "Tree planter", look = "Forester" },
    plant = { radius = 6 },
    tools = { baseSpeed = 25, tools = { "Shovel" } },
    workTime = 66,
    waitTime = 704,
    aiNearBorder = false,
    knowledge = { LWP = 1, IWP = 1 },
}

rttr:SetBuilding{
    name = "Sawmill", -- Sägewerk
    title = "Sawmill",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 3 },
    look = "Sawmill",
    worker = { title = "Carpenter", look = "Carpenter" },
    inputs = { "Wood" },
    storage = 6,
    outputs = { "Boards" },
    outputRatio = 1,
    tools = { baseSpeed = 25, tools = { "Saw" } },
    workTime = 479,
    waitTime = 600,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { IWP = 3 },
}

rttr:SetBuilding{
    name = "Quarry", -- Steinbruch
    title = "Quarry",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },
    look = "Quarry",
    worker = { title = "Stonemason", look = "Stonemason" },
    collect = { radius = 8 },
    outputs = { "Cobblestones" },
    outputRatio = 1,
    tools = { baseSpeed = 25, tools = { "PickAxe" } },
    workTime = 129,
    waitTime = 925,
    aiNearBorder = true,
    knowledge = { IWP = 2 },
}

rttr:SetBuilding{
    name = "GraniteMine", -- Granitmine
    title = "Granite mine",
    size = "mine",
    cost = { BasicBuildingMaterial = 4, Rope = 1 },
    look = "GraniteMine",
    worker = { title = "Miner", look = "Miner" },
    mine = { resource = "Granite" },
    inputs = { { tags = "Food", storage = 6 } },
    outputs = { "Cobblestones" },
    outputRatio = 1,
    tools = { baseSpeed = 25, tools = { "PickAxe" } },
    workTime = 583,
    waitTime = 558,
    waitAfterTime = 5,
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:SetBuilding{
    name = "CoalMine", -- Kohlemine
    title = "Coal mine",
    size = "mine",
    cost = { BasicBuildingMaterial = 4, Rope = 1 },
    look = "CoalMine",
    worker = { title = "Miner", look = "Miner" },
    mine = { resource = "Coal" },
    inputs = { { tags = "Food", storage = 6 } },
    outputs = { "Coal" },
    outputRatio = 1,
    tools = { baseSpeed = 25, tools = { "PickAxe" } },
    workTime = 583,
    waitTime = 558,
    waitAfterTime = 5,
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:SetBuilding{
    name = "IronMine", -- Eisenmine
    title = "Iron mine",
    size = "mine",
    cost = { BasicBuildingMaterial = 4, Rope = 1 },
    look = "IronMine",
    worker = { title = "Miner", look = "Miner" },
    mine = { resource = "Iron" },
    inputs = { { tags = "Food", storage = 6 } },
    outputs = { "IronOre" },
    outputRatio = 1,
    tools = { baseSpeed = 25, tools = { "PickAxe" } },
    workTime = 583,
    waitTime = 558,
    waitAfterTime = 5,
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:SetBuilding{
    name = "GoldMine", -- Goldmine
    title = "Gold mine",
    size = "mine",
    cost = { BasicBuildingMaterial = 4, Rope = 1 },
    look = "GoldMine",
    worker = { title = "Miner", look = "Miner" },
    mine = { resource = "Gold" },
    inputs = { { tags = "Food", storage = 6 } },
    outputs = { "Gold" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "PickAxe" } },
    workTime = 583,
    waitTime = 558,
    waitAfterTime = 5,
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:SetBuilding{
    name = "Charburner", -- Köhler (Köhlerei-Addon)
    title = "Charburner",
    size = "large",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 3 },
    look = "Charburner",
    worker = { title = "Charburner", look = "CharBurner" },
    plant = { radius = 3 },
    inputs = { "Wood", "Grain" },
    storage = 6,
    outputs = { "Coal" },
    outputRatio = 1,
    tools = { baseSpeed = 25, tools = { "Shovel" } },
    workTime = 117,
    waitTime = 806,
    aiNearBorder = false,
    knowledge = { IWP = 4 },
}

-------------------------------------------------------------------------------------------------------------------
-- Nahrung und Landwirtschaft
-------------------------------------------------------------------------------------------------------------------

rttr:SetBuilding{
    name = "Fishery", -- Fischer
    title = "Fishery",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },
    look = "Fishery",
    worker = { title = "Fisher", look = "Fisher" },
    collect = { radius = 7 },
    outputs = { "Fish" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "RodAndLine" } },
    workTime = 729,
    waitTime = 625,
    aiNearBorder = true,
    knowledge = { LWP = 2 },
}

rttr:SetBuilding{
    name = "Hunter", -- Jäger
    title = "Hunter",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },
    look = "Hunter",
    worker = { title = "Huntsman", look = "Hunter" },
    collect = { radius = 19 },
    outputs = { "Meat" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Bow" } },
    waitTime = 900,
    aiNearBorder = false,
    knowledge = { LWP = 2 },
}

rttr:SetBuilding{
    name = "Well", -- Brunnen
    title = "Well",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2, Rope = 1 },
    look = "Well",
    outputs = { "Water" },
    outputRatio = 1,
    workTime = 385,
    waitTime = 400,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { LWP = 1 },
}

rttr:SetBuilding{
    name = "Farm", -- Bauernhof
    title = "Farm",
    size = "large",
    cost = { BasicBuildingMaterial = 6, BasicRoofMaterial = 4 },
    look = "Farm",
    worker = { title = "Farmer", look = "Farmer" },
    plant = { radius = 2 },
    outputs = { "Grain" },
    outputRatio = 1,
    tools = { baseSpeed = 25, tools = { "Scythe" } },
    workTime = 117,
    waitTime = 906,
    aiNearBorder = false,
    knowledge = { LWP = 3 },
}

rttr:SetBuilding{
    name = "Mill", -- Mühle
    title = "Mill",
    size = "medium",
    cost = { BasicBuildingMaterial = 6, BasicRoofMaterial = 2 },
    look = "Mill",
    worker = { title = "Miller", look = "Miller" },
    inputs = { "Grain" },
    storage = 6,
    outputs = { "Flour" },
    outputRatio = 1,
    workTime = 470,
    waitTime = 595,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { LWP = 3 },
}

rttr:SetBuilding{
    name = "Bakery", -- Bäcker
    title = "Bakery",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2, Stones = 3 },
    look = "Bakery",
    worker = { title = "Baker", look = "Baker" },
    inputs = { "Flour", "Water" },
    storage = 6,
    outputs = { "Bread" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Rollingpin" } },
    workTime = 470,
    waitTime = 600,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { LWP = 3 },
}

rttr:SetBuilding{
    name = "PigFarm", -- Schweinezucht
    title = "Pig farm",
    size = "large",
    cost = { BasicBuildingMaterial = 8, BasicRoofMaterial = 5 },
    look = "PigFarm",
    worker = { title = "Pig breeder", look = "PigBreeder" },
    inputs = { "Grain", "Water" },
    storage = 6,
    outputs = { "Ham" },
    outputRatio = 1,
    workTime = 390,
    waitTime = 660,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { LWP = 4 },
}

rttr:SetBuilding{
    name = "Slaughterhouse", -- Fleischerei
    title = "Slaughterhouse",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 2 },
    look = "Slaughterhouse",
    worker = { title = "Butcher", look = "Butcher" },
    inputs = { "Ham" },
    storage = 6,
    outputs = { "Meat" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Cleaver" } },
    workTime = 478,
    waitTime = 600,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { LWP = 3 },
}

rttr:SetBuilding{
    name = "Brewery", -- Brauerei
    title = "Brewery",
    size = "medium",
    cost = { BasicBuildingMaterial = 6, BasicRoofMaterial = 5 },
    look = "Brewery",
    worker = { title = "Brewer", look = "Brewer" },
    inputs = { "Grain", "Water" },
    storage = 6,
    outputs = { "Beer" },
    outputRatio = 1,
    workTime = 530,
    waitTime = 500,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { LWP = 1, MWP = 1 },
}

rttr:SetBuilding{
    name = "DonkeyBreeder", -- Eselzucht
    cost = { BasicBuildingMaterial = 8, BasicRoofMaterial = 5 },
    aiNearBorder = false,
    knowledge = { LWP = 4 },
}

rttr:SetBuilding{
    name = "Vineyard", -- Weinberg (Wein-Addon)
    title = "Vineyard",
    size = "large",
    cost = { BasicBuildingMaterial = 3, BasicRoofMaterial = 2 },
    look = "Vineyard",
    worker = { title = "Winegrower", look = "Winegrower" },
    plant = { radius = 2 },
    inputs = { "Wood", "Water" },
    storage = 6,
    outputs = { "Grapes" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Shovel" } },
    workTime = 117,
    waitTime = 906,
    aiNearBorder = false,
    knowledge = { LWP = 4 },
}

rttr:SetBuilding{
    name = "Winery", -- Winzerei (Wein-Addon)
    title = "Winery",
    size = "medium",
    cost = { BasicBuildingMaterial = 6, BasicRoofMaterial = 3 },
    look = "Winery",
    worker = { title = "Vintner", look = "Vintner" },
    inputs = { "Grapes" },
    storage = 6,
    outputs = { "Wine" },
    outputRatio = 1,
    workTime = 470,
    waitTime = 595,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { LWP = 3 },
}

-------------------------------------------------------------------------------------------------------------------
-- Metall und Handwerk
-------------------------------------------------------------------------------------------------------------------

rttr:SetBuilding{
    name = "Ironsmelter", -- Eisenschmelze
    title = "Iron smelter",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4, Stones = 4 },
    look = "Ironsmelter",
    worker = { title = "Iron founder", look = "IronFounder" },
    inputs = { "IronOre", "Coal" },
    storage = 6,
    outputs = { "Iron" },
    outputRatio = 1,
    tools = { baseSpeed = 25, tools = { "Crucible" } },
    workTime = 950,
    waitTime = 360,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { IWP = 3 },
}

rttr:SetBuilding{
    name = "Metalworks", -- Schlosserei
    title = "Metalworks",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4, Stones = 2 },
    look = "Metalworks",
    worker = { title = "Metalworker", look = "Metalworker" },
    inputs = { "Iron", "Boards" },
    storage = 6,
    outputs = { "IronTongs", "IronHammer", "IronAxe", "IronSaw", "IronPickAxe", "IronShovel", "IronCrucible",
        "RodAndLine", "IronScythe", "IronCleaver", "Rollingpin", "Bow" },
    outputRatio = 1,
    tools = { baseSpeed = 25, tools = { "Tongs" } },
    workTime = 850,
    waitTime = 500,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { IWP = 3 },
}

rttr:SetBuilding{
    name = "Armory", -- Waffenschmiede
    title = "Armory",
    size = "medium",
    cost = { BasicBuildingMaterial = 6, BasicRoofMaterial = 5, Stones = 5 },
    look = "Armory",
    worker = { title = "Armorer", look = "Armorer" },
    inputs = { "Iron", "Coal" },
    storage = 6,
    outputs = { "IronSword", "IronShield" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Hammer" } },
    workTime = 940,
    waitTime = 570,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { IWP = 2 },
}

rttr:SetBuilding{
    name = "Mint", -- Münzprägerei
    title = "Mint",
    size = "medium",
    cost = { BasicBuildingMaterial = 6, BasicRoofMaterial = 5, Stones = 5 },
    look = "Mint",
    worker = { title = "Minter", look = "Minter" },
    inputs = { "Gold", "Coal" },
    storage = 6,
    outputs = { "Coins" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Crucible" } },
    workTime = 1050,
    waitTime = 370,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { IWP = 3 },
}

rttr:SetBuilding{
    name = "Shipyard", -- Werft
    cost = { BasicBuildingMaterial = 7, BasicRoofMaterial = 6, Rope = 2 },
    tools = { baseSpeed = 0, tools = { "Hammer" } },
    aiNearBorder = false,
    knowledge = { IWP = 3 },
}

rttr:SetBuilding{
    name = "Temple", -- Tempel (Wein-Addon)
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 6, Stones = 8, Rope = 2 },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Crucible" } },
    aiNearBorder = false,
    knowledge = { IWP = 3 },
}

rttr:SetBuilding{
    name = "Skinner", -- Abdecker (Leder-Addon)
    title = "Skinner",
    size = "small",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 1 },
    look = "Skinner",
    worker = { title = "Skinner", look = "Skinner" },
    inputs = { "Ham" },
    storage = 3,
    outputs = { "Skins" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Cleaver" } },
    workTime = 470,
    waitTime = 695,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { IWP = 2 },
}

rttr:SetBuilding{
    name = "Tannery", -- Gerberei (Leder-Addon)
    title = "Tannery",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4 },
    look = "Tannery",
    worker = { title = "Tanner", look = "Tanner" },
    inputs = { "Skins", "Boards" },
    storage = 6,
    outputs = { "Leather" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Saw" } },
    workTime = 470,
    waitTime = 495,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { IWP = 3 },
}

rttr:SetBuilding{
    name = "LeatherWorks", -- Lederei (Leder-Addon)
    title = "Leatherworks",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4 },
    look = "LeatherWorks",
    worker = { title = "Leatherworker", look = "LeatherWorker" },
    inputs = { "Leather" },
    storage = 6,
    outputs = { "LeatherArmor" },
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Tongs" } },
    workTime = 470,
    waitTime = 695,
    waitAfterTime = 5,
    aiNearBorder = false,
    knowledge = { MWP = 3 },
}

-------------------------------------------------------------------------------------------------------------------
-- Werkstätten (rttr:AddWorkshop)
-------------------------------------------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "LoamProcessor",
    title = "Lehmverarbeiter",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2  },
    look = "Woodcutter",
    worker = { title = "Lehmverarbeiter", look = "Miner" },
    inputs = { "Loam" },
    outputs = { "Clay" },
    outputRatio = 0.5,
    workTime = 740,
}

-------------------------------------------------------------------------------
-- Knochen-Kette (Schritt 4a): 1 Knochen -> 1 geritzter Knochen (LWP, IWP oder MWP).
-- Entschieden: kleines Gebäude, Arbeiter mit Beil. Freigeschaltet durch "bone_carving" (techtree.lua).
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "BoneCarver",
    title = "Knochenschnitzer",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 }, 
    look = "Hunter",      
    worker = { title = "BoneCarver", look = "Butcher" },
    inputs = { "Bones" },
    outputs = { "GeritzterKnochen" },
    outputChoice = "knowledge",
    tools = { baseSpeed = 0, tools = { "Cleaver" } },
    workTime = 1000,         
}
-------------------------------------------------------------------------------
-- Bibliothek (Schritt 4c). Entschieden: mittelgroß, fasst 20 Medien, Arbeiter "Bibliothekar" ohne Werkzeug, erst ab
-- Ära 2 (Technologie "libraries" in techtree.lua).
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "Library",
    title = "Bibliothek",
    size = "large",
    cost = { BasicBuildingMaterial = 8, BasicRoofMaterial = 6, Stones = 6  }, 
    look = "Temple",
    worker = { title = "Bibliothekar", look = "Helper" },
    library = { capacity = 20 },
}

-------------------------------------------------------------------------------
-- Lehmgrube: sammelt Lehmvorkommen (mapobjects.lua) wie der Steinbruch die Steinhaufen.
-- Der Lehmstecher gräbt mit der Schaufel (Grafik-Vorbild: Planierer).
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "LoamPit",
    title = "Lehmgrube",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2  },           
    look = "Quarry",                
    worker = { title = "Lehmstecher", look = "Planer" },
    collect = { object = "LoamDeposit", radius = 9 }, 
    outputs = { "Loam" },
    outputRatio = 0.75,
    tools = { baseSpeed = 25, tools = { "Shovel" } },
    workTime = 140,                    
    waitTime = 540,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "PeatCollector",
    title = "PeatCollector",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },           
    look = "Quarry",                
    worker = { title = "Lehmstecher", look = "Planer" },
    collect = { object = "PeatDeposit", radius = 9 }, 
    outputs = { "Coal" },
    outputRatio = 0.4,
    tools = { baseSpeed = 25, tools = { "Shovel", "Bucket" } },
    workTime = 140,                    
    waitTime = 400,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "TinCollector",
    title = "TinCollector",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },           
    look = "Quarry",                
    worker = { title = "Miner", look = "Miner" },
    collect = { object = "TinDeposit", radius = 9 }, 
    outputs = { "TinOre" },
    outputRatio = 0.6,
    tools = { baseSpeed = 0, tools = { "PickAxe", "Bucket" } },
    workTime = 140,                    
    waitTime = 540,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "CopperCollector",
    title = "CopperCollector",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },           
    look = "Quarry",                
    worker = { title = "Miner", look = "Miner" },
    collect = { object = "CopperDeposit", radius = 9 }, 
    outputs = { "CopperOre" },
    outputRatio = 0.5,
    tools = { baseSpeed = 25, tools = { "PickAxe", "Bucket" } },
    workTime = 140,                    
    waitTime = 600,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "IronCollector",
    title = "IronCollector",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },           
    look = "Quarry",                
    worker = { title = "Miner", look = "Miner" },
    collect = { object = "IronDeposit", radius = 9 }, 
    outputs = { "IronOre" },
    outputRatio = 0.7,
    tools = { baseSpeed = 25, tools = { "PickAxe", "Bucket" } },
    workTime = 140,                    
    waitTime = 640,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "ObsidianCollector",
    title = "ObsidianCollector",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2 },           
    look = "Quarry",                
    worker = { title = "Miner", look = "Miner" },
    collect = { object = "ObsidianDeposit", radius = 9 }, 
    outputs = { "Obsidian" },
    outputRatio = 0.3,
    tools = { baseSpeed = 25, tools = { "PickAxe", "Bucket" } },
    workTime = 140,                    
    waitTime = 540,                   
    aiNearBorder = true,
    knowledge = { IWP = 1 },
}

-------------------------------------------------------------------------------
-- Kupfermine: baut das Erz Kupfer (mapobjects.lua) ab wie die Eisenmine das Eisenerz.
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "CopperMine",
    title = "Kupfermine",
    size = "mine",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2, Rope = 1 },
    look = "IronMine",
    worker = { title = "Miner", look = "Miner" },
    inputs = { { tags = "Food", storage = 6 } },
    mine = { resource = "Copper" },
    outputs = { "CopperOre" },
    outputRatio = 0.5,
    tools = { baseSpeed = 25, tools = { "PickAxe" } },
    workTime = 560,
    waitTime = 300,
    knowledge = { IWP = 1 },
    aiNearBorder = true,
}

rttr:AddWorkshop{
    name = "TinMine",
    title = "TinMine",
    size = "mine",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2, Rope = 1 },
    look = "GraniteMine",
    worker = { title = "Miner", look = "Miner" }, -- es sollte nicht für jedes Gebäude ein neuer Job erstellt werden, sondern auch vorhandene 
    -- Jobs  wie zB. Miner übernommen werden können
    inputs = { "Wood", { tags = "Food", storage = 6 } },
    storage = 2,
    mine = { resource = "Tin" },
    outputs = { "TinOre" },
    outputRatio = 0.7,
    tools = { baseSpeed = 0, tools = { "PickAxe" } },
    workTime = 560,
    waitTime = 300,
    knowledge = { IWP = 1 },
    aiNearBorder = true,
}

rttr:AddWorkshop{
    name = "LimeMine",
    title = "LimeMine",
    size = "mine",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 2, Rope = 1 },
    look = "GraniteMine",
    worker = { title = "Miner", look = "Miner" }, -- es sollte nicht für jedes Gebäude ein neuer Job erstellt werden, sondern auch vorhandene 
    -- Jobs  wie zB. Miner übernommen werden können
    inputs = { "Wood", { tags = "Food", storage = 6 } },
    storage = 2,
    mine = { resource = "Lime" },
    outputs = { "Lime" },
    outputRatio = 0.8,
    tools = { baseSpeed = 0, tools = { "PickAxe" } },
    workTime = 560,
    waitTime = 360,
    knowledge = { IWP = 1 },
    aiNearBorder = true,
}

-------------------------------------------------------------------------------
-- Papyrusplantage: pflanzt Papyrus (mapobjects.lua) dort, wo er auch natürlich wächst (am Süßwasser mit passenden
-- Temperatur- und Feuchtigkeitswerten, siehe world/mapgen.lua, ohne Zonenbeschränkung), gießt ihn mit Wasser und
-- erntet ihn selbst.
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "PapyrusPlantation",
    title = "PapyrusPlantation",
    size = "small",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4 },
    look = "Farm",
    worker = { title = "Papyrusbauer", look = "Farmer" },
    inputs = { "Water" },
    plant = { object = "PapyrusDeposit", radius = 9 },
    outputs = { "PapyrusPlant" },
    outputRatio = 0.5,
    tools = { baseSpeed = 0, tools = { "Scythe" } },
    workTime = 120,
    waitTime = 1020,
    knowledge = { LWP = 2 },
}

rttr:AddWorkshop{
    name = "SpinningMill",
    title = "SpinningMill",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 3 },
    look = "Winery",
    worker = { title = "Spinner", look = "Farmer" },
    inputs = { { tags = "Fibers" } },
    outputs = { "Rope" },
    outputRatio = 0.8,
    workTime = 120,
    waitTime = 600,
    tools = { baseSpeed = 25, tools = { "Spindle" } },
    knowledge = { LWP = 1, IWP = 2 },
}

-------------------------------------------------------------------------------
-- Werkzeughersteller
-------------------------------------------------------------------------------

rttr:AddWorkshop{
    name = "WoodWorkshop",
    title = "WoodWorkshop",
    size = "small",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 3 },
    look = "Sawmill",
    worker = { title = "Metalworker", look = "Metalworker" },
    inputs = { "Wood", "Rope" }, 
    outputs = { "Rollingpin", "RodAndLine", "Bow", "WoodenSpindle", "WoodenBucket", "WoodenHammer" },
    outputRatio = 0.6,
    tools = { baseSpeed = 25, tools = { "Saw" } },
    workTime = 700,
    knowledge = { IWP = 2 },
}

rttr:AddWorkshop{
    name = "ObsidianWorkshop",
    title = "ObsidianWorkshop",
    size = "small",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 4 },
    look = "Metalworks",
    worker = { title = "Metalworker", look = "Metalworker" },
    inputs = { "Obsidian", "Wood" }, -- "adhesive" ergänzen wenn diese endlich collectable auf der Map sind
    outputs = { "ObsidianAxe", "ObsidianSaw", "ObsidianScythe", "ObsidianCleaver", "ObsidianSword", "ObsidianSpear", "ObsidianArrow"}, -- eigentlich ObsidianSword auch für Rekrutierung nutzbar sein, aber das funktioniert noch nicht. 
    outputRatio = 0.8,
    workTime = 600,
    knowledge = { IWP = 2 },
}

rttr:AddWorkshop{
    name = "Copperworks",
    title = "Copperworks",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 4, Stones = 4 },
    look = "Metalworks",
    worker = { title = "Metalworker", look = "Metalworker" },
    inputs = { "CopperOre", "Coal" },
    outputs = { "CopperTongs", "CopperHammer", "CopperAxe", "CopperSaw", "CopperPickAxe" },
    outputRatio = 0.5,
    tools = { baseSpeed = 25, tools = { "Tongs" } },
    workTime = 600,
    knowledge = { IWP = 2 },
}

rttr:AddWorkshop{
    name = "AdobeProducer",
    title = "AdobeProducer",
    size = "small",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 2  },
    look = "Skinner",
    worker = { title = "AdobeProducer", look = "Metalworker" },
    inputs = { "Loam", { tags = "Fibers" } },
    outputs = { "Adobe" },
    workTime = 700,
    knowledge = { IWP = 2 },
}

rttr:AddWorkshop{
    name = "Brickworks",
    title = "Brickworks",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 4 },
    look = "LeatherWorks",
    worker = { title = "Brickworker", look = "Metalworker" },
    inputs = { "Loam", "Coal" },
    outputs = { "LoamBrick" },
    workTime = 1000,
    knowledge = { IWP = 3 },
}
--[[
rttr:AddWorkshop{
    name = "AsphaltDrawer",
    title = "AsphaltDrawer",
    size = "small",
    cost = { Wood = 4 },
    look = "Fishery",
    worker = { title = "AsphaltDrawer", look = "AsphaltDrawer" },
    inputs = { "Bucket" }, 
    outputs = { "Asphalt" },
    outputRatio = 1,
    workTime = 2400,
    knowledge = { IWP = 1 },
}

rttr:AddWorkshop{
    name = "BitumenProducer",
    title = "BitumenProducer",
    size = "medium",
    cost = { Boards = 2, Stones = 4 },
    look = "Leatherworks",
    worker = { title = "BitumenProducer", look = "Metalworker" },
    inputs = { "Asphalt", "Firewood" }, 
    outputs = { "Bitumen" },
    outputRatio = 0.5,
    workTime = 2400,
    knowledge = { IWP = 3 },
}

rttr:AddWorkshop{
    name = "FirewoodSplitter",
    title = "FirewoodSplitter",
    size = "small",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 4 },
    look = "Woodcutter",
    worker = { title = "Woodcutter", look = "Woodcutter" },
    inputs = { "Wood" }, 
    outputs = { "Firewood" },
    outputRatio = 2,
    tools = { baseSpeed = 0, tools = { "Axe" } },
    workTime = 800,
    knowledge = { LWP = 1, IWP = 1 },
}]]

rttr:AddWorkshop{
    name = "BronzeSmelter",
    title = "Bronze Smelter",
    size = "medium",
    cost = { BasicBuildingMaterial = 4, BasicRoofMaterial = 6, Stones = 4 },
    look = "Ironsmelter",
    worker = { title = "Iron founder", look = "Minter" },
    inputs = { "CopperOre", "Coal", "TinOre" },
    outputs = { "BronzeIngot" },
    outputRatio = 0.5,
    tools = { baseSpeed = 25, tools = { "Crucible" } },
    workTime = 1000,
    knowledge = { IWP = 3 },
}

rttr:AddWorkshop{
    name = "BronzeArmory",
    title = "Bronze Armory",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 6, Stones = 5 },
    look = "Armory",
    worker = { title = "Armorer", look = "Armorer" },
    inputs = { "BronzeIngot", "Coal"},
    outputs = { "BronzeSword", "BronzeShield" }, --  BronzeSword und BronzeShield kann noch nicht Verwendet werden. 
    outputRatio = 1,
    tools = { baseSpeed = 0, tools = { "Hammer" } },
    workTime = 1000,
    knowledge = { IWP = 2, MWP = 2 },
}

rttr:AddWorkshop{
    name = "Bronzeworks",
    title = "Bronzeworks",
    size = "medium",
    cost = { BasicBuildingMaterial = 5, BasicRoofMaterial = 6, Stones = 5 },
    look = "Metalworks",
    worker = { title = "Armorer", look = "Armorer" },
    inputs = { "BronzeIngot", "Coal"},
    outputs = { "BronzeTongs", "BronzeAxe", "BronzeSaw",  "BronzeHammer", "BronzePickAxe", "BronzeScythe", "BronzeShovel", "BronzeCrucible", "BronzeCleaver" }, 
    outputRatio = 0.8,
    tools = { baseSpeed = 25, tools = { "Tongs" } },
    workTime = 900,
    knowledge = { IWP = 2, MWP = 2 },
}
--[[
rttr:AddWorkshop{
    name = "Sawwmill",
    title = "Sawwmill",
    size = "medium",
    cost = { Boards = 2 },
    look = "Sawmill",
    worker = { title = "Copperworker", look = "Metalworker" },
    inputs = { "Holzstamm"},
    outputs = { "Boards" },
    tools = { baseSpeed = 0, tools = { "Scythe" } },
    workTime = 2400,
}]]
