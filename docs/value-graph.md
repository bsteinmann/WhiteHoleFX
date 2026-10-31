# Werte-Graph: CPU-Nodes, Modulation und Konstanten

Dieses Dokument beschreibt, wie Zahlen und Farben, die einmal pro Frame auf der CPU
entstehen, Parameter der Field-Nodes steuern. Es gehört zu `field-nodes.md`
(GPU-Seite) und richtet sich an alle, die einen CPU-Node schreiben oder Modulation
in der GUI anbinden.

## Zwei Welten, eine Schnittstelle

| | Field-Nodes | CPU-Nodes (Werte-Nodes) |
|---|---|---|
| Läuft | auf der GPU, für jeden Pixel | auf der CPU, einmal pro Frame |
| Ergebnis | Farbe an einer Koordinate | Einzelwert (Zahl, Farbe, Bool, ...) |
| Code | WGSL-Snippet | C++-Kernel |
| Modul | `wh_compiler` | `wh_values` |
| Beispiele | Gradient, Rotate, Mix | Time, LFO, Random, Smooth |

Die Werte der CPU-Nodes fließen in die GPU-Seite, und zwar über den Parameterpuffer.
Der Shader erfährt nichts davon: Er liest weiter `wh_params.n7_angle`. Was dort steht,
bestimmt der Graph.

## Modulation: ein Eingang, der heißt wie der Parameter

Hat ein Node-Typ einen **Eingang und einen Parameter mit demselben Schlüssel**, dann
ersetzt eine Verbindung an diesem Eingang den gespeicherten Parameterwert.

```
Time ──seconds──▶ angle ┌────────┐
                        │ Rotate │   Parameter "angle" = Wert von Time
Gradient ──out──▶ source└────────┘
```

Der Portyp richtet sich nach dem Parametertyp. Die Registry lehnt Paare ab, die nicht
passen.

| Parameter | Eingang | Auch verbindbar von |
|---|---|---|
| Zahl (`double`) | `Scalar` | `Integer`, `Boolean` |
| Ganzzahl (`int64`) | `Integer` | |
| Schalter (`bool`) | `Boolean` | `Trigger` |
| Farbe | `Color` | |
| Vec2, Vec3, Text | (keiner) | |

Regeln, die immer gelten:

- **Zahlen werden auf den Bereich des Parameters begrenzt.** Eine Quelle kann einen
  Regler nicht über sein Maximum treiben; ein Maßstab von 0 (Division im Shader) bleibt
  durch `minValue` ausgeschlossen.
- **NaN und Unendlich kommen nicht durch.** Liefert die Quelle so etwas, gilt der
  gespeicherte Wert.
- **Ohne Wert gilt der gespeicherte Wert.** Das betrifft einen unverbundenen Eingang,
  eine Quelle, die in diesem Frame nichts geliefert hat, und einen falschen Typ.
- **Verbinden und Trennen braucht keine Neukompilierung.** Der Shadertext und damit der
  `structureHash` ändern sich nicht, nur die Bytes im Puffer.

Node-Autoren schreiben den Eingang nicht von Hand. `graph::addModulationPorts(descriptor)`
ergänzt für jeden modulierbaren Parameter einen optionalen Eingang mit gleichem Schlüssel
und Anzeigenamen. Die eingebauten Nodes tun das alle.

Die GUI erkennt Modulations-Eingänge daran, dass `port.key == parameter.key` gilt, und
kann sie neben dem Reglerwert anzeigen statt als eigene Buchse.

## Konstanten als Field

Ein `Scalar` oder eine `Color` darf auch an einen **Field-Eingang** gehängt werden. Der
Compiler legt dafür einen Uniform-Slot an (`SlotOrigin::FieldConstant`) und setzt im
Shader ein konstantes Feld ein:

```wgsl
// Scalar:  vec4<f32>(wh_params.n3_b, wh_params.n3_b, wh_params.n3_b, 1.0)
// Color:   wh_params.n3_b
```

`docs/examples/modulation_and_constants.wgsl` ist die echte Compiler-Ausgabe für einen
Graphen, in dem `Time` den Drehwinkel von `Rotate` moduliert, ein `Number` als konstantes
Feld in `Add` und ein `Color` als konstantes Feld in `Mix` eingeht. Im Parameter-Struct
stehen die Konstanten als `n7_a` (f32) und `n9_b` (vec4), der modulierte Winkel als
gewöhnliches `n2_angle`.

Die Koordinate spielt keine Rolle. Anders als bei der Modulation ändert sich hier der
Shadertext, weil statt eines Funktionsaufrufs ein Uniform gelesen wird. Das Umstecken
von einem Field-Node auf eine Konstante ergibt also einen neuen `structureHash`, das
Ändern des Werts nicht.

## Wie ein CPU-Node aufgebaut ist

Wie beim Field-Node gibt es zwei Teile unter derselben Typ-ID:

1. **`NodeDescriptor`** mit Domäne `Cpu`: Ports und Parameter, für die GUI sichtbar.
2. **Kernel**: eine Klasse, die `ICpuKernel::evaluate` implementiert.

