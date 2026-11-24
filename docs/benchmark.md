# Benchmark

Warum es diese Seite gibt: Ob eine Grafikkarte eine bestimmte Auflösung oder LED-Zahl mit einem Effekt flüssig schafft,
soll man messen statt raten können, und die Ergebnisse verschiedener Rechner sollen vergleichbar sein. Plan und
Entscheidungen stehen in [entwurf-benchmark.md](entwurf-benchmark.md); hier steht, was es heute gibt.

## Aufruf

```
whitehole_cli --benchmark both                 # beide Messreihen, voller Lauf (28 Stufen, etwa 5,5 Minuten)
whitehole_cli --benchmark leds --quick         # Schnelltest: drei Stufen, unter einer Minute
whitehole_cli --benchmark projection --out ergebnis.md   # zusätzlich als Datei (.md oder .csv)
```

Kein Projekt, kein Netzwerk: Gemessen werden vier fest eingebaute Szenen (Plasma, Partikel, Labyrinth, „Schichten“ aus
Plasma + Partikel → Kaleidoskop + Labyrinth). Strg+C bricht nach dem laufenden Frame ab; der Bericht erscheint trotzdem.
`--software` misst den Software-Adapter. Rückgabewert: 0 vollständig, 1 abgebrochen, 2 keine GPU oder Datei nicht schreibbar.
Der Knopf im Studio folgt.

## Messreihen

- **Projektion:** 480×270, 960×540, 1920×1080, offscreen in eine Textur in Zielauflösung (ohne Fenster, Vollbild oder Vsync).
  Zurückgelesen wird nur ein kleiner Zaun-Puffer, weil eine Projektion auf der GPU bleibt.
- **LEDs:** 1 000, 10 000, 100 000, 1 Mio. LEDs auf einer Matrix; die Farben werden wie im Betrieb zurückgelesen.

Je Reihe geht es vom kleinsten zum größten Aufwand. Je Stufe: etwa 2 s Aufwärmen (verworfen, der erste Frame zählt nie, weil
Shader dann übersetzt werden), etwa 10 s Messen, mindestens fünf Frames.

## Abbruchregeln

- Median über dem doppelten Budget (2 × 33 ms): Ergebnis `over`, größere Stufen derselben Szene werden übersprungen.
- Hochrechnung: Läge eine Stufe nach der vorigen derselben Szene über 500 ms je Frame, wird sie übersprungen, ohne zu rendern.
- Einzelner Frame über 500 ms: ganzer Lauf bricht ab (unter Windows setzt der Treiber die GPU nach rund 2 s zurück).
- GPU-Fehler, verlorenes Gerät oder keine Antwort der GPU (5 s): sofortiger Abbruch mit Angabe der Stufe.
- 95. Perzentil über dem Dreifachen des Medians: Stufe gilt als „am Limit“.

## Ergebnis

Markdown-Tabelle (zum Einfügen in ein Issue) oder CSV, mit Gerät, Grafikschnittstelle, Treiber, Programmversion und Commit. Spalten: Median, 95. Perzentil,
CPU-Zeit (Aufzeichnen, Einreichen, CPU-Nodes) und GPU-Zeit (über Zeitstempel-Abfragen der GPU, vom Anfang des ersten bis
zum Ende des letzten Compute-Passes eines Frames; leer, wenn Adapter oder Treiber keine Zeitstempel kann). Darunter je Reihe die größte
Stufe, bei der jede Szene stabil lief (95. Perzentil unter 33 ms, nicht am Limit).

Kennzeichen in der Spalte Ergebnis: `ok`, `over` (über Grenze), `skipped` (übersprungen), `not-run` (Lauf vorher
abgebrochen), `failed` (Lauf an dieser Stufe abgebrochen).

Code: `libs/wh_runtime` (`benchmark_plan`, `benchmark_runner`, `benchmark_report`, `benchmark_scenes`,
`benchmark_gpu_target`), `apps/whitehole_cli/cli_benchmark.cpp`.
