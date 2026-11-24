# Entwurf: Benchmark-Knopf (WhiteHoleFX)

Warum es diese Datei gibt: Der Benchmark-Knopf wurde in einem anderen Chat entworfen; hier steht der Entwurf samt Prüfung gegen den
Code, damit die Umsetzung später ohne den Chat starten kann.

**Status: Entwurf, nicht freigegeben.** Umsetzung erst nach ausdrücklicher Freigabe des Nutzers (CLAUDE.md, Backlog-Regel).

## 1. Ziel

Ein Knopf im Studio, der die Leistungsmessung vollständig automatisch durchführt. Der Benchmark ist unabhängig von der
Projektions-Ausgabe: Die Projektionsstufen messen offscreen mit einer Textur in Zielauflösung, ohne Display-Fixture, Vollbild
oder Vsync.

## 2. Ablauf je Stufe

- Aufwärmen etwa 2 s, danach Messung etwa 10 s.
- Median und 95. Perzentil der Frame-Zeit.
- GPU-Zeit und CPU-Zeit getrennt erfassen.
- Die ersten Frames verwerfen, weil Shader beim Start kompiliert werden.

## 3. Testmatrix

- Projektion: 480×270, 960×540, 1920×1080.
- LED: 1.000, 10.000, 100.000, 1 Mio. Punkte.
- Szenen: Plasma, Partikel, Labyrinth, Shader mit mehreren Schichten.

Reihenfolge vom kleinsten zum größten Aufwand, damit die Grenze früh erkannt wird.

## 4. Abbruchkriterien (nach jedem Durchlauf)

- Median deutlich über dem Budget (etwa das Doppelte von 33 ms): größere Stufen werden als „über Grenze“ markiert und
  übersprungen.
- 95. Perzentil stark ausreißend: System am Limit.
- GPU-Fehler, verlorenes Gerät oder Speichermangel: sofortiger Abbruch, mit Angabe der auslösenden Stufe.
- Einzelner Frame über etwa 500 ms: Abbruch, weil Windows sonst die GPU zurücksetzt (TDR, rund 2 s).
- Knopf zum jederzeitigen Abbrechen, der die Ausgabe wiederherstellt.

Vorab-Prüfung: Läuft gerade eine Ausgabe, wird der Start verweigert oder gewarnt.

## 5. Ergebnis

- Tabelle mit Gerätename, GPU-Treiber, Programmversion und Commit-Nummer.
- Ampel pro Stufe, je nach Restbudget bei 30 oder 25 fps.
- Empfehlung, welche Kombination auf dieser Hardware stabil läuft.
- Export als CSV oder Markdown, zum Einfügen in Dokumentation oder Issues.
- Zusätzlich GPU-Temperatur und Takt während des Laufs mitschreiben, damit thermisch bedingte Einbrüche erkennbar sind.

## 6. Modi, Referenzvergleich, Community

- **Schnelltest:** drei Stufen, unter einer Minute, für den Alltag. **Voller Lauf:** für Vergleiche.
- **Referenzvergleich:** gespeicherte Messung laden und die Differenz pro Stufe anzeigen (nach Treiber-Updates oder
  Code-Änderungen).
- **Community:** Nutzer mit anderen Karten reichen ihre Tabelle als Issue ein; so entsteht eine Liste, welche Hardware welche
  Auflösung schafft.

## 7. Tests

- Funktionstests in der CI mit Catch2.
- Rauchtest für den Benchmark in der CI, ohne Zeit-Schwellwerte (Runner sind geteilt und haben keine GPU).
- Echte Leistungsmessungen manuell auf der Zielhardware.

## 8. Umsetzungsschritte (nach Freigabe)

1. Benchmark-Knopf mit Abbruchkriterien, Ergebnistabelle und Export.
2. Schnelltest und voller Lauf.
3. Referenzvergleich und Ampel.

## 9. Prüfung gegen den Code (24.11.)

Was schon da ist und was fehlt, nach Durchsicht von `wh_engine`, `wh_executor`, `wh_gpu*`, `wh_runtime` und `tests/bench`:

1. **Projektionsstufen sind machbar, ohne die Projektions-Ausgabe zu bauen.** Die `CanvasRenderStage`
   (`libs/wh_executor`, rastert eine Field-Kette in eine Textur beliebiger Größe) gibt es schon; kein Studio-Code benutzt sie
   bisher. Genau sie würde eine spätere Projektions-Ausgabe verwenden, die Messung bliebe also auch dafür gültig. Die
   Projektions-Ausgabe selbst bleibt zurückgestellt (CLAUDE.md, Backlog).
2. **LED-Stufen laufen über die `SamplingRenderStage`** (ein Abtastpunkt je LED). Die Obergrenze liegt bei
   `kMaxLedsPerLayout` = 2^23 (rund 8,4 Mio.), 1 Mio. LEDs ist also zulässig. Beide Achsen sind getrennte Stages mit eigenem
   Aufwand; „Projektion × LED“ ist deshalb kein Kreuzprodukt, sondern zwei Messreihen (siehe Frage A).
