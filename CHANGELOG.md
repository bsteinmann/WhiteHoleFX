# Änderungen

## 0.14.1

- **Lizenz:** WhiteHoleFX steht jetzt unter der Apache License 2.0 (`LICENSE`, `NOTICE`); README und
  Debian-Paket (`copyright`) nennen sie. Das Repository ist öffentlich

## 0.14.0

- **Mapping-Import:** LED-Positionen aus CSV- oder JSON-Dateien als Punktwolke laden („Import …“ im
  Layout-Fenster). CSV mit Komma, Semikolon, Tabulator oder Leerraum, Dezimalkomma, Kopfzeile mit
  Spaltennamen; JSON als Liste von Punkten oder Objekten. Optionen: Maßstab, y spiegeln, auf den Ursprung
  schieben. Fehler nennen Datei und Zeile. Beispiel: `examples/bogen.csv`
- Neu: `wh_mapping/point_import.hpp` (`parsePointsCsv`, `parsePointsJson`, `parsePoints`), kleiner JSON-Leser
  (intern), `wh_runtime/point_file.hpp` (`loadPointsFile`, `pointsFixtureName`)
- Studio: `layout_panel_add.cpp` (Fixtures anlegen und importieren), Rauchtest für den Import

## 0.13.1

- **Kommandozeile:** `--dmx-in artnet,sacn` schaltet den DMX-Eingang für die DMX-Nodes ein; `--check` nennt die
  Universen, die das Projekt liest, und der Runner weist darauf hin, wenn DMX gelesen, der Empfang aber aus ist
- Art-Net-/sACN-Ports und die Kanalzahl je Universe stehen nur noch einmal (`whitehole/core/dmx_constants.hpp`)
- Dateien wieder unter 400 Zeilen: `parameter_panel.cpp` (neu: `parameter_editors.cpp`), `node_editor.cpp`
  (neu: `node_editor_edit.cpp`), `stage_on_device_test.cpp` (neu: `sampling_on_device_test.cpp`)
- Alle Quelldateien haben wieder einen Kopfkommentar (63 ergänzt)
- Doku: README, Ausgabe (Windows-Aussage war veraltet), Projektdatei, Werte-Graph

## 0.13.0

- **DMX-Eingang über Art-Net und sACN:** neues Modul `wh_input` liest ArtDmx- und E1.31-Pakete, hält den
  letzten Stand jedes Universe und stellt den UDP-Empfänger (Linux und Windows). Menü „Eingang“ im
  Studio, standardmäßig aus
- **Neue Nodes** (Kategorie „Input“): „DMX Channel“ (Zahl, Schalter mit Schwelle, „Signal da“, optional
  16 Bit) und „DMX Color“ (RGB oder RGB + Dimmer). Zeitlimit und „letzten Wert halten“ bei Signalverlust
- sACN tritt nur den Multicast-Gruppen der Universen bei, die das Projekt liest
- Neu in `wh_runtime`: `findDmxUsage` / `Player::dmxUsage`; `BuiltinLibrary::dmx` ist der gemeinsame Speicher

## 0.12.0

- **Master und Blackout:** Regler „Master“ (Gesamthelligkeit) und Schalter „Blackout“ (Strg+B) in der
  Leiste „Betrieb“. Nicht im Projekt gespeichert; Blackout gewinnt gegen Master und Testbild, die Pakete
  laufen weiter. Banner „BLACKOUT“ in der Vorschau
- **LED-Info:** Tooltip beim Überfahren der Vorschau mit Fixture, Nummer am Kabel, Position, Universe und
  Kanal und aktueller Farbe
- **Zuletzt geöffnet** im Menü Datei (zehn Einträge, fehlende Dateien fallen heraus)
- Neu in `wh_runtime`: `Session::setOutputLevel`, `Player::setMasterBrightness` / `setBlackout`,
  `ledAtPixel` / `describeLed` / `formatLedInfo` / `Player::ledInfoAt`, `addRecentFile` / `removeRecentFile`

