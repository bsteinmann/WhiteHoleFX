# Audio-Eingang

WhiteHoleFX hört Audio und macht daraus Werte für Effekte: Lautstärke, Frequenzbänder, Schläge (Beats)
mit Tempo und ein Spektrum, das als Equalizer auf der Wand erscheint. Alles läuft lokal; das Audio
verlässt den Rechner nicht.

## Einschalten

Der Audio-Eingang ist nach der Installation **aus**. Das Mikrofon wird erst auf ausdrückliche Wahl
geöffnet (das Betriebssystem fragt dann ggf. nach der Erlaubnis).

- **Studio:** Einstellungen (Bearbeiten → Einstellungen), Seite *Audio*: *Aus*, *Standardgerät*, ein Gerät aus der
  Liste oder *WAV-Datei in Schleife*. Die Statuszeile zeigt „Audio: Quelle, Pegel, BPM“. Die Wahl wird gemerkt;
  scheitert das Öffnen beim nächsten Start, bleibt der Eingang aus. Enthält das Projekt Audio-Nodes und der
  Eingang ist aus, weist die Statuszeile darauf hin.
- **Kommandozeile:** `whitehole_cli projekt.whfx --audio-in default` (Standardgerät),
  `--audio-in 2` oder `--audio-in "USB"` (Nummer oder Namensteil aus `whitehole_cli --audio-devices`),
  `--audio-in musik.wav` (Datei in Schleife, in Echtzeit abgespielt).

Unter Linux erscheinen die „Monitor“-Quellen von PulseAudio und PipeWire (das, was der Rechner gerade
abspielt) als gewöhnliche Eingänge. Unter Windows geht das vorerst nur über Geräte wie „Stereomix“,
falls der Treiber eines anbietet.

## Was berechnet wird

Der Audio-Thread schickt die Samples (zu Mono gemischt) durch eine Analyse und legt nach jedem Schritt
(512 Samples, rund 90 Mal pro Sekunde) das Ergebnis in einen gemeinsamen Speicher; die Nodes lesen es
einmal pro Bild.

| Größe | Beschreibung |
|---|---|
| Pegel | RMS und Spitze, linear 0..1 (Vollaussteuerung = 1) |
| Spektrum | 32 Bänder, logarithmisch von 40 Hz bis 16 kHz (FFT mit 2048 Punkten, Hann-Fenster); Wert je Band in dBFS von −60 dB (= 0) bis 0 dB (= 1) |
| Beat | Spektralfluss der Bänder bis etwa 1,5 kHz, adaptive Schwelle, Mindestabstand 0,3 s (höchstens 200 BPM) |
| Tempo | Median der letzten Schlagabstände, in den Bereich 70–160 BPM gefaltet; erst ab fünf Schlägen |

Die Beat-Erkennung ist eine Heuristik für Musik mit erkennbarem Bass. Bei dichtem Material ohne Kick, bei
Tempowechseln oder sehr leiser Musik irrt sie sich. Dafür gibt es *Tap Tempo* und das feste Tempo im
Beat-Node.

## Nodes (Kategorie „Audio“)

| Typ | Ausgänge | Wichtige Parameter |
|---|---|---|
| `wh.value.audiolevel` | `level`, `peak` (Scalar), `present` (Boolean) | `scale` (0 Linear, 1 dB), `gain`, `attack`, `release`, `auto` (Auto-Gain) |
| `wh.value.audioband` | `value` (Scalar), `present` | `from`, `to` (Hz), `gain`, `floor` (Rauschgrenze), `attack`, `release` |
| `wh.value.audiospectrum` | `spectrum` (AudioSpectrum), `present` | `gain`, `floor`, `attack`, `release` |
| `wh.value.audiowave` | `wave` (AudioWave), `present` | `span` (Fenster in ms, 2–100), `gain`, `trigger` (steigender Nulldurchgang), `level` (Triggerschwelle), `smooth` |
| `wh.value.beat` | `beat` (Trigger), `pulse`, `phase`, `bpm` (Scalar), `present` | `threshold` (Mindeststärke), `decay`, `manualBpm` (0 = erkennen) |
| `wh.value.tap` | `beat` (Trigger), `pulse`, `phase`, `bpm` | `tap` (jedes Einschalten zählt), `manualBpm`, `decay` |
| `wh.generator.scope` | Field | Eingang `wave`; `amplitude` (halbe Höhe), `thickness`, `glow`, `fill` (Fläche zur Mittellinie), `color` |
| `wh.generator.spectrum` | Field | Eingang `spectrum`; `bars` (4–32), `gap`, `mode` (von unten / von der Mitte), `softness`, `color`, `colorTop` |

- **Attack/Release** glätten den Wert: Anstieg und Abfall mit eigener Zeit in Sekunden (0 = sofort). Kurzes
  Attack und längeres Release lassen Lichter schnell aufblitzen und weich ausklingen.
- **`gain` und `floor`:** erst wird alles unterhalb von `floor` abgezogen, dann mit `gain` multipliziert und auf
  0..1 begrenzt. Gegen Rauschen helfen `floor` 0,2–0,4; für leise Quellen mehr `gain`.
- **Auto-Gain** (`auto` im Pegel-Node) skaliert auf das Maximum der letzten Sekunden; es sinkt langsam (10 s).
- **Band:** nimmt den größten Wert unter den Bändern, die `from`…`to` berühren. Richtwerte:
  Bass 40–250 Hz, Mitten 250–2000 Hz, Höhen 2000–16000 Hz.
- **Beat:** `beat` ist nur im Bild des Schlags wahr (Trigger). `pulse` springt dann auf 1 und klingt mit
  `decay` ab. `phase` läuft von 0 bis 1 pro Schlag mit dem erkannten (oder festen) Tempo; ein LFO-Ersatz,
  der im Takt bleibt. Mit `threshold` > 1 zählen nur stärkere Schläge.
