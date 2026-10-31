# Änderungen

## 0.4.0

- Node-Editor: Der Node am Ausgang lässt sich löschen; der Ausgang rückt auf den Node davor
  (vorher war das gesperrt). Rückgängig stellt ihn wieder her
- Node-Editor: Kopieren, Einfügen, Duplizieren (Strg+C, Strg+V, Strg+D) samt Parametern und den
  Kabeln zwischen den Nodes; ein Schritt für Rückgängig
- Node-Editor: Suchfeld zum Hinzufügen von Nodes (Strg+Leertaste)
- Neuer Generator „Digital Rain“ (fallende Pixel, `wh.generator.rain`), auch im Cube

## 0.3.0

- Oberfläche: **Datei → Neu** (Strg+N) legt ein Projekt aus einer Vorlage an; **Speichern**
  (Strg+S) neben „Speichern unter“
- Oberfläche: Ein Klick auf einen Node im Editor zeigt dessen Parameter (Dock „Parameter“ nach
  vorn, Gruppe orange umrandet); die Auswahl überlebt Änderungen am Graphen
- Farben können **transparent** sein: Kästchen „Transparent“ an jeder Farbe, Schachbrett im Farbfeld;
  Mix, Add, Verlauf, Kugel und Welle verrechnen Alpha vormultipliziert (deckende Farben unverändert)
- Pakete: Das Linux-.deb wird auf Ubuntu 22.04 gebaut und läuft damit auch auf Linux Mint 21 und
  Ubuntu 24.04

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
