# WhiteHoleFX

Open-Source-Echtzeitsteuerung für LED-Matrizen, LED-Cubes, Strips und Pixelinstallationen.
Effekte entstehen in einem Node-Graphen und laufen als Shader auf der GPU. Dieselben
Effekte gelten für flache Matrizen und für LEDs im 3D-Raum.

> **Status: früh, aber benutzbar.** Projekte lassen sich in einer Qt-Oberfläche bearbeiten
> (Parameterregler, Node-Editor, Layout-Editor, Rückgängig/Wiederholen), live ansehen und als
> Art-Net, sACN oder DDP senden; ohne Oberfläche geht es mit `whitehole_cli`. Läuft in der CI auf
> Linux und Windows (GPU dort nur als Software-Adapter geprüft). Bauen und Installieren (Linux-.deb, Windows-Paket): [BUILDING.md](BUILDING.md).
> Siehe [Fahrplan](#fahrplan) und [Projektdatei](docs/project-format.md).

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
| `wh_runtime` | Session und Player: Projekt zu laufender Ausgabe zusammensetzen, Graph/Layout/Parameter bearbeiten, Rückgängig; Prüfbericht |
| `apps/whitehole_studio` | Qt-6-Oberfläche: Vorschau, Parameter, Node-Editor, Layout-Editor, Rückgängig (optional, siehe [Oberfläche](docs/studio.md)) |
| `apps/whitehole_cli` | Konsolen-Runner: `--check`, rendern und senden |

Abhängigkeiten zeigen nur nach unten. Kein Modul außer `apps/whitehole_studio` kennt Qt.

## Bauen und testen

Kurzfassung (Kern ohne GPU und Oberfläche; CMake ≥ 3.25, GCC 13 / Clang 16 / MSVC 2022):

```bash
cmake --preset dev
cmake --build --preset dev --parallel
ctest --test-dir build/dev --output-on-failure
```

Mit GPU und Oberfläche, Windows und alle Fallstricke: **[BUILDING.md](BUILDING.md)**.

## Dokumentation

- [Field-Nodes und Compiler](docs/field-nodes.md)
- [Werte-Graph (CPU-Nodes, Modulation)](docs/value-graph.md)
- [Canvas-Stage](docs/canvas-stage.md)
- [Mapping und Abtast-Pass](docs/mapping.md)
- [Ausgabe (Art-Net, sACN, DDP)](docs/output.md)
- [Projektdatei und Konsolen-Runner](docs/project-format.md)
- [Bauen und Starten](BUILDING.md)
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
- [x] 8: Oberfläche (Qt 6): Live-Vorschau, Parameterregler, Node-Editor, Layout-Editor, Rückgängig/Wiederholen
- [x] GPU-Backend läuft in der CI auf Software-Vulkan (Linux) und Software-Adapter (Windows); Prüfung auf echter GPU steht aus
- [x] Projektdatei und Konsolen-Runner (`whitehole_cli`)
- [ ] Raster-Nodes (Blur, Feedback), Dawn-Backend

## Hinweis zur Verifikation

Die CI baut und testet auf Linux (GCC mit ASan/UBSan, Clang) und Windows (MSVC) und lässt die
erzeugten Shader auf einem Software-Vulkan-/Software-Adapter laufen; die Oberfläche wird im
unsichtbaren Fenster bedient (Regler, Node-Editor, Layout-Editor, Rückgängig). **Noch nicht geprüft**
ist der Betrieb auf einer echten GPU, mit echten Controllern im Netz und die Bedienung mit Maus und
Bildschirm. Rückmeldungen dazu sind willkommen.

## Lizenz

Noch nicht festgelegt. Bis eine `LICENSE`-Datei existiert, gilt: alle Rechte vorbehalten.
