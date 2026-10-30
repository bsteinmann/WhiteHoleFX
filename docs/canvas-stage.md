# Canvas-Stage: vom Graphen zum Bild

Modul `wh_executor`. Die `CanvasRenderStage` ist der erste Baustein, der alle
bisherigen Module zusammensetzt: Sie nimmt einen Graphen, wertet die CPU-Nodes aus,
übersetzt die Field-Nodes zu einem Shader und rastert das Ergebnis pro Frame in eine
Textur. Sie hängt sich als `engine::IRenderStage` in die Engine.

```
Graph ──setGraph──► compileField ──► WGSL-Bibliothek + ParameterLayout
                         │                   │ (nur bei neuem structureHash)
                         │                   ▼
                         │            Compute-Pipeline  (canvasShader hängt den Einstieg an)
                         ▼
record(FrameTime):  ValueEvaluator ─► ValueStore ─► packParameters ─► Parameterpuffer
                    Frame-Daten (Zeit, Delta, Nummer) ─► Frame-Puffer
                    dispatch(Pipeline, Gruppe 0: Frame + Parameter, Gruppe 1: Zieltextur)
```

## Dateien

| Datei | Aufgabe |
|---|---|
| `canvas_shader.hpp/.cpp` | Hängt den Compute-Einstieg `main` an die Feldbibliothek. Reiner Text, ohne GPU testbar. |
| `field_pass.hpp/.cpp` | Der gemeinsame Ablauf aller Pässe: CPU-Nodes, Parameter, Pipeline-Lebensdauer, Fehlerverhalten. Wird auch vom Abtast-Pass genutzt (`mapping.md`). |
| `canvas_render_stage.hpp/.cpp` | Besitzt die Zieltextur und setzt den Dispatch über die Pixel auf. |

Die Trennung hat einen Grund: Der Einstieg ist der einzige Teil, der sich zwischen
Konsumenten unterscheidet. Der Mapping-Pass (Schritt 5) hängt später einen Einstieg an,
der das Feld an LED-Positionen abtastet; Bibliothek, Parameterpuffer und Pipeline-Logik
bleiben gleich.

## Bindings

| Gruppe | Binding | Inhalt |
|---|---|---|
| 0 | 0 | `FrameUniforms` (Zeit, Delta, Frame-Nummer) |
| 0 | 1 | Parameterpuffer gemäß `ParameterLayout` |
| 1 | 0 | Zieltextur (`texture_storage_2d<FORMAT, write>`) |

Die Zieltextur hat die Nutzung `Storage | Sampled | CopySource`: Der Canvas-Pass
schreibt sie, spätere Raster-Nodes und der Mapping-Pass lesen sie, die Vorschau kann sie
kopieren. Formate: `Rgba8Unorm`, `Rgba16Float` (Standard), `Rgba32Float`.

## Beispiel

`docs/examples/canvas_plasma_rotate_time.wgsl` ist der vollständige Shader für
`Plasma -> Rotate`, wobei `Time` den Drehwinkel steuert (Format `Rgba16Float`). Die
Bibliothek des Compilers steht oben, der angehängte Einstieg unten. Dass `Time` den Winkel
steuert, sieht man dem Text nicht an: Die Zeit steckt im Parameterpuffer, nicht im Shader.

## Koordinaten

Pixel `(x, y)` tastet das Feld bei `p = ((x + 0.5) / breite, 1 - (y + 0.5) / höhe, 0)` ab.
Die oberste Zeile (`y = 0`) liegt also bei `p.y` nahe 1: **y zeigt nach oben**, wie im Raum
und im Layout von `wh_mapping`. Eine Matrix aus dem Mapping trifft damit exakt dieselben
Punkte wie die Pixel dieser Textur.

## Pipeline-Lebensdauer

| Änderung | Folge |
|---|---|
| Parameter, Modulation, Zeit | Nur Pufferinhalte. Keine neue Pipeline. |
| Struktur (Node einfügen, Verbindung ändern) | Neuer Shadertext, neuer `structureHash`: neue Pipeline. |
| Größerer oder kleinerer Parameterpuffer | Nur dann ein neuer Puffer. Passt eine Änderung in die Lücken des bestehenden Layouts (zwei `f32` neben einem `vec4`), bleibt der Puffer. |

Reihenfolge beim Umbau: Zuerst entsteht alles Neue (Pipeline, gegebenenfalls Puffer),
erst danach wird das Alte freigegeben. Scheitert ein Schritt, nimmt die Stage das
bereits Angelegte zurück, und der letzte gute Zustand rendert weiter.

## Fehlerverhalten

- **`setGraph` scheitert** (Graph nicht übersetzbar, Pipeline- oder Pufferfehler): Die
  Stage behält Graph, Layout und Pipeline des letzten Erfolgs. Der Fehler steht in
  `graphError()` und wird zurückgegeben. So kann man einen halb verdrahteten Graphen
  bearbeiten, ohne dass die Ausgabe schwarz wird. Der nächste erfolgreiche Aufruf
  löscht den Fehler.
- **Ein CPU-Node liefert Unsinn** (falscher Ausgabetyp, fehlender Kernel): Der Frame
  läuft mit den gespeicherten Parameterwerten weiter; der Fehler steht in
  `frameError()`.
- **Upload scheitert**: Der Frame entfällt (`framesSkipped`), `frameError()` meldet es,
  der nächste Frame versucht es erneut.
- **Kein Graph**: `record` zeichnet nichts auf und zählt `framesSkipped`.

Die Stage arbeitet mit einem Schnappschuss: Änderungen am Graphen werden erst mit dem
nächsten `setGraph` sichtbar. Das passt zum Engine-Thread-Modell, in dem die GUI
Schnappschüsse übergibt, statt am lebenden Graphen zu ziehen.

## Grenzen

- Nur 2D. Cubes und Punktwolken übernimmt das Mapping (Schritt 5).
- Die Zeit geht als `f32` an den Shader; nach etwa 10^5 s Laufzeit sinkt die Auflösung
  auf rund 8 ms.
- Genau ein Ziel (`target`). Mehrere Ausgänge und Raster-Nodes (Blur, Feedback) brauchen
  einen Ausführungsplan mit Zwischentexturen; der kommt, sobald die erste Raster-Node da ist.
- Die Textur ist ein GPU-Bild. `RenderEngine` kopiert einen *Puffer* in den Readback-Ring;
  für LEDs liefert den der Abtast-Pass (`mapping.md`). Die Canvas-Textur dient Vorschau
  und späteren Raster-Nodes.
- Der erzeugte WGSL-Text ist mit einem Parser auf Syntax geprüft, aber noch nicht von
  Tint oder naga validiert, solange kein echtes Dawn-Backend läuft.
