# Mapping: wo sitzt welche LED, und wie kommt der Effekt dorthin?

Zwei Module arbeiten zusammen:

- `wh_mapping` (reine Rechenlogik, keine GPU): beschreibt die Hardware und backt sie zu
  einer `LedMap` mit der Position jeder LED im Einheitswürfel [0,1]³.
- `wh_executor` (`SamplingRenderStage`): tastet den Effekt an diesen Positionen ab und
  liefert eine Farbe je LED.

```
Fixtures ──► Layout.bake() ──► LedMap (unit: Vec3 je LED) ──setMap──► Positionspuffer
                                                                          │
Graph ──setGraph──► FieldPass ──► wh_field(p) ──► Compute: Farbe je LED ◄─┘
                                                      │
                                                      ▼
                                      Farbpuffer (16 Byte je LED) ──► RenderEngine/Readback
```

Weil ein Effekt nur eine Funktion der Position ist, gilt derselbe Graph für eine flache
Matrix, einen Strip, einen Cube und eine Punktwolke. Es wird kein Volumen gerastert.

## Koordinaten

- Rechtshändig: **x nach rechts, y nach oben, z zum Betrachter**.
- Längen sind frei wählbare Einheiten (z. B. Zentimeter). Erst `bake()` bildet sie auf
  [0,1]³ ab.
- Jede LED sitzt in der **Mitte ihrer Zelle**. Eine Matrix mit Raster 1 und 16 Spalten
  belegt x in [0,16]; die erste LED sitzt bei 0,5. Das entspricht den Pixelmitten des
  Canvas (`canvas-stage.md`).
- Matrix: Zeile 0 ist oben (wie ein Bild). Gitter: y = 0 ist unten (wie im Raum).

## Fixtures

| Form | Beschreibung |
|---|---|
| `MatrixShape` | Spalten × Zeilen, Rasterabstand, Verkabelung |
| `StripShape` | n LEDs gleichmäßig zwischen Anfang und Ende (beliebige Richtung im Raum) |
| `GridShape` | nx × ny × nz, Rasterabstand, Achsenfolge, optional Serpentine |
| `PointsShape` | beliebige Positionen in Kabelreihenfolge |

Jedes Fixture hat einen Ursprung im Raum. Mehrere Fixtures bilden ein `Layout`, z. B. eine
Wand aus vier Panels und ein Strip an der Decke. Ihre Boxen werden vereinigt und
gemeinsam auf den Einheitswürfel abgebildet.

### Verkabelung

Die Reihenfolge am Datenkabel steckt in `wiring.hpp`:

- Matrix: Startecke (4), zeilen- oder spaltenweise (2), Serpentine ja/nein = 16 Varianten.
- Gitter: Achsenfolge (6), Serpentine ja/nein. Die Serpentine ist durchgehend: Jede Linie
  beginnt dort, wo die vorige endete, auch beim Wechsel der Ebene.

Die Tests prüfen für alle Varianten, dass jede Zelle genau einmal vorkommt, der Strang in
der gewählten Ecke beginnt und bei Serpentine aufeinanderfolgende LEDs wirklich Nachbarn
sind. Die Ausgabe (Schritt 6) packt Pixel in genau dieser Reihenfolge in Universen.

## Normalisierung

| Modus | Wirkung |
|---|---|
| `Stretch` (Standard) | Die Box aller Fixtures wird je Achse auf [0,1] gestreckt. Ein 16×8-Panel füllt den ganzen Würfel, genau wie die Canvas-Textur. |
| `KeepAspect` | Alle Achsen mit demselben Faktor, im Würfel zentriert. Eine Kugel bleibt rund, auch in einer breiten Wand. |

Eine Achse ohne Ausdehnung (flache Matrix, gerader Strip) bleibt bei 0. Eine flache Anlage
hat also immer `p.z = 0`.

## Effekt auf einen Teil der Leinwand (Region-Node)

Soll ein Effekt nur einen Teil der Leinwand füllen, legt man den Node **Region**
(`wh.transform.region`, Kategorie Transform) hinter den Generator: Er bildet seinen Eingang auf ein
Rechteck der Leinwand ab, als wäre dieses Rechteck die ganze Leinwand; außerhalb ist er durchsichtig
(Alpha 0), diese LEDs bleiben aus. Das Rechteck steht in den Parametern `left`, `top`, `width` und
`height` (Anteile 0..1, `top` zählt von oben); die Mapping-Schicht weiß davon nichts, die
Normalisierung gilt immer für die ganze Wand. Mehrere Region-Nodes mit je einem Generator lassen
sich mit einem Mix-Node zusammensetzen.

Für das Ziehen in der Vorschau liefert `region_edit.hpp` die Rechnung (`dragRegion`,
`regionFromCorners`, `Region`); `Player::setRegionRect` schreibt die vier Parameter in einem
Undo-Schritt.

## Abtast-Pass

