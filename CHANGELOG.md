# Änderungen

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
