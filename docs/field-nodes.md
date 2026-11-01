# Field-Nodes und der Feld-Compiler

Dieses Dokument beschreibt, wie ein Field-Node aufgebaut ist und was der Compiler
(`wh_compiler`) daraus macht. Es richtet sich an alle, die einen Node schreiben,
eingebaut oder später als Plugin.

## Das Modell

Ein **Field** ist eine Funktion von Koordinate und Zeit auf eine Farbe:

```wgsl
fn wh_field(p: vec3<f32>) -> vec4<f32>
```

- `p` liegt in [0,1]³. Bei einem 2D-Canvas ist `p.z = 0`.
- Die Farbe ist linear RGBA mit „geradem“ Alpha (RGB und Alpha getrennt). Gamma und Farbreihenfolge
  setzt erst der Ausgabe-Pass, der die Farbe zuletzt mit Alpha multipliziert (transparent = LED aus).
  Wer Farben überblendet oder addiert, nimmt `wh_mix_color` und `wh_add_color` (Prelude
  `kColorPrelude` in `builtin_internal.hpp`); sie rechnen vormultipliziert, bei deckenden Farben
  kommt dasselbe heraus wie bei `mix` und `clamp(a + b)`.
- Die Zeit steht in `wh_frame.time` (Sekunden), nie in der Systemuhr.

Der Compiler baut aus allen Nodes, die ein Ziel-Ausgang rückwärts erreicht, **eine**
Funktion `wh_field`. Es gibt keine Zwischentexturen und keine feste Auflösung. Dieselbe
Funktion bedient Matrix, Strip, Cube und Pixelwolke, weil der Konsument sie an
beliebigen Koordinaten abtastet.

## Mitgelieferte Field-Nodes