`SamplingRenderStage` legt beim Erzeugen drei Puffer für `ledCapacity` LEDs an:
Positionen (`vec4`, w = 0), Farben (`vec4`, linear RGBA, f32) und die gültige Anzahl.
`setMap` lädt die Positionen hoch, `record` tastet ab.

- **Ausgabe:** `output()` ist ein Puffer mit 16 Byte je LED. Genau das kopiert die
  `RenderEngine` in den Readback-Ring (`RenderEngineConfig::outputBuffer`). Gamma,
  Farbreihenfolge und Universen erledigt der Ausgabe-Pass in Schritt 6.
- **Stabiles Handle:** Die Kapazität legt die Puffergröße fest, damit die Engine das Handle
  nur einmal bekommt. Ein kleineres Layout ist jederzeit erlaubt; ein größeres braucht
  eine neue Stage. Hinter den gültigen LEDs steht alter Inhalt; der Ausgabe-Pass liest nur
  `ledCount()` Einträge.
- **Dispatch:** 64 LEDs je Arbeitsgruppe. WebGPU erlaubt höchstens 65535 Gruppen je
  Dimension; der Dispatch ist daher 2D, damit auch 8 Mio. LEDs funktionieren.
- **Fehlerverhalten:** wie beim Canvas (`field_pass.hpp`): Ein fehlerhafter Graph lässt die
  letzte gute Ausgabe weiterlaufen. Eine zu große oder leere Karte wird abgelehnt, die
  bisherige bleibt aktiv.

Das vollständige Beispiel steht in `docs/examples/sampling_wave_rotate3d_sphere.wgsl`
(Welle → Rotate 3D, addiert mit einer Kugel, Zeit steuert die Y-Drehung).

## Mitgelieferte 3D-Nodes

`Sphere`, `Plane Wave` und `Rotate 3D` (siehe `field-nodes.md`). Rotate 3D dreht
rechtshändig um die Achsen durch die Mitte des Würfels; Reihenfolge `Rz · Ry · Rx`.

## Grenzen und spätere Releases

- Fixtures lassen sich nicht drehen (nur verschieben); Strips sind gerade Strecken.
  Gedrehte Panels und gekrümmte Strips lassen sich bis dahin als Punktwolke beschreiben.
- Kein Dateiformat für Layouts und kein Editor (GUI, Schritt 8).
- Nicht jeder Node kennt z: Gradient, Plasma, Rotate, Mirror und Scale wirken in jeder
  z-Ebene gleich. Die 3D-Varianten kommen nach Bedarf.
- Der Shader ist mit einem WGSL-Parser auf Syntax geprüft, nicht von Tint/naga
  validiert und nicht auf einer GPU gelaufen. Die Drehmatrizen von Rotate 3D sind von Hand
  an den Achsen nachgerechnet (z. B. +x → +y unter angleZ = 90°), aber nicht numerisch
  gegen einen Referenzlauf geprüft.
- Die Kapazität einer Stage ist fest; ein Layoutwechsel über die Kapazität hinaus
  braucht den Neuaufbau von Stage und Engine.

## Import von Positionen

LED-Positionen aus einem anderen Programm (CAD, Tabelle, Skript) werden als Punktwolke geladen
(`mapping/point_import.hpp`, Datei lesen in `runtime/point_file.hpp`, im Studio „Import …“ im Layout-Fenster).
Die Reihenfolge der Punkte ist die Reihenfolge am Datenkabel.

**CSV** (auch `.txt`, `.tsv`): eine LED je Zeile, `x,y` oder `x,y,z`.
- Trenner Komma, Semikolon, Tabulator oder Leerraum; er wird an der ersten Datenzeile erkannt. Bei
  Semikolon und Tabulator ist ein Dezimalkomma erlaubt (deutsches Excel: `1,5;2,25`).
- Eine Kopfzeile (erste Zeile mit Text) ordnet die Spalten über die Namen `x`, `y`, `z`; weitere Spalten
  (z. B. `index`, `name`) werden ignoriert. Ohne Kopfzeile sind genau 2 oder 3 Spalten erlaubt.
- Leere Zeilen und Zeilen mit `#` werden übersprungen; ein Byte-Order-Mark am Anfang schadet nicht.

**JSON**: eine Liste von `[x,y]`, `[x,y,z]` oder `{"x":..,"y":..,"z":..}`, auch als Wert eines Feldes
`points`, `leds`, `pixels` oder `positions` in einem Objekt.

**Optionen:** Maßstab (Faktor auf alle Koordinaten, z. B. 0,1 für Millimeter in Zentimeter), y spiegeln (für
Programme, in denen y nach unten zeigt) und „kleinste Koordinate auf 0“ (die Lage im Raum bestimmt dann der
Ursprung des Fixtures). Reihenfolge: Maßstab, Spiegeln, Verschieben.

**Fehler** nennen Datei, Zeile und Grund („wand.csv: Zeile 12: y ist keine Zahl: "abc""). Höchstens
4 Millionen Punkte je Fixture. Nicht enthalten: SVG, DXF und Bilder (PNG) als Quelle.
