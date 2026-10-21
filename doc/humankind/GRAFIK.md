<!--
Copyright (C) 2026 DA4-cpu

SPDX-License-Identifier: GPL-2.0-or-later
-->

# Grafiken für den Zivilisationsmodus (Recherche, Stand 2026-10-18)

> **Stand 2026-10-20:** Neue Grafiken erzeugt der Nutzer mit Gemini und bearbeitet sie nach. Der Weg über ein eigenes
> LoRA ist eingestellt. Der Abschnitt „Was S2/RttR technisch verlangt“ gilt weiterhin (welche Bilder eine neue Ware,
> ein Gebäude oder ein Beruf braucht); bis Grafiken da sind, verwendet der Code Platzhalter.

## Was S2/RttR technisch verlangt

- **Palette:** 256 Farben (`PAL5.BBM`). Alles muss am Ende auf diese Palette reduziert werden.
- **Spielerfarbe:** separate Bilder mit Subtyp `player` (Fahnen, Soldaten, Dächer bekommen die Spielerfarbe).
- **Schatten:** separate Bilder mit Subtyp `shadow`.
- **Gebäude:** ein Bild pro Volk (Römer, Wikinger, Afrikaner, Japaner, Babylonier) + Schatten + Baustellen-Gerüst.
- **Berufe (Figuren):** Laufanimation in **6 Richtungen × 8 Bilder**, dazu Arbeitsanimation und Icon. Figuren sind nur ca. 10–20 Pixel hoch.
- **Waren:** Icon, Bild „auf der Fahne“, **Träger mit dieser Ware** (dicker + dünner Träger × 6 Richtungen × 8 Bilder = 96 Bilder), Esel/Boot mit Ware.
  Zum Vergleich: Das Leder-Addon (3 Gebäude, 3 Waren, 3 Berufe) bringt rund 640 Einzelbilder mit.

### Einbinden ohne Packer

RttR lädt Ordner wie Archive (siehe `doc/ResourceLoading.md`). Eine neue Grafik ist einfach eine BMP-Datei:

```text
RTTR/assets/addons/<Addon-ID als Hex>/<archiv>/<index>.nx<X>.ny<Y>.bmp
RTTR/assets/addons/<Addon-ID als Hex>/<archiv>/<index>.player.bmp
RTTR/assets/addons/<Addon-ID als Hex>/<archiv>/<index>.shadow.bmp
```

`nx/ny` verschieben den Ursprung (z. B. die Tür des Gebäudes). Eigene Testgrafiken können auch im Benutzerordner
`LSTS` liegen und überschreiben dann alles andere.

## Drei Wege, Grafiken per KI zu erstellen

### A) 2D-Pixel-Art-Generatoren – gut für Icons und Waren

| Werkzeug | Stärken | Kosten (Stand Aug–Okt 2026) |
|----------|---------|------------------------------|
| PixelLab | 8 Richtungen auf einmal, Skelett-Animation, Stil-Referenzbilder, Aseprite-Plugin; max. 168×168 px | 40 Gratis-Generierungen, ab 12 $/Monat |
| Retro Diffusion | Palette per Bild erzwingbar, Animationen (kurze Loops), Aseprite-Erweiterung lokal | einmalig 20–65 $ (Erweiterung) oder Credits |
| Scenario | Eigenes Modell auf eigene Referenzgrafiken trainieren → sehr einheitlicher Stil | ab 15 $/Monat, Training ab 45 $ |

Schwächen: Die exakte S2-Perspektive und die winzigen Figuren (10–20 px) treffen 2D-Generatoren schlecht;
Richtungen und Animationsbilder passen oft nicht sauber zueinander. Für **Waren-Icons, Gebäude-Entwürfe und Porträts** gut geeignet.

### B) KI-3D-Modell + Blender-Rendering – empfohlen für Gebäude und Figuren

1. 3D-Modell per KI aus Text oder Bild erzeugen (Meshy, Tripo; **Hunyuan3D** ist Open Source und läuft lokal auf der eigenen GPU).
   Meshy und Tripo bieten automatisches Rigging/Animationen für Figuren.
2. In Blender mit **fester S2-Kamera und S2-Licht** rendern – 6 Richtungen, 8 Animationsbilder – automatisch per Skript.
3. Verkleinern, auf die S2-Palette reduzieren, Schatten und Spielerfarbe als eigene Bilder ausgeben, BMPs nach obigem Schema benennen.

Vorteil: Alle Richtungen und Bilder sind zueinander konsistent, Licht und Perspektive stimmen mit S2 überein.
Die Skripte für Schritt 2 und 3 kann Claude schreiben; Schritt 1 machst du im jeweiligen Werkzeug.

### C) Zusammensetzen per Skript – für Träger mit neuen Waren

Die 96 Trägerbilder pro neuer Ware lassen sich ohne KI erzeugen: vorhandene Trägerbilder nehmen und das (kleine)
Waren-Icon an die Hand-Position kopieren. Das kann ein Skript aus deinen Original-S2-Dateien machen.

## Rechtliches (kein Rechtsrat)

- Ein KI-Modell auf **Original-S2-Grafiken** zu trainieren oder sie als Stilreferenz zu nutzen ist für den
  **privaten Gebrauch** unproblematischer als für eine Veröffentlichung. Für eine Veröffentlichung eigene Referenzen nutzen.
- Lizenzbedingungen der Werkzeuge beachten: z. B. sind Meshy-Gratisergebnisse CC BY 4.0 (Namensnennung nötig),
  Tripo-Gratisergebnisse nicht kommerziell nutzbar.

## Vorschlag für den nächsten Schritt

Ein Testobjekt komplett durch die Pipeline schicken: das **Gelehrtenhaus** (Phase 2) über Weg B
und die Ware **Schriftrolle** (Icon über Weg A, Träger über Weg C). Danach wissen wir, was die Pipeline taugt.

## Quellen

- [Best AI Pixel Art and Sprite Generators for Games in 2026 (Cinevva)](https://app.cinevva.com/guides/ai-pixel-art-generators)
- [PixelLab – Create 8 rotations (Pro)](https://www.pixellab.ai/docs/tools/create-8-rotations-pro)
- [Retro Diffusion Models – The Essentials (Scenario)](https://help.scenario.com/en/articles/retro-diffusion-models-the-essentials/)
- [Best AI 3D Model Generators 2026 (Krea)](https://www.krea.ai/blog/best-ai-3d-model-generators-2026)