## 0.11.0

- **Node-Presets:** ausgewählte Nodes samt Parametern und Kabeln als Datei merken („Als Preset speichern …“
  im Kontextmenü des Node-Editors) und über „Preset einfügen“ als normale Nodes wieder einfügen,
  in jedem Projekt. Dateien: `<Name>.whfxnodes` im Datenordner, Projektformat mit `node`/`connect`
- Neu in `wh_runtime`: `presetToText`, `presetFromText`, `PresetStore` (Ordner mit Presets),
  `applyClipboard` (gemeinsamer Weg für Einfügen und Preset-Export), `Player::registry()`

## 0.10.0

- **Neue Generatoren** (eigene Umsetzungen gängiger Lichteffekt-Ideen, alle zustandslos): Metaballs,
  Starfield (mit Drift), Fading Pixels, Expanding Shapes (Kreis, Quadrat, Raute; nach außen oder innen) und
  Sine Lines (1–8 Wellen mit Farbverlauf). Rezepte in `docs/field-nodes.md`
- Der WGSL-Code der neuen Nodes wurde in einem echten WebGPU (Chromium) übersetzt und mit 37 Zahlenprüfungen
  geprüft; die Tests auf dem Gerät (`node_patterns_on_device_test.cpp`) wiederholen die wichtigsten
- Die Hilfen der Gerätetests für Nodes liegen jetzt in `tests/gpu_device/node_render_support.hpp`

## 0.9.0

- **Testmodus** (Dock „Testmodus“): Testbilder ersetzen den Effekt zur Inbetriebnahme: Farbe,
  Auswahl hervorheben (100 % Weiß), Laufbalken entlang x/y/z, Farbreihenfolge (R, G, B, W) und
  Lauflicht in Kabelreihenfolge. Sie gehen nur raus, wenn „Ausgabe senden“ an ist; ein rotes Banner
  „TESTMODUS“ in der Vorschau und die Statuszeile zeigen den Modus an. Das Testbild gehört nicht zum
  Projekt (nicht gespeichert, nicht im Undo)
- Neu in `wh_runtime`: `renderTestPattern` (reine Rechnung, ohne GPU getestet), `Session::setTestPattern`,
  `Player::setTestPattern` / `clearTestPattern`

## 0.8.0

- **Ausgangs-Node „Output“** (`wh.output.canvas`): der Ausgang als Node; bespielt die ganze Leinwand
  oder nur ein Rechteck (Maßstab einpassen oder Fenster ausschneiden) und hat eine Gesamthelligkeit.
  Ein neu angelegter Output-Node wird automatisch zum Ausgang und hängt sich hinter den bisherigen; die
  Vorlage „Neues Projekt“ benutzt ihn. In der Vorschau lässt sich sein Rechteck wie beim Region-Node ziehen
- **Mischer** (Anregung: die Mischmodi klassischer LED-Software): Blend (Add, Multiply, Screen, Subtract,
  Difference, Lighten, Darken, Over), Crossfade (linear oder progressiv), Mask (Form/Intensität), Key
  (Schwarz durchsichtig)
- **Modifier:** Color Adjust (Farbton, Sättigung, Helligkeit, Gamma, Invertieren), Strobe, Outline
- **Transform:** Slide (Scroll/Übergang), Tile, Pixelate
- **Generatoren:** Solid Color, Scan Line, Ripples, Fire
- Rezepte für Kombinationen in `docs/field-nodes.md`

## 0.7.0

Aus den GitHub-Issues:

- #7 Fixtures hinzufügen: Ohne geladenes Projekt (frisch gestartetes Studio) taten „Matrix +“ und
  „Strip +“ stumm nichts. Jetzt legen sie ein neues Projekt an, dessen erstes Fixture das
  gewünschte ist; der Startbildschirm nennt Strg+N
- #8 LFO: Die Form ist ein Auswahlfeld (Sine, Triangle, Saw, Square). Allgemein gibt es jetzt
  Ganzzahl-Parameter mit Namen (`ParameterDescriptor::choices`), die als Dropdown erscheinen; in der
  Projektdatei bleibt es der Index
