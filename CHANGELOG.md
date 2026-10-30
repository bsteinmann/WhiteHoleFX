# Änderungen

## 0.2.0

- Oberfläche (Qt 6): Parameterregler, Node-Editor, Layout-Editor (Fixtures, Patches, Ziele,
  Einstellungen), Rückgängig/Wiederholen, Beispiele-Menü
- Projektformat: optionale Zeile `at x y` im node-Block (Lage im Editor); alte Dateien bleiben lesbar
- Player (`wh_runtime`): Graph, Layout und Ausgabe bearbeiten; jede Änderung wird samt Ausgabeplan
  geprüft und lässt sich zurücknehmen
- Bauen: CMake-Presets, Skripte zum Laden von wgpu-native (Version gepinnt), `BUILDING.md`
- Pakete: Linux `.deb` (Ubuntu 24.04 / Debian), Windows-Paket aus der CI

## 0.1.0

- Kern: Graph, Compiler, Werte-Graph, Mapping, Ausgabe (Art-Net, sACN, DDP), GPU-Backend
  (wgpu-native), Projektdatei, Konsolen-Runner, erste Oberfläche mit Vorschau