| Typ | Wirkung | Raum |
|---|---|---|
| `wh.generator.gradient` | Verlauf entlang einer Richtung in der xy-Ebene | 2D |
| `wh.generator.noise` | Value-Noise, die Zeit läuft über die z-Achse | 3D |
| `wh.generator.plasma` | klassisches Plasma | 2D |
| `wh.generator.sphere` | Kugel mit weichem Rand, Mittelpunkt und Radius einstellbar | 3D |
| `wh.generator.wave` | ebene Welle entlang einer 3D-Richtung | 3D |
| `wh.generator.rain` | „Digital Rain“: fallende Pixel mit Spur (Spalten, Tempo, Spurlänge, Dichte, Kopf-/Spurfarbe); die Spur ist durchsichtig und lässt sich über andere Effekte legen. Im Cube fallen Säulen in x/z | 3D |
| `wh.generator.solid` | Fläche in einer Farbe (zum Einfärben mit Mask/Blend oder als Blitzfläche) | beliebig |
| `wh.generator.scan` | Scan-Linie / Knight Rider: ein Streifen quer zur Richtung `angle` an der Stelle `position` (0..1), Breite und Weichheit einstellbar; die Position mit einem Dreieck-LFO steuern lässt ihn hin- und herlaufen. Der Rest ist durchsichtig | 2D |
| `wh.generator.ripples` | Wellenringe wie Regentropfen im Wasser (Tropfen pro Sekunde, Ausbreitung, Ringbreite, Farbe), ohne Zustand, durchsichtig außerhalb der Ringe | 2D |
| `wh.generator.fire` | Feuer: nach oben wanderndes Rauschen mit Hitzeverlauf von unten nach oben, Palette Schwarz–Rot–Gelb–Weiß | 2D |
| `wh.generator.metaballs` | Metaballs: 1–6 weiche Kugeln wandern auf eigenen Bahnen und fließen ineinander, wenn sie sich nähern (Anzahl, Größe, Tempo, Randweichheit, Hintergrund- und Blobfarbe) | 2D |
| `wh.generator.starfield` | Sternenhimmel: `cells` Zellen je Achse, in jeder mit Wahrscheinlichkeit `density` ein flimmernder Stern (Größe, Flimmertempo, `drift` nach unten, zwei Farben nach Zufall); der Himmel ist durchsichtig und lässt sich über andere Effekte legen | 2D |
| `wh.generator.fading` | Fading Pixels: einzelne Zellen leuchten auf und verlöschen, jede in eigenem Takt (Zellen, Rate, Dichte, `sharpness` = Abklingkurve, zwei Farben nach Zufall); der Rest ist durchsichtig | 2D |
| `wh.generator.shapes` | Expanding Shapes: Kreise, Quadrate oder Rauten (Auswahlfeld), die von `cx`/`cy` nach außen laufen (Abstand, Tempo — negativ nach innen —, Dicke, Verblassen nach außen); durchsichtig zwischen den Ringen | 2D |
| `wh.generator.sinelines` | Sine Lines: 1–8 Sinuswellen quer über die Leinwand, jede um `spread` Perioden gegen die vorige verschoben, Farbe von der ersten zur letzten Linie; überlappende Linien addieren sich | 2D |
| `wh.generator.lightning` | Lightning: bis zu drei Blitze (`bolts`) hängen oben an der Leinwand und laufen nach unten. `rate` Schläge pro Sekunde, `duration` Sekunden sichtbar (höchstens ein ganzer Takt), `decay` Abklingen, `width`, `jag` Zackigkeit, `glow` Leuchthof, `branches` Helligkeit eines Seitenastes (0 = keiner), `reach` wie tief der Blitz reicht, `flash` hellt die ganze Fläche mit auf; Farbe `color`. Die Stelle jedes Schlags wird aus der Taktnummer gewürfelt, ein Nachschlag bei etwa 30 % der Sichtbarkeit. Der Rest ist durchsichtig, siehe `examples/gewitter.whfx` | 2D |
| `wh.generator.particles` | Particles: `count` (bis 48) Teilchen strömen aus (`ex`, `ey`), gestaffelt über `lifetime` Sekunden. `direction` in Grad (0 = rechts, 90 = oben), `spread` der Fächer in Umläufen (1 = rundum), `speed`, `gravity` (positiv = nach unten), `size`, `trail` Schweiflänge in Sekunden, `shrink` Schrumpfen über das Leben, Palette *Two colors* (zufällig zwischen `colorA` und `colorB`) oder *Rainbow* (Konfetti). Jedes Teilchen hat in jedem Durchlauf eine neue Richtung und ein neues Tempo (Hash), die Bahn ist eine Wurfparabel in geschlossener Form: kein Zustand, gleiches Bild bei jedem Abspielen. Durchsichtig außerhalb der Teilchen, siehe `examples/funken.whfx` | 2D |
| `wh.generator.spectrum` | Spectrum Bars: Equalizer aus `bars` Balken, die von unten (oder von der Mitte aus) so hoch wachsen wie das Audio-Spektrum am Eingang `spectrum` (Porttyp AudioSpectrum, von „Audio Spectrum“); `gap`, `softness`, Farbverlauf `color` → `colorTop`. Ohne Verbindung durchsichtig, siehe [Audio](audio.md) | 2D |
| `wh.generator.text` | Text / Laufschrift: der Text (UTF-8, `text`) in einer 5×7-Punktschrift mit ASCII und ä ö ü Ä Ö Ü ß; alles andere wird zum Fragezeichen. `mode` *Scroll* (Tempo `speed` in Schriftpunkten pro Sekunde, negativ nach rechts, `gap` Pause vor dem nächsten Durchlauf) oder *Static* mit `align` links, mittig, rechts. `height` ist die Höhe der acht Schriftzeilen als Anteil der Leinwand, `y` die Mitte, `spacing` der Abstand zwischen Buchstaben in Punkten, `aspect` das Seitenverhältnis der Leinwand (Breite / Höhe, 32×16 = 2), damit die Punkte quadratisch sind. `color`, `background` (standardmäßig durchsichtig). Siehe unten | 2D |
| `wh.generator.scope` | Oscilloscope: die 32 Punkte der Wellenform am Eingang `wave` (Porttyp AudioWave, von „Audio Waveform“) als Linie über die Breite, `amplitude` halbe Höhe, `thickness`, `glow`, `fill` für die Fläche zur Mittellinie, `color`. Ohne Verbindung eine flache Linie in der Mitte, siehe [Audio](audio.md) | 2D |
| `wh.transform.rotate` | Drehung in der xy-Ebene um die Mitte | 2D |
| `wh.transform.rotate3d` | Drehung um alle drei Achsen um die Mitte (0,5; 0,5; 0,5) | 3D |
| `wh.transform.symmetry` | Symmetrie: eine Hälfte des Eingangs wird an einer Linie gespiegelt. `axis` (X, Y, Z) und `half` (untere oder obere Hälfte als Vorlage) sind Auswahlfelder, `center` setzt die Linie. Einfacher zu bedienen als Mirror, der mehrere Achsen gleichzeitig kann | 2D, 3D |
| `wh.transform.region` | Bildet den Eingang auf ein Rechteck der Leinwand ab (`left`, `top` von oben, `width`, `height`), außerhalb durchsichtig; der Effekt füllt so nur einen Teil der Wand | 2D |
| `wh.transform.mirror`, `wh.transform.scale` | Spiegeln an einer Linie in x, y und/oder z (Lage `centerX/Y/Z`, mit `otherHalf` ist die obere Hälfte die Vorlage), Zoomen in x und y | 2D (Spiegeln auch 3D) |
| `wh.transform.kaleidoscope` | Kaleidoskop: die Ebene wird um (`cx`, `cy`) in `segments` Keile geteilt, jeder zweite ist das Spiegelbild des Nachbarn. `angle` dreht den Ausschnitt, `zoom` vergrößert. `aspect` (Breite / Höhe der Leinwand, 32×16 = 2) hält die Winkel gleich, siehe unten | 2D |
| `wh.transform.tunnel` | Tunnel: Winkel und Tiefe (1 / Abstand) werden zu den Koordinaten des Eingangs. `repeat` Wiederholungen um den Tunnel, `speed` Kacheln pro Sekunde in die Tiefe (negativ: heraus), `depth` Verkleinerung zur Mitte, `twist` dreht mit der Tiefe, `fade` blendet die Mitte aus (Alpha) | 2D |
| `wh.transform.vortex` | Wirbel: dreht die Koordinate um (`cx`, `cy`) umso stärker, je näher sie der Mitte liegt: `turns` Umdrehungen im Mittelpunkt, keine bei `radius` und außerhalb | 2D |
| `wh.transform.slide` | Verschiebt das Bild um `x`, `y` (Anteile der Leinwand), mit `wrap` kommt es auf der anderen Seite wieder herein, sonst bleibt der Rest durchsichtig. Mit Time oder LFO an `x` entsteht ein Scroll oder ein Übergang („Move left/right“) | 2D |
| `wh.transform.tile` | Kachelt das Bild `countX` × `countY` mal, optional jede zweite Kachel gespiegelt | 2D |
| `wh.transform.pixelate` | Mosaik: `cellsX` × `cellsY` Zellen, jede zeigt den Wert an ihrer Mitte | 2D |
| `wh.blend.mix`, `wh.blend.add` | Überblenden, Addieren | beliebig |
| `wh.blend.blend` | B über A mit Modus (Auswahlfeld): Add, Multiply, Screen, Subtract, Difference, Lighten, Darken, Over; `amount` überblendet zwischen A und dem Ergebnis | beliebig |
| `wh.blend.crossfade` | Überblenden mit `amount` (Position): *Linear* blendet A aus, während B einblendet; *Progressive* lässt beide in der Mitte voll leuchten | beliebig |
| `wh.blend.mask` | Die Helligkeit der Maske bestimmt, wie viel vom Inhalt zu sehen ist: *Shape* als harte Form (Schwelle, Weichheit), *Intensity* behält Verläufe und weiche Ränder; `invert` kehrt um. So wird eine Form mit dem Inhalt eines anderen Effekts gefüllt | beliebig |
| `wh.blend.key` | Schwarz wird durchsichtig (Alpha folgt der Helligkeit, Schwelle und Weichheit einstellbar); danach mit Blend (Over) oder Add über etwas legen | beliebig |
| `wh.modifier.coloradjust` | Farbton (in Umläufen), Sättigung, Helligkeit, Gamma, Invertieren | beliebig |
| `wh.modifier.colormap` | Color Map: ersetzt die Helligkeit (Luma) des Eingangs durch einen Verlauf aus `count` (2–4) Farben `color1`–`color4`, wie Wärmebild-Paletten. `shift` verschiebt die Helligkeit (mit Time oder LFO rotiert die Palette), `wrap` macht den Verlauf zum Ring; die Durchsichtigkeit bleibt die des Eingangs | beliebig |
| `wh.modifier.strobe` | Blitzen: `rate` Hz, `duty` Anteil der Periode an, `fade` Ausklingen; in der Dunkelphase gehen die LEDs aus | beliebig |
| `wh.modifier.outline` | Zeichnet in `width` Abstand um helle Stellen einen Rand in der Randfarbe (z. B. Text mit Umriss über Plasma) | 2D |
| `wh.output.canvas` | **Ausgang:** bespielt die ganze Leinwand oder nur ein Rechteck (`left`, `top` von oben, `width`, `height`); *Scale into area* schiebt das Bild ins Rechteck, *Window* lässt es an seinem Platz und schneidet nur aus; `brightness` ist die Gesamthelligkeit (Master). Außerhalb zeigt der Ausgang den optionalen Eingang `behind` (ohne Verbindung sind die LEDs dort aus). **Mehrere Ausgänge mit getrennten Rechtecken:** jeder bekommt seinen eigenen Effekt an `source`, und `behind` führt den davor an; so legen sie sich nebeneinander, statt sich zu löschen. Ein neu angelegter Ausgangs-Node wird automatisch zum Ausgang des Projekts und hängt sich über den bisherigen (dessen Ausgang kommt an `behind` und zunächst auch an `source`, damit der Graph gültig bleibt; dort schließt man den eigenen Effekt an) | beliebig |

