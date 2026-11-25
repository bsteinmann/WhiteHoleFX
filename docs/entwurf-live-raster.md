# Entwurf: Freies Live-Raster (Felder mit Looks belegen)

Warum es dieses Papier gibt: Der Wunsch vom 25.11. ändert das Datenmodell des Live-Modus (Flash/Zeiten wandern vom Look
auf das Feld, neue Zeilen im Projektformat). Das soll vor dem Bauen abgestimmt sein, nicht erst im Pull Request.

## 0. Wunsch des Nutzers (25.11.)

- Das Raster (Spalten × Zeilen, seit PR „festes Raster“) ist **fest**, die Felder werden **frei mit Looks belegt**.
- Nicht jeder gespeicherte Look kommt in den Live-Modus; nicht belegte Felder bleiben leer.
- **Derselbe Look kann in mehreren Feldern liegen**, z. B. einmal als Flash und einmal als normaler Knopf. Darum gehören
  *Flash, Einblendzeit und Zurückblendzeit zum Feld*, nicht mehr zum Look.

## 1. Datenmodell (Vorschlag)

- `project::LiveGrid { columns, rows, cells }`, `cells` hat `columns × rows` Einträge (zeilenweise, wie das Raster).
- `project::LiveCell { std::optional<std::string> look; LookTrigger trigger; optional<double> flashInSeconds, returnSeconds; }`.
  `look` ist der **Name** (Looknamen sind eindeutig, `findLook`); Umbenennen und Löschen eines Looks zieht die Felder nach
  (Umbenennen ändert den Namen im Feld, Löschen leert das Feld).
- Projektdatei: ein Block `livegrid <spalten> <zeilen>`, darin je belegtem Feld `cell <index> "<look>" [flash] [in s] [return s]`.
  Leere Felder stehen nicht in der Datei.
- Die Größe (Spalten/Zeilen) zieht mit der Belegung ins Projekt, denn ein Feldindex ist nur mit der Spaltenzahl
  eindeutig. `AppSettings.liveColumns/liveRows` bleiben als Vorgabe für neue Projekte und für die Übernahme (2.).
- `Look.trigger/flashInSeconds/returnSeconds` bleiben im Look-Block erhalten: als **Vorgabe**, wenn ein Look in ein Feld
  gelegt wird (Überschreiben eines Looks behält sie wie bisher).

## 2. Übernahme bestehender Projekte

Projekt ohne `livegrid`: beim Laden entsteht das Raster aus den Looks, zeilenweise, mit Trigger und Zeiten des Looks und
der Größe aus den Programmeinstellungen (mindestens so viele Zeilen, dass alle Looks hineinpassen). Es sieht danach genau
so aus wie heute. Beim Speichern steht der Block in der Datei. **Einbahnstraße:** Programme bis 1.1.x lesen Projekte mit
`livegrid` nicht (unbekannte Zeile).

## 3. Player und Eingänge

- Auslösen/Loslassen arbeitet auf **Feldern** (`pressCell(i)`/`releaseCell(i)`); die bisherigen Funktionen je Look-Index
  bleiben für Cues und Vorschau.
- MIDI-Pads, DMX-Auswahlkanal und Zifferntasten 1–9 zählen **Felder** (Pad = Raster-Position, Taste n = Feld n);
  ein leeres Feld tut nichts. Pad-LED: leer = aus, sonst Farbe nach Zustand des Feldes (Flash/Toggle wie bisher).
- „Ist dieser Look gerade live?“ gilt je Look: liegt er in mehreren Feldern, leuchten alle.

## 4. Bedienung im Live-Fenster

- Rechtsklick auf ein **leeres Feld**: „Look hierher legen“ → Liste aller Looks.
- Rechtsklick auf ein **belegtes Feld**: Look wechseln, Flash-Haken, Einblend-/Zurückblendzeit (je Feld), Feld leeren.
- Neu gespeicherter Look: wird ins nächste freie Feld gelegt, falls eines frei ist (sonst nur gespeichert).
- Verschieben per Ziehen: später, nicht in der ersten Stufe.

## 4a. Vorschaubilder

- Das Standbild gehört weiter zum **Look** (wird beim Speichern des Looks aufgenommen, 96 × 54 Pixel, PR #65) und erscheint
  auf **jedem Feld**, das den Look enthält. Liegt ein Look in zwei Feldern, zeigen beide dasselbe Bild.
- Die Felder bleiben `LookButton`s: das Bild wächst mit der Feldgröße (Seitenverhältnis bleibt). Ein leeres Feld zeigt
  keinen Knopf, nur eine schwach umrandete Fläche, damit man es zum Belegen findet.
- Kein Bild (alter Look ohne Standbild): Feld zeigt nur den Namen, wie heute. Neu speichern nimmt das Bild auf.

## 5. Schritte

1. Kern: `LiveGrid`/`LiveCell`, Parser/Writer, Übernahme, Nachziehen bei Umbenennen/Löschen, Tests (Catch2).
2. Player: Felder auslösen/loslassen, MIDI/DMX/Tasten auf Felder, Pad-LEDs; Tests.
3. Studio: Raster zeigt Felder mit Vorschaubild, leere Felder als Fläche, Rechtsklick-Belegung, Einstellungen je Feld; Rauchtest, Übersetzungen.
4. Doku (`project-format.md`, `studio.md`, `entwurf-midi.md`), Changelog.

## 6. Offen

- Ob die Belegung im Projekt oder in den Programmeinstellungen liegt (Vorschlag: Projekt, weil sie zur Show gehört und
  zusammen mit den Looks weitergegeben wird).
- Ob ein neu gespeicherter Look automatisch ins nächste freie Feld kommt (Vorschlag: ja).
