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

**Verboten** im Kernel: Systemuhr, globale Zufallsquellen, Blockieren. Das Ergebnis muss
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
| `wh.value.time` | `seconds` | `speed`, `offset` | Zeit × Tempo + Versatz; negatives Tempo läuft rückwärts |
| `wh.value.number` | `out` | `value` | Konstante, oder Relais für einen modulierten Wert |
| `wh.value.color` | `out` (Color) | `color` | Farbquelle für Farbparameter und Field-Eingänge |
| `wh.value.lfo` | `out` | `shape`, `frequency`, `phase`, `min`, `max` | Sinus, Dreieck, Sägezahn, Rechteck; zustandslos |
| `wh.value.random` | `out` | `rate`, `seed`, `smooth` | Hash-basiert, reproduzierbar; weich oder gehalten |
| `wh.value.map` | `out` | `value`, `inMin`, `inMax`, `outMin`, `outMax`, `clamp` | Bereich umrechnen |
| `wh.value.smooth` | `out` | `value`, `time` | Tiefpass erster Ordnung; hat Zustand |

Jeder Parameter hat einen Modulations-Eingang. Beispiel für ein langsam wanderndes
Tempo: `Random` → `Smooth` → `LFO.frequency`.

## Grenzen (Stand Schritt 4b, Teil A)

- **Kernel liefern Einzelwerte.** Spektren (Audio-FFT), Punktwolken und Bilder bekommen
  eigene Kanäle, wenn diese Nodes entstehen. `registerCpuDefinitions` lehnt solche
  Ausgänge bis dahin ausdrücklich ab.
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