### Rezepte

- **Form mit anderem Effekt füllen:** Form-Generator (z. B. Ripples, Scan Line) in `mask` an *Mask*, Plasma
  an *Content*.
- **Text oder Form mit Rand über Plasma:** Form → Outline → Blend (Mode *Over*, B) über Plasma (A).
- **Zwei Effekte im Wechsel:** Time/LFO (Sägezahn oder Dreieck) an `amount` von Crossfade.
- **Bewegungsübergang:** Slide an A (`x` = −t), Slide an B (`x` = 1 − t), beide in Add; t von einem LFO.
- **Jeder Effekt auf seinem Bereich:** jeweils ein Ausgangs-/Region-Node pro Effekt (Rechteck in der Vorschau
  ziehen), dann mit Add oder Blend zusammensetzen, am Ende ein Ausgang.
- **Sternenhimmel über Farbverlauf:** Starfield (durchsichtig) mit Blend (Mode *Over*, B) über einen dunklen Linear
  Gradient; `drift` lässt ihn fliegen.
- **Lava-Lampe:** Metaballs, danach Color Adjust (Farbton mit einem langsamen LFO) für wechselnde Farben.
- **Ringe mit Inhalt:** Expanding Shapes als `mask` einer Mask, Plasma als Content.
- **Master:** Ausgang `brightness` für die Gesamthelligkeit, Color Adjust `gamma`, Strobe für den Master-Blitz.

