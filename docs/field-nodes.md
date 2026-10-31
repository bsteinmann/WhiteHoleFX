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
| `wh.transform.rotate` | Drehung in der xy-Ebene um die Mitte | 2D |
| `wh.transform.rotate3d` | Drehung um alle drei Achsen um die Mitte (0,5; 0,5; 0,5) | 3D |
| `wh.transform.mirror`, `wh.transform.scale` | Spiegeln, Zoomen in x und y | 2D |
| `wh.blend.mix`, `wh.blend.add` | Überblenden, Addieren | beliebig |

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
Es gibt drei Platzhalter:

| Platzhalter | Wird ersetzt durch |
|---|---|
| `@in(port, koordinate)` | Aufruf der Funktion, die am Field-Eingang `port` hängt, an der Koordinate |
| `@param(schluessel)` | Zugriff auf den Parameter im Uniform-Puffer |
| `@time` | `wh_frame.time` |

- Platzhalter dürfen verschachtelt werden: `@in(a, p + vec3<f32>(@param(shift), 0.0, 0.0))`.
- Ein **nicht verbundener optionaler** Eingang liefert `vec4<f32>(0.0, 0.0, 0.0, 0.0)`.
  Ein nicht verbundener **Pflicht**-Eingang ist ein Kompilierfehler.
- Hängt an einem Field-Eingang ein `Scalar` oder eine `Color`, liefert `@in(...)` dessen
  Wert als konstantes Feld, an jeder Koordinate. Die Koordinate im Platzhalter wird dann
  ignoriert.
- Der Name `p` ist belegt. Lokale Variablen heißen anders.
- `@param` funktioniert nur für Parameter mit GPU-Entsprechung (Bool, Zahl, Vec2, Vec3,
  Farbe). Texte sind nicht GPU-fähig.

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

## Grenzen (Stand Schritt 4b, Teil A)

Der Compiler meldet diese Fälle ausdrücklich als `Unsupported`, statt sie zu ignorieren:

- Nodes der Raster-Domäne (Blur, Feedback, Video, Text, Script). CPU-Nodes übersetzt er
  nicht; ihre Werte liefert `wh_values` (siehe `value-graph.md`).
- Verbundene Nicht-Field-Eingänge, die keinen gleichnamigen Parameter haben. Ein Skalar
  braucht ein Ziel, das er modulieren kann.
- Bilder als Field-Eingang.
- Nodes mit mehr als einem Field-Ausgang.

Ein Node, den zwei Verbraucher nutzen, wird zweimal ausgewertet. Teure Nodes gehören
deshalb in die Raster-Domäne, wo ihr Ergebnis zwischengespeichert werden kann.

Die erzeugten Shader sind bisher nur syntaktisch geprüft, nicht von einem WGSL-Compiler
(Tint, naga) übersetzt. Das passiert mit dem GPU-Backend.