```cpp
class DoubleKernel final : public values::ICpuKernel {
public:
    void evaluate(const values::KernelContext& context, values::KernelOutputs& outputs) override {
        outputs.set("out", context.inputs.number("value") * 2.0);
    }
};

values::CpuNodeDefinition def;
def.descriptor.typeId = "my.value.double";
def.descriptor.domain = graph::NodeDomain::Cpu;
def.descriptor.ports = {{"out", "Out", graph::PortType::Scalar, graph::PortDirection::Output, false}};
def.descriptor.parameters = {{"value", "Value", 0.0, std::nullopt, std::nullopt}};
graph::addModulationPorts(def.descriptor);   // macht "value" modulierbar
def.factory = [] { return std::make_unique<DoubleKernel>(); };
values::registerCpuDefinitions(std::array{def}, registry, kernelLibrary);
```

**Eingänge** liest der Kernel über `context.inputs`. Für jeden Parameter steht der
aktuelle Wert bereit, gespeichert oder moduliert. Dazu kommen verbundene Eingänge ohne
gleichnamigen Parameter. `number()` versteht auch Ganzzahlen und Bool; fehlt ein Wert,
gibt es den Rückfallwert.

**Ausgänge** setzt der Kernel mit `outputs.set(port, wert)`. Der Typ muss zum Portyp
passen (Scalar: `double`, Integer: `int64_t`, Boolean und Trigger: `bool`, Color:
`ColorRgba`, Text: `string`). Der Evaluator prüft das und meldet `InvalidArgument`, statt
einen falschen Wert weiterzureichen.

**Zustand** ist erlaubt. Pro Node im Graphen gibt es genau eine Kernel-Instanz, die über
Frames hinweg lebt (siehe `wh.value.smooth`). Sie verschwindet, wenn der Node gelöscht
wird. Wer Zustand hält, verliert die Fähigkeit, direkt auf eine Zeit zu springen. Wo es
geht, sind Kernel zustandslos und lesen die Zeit aus `context.time` (siehe `wh.value.lfo`).

**Verboten** im Kernel (Ausnahme: die DMX-Nodes, deren Wert aus dem Netzwerk kommt): Systemuhr, globale Zufallsquellen, Blockieren. Das Ergebnis muss
bei gleichem Graph und gleicher Zeit gleich sein, sonst sind Offline-Rendern und Tests
nicht reproduzierbar.

## Ablauf pro Frame

```cpp
values::ValueEvaluator evaluator{kernelLibrary};   // lebt über Frames hinweg
values::ValueStore store;

// pro Frame, auf dem Engine-Thread:
evaluator.evaluate(graph, frameTime, store);                       // 1. CPU-Nodes auswerten
auto bytes = compiler::packParameters(graph, compiled.layout, &store);  // 2. Puffer füllen
device.writeBuffer(parameterBuffer, 0, bytes);                     // 3. hochladen
```

Der Evaluator geht die CPU-Nodes in Abhängigkeitsreihenfolge durch. Ein Kernel sieht
immer die Werte seiner Vorgänger aus **demselben** Frame.

**Welche Nodes laufen?** Nur solche, deren Ergebnis jemand braucht: Ein CPU-Node wird
ausgewertet, wenn ein Ausgang zu einem Nicht-CPU-Node führt oder zu einem CPU-Node, der
selbst gebraucht wird. CPU-Nodes ohne jeden Ausgang (spätere Senken wie "OSC senden")
laufen immer. Ein vergessener, unverbundener Node kostet nichts.

## Eingebaute Werte-Nodes

| Typ | Ausgang | Parameter | Hinweis |
|---|---|---|---|
| `wh.value.time` | `seconds` | `speed`, `offset`, `speedMin`, `speedMax` | Zeit × Tempo + Versatz; negatives Tempo läuft rückwärts. `speedMin`/`speedMax` stellen nur den Bereich des Reglers in der Oberfläche ein |
| `wh.value.number` | `out` | `value` | Konstante, oder Relais für einen modulierten Wert |
| `wh.value.color` | `out` (Color) | `color`, `hue`, `saturation`, `brightness` | Farbquelle für Farbparameter und Field-Eingänge. `hue` verschiebt den Farbton in Umläufen (1 = einmal rum), `saturation` und `brightness` multiplizieren; mit Time, LFO, Random oder Number an diesen Eingängen entsteht Regenbogen, Pulsieren oder Blitzen. Standardwerte 0 / 1 / 1 lassen die Farbe unverändert |
| `wh.value.colorramp` | `out` (Color) | `count` (2–4), `position`, `loop`, `color1`–`color4` | Überblendet mit `position` (0..1) zwischen den ersten `count` Farben; mit `loop` geht es von der letzten zurück zur ersten, `position` 0..1 ist dann ein voller Umlauf (Sägezahn-LFO = ewiger Farbwechsel). Die Farben können selbst von Color-Nodes kommen |
| `wh.value.lfo` | `out` | `shape` (Auswahlfeld, in der Datei der Index 0–3), `frequency`, `phase`, `min`, `max`, `frequencyMin`, `frequencyMax` | Sinus, Dreieck, Sägezahn, Rechteck; zustandslos. `frequencyMin`/`frequencyMax`: Bereich des Reglers |
| `wh.value.random` | `out` | `rate`, `seed`, `smooth` | Hash-basiert, reproduzierbar; weich oder gehalten |
| `wh.value.map` | `out` | `value`, `inMin`, `inMax`, `outMin`, `outMax`, `clamp` | Bereich umrechnen |
| `wh.value.smooth` | `out` | `value`, `time` | Tiefpass erster Ordnung; hat Zustand |
| `wh.value.dmx` | `value`, `on` (Boolean), `present` (Boolean) | `source` (0 Art-Net, 1 sACN), `universe`, `channel`, `bits16`, `min`, `max`, `threshold`, `timeout`, `hold` | DMX-Eingang, siehe unten |
| `wh.value.dmxcolor` | `out` (Color) | `source`, `universe`, `channel`, `mode` (0 RGB, 1 RGB + Dimmer), `timeout`, `hold` | Drei oder vier Kanäle als Farbe |