2D-Nodes lassen `p.z` unverändert, sie laufen also auch in einem Cube, wirken dort aber
in jeder z-Ebene gleich. Die z-Variante kommt, wo sie sich lohnt (Rotate 3D ist da).

## Ein Node besteht aus zwei Teilen

1. **`NodeDescriptor`**: Typ-ID, Ports, Parameter, Domäne. Das sieht auch die GUI.
2. **`NodeSnippet`**: der WGSL-Code. Er liegt getrennt, in einer `SnippetLibrary`,
   unter derselben Typ-ID. Beides zusammen heißt `NodeDefinition`;
   `registerDefinitions` trägt beide ein.

### Regeln für den Descriptor

- Typ-ID nach dem Schema `wh.<kategorie>.<name>`; Plugins nehmen ihren eigenen Präfix.
- Port- und Parameterschlüssel sind gültige Bezeichner (`[A-Za-z_][A-Za-z0-9_]*`),
  weil sie im Shader als Namen auftauchen. Die Registry lehnt andere ab.
- Ein Field-Node hat **genau einen** Field-Ausgang (Schlüssel frei wählbar, üblich `out`).
- Domäne `Field`. Raster- und CPU-Nodes kompiliert dieser Compiler nicht (siehe unten).
- Zahlen mit `minValue`/`maxValue` werden beim Setzen begrenzt, NaN und Unendlich abgelehnt.
- Ein Eingang mit dem Schlüssel eines Parameters moduliert diesen Parameter. Der Portyp muss
  zum Parametertyp passen, die Registry prüft das. `graph::addModulationPorts` ergänzt solche
  Eingänge. Siehe `value-graph.md`.