- **Tap Tempo:** `tap` an einen Schalter hängen (zum Beispiel `on` eines DMX-Nodes) oder von Hand ein- und
  ausschalten; jedes Einschalten ist ein Tipp. Aus den letzten vier Abständen wird das Tempo, danach läuft
  der Takt weiter. Mit `manualBpm` > 0 bleibt das Tempo fest und ein Tipp richtet nur die Phase aus.
- **Kein Signal:** Ist der Eingang aus oder der letzte Frame älter als eine halbe Sekunde, wird `present`
  falsch, und alle Werte laufen über ihr Release gegen 0.

Jeder Parameter hat wie üblich einen Modulations-Eingang. Rezepte:

- **Equalizer:** `Audio Spectrum` → `Spectrum Bars` → Ausgang. Über Plasma legen: `Blend` (Mode *Over*) mit dem
  Plasma als A und den Balken als B.
- **Oszilloskop:** `Audio Waveform` → `Oscilloscope` → Ausgang. Beispiel: `examples/audio/oszilloskop.whfx`.
- **Blitz im Takt:** `Beat.pulse` → `Color Adjust.brightness` eines Effekts.
- **Bass lässt es wachsen:** `Audio Band` (40–150 Hz) → `Scale.factor`.
- **Farbwechsel im Takt:** `Beat.phase` → `Color Ramp.position` (mit `loop`).

## Spectrum Bars

Das Spektrum ist kein Einzelwert und läuft deshalb auf einem eigenen Kanal: Der Ausgang `spectrum` hat den
Porttyp *AudioSpectrum* und lässt sich nur mit Eingängen desselben Typs verbinden. Der Feldcompiler legt für
einen solchen Eingang 32 Zahlen (8 × `vec4<f32>`, 128 Bytes) in den Parameterpuffer; im Shader liest der
Platzhalter `@spectrum(port, band)` ein Band (siehe [Field-Nodes](field-nodes.md)). Ist nichts
verbunden, sieht der Node Stille und bleibt durchsichtig.

`Spectrum Bars` teilt die Breite in `bars` Spalten und nimmt je Spalte den größten Wert der zugehörigen Bänder
(bei 16 Balken zwei Bänder je Balken). Gezeichnet wird von unten (`p.y = 0`) oder, im Modus *From center*,
symmetrisch von der Mitte aus; die Farbe läuft von `color` unten nach `colorTop` oben. `softness` macht die
Oberkante weich, `gap` lässt Lücken zwischen den Balken.

## Oscilloscope

`Audio Waveform` zeigt die Kurve selbst statt einer Kennzahl. Der Analyse-Frame trägt dafür die letzten 8192
Samples (bei 48 kHz 170 ms); der Node schneidet ein Fenster von `span` Millisekunden heraus (höchstens die
Hälfte davon) und teilt es in 32 Stücke. Von jedem Stück bleibt der Sample mit dem größten Betrag samt
Vorzeichen, damit auch schnelle Töne ihre Höhe behalten (ein Mittelwert würde sie wegglätten).

Der **Trigger** wirkt wie am echten Oszilloskop: Das Fenster beginnt am letzten steigenden Durchgang durch
`level`, sonst zittert die Kurve von Bild zu Bild. Findet sich kein Durchgang (Rauschen, Gleichspannung), läuft
das Fenster frei. Mit `smooth` > 0 wird die Kurve zusätzlich zeitlich geglättet. Ohne Signal ist sie flach.

Die 32 Punkte laufen auf demselben Kanal zur GPU wie das Spektrum, mit eigenem Porttyp *AudioWave* (Werte
−1..1). Beide Typen lassen sich nicht vertauschen: Eine Wellenform an `Spectrum Bars` oder ein Spektrum an
`Oscilloscope` anzuschließen wird abgelehnt. `Oscilloscope` verbindet die Punkte mit geraden Strecken; die Linie
ist der Abstand zu den drei Strecken um die Spalte und hat deshalb auch bei steilen Flanken keine Lücken.
`amplitude` ist die halbe Höhe der Kurve (0,5 = ganze Leinwand), `thickness` die Breite der Linie, `glow` ein
weicher Hof, `fill` die Helligkeit der Fläche zwischen Mittellinie und Kurve (0 = nur Linie).

Mit 32 Punkten ist die Kurve grob: Auf einer Wand mit 32 bis 64 Spalten reicht das, bei sehr breiten Wänden
wird zwischen den Punkten gerade verbunden. Ein Ton mit mehr Perioden im Fenster als Punkten (zum Beispiel 1 kHz
bei 30 ms) wird unterabgetastet; dann `span` verkürzen.

## Grenzen

- **Nicht reproduzierbar:** Der Wert kommt vom Eingangssignal, nicht von der Zeit. Offline-Rendern mit
  Audio-Nodes ergibt je nach Quelle verschiedene Ergebnisse.
- **Latenz:** Gerätepuffer (meist 10–20 ms) plus ein Analysefenster von 43 ms. Für Lichtshows reicht das; ein
  Schlag erscheint rund 50–80 ms nach dem Ton, was sich über ein späteres Auslösen nicht ausgleichen lässt.
- **Mono, eine Quelle:** Mehrkanal wird gemittelt; es läuft immer nur eine Quelle.
- **Kein System-Loopback unter Windows** (WASAPI-Loopback folgt später); keine MP3- oder FLAC-Dateien, nur WAV.
- **Ohne Aufnahme-Unterstützung** (`-DWHITEHOLEFX_AUDIO_CAPTURE=OFF`) bleibt nur die WAV-Datei als Quelle.