3. **GPU-Zeit fehlt bisher.** `EngineStats` zählt nur Frames (`framesRendered`, `framesDropped`); `IGpuDevice` hat weder
   Zeitstempel-Abfragen noch Adapter-/Treiberinformationen (nur `deviceName` intern in `wh_gpu_webgpu`). Nötig sind:
   - Zeitstempel-Abfragen in der GPU-Schicht (wgpu-native: Feature „Timestamp Query“; nicht jeder Adapter hat es, der
     Software-Adapter und manche Treiber nicht). Fehlt es, zeigt der Benchmark „GPU-Zeit n. v.“ und misst nur die
     Gesamtzeit je Frame (Einreichen bis Readback fertig).
   - Zeitmessung der CPU-Teile je Frame (Werte-Kernels, Aufzeichnen, Einreichen); `wh_bench` misst heute nur Vorschau und
     Layout-Bake auf der CPU (`tests/bench/bench.cpp`).
   - Adaptername, Backend und Treiberversion aus `wgpu`-Adapterinfo durchreichen.
4. **GPU-Temperatur und Takt** gibt es über wgpu nicht. Realistisch nur herstellerspezifisch: NVIDIA über NVML (zur Laufzeit
   geladen, kein Linkzwang), AMD unter Linux über sysfs/hwmon, unter Windows ADL, Intel kaum. Vorschlag: zuerst NVIDIA-only,
   sonst Spalte leer, ausdrücklich als optional markiert.
5. **TDR-Abbruch (500 ms)** kann nur *nach* einem Frame greifen, eine laufende Übergabe lässt sich nicht unterbrechen. Darum
   ist die Reihenfolge „klein nach groß“ entscheidend, dazu eine Vorab-Schätzung: nach der ersten, kleinsten Stufe einer
   Reihe hochrechnen und Stufen überspringen, deren erwartete Zeit klar über dem Limit liegt. Verlorenes Gerät kommt über den
   Device-Lost-Rückruf von wgpu.
6. **Eigene Session statt Player.** Der Benchmark braucht einen Treiber ohne Takt (so schnell wie möglich, ohne fps-Begrenzer)
   und ohne Netzwerk-Ausgabe, also eine eigene `Session` ohne Ziele. Weil sie dieselbe GPU belegt, muss die Vorab-Prüfung die
   laufende Ausgabe zwingend ablehnen (Warnung reicht nicht, die Messung wäre verfälscht).
7. **Szenen** gibt es als Nodes: `wh.generator.plasma`, `wh.generator.particles`, `wh.generator.maze`. „Shader mit mehreren
   Schichten“ ist noch zu definieren (fester Graph aus mehreren Generatoren und Blend-Nodes). Die Szenen sollen im Programm
   fest eingebaut sein (nicht als Beispieldateien), damit Messungen vergleichbar bleiben; alle Nodes sind zeitlich
   deterministisch (Zufall aus Hashfunktion, siehe `builtin_values.cpp`).
8. **Laufzeit:** Voller Lauf = 48 Stufen (3 × 4 × 4) × ca. 12 s ≈ 10 Minuten ohne Abbrüche; Schnelltest = 3 Stufen × 12 s ≈ 36 s
   (passt zu „unter einer Minute“).
9. **Testbarkeit lokal:** Qt lässt sich hier nicht bauen. Darum den Kern (Matrix, Stufen, Statistik, Abbruchkriterien,
   Berichtsformat, CSV/Markdown, Referenzvergleich) Qt-frei in einer Bibliothek (`wh_runtime/benchmark`) bauen, mit Catch2
   auf dem Software-Adapter prüfen und zusätzlich über `whitehole_cli --benchmark` ansprechbar machen. Der Studio-Knopf ist
   dann nur Oberfläche (Fortschritt, Abbrechen, Ergebnistabelle). Das nützt auch der Community-Idee: Ergebnisse lassen sich
   ohne Studio erzeugen und als Issue einreichen.

## 10. Offene Fragen an den Nutzer

- **A:** Soll die Matrix zwei getrennte Reihen sein (Projektion: 3 Auflösungen × 4 Szenen; LEDs: 4 Größen × 4 Szenen) statt
  eines Kreuzprodukts? Das senkt die Laufzeit des vollen Laufs auf rund 5 Minuten.
- **B:** Reicht für die Temperatur zunächst NVIDIA (NVML), und ist „leer, wenn nicht verfügbar“ in Ordnung?
- **C:** Darf der Kern zuerst als `whitehole_cli --benchmark` entstehen (lokal prüfbar), der Studio-Knopf danach?
- **D:** Welche Modelle: Schritt 1 (Zeitstempel, Abbruchlogik, Engine-Eingriff) mit Opus, Schritt 2 und 3 (Modi, Vergleich,
  Export, Ampel) mit Sonnet.