### Regeln für das Snippet

`body` ist der Rumpf von `fn wh_f<ID>(p: vec3<f32>) -> vec4<f32>` und endet mit `return`.
Es gibt sieben Platzhalter:

| Platzhalter | Wird ersetzt durch |
|---|---|
| `@in(port, koordinate)` | Aufruf der Funktion, die am Field-Eingang `port` hängt, an der Koordinate |
| `@param(schluessel)` | Zugriff auf den Parameter im Uniform-Puffer |
| `@spectrum(port, band)` | ein Wert (0–31, `u32`-Ausdruck) am Eingang `port` vom Typ AudioSpectrum (Band, 0..1) oder AudioWave (Punkt der Wellenform, −1..1), als `f32` |
| `@text(schluessel)` | ein Textparameter als `array<u32, N>(...)` aus Unicode-Codepunkten (höchstens 256, N mindestens 1) |
| `@textlen(schluessel)` | die Zeichenzahl des Textparameters als `u32`-Literal (`5u`, bei leerem Text `0u`) |
| `@time` | `wh_frame.time` |

- Platzhalter dürfen verschachtelt werden: `@in(a, p + vec3<f32>(@param(shift), 0.0, 0.0))`.
- Ein **nicht verbundener optionaler** Eingang liefert `vec4<f32>(0.0, 0.0, 0.0, 0.0)`.
  Ein nicht verbundener **Pflicht**-Eingang ist ein Kompilierfehler.
- Hängt an einem Field-Eingang ein `Scalar` oder eine `Color`, liefert `@in(...)` dessen
  Wert als konstantes Feld, an jeder Koordinate. Die Koordinate im Platzhalter wird dann
  ignoriert.
- Der Name `p` ist belegt. Lokale Variablen heißen anders.
- `@param` funktioniert nur für Parameter mit GPU-Entsprechung (Bool, Zahl, Vec2, Vec3,
  Farbe). Texte gehen über `@text` und `@textlen`: Der Compiler schreibt sie als Zahlenfeld in den Shader,
  sie belegen also keinen Slot im Parameterpuffer, und ein neuer Text ändert die Struktur (die Pipeline
  wird neu gebaut, anders als bei Zahlen und Farben).

**Koordinaten-Nodes** (Rotate, Mirror, Scale, ...) ändern `p` und rufen ihren Eingang an
der neuen Koordinate auf. Das ist der ganze Trick: Sie wirken auf alles dahinter.

`prelude` enthält optionale Hilfsfunktionen (zum Beispiel eine Hashfunktion). Gleiche
Preludes mehrerer Nodes erscheinen einmal im Shader. Sie werden nicht expandiert, dort
sind auch WGSL-Attribute mit `@` erlaubt. Funktionsnamen im Prelude tragen den Präfix
des Nodes (`wh_`, ...), damit sie nicht kollidieren.

## Was der Compiler liefert

`compileField(graph, snippets, ziel)` gibt ein `CompiledField` zurück:

- `wgsl`: Bibliothek mit Bindings, Structs, Preludes, einer Funktion je Node und `wh_field`.
- `layout`: Lage jedes Parameters im Uniform-Puffer. `packParameters(graph, layout)`
  füllt die Bytes aus den aktuellen Werten.
- `structureHash`: FNV-1a des WGSL-Texts.
- `nodes`: beteiligte Nodes, Eingänge zuerst.

### Bindings

Die Feldbibliothek belegt **Bind-Group 0**:

| Binding | Inhalt |
|---|---|
| 0 | `FrameUniforms` (`time`, `delta`, `frame`, 16 Bytes) |
| 1 | Parameterpuffer gemäß `layout` (`sizeBytes`, Vielfaches von 16) |

Konsumenten (Mapping-Pass, Canvas-Pass) verwenden Bind-Group 1 und höher, hängen ihren
Compute-Einstieg an die Bibliothek und rufen `wh_field(p)` auf. Der Canvas-Pass
(`wh_executor`, siehe `canvas-stage.md`) legt seine Zieltextur auf `@group(1) @binding(0)`.

