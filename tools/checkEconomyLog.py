#!/usr/bin/env python3
# Copyright (C) 2026 DA4-cpu
#
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prueft die ECONOMY-Berichte eines Logs auf Widersprueche in der Prognose.

- Prognose = frei + Herstellung - Verbrauch (auf 1 genau, Prognose ist abgerundet)
- frei = Bestand - offen - geplant
- Eine Ware oder Gruppe im Ueberschuss, waehrend eine Gruppe im Mangel ist, die sie enthaelt
- Eine Ware oder Gruppe im Mangel mit einer Prognose von 0 oder mehr
- Bewertet (Auswertung des Mangels, Baukosten x Faktor Baumaterial) unter der Prognose

Aufruf: checkEconomyLog.py <log>
"""
import re
import sys
from collections import Counter

STATUS = re.compile(
    r" ([^;()]+?) \(Prognose (-?[\d.]+) = frei (-?[\d.]+) \+ Herstellung (-?[\d.]+) - Verbrauch (-?[\d.]+); "
    r"Bestand (\d+), angefragt: offen (-?[\d.]+), geplant (-?[\d.]+)(?:, davon bekommt keine Ware ([\d.]+))?; "
    r"unterwegs zum Abnehmer (\d+)(?:; bewertet (-?[\d.]+))?\);")
SURPLUS = re.compile(r" ([^;()]+?) \(Prognose (-?[\d.]+)\);")


def parse_reports(path):
    report = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r"ECONOMY player (\d+), GF (\d+):", line)
            if m:
                if report:
                    yield report
                report = {"player": int(m.group(1)), "gf": int(m.group(2)), "short": [], "surplus": []}
            elif report is not None and line.startswith("  Mangel:"):
                for m in STATUS.finditer(line):
                    report["short"].append(m.groups())
            elif report is not None and line.startswith("  Überschuss:"):
                for m in SURPLUS.finditer(line):
                    report["surplus"].append((m.group(1), float(m.group(2))))
    if report:
        yield report


def main():
    problems = Counter()
    examples = {}
    num = 0
    for rep in parse_reports(sys.argv[1]):
        num += 1
        where = "Spieler %d, GF %d" % (rep["player"], rep["gf"])
        short_groups = []
        for name, fc, free, prod, cons, stock, open_, planned, missing, transit, evaluated in rep["short"]:
            # Alte Berichte: Prognose abgerundet (ganze Zahl), neue: eine Nachkommastelle wie die Teile
            tolerance = 0.25 if "." in fc else 1.05
            fc, free, prod, cons = float(fc), float(free), float(prod), float(cons)
            stock, open_, planned = int(stock), float(open_), float(planned)
            if abs((free + prod - cons) - fc) > tolerance:
                key = "Prognose passt nicht zur Formel"
                problems[key] += 1
                examples.setdefault(key, "%s: %s" % (where, name))
            if abs(stock - open_ - planned - free) > 0.15:
                key = "frei passt nicht zu Bestand - offen - geplant"
                problems[key] += 1
                examples.setdefault(key, "%s: %s" % (where, name))
            if fc >= 0:
                key = "im Mangel mit Prognose >= 0"
                problems[key] += 1
                examples.setdefault(key, "%s: %s" % (where, name))
            # Bewertet (Baukosten x Faktor Baumaterial, Faktor hoechstens 1): nie unter der Prognose
            if evaluated is not None and float(evaluated) < fc - 0.15:
                key = "bewertet unter der Prognose"
                problems[key] += 1
                examples.setdefault(key, "%s: %s" % (where, name))
            short_groups.append(set(name.split("/")))
        for name, fc in rep["surplus"]:
            wares = set(name.split("/"))
            for group in short_groups:
                if wares <= group:
                    key = "Überschuss einer Ware/Gruppe, die in einer Mangelgruppe steckt"
                    problems[key] += 1
                    examples.setdefault(key, "%s: %s (Prognose %s) in %s" % (where, name, fc, "/".join(sorted(group))))
    print("Berichte:", num)
    if not problems:
        print("keine Widersprüche gefunden")
    for key, count in problems.most_common():
        print("%5d  %s   z. B. %s" % (count, key, examples[key]))


if __name__ == "__main__":
    main()