- #9 Neuer Node „Symmetry“ (`wh.transform.symmetry`): Achse (X, Y, Z) und Seite als Auswahlfelder,
  Lage der Linie einstellbar. Mirror kann das auch (mehrere Achsen, „Use other half“)
- #10 Color: Der Node hat jetzt Eingänge für Farbton (`hue`), Sättigung und Helligkeit, die mit
  LFO, Random, Time oder Number gesteuert werden können. Neuer Node „Color Ramp“
  (`wh.value.colorramp`) überblendet zwischen 2 bis 4 Farben mit einer Zahl, auf Wunsch im Kreis

## 0.6.0

- **Region-Node:** Der Node „Region“ (`wh.transform.region`) legt den Effekt eines Generators auf ein
  Rechteck der Leinwand (`left`, `top`, `width`, `height`); der Rest bleibt aus. Ist er im
  Node-Editor gewählt, zeigt die Vorschau das Rechteck mit Griffen zum Ziehen (Ecken: Größe, Mitte:
  Verschieben, Pfeiltasten); jede Ziehbewegung ist ein Undo-Schritt
- Die globale Bereichs-Einstellung aus 0.5.0 (`settings … region`, Seite „Bereich“ im Layout-Dock)
  ist wieder entfernt, der Region-Node ersetzt sie. **Hinweis:** Projekte mit `settings … region`
  lassen sich nicht mehr laden; die Zeile löschen und einen Region-Node einfügen
- Kern: `Player::setRegionRect` (Rechteck eines Region-Nodes, ein Undo-Schritt), `Player::setRegion`
  entfällt

## 0.5.1

Aus den GitHub-Issues:

- #1 Vorschau: Die Punktgröße richtet sich nach dem kleinsten Abstand der Form (Matrix mit
  verschiedenem Abstand in x und y, Gitter, Punktwolken), die Punkte überlappen nicht mehr
- #2 Layout: „Entfernen“ entfernt alle gewählten Fixtures; der Rauchtest prüft das Entfernen eines
  Fixtures vorn in der Liste (im Kern ging es immer, ein Test deckt jede Reihenfolge ab)
- #3 Verlauf „nicht linear“: Die Vorschau rechnet jetzt wie die Ausgabe (`sRGB(v^gamma)`, Gamma je
  Fixture aus dem Patch) und zeigt Werte so, wie sie gemeint sind; Farbwähler speichern den
  sichtbaren Wert ohne Umrechnung. **Hinweis:** In älteren Projekten gespeicherte Farben erscheinen
  dadurch anders (sie waren als Licht abgelegt)
- #4 Zahlenfelder: sinnvolle Schrittweiten (Winkel 5° statt 0,1°, ohne Bereich 1, mit Bereich ein
  Hundertstel gerundet), gedrückt halten beschleunigt
- #5 Mirror: auch an der z-Achse, Lage der Spiegellinie einstellbar, wahlweise die obere Hälfte als Vorlage
- #6 Time und LFO: Bereich des Reglers einstellbar (`speedMin`/`speedMax`, `frequencyMin`/`frequencyMax`)

## 0.5.0

- Globaler Effektbereich (`settings … region`): in 0.6.0 durch den Region-Node ersetzt
- Vorschau als Arbeitsfläche: Fixtures anklicken, mit Strg/Umschalt mehrere wählen, per Gummiband
  wählen, ziehen und mit den Pfeiltasten schieben; jede Ziehbewegung ist ein Undo-Schritt
- Layout-Dock: Mehrfachauswahl, Ausrichten (links, rechts, oben, unten, Mitte) und gleichmäßig
  Verteilen
- Kern: `moveFixtures`, `alignFixtures`, `distributeFixtures`; `wh_preview`
  meldet den gezeichneten Ausschnitt (`ViewFrame`) und bietet Treffertests

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