### Beispiel

`docs/examples/plasma_rotate_mirror_noise_mix.wgsl` ist die echte Compiler-Ausgabe für
`Plasma -> Rotate -> Mirror`, gemischt mit `Noise`. Sie enthält zusätzlich einen
angehängten Compute-Einstieg, wie ihn später der Mapping-Pass erzeugt (Bind-Group 1:
Positionen rein, Farben raus). Man sieht daran: ein Prelude, eine Funktion je Node in
Abhängigkeitsreihenfolge, `wh_field` am Ende.

### Parameter ändern, ohne neu zu kompilieren

Parameterwerte stehen nicht im Shadertext, sondern im Uniform-Puffer. Eine Änderung
bedeutet nur: Puffer neu packen und hochladen. Ändert sich der `structureHash` nicht,
bleibt auch die Pipeline gültig. Nur Struktur-Änderungen (Node einfügen, Verbindung ändern)
erzeugen neuen Text und damit eine neue Pipeline.

## Polarkoordinaten und das Seitenverhältnis

Kaleidoskop, Tunnel und Wirbel rechnen um einen Mittelpunkt in Winkel und Abstand. Der Shader kennt die
Leinwandgröße nicht: Auf einer 32×16-Wand wären die Koordinaten 0..1 in x und y gleich lang, die Pixel aber
nicht quadratisch, und Kreise würden zu Ellipsen. `aspect` (Breite / Höhe der Leinwand, Standard 1) streckt x
vor der Umrechnung und staucht zurück, bevor der Eingang abgetastet wird. Für eine Wand mit 32×16 LEDs stellt man 2 ein.

## Text und Laufschrift

Der Text-Node zeichnet Buchstaben im Shader aus einer eingebauten Punktschrift (`builtin_text_font.hpp`,
je Zeichen 5 Spalten à 8 Bit, zwei `u32`), ohne Textur und ohne Schriftdatei. Eine Zelle ist 8 Punkte hoch
(7 Zeilen plus eine freie für Umlautpunkte und Unterlängen) und `5 + spacing` Punkte breit. Passt `height` zur
Wand (bei 16 Zeilen: 0,5), ist ein Schriftpunkt genau eine LED und die Schrift scharf; die Lage wird auf ganze
Schriftpunkte gerundet, sodass die Laufschrift nicht flimmert.

Der Shader kennt die Größe der Leinwand nicht; `aspect` (Breite / Höhe der Leinwand) macht die Punkte
quadratisch. Ein Durchlauf der Laufschrift ist Textbreite + Leinwandbreite + `gap` lang: Der Text läuft
vollständig hinaus, bevor er von der anderen Seite wieder hereinkommt. Beispiel: `examples/laufschrift.whfx`.
Im Projekt steht der Text in Anführungszeichen (`set text "Hallo Köln"`).

Grenzen: höchstens 256 Zeichen; keine Proportionalschrift, keine Schriftarten, keine Zeilenumbrüche; Zeichen
außerhalb von ASCII und den sieben deutschen Sonderzeichen erscheinen als `?`.

## Grenzen (Stand Schritt 4b, Teil A)

Der Compiler meldet diese Fälle ausdrücklich als `Unsupported`, statt sie zu ignorieren:

- Nodes der Raster-Domäne (Blur, Feedback, Video, Script). CPU-Nodes übersetzt er
  nicht; ihre Werte liefert `wh_values` (siehe `value-graph.md`).
- Verbundene Nicht-Field-Eingänge, die keinen gleichnamigen Parameter haben. Ein Skalar
  braucht ein Ziel, das er modulieren kann.
- Bilder als Field-Eingang.
- Nodes mit mehr als einem Field-Ausgang.

Ein Node, den zwei Verbraucher nutzen, wird zweimal ausgewertet. Teure Nodes gehören
deshalb in die Raster-Domäne, wo ihr Ergebnis zwischengespeichert werden kann.

Die erzeugten Shader sind bisher nur syntaktisch geprüft, nicht von einem WGSL-Compiler
(Tint, naga) übersetzt. Das passiert mit dem GPU-Backend.
