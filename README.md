# WhiteHoleFX

Open-Source-Echtzeitsteuerung für LED-Matrizen, LED-Cubes, Strips und Pixelinstallationen.
Effekte entstehen in einem Node-Graphen und laufen als Shader auf der GPU. Dieselben
Effekte gelten für flache Matrizen und für LEDs im 3D-Raum.

> **Status: früh, aber lauffähig.** Eine Projektdatei lässt sich lesen, prüfen und per
> Konsole rendern und als Art-Net, sACN oder DDP senden (`whitehole_cli`). Es gibt noch keine
> Oberfläche. Siehe [Fahrplan](#fahrplan) und [Projektdatei](docs/project-format.md).

## Idee in drei Sätzen

Ein Effekt ist eine Funktion von Position und Zeit auf eine Farbe. Der Compiler verschmilzt
einen ganzen Node-Graphen zu **einer** Shader-Funktion. Das Mapping sagt, wo jede LED im
Raum sitzt, und tastet die Funktion genau dort ab.

## Module

| Modul | Aufgabe |
|---|---|
| `wh_core` | Grundtypen, Fehlerbehandlung ohne Ausnahmen, Dienste-Schnittstellen |
| `wh_gpu` | abstrakte GPU-Schnittstelle, Leser für Shader-Bindungen |
| `wh_gpu_webgpu` | optionales WebGPU-Backend (wgpu-native), läuft in der CI; siehe [GPU-Backend](docs/gpu-backend.md) |
| `wh_graph` | Graph-Modell, Node-Verzeichnis, Parameter und Modulation |
| `wh_values` | CPU-Nodes (Zeit, LFO, Zufall, ...) pro Frame auswerten |
| `wh_compiler` | Field-Nodes zu WGSL übersetzen |
| `wh_nodes` | mitgelieferte Nodes |
| `wh_engine` | Renderloop, Readback-Ring, Stages |
| `wh_executor` | Graph pro Frame ausführen: Canvas-Textur und Abtastung an LED-Positionen |
| `wh_mapping` | Fixtures (Matrix, Strip, 3D-Gitter, Punktwolke), Verkabelung, Normalisierung |
| `wh_output` | Farbumrechnung, Patch auf Universen, Art-Net / sACN / DDP, Ausgabe-Thread, UDP |
| `wh_project` | Projektdatei (Text): Layout, Effektgraph, Ausgabe lesen und schreiben |
| `wh_preview` | LED-Vorschau als Bild (CPU) und PNG-Schreiber |
| `wh_runtime` | Session: Projekt zu laufender Ausgabe zusammensetzen; Prüfbericht |
| `apps/whitehole_studio` | Qt-6-Oberfläche: Vorschau, Start/Stopp, Ausgabe schalten (optional, siehe [Oberfläche](docs/studio.md)) |
| `apps/whitehole_cli` | Konsolen-Runner: `--check`, rendern und senden |

Abhängigkeiten zeigen nur nach unten. Kein Modul bis auf die spätere GUI kennt Qt.

## Bauen und testen

Voraussetzungen: CMake ≥ 3.25, ein C++20-Compiler (GCC 13, Clang 16 oder neuer). Catch2
wird beim Konfigurieren automatisch geladen.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Strenge Warnungen als Fehler: `-DWHITEHOLEFX_WARNINGS_AS_ERRORS=ON`.

## Dokumentation

- [Field-Nodes und Compiler](docs/field-nodes.md)
- [Werte-Graph (CPU-Nodes, Modulation)](docs/value-graph.md)
- [Canvas-Stage](docs/canvas-stage.md)
- [Mapping und Abtast-Pass](docs/mapping.md)
- [Ausgabe (Art-Net, sACN, DDP)](docs/output.md)
- [Projektdatei und Konsolen-Runner](docs/project-format.md)
- [Oberfläche (Qt)](docs/studio.md)
- [GPU-Backend (WebGPU)](docs/gpu-backend.md)

## Konventionen

- C++20, keine Ausnahmen im Echtzeitpfad (`Result<T>`), keine Quelldatei über etwa 400 Zeilen.
- Koordinaten: rechtshändig, x nach rechts, **y nach oben**, z zum Betrachter; Farben linear RGBA.
- Jede Quelldatei beginnt mit einem Kommentar, der erklärt, wozu sie da ist.

## Fahrplan

- [x] 1–4: Architektur, Graph, Compiler, Werte-Graph, GPU-Ausführung
- [x] 5: Mapping und Abtast-Pass
- [x] 6: Ausgabe (Art-Net, sACN, DDP; WLED über DDP/Art-Net). Serial, MQTT, Hyperion, MIDI folgen später
- [ ] 7: Plugins und Skripte
- [ ] 8: Oberfläche (Qt 6): Fenster mit Live-Vorschau steht, Node-Editor und Layout-Editor folgen
- [x] GPU-Backend läuft in der CI auf Software-Vulkan; Prüfung auf echter GPU und Windows-CI stehen aus
- [x] Projektdatei und Konsolen-Runner (`whitehole_cli`)
- [ ] Raster-Nodes (Blur, Feedback), Dawn-Backend

## Hinweis zur Verifikation

Gebaut und getestet wurde bisher mit g++ direkt (inklusive ASan/UBSan). Die CMake-Dateien
und die CI sind neu und noch nicht in einem echten Lauf bestätigt. Die erzeugten Shader sind
mit einem WGSL-Parser auf Syntax geprüft, aber noch nicht auf einer GPU gelaufen.

## Lizenz

Noch nicht festgelegt. Bis eine `LICENSE`-Datei existiert, gilt: alle Rechte vorbehalten.