| `wh.value.audiolevel`, `audioband`, `audiospectrum`, `beat`, `tap` | siehe [Audio](audio.md) | | Audio-Eingang (Kategorie „Audio“): Pegel, Frequenzband, Spektrum, Beat und Tap Tempo |

### DMX-Eingang

Die beiden DMX-Nodes (Kategorie „Input“) lesen Kanalwerte, die per Art-Net oder sACN ins Studio kommen,
zum Beispiel von einem Lichtpult. So steuern Fader und Farben des Pults Effekte, Geschwindigkeiten und
Farbparameter.

- **Universe und Kanal** wählen den Wert: `universe` (Art-Net: Port-Address 0–32767, sACN: 1–63999) und
  `channel` 1–512. Art-Net und sACN sind getrennte Universen: Art-Net 1 ist nicht sACN 1.
- **DMX Channel** rechnet 0–255 (mit `bits16` zwei Kanäle als 0–65535) auf `min`…`max` um. `on` ist wahr,
  wenn der Wert (vor der Umrechnung, 0..1) mindestens `threshold` erreicht, nützlich für Schalter und Tasten.
  `present` ist wahr, solange Pakete dieses Universe ankommen.
- **Signalverlust:** Kommt länger als `timeout` Sekunden (0 = nie) kein Paket, wird `present` falsch. Mit
  `hold` (Standard) bleibt der letzte Wert stehen, wie es DMX-Geräte gewöhnlich tun, sonst fällt er auf 0
  bzw. Schwarz.
- **DMX Color:** Kanal, Kanal+1, Kanal+2 sind Rot, Grün, Blau; im Modus „RGB + Dimmer“ skaliert Kanal+3 die
  Helligkeit.
- **Voraussetzung:** Der Empfang ist im Studio im Menü „Eingang“ einzuschalten (standardmäßig aus). Ohne
  Empfang liefern die Nodes dauerhaft „kein Signal“.
- **Nicht reproduzierbar:** Der Wert kommt aus dem Netzwerk, nicht aus der Zeit. Offline-Rendern mit
  DMX-Nodes ergibt je nach Eingang verschiedene Ergebnisse.

Jeder Parameter hat einen Modulations-Eingang. Beispiel für ein langsam wanderndes
Tempo: `Random` → `Smooth` → `LFO.frequency`.

## Grenzen (Stand Schritt 4b, Teil A)

- **Kernel liefern Einzelwerte und Spektren.** Ein Spektrum (Porttyp `AudioSpectrum`, 32 Bänder)
  läuft auf einem eigenen Kanal: `KernelOutputs::setSpectrum`, `ValueStore::spectrum` und
  `IValueLookup::spectrum`. Punktwolken und Bilder bekommen eigene Kanäle, wenn diese Nodes
  entstehen; `registerCpuDefinitions` lehnt solche Ausgänge bis dahin ausdrücklich ab.
- **`wh.value.lfo` ist zustandslos.** Ändert sich die Frequenz im Lauf, springt die
  Phase, weil sie aus der absoluten Zeit folgt. Für saubere Frequenzmodulation braucht es
  später einen akkumulierenden Oszillator als eigenen Node.
- **Der Evaluator legt pro Node und Frame kleine Maps an.** Bei den üblichen Graphen ist
  das unerheblich. Wird es messbar, kommt ein vorab berechneter Auswertungsplan, der nur
  bei Strukturänderungen neu entsteht.
- **Ein Wert pro Frame.** Es gibt keine Unterabtastung innerhalb eines Frames. Schnelle
  Modulation (Audiorate) gehört in einen Field-Node, nicht in einen CPU-Node.
- **Noch nicht vorhanden:** Bilder als Field-Eingang, Raster-Pässe, der GPU-Executor, der
  die drei Schritte oben pro Frame ausführt (Teil B).
