# Änderungen

## 0.63.0

- **Achsen-Griff in der Bühnenansicht (Reiter Layout).** Am ersten gewählten Fixture erscheinen drei Pfeile (x rot, y grün, z blau). Pfeil fassen und ziehen verschiebt alle gewählten Fixtures entlang der Achse, wie bei Slicern; Einrasten auf 0,1 (Alt: aus). Achsen, die zur Kamera zeigen, sind nur angedeutet. Das Drehen mit Ringen folgt als nächster Schritt. Neu in `wh_preview`: `gizmo.hpp` (Geometrie, Treffer, Zugweite; getestet).

## 0.62.1

- **Controller-Bibliothek: „Auf gewählten anwenden".** Überschreibt Name, Protokoll, Adresse und Port des gewählten Controllers mit einem gespeicherten Eintrag. Damit betreibst du dasselbe Gerät mit anderen Einstellungen (zum Beispiel Art-Net oder sACN) und lädst sie per Klick; die Patches bleiben auf dem Controller, bei Protokollwechsel verschiebt sich das Universe wie gewohnt.

## 0.62.0

- **Controller-Bibliothek.** Wie bei den Bauformen lassen sich Controller (Name, Protokoll, Adresse, Port) im Reiter Ausgabe speichern und in jedem Projekt wieder holen: **In Bibliothek speichern** legt den gewählten Controller unter seinem Namen ab (ohne Namen fragt das Studio danach; gleicher Name ersetzt den alten Eintrag), **Aus Bibliothek +** fügt einen gespeicherten Controller als neuen Controller ein, **Aus Bibliothek löschen** entfernt einen Eintrag aus der Bibliothek (das Projekt bleibt unverändert). Die Datei heißt `controller-library.whfx` und liegt neben `fixture-library.whfx` im Benutzerverzeichnis; die Umgebungsvariable `WHITEHOLEFX_CONTROLLER_LIBRARY` ersetzt den Pfad. Neu in `wh_project`: `writeControllerLibrary`, `parseControllerLibrary`.
- **CI:** Der Paketbau (.deb) kompiliert mit höchstens drei Aufträgen gleichzeitig, weil der Runner bei unbegrenzter Parallelität im Speicher ausging.

## 0.61.0

- **Das Protokoll gehört zum Controller.** Im Reiter Ausgabe wählst du Art-Net, sACN oder DDP in der Controller-Tabelle (neue Spalten Name und Protokoll), nicht mehr je Fixture. Die Patch-Tabelle wählt den Controller und zeigt das Protokoll nur an; **Universe und Erster Kanal** sind bei Art-Net und sACN aktiv, der **DDP-Offset** nur bei DDP. Die Universe-Spalte nennt die Zählweise (**Art-Net ab 0, sACN ab 1**) und begrenzt den Bereich je Protokoll. sACN-Priorität und Quellname erscheinen nur, wenn ein Controller sACN spricht.
- **Protokollwechsel verschiebt das Universe.** Wechselt ein Controller zwischen Art-Net und sACN (oder ein Fixture auf einen Controller mit anderem Protokoll), wird die Universe-Nummer der Patches um 1 verschoben, damit das „gleiche“ Universe gemeint bleibt. Die Statuszeile nennt es, Rückgängig nimmt es zurück.
- **Dateiformat:** `destination <n> <adresse|multicast> [port] [protocol artnet|sacn|ddp] [name "…"]`. Ältere Dateien laden wie bisher: jeder Controller bekommt das Protokoll seiner Patches, bei gemischten Protokollen auf einer Adresse entstehen zusätzliche Controller. Neue Dateien lassen sich in Programmen vor 0.61 nicht öffnen.
- **Fix: falscher „Stillstand“-Bericht beim Speichern.** Der Wächter hielt den offenen Windows-Dialog „Speichern unter“ (oder „Öffnen“) nach fünf Sekunden für ein Einfrieren und schrieb einen Bericht. Während dieser Dialoge ist der Wächter jetzt angehalten.

## 0.60.0

- **Neue Gruppe „Generators 1D“ für Strips.** Sechs Generatoren, die mit einer Koordinate entlang einer wählbaren Achse (`axis` X/Y/Z) rechnen: **Comet** (Lauflicht mit Schweif, auch Ping-pong), **Pulse Waves**, **Strip Gradient** (drei Farben, durchlaufend), **Meteors** (bis zu acht), **Sparkle** (Funkeln, `cells` = LED-Zahl) und **Level Bar** (Pegelbalken, `level` per Kabel, zum Beispiel von „Audio Level“). Sie laufen auf jedem Fixture, das in der Achsenrichtung ausgedehnt ist.
- **Rechtsklick auf einen Ein- oder Ausgang** im Node-Editor zeigt Nodes mit passendem Port; der gewählte Node entsteht daneben und wird sofort verbunden (wie beim Rechtsklick auf freie Fläche, nur mit passender Auswahl).

## 0.59.1

- **Localhost in der Netzwerkkarten-Auswahl (#35).** Einstellungen → Netzwerk bietet zum Senden und Empfangen jetzt auch „Localhost“ (127.0.0.1) an, zum Testen auf demselben Rechner.
- **Auswahlfelder zeigen die Zahl.** Bei Parametern mit Liste (zum Beispiel die Form bei „Expanding Shapes“) steht vor jedem Eintrag der gespeicherte Wert („0 – Circle“, „1 – Square“ …). Das ist die Zahl, die in der Projektdatei steht.
- **Audio-Nodes besser einzuordnen.** Die Beschreibungen von „Kick / Snare / Hat“ und „Audio Level“ sagen, wann man welchen nimmt (Einzelschläge, Gesamtlautstärke, Takt). Die Nodes bleiben getrennt: Kick / Snare / Hat hat eigene Ausgänge (Treffer, Pulse, Age) und eine eigene Schwelle.

## 0.59.0

- **Benutzerbibliothek für Bauformen.** Bauformen aus dem Reiter Fixtures bleiben jetzt über neue Programmversionen und neue Projekte erhalten: Das Studio spiegelt jede Änderung (anlegen, ändern, umbenennen, duplizieren, entfernen) in `fixture-library.whfx` im Benutzerverzeichnis (Linux `~/.local/share/WhiteHoleFX`, Windows `%APPDATA%\WhiteHoleFX`). Neue Projekte beginnen mit diesen Bauformen; der Knopf **Aus Benutzerbibliothek ergänzen** holt fehlende in ein geöffnetes Projekt. Die Datei wird atomar geschrieben; ist sie defekt, bleibt sie unangetastet und es gibt eine Meldung. Die Projektdatei bleibt eigenständig (die Bauformen stehen weiter darin). Neu in `wh_project`: `writeFixtureLibrary`, `parseFixtureLibrary`.

## 0.58.1

- **Ausgabe: Port-Spalte sichtbar.** In der Zieltabelle sind jetzt beide Spalten (Adresse, Port) in der Breite ziehbar und verschiebbar; die Port-Spalte war vorher nur ein schmaler Streifen.
- **Neues Beispiel `examples/einstieg/zwei-ausgaenge.whfx`:** zwei Output-Nodes parallel, jeder mit eigenem Ziel (`target fixture`), unsichtbar addiert. Die bisherigen Beispiele haben nur einen Output im Hauptgraphen (`looks`/`cuelist`: je Look ein eigener Graph mit einem Output), deshalb zeigte keines mehrere Outputs.

## 0.58.0

- **Reiter Test mit Bühne.** Neben den Testbildern (Farbe, Auswahl, Laufbalken, Farbreihenfolge, Lauflicht) zeigt der Reiter Test die ganze Bühne in 3D mit den Farben des Testbilds und lässt es laufen, auch wenn „Ausgabe senden“ aus ist. Ohne Testbild zeigt sie die Anlage.
- Tutorial (alle fünf Sprachen) und Studio-Handbuch beschreiben die vier Reiter Fixtures, Layout, Ausgabe, Test.
- CI: Der Studio-Lauf unter Linux baut mit höchstens 3 Compilern gleichzeitig (der unbegrenzte Lauf wurde bei den vielen neuen Dateien vom Runner abgebrochen, „Exit code 143“).

## 0.57.0

- **Stage Config hat vier Reiter: Fixtures, Layout, Ausgabe, Test.** Die Bühnenansicht liegt jetzt im Reiter Layout neben der Fixture-Liste (die eigene Seite „Ansicht“ und die kleine Ansicht entfallen). Neuer Reiter **Ausgabe** mit Zieltabelle und einer **Patch-Tabelle** (eine Zeile je Fixture: Senden, Protokoll, Ziel, Universe, Kanal, DDP-Offset, Farbreihenfolge, Gamma, Helligkeit). Der Patch steht nicht mehr im Fixture-Formular.
- **Jedes Ziel lässt sich entfernen**, nicht nur das letzte (`Player::removeDestination`). Fixtures auf höheren Zielen rücken mit ihrer Nummer nach; ein noch benutztes Ziel bleibt stehen, die Meldung nennt das Fixture.
- **Spalten der Tabellen sind verschiebbar** (Ziele und Patch); die Reihenfolge der Patch-Tabelle wird gemerkt. Menü Stage Config: Fixtures …, Fixture hinzufügen …, Layout … (F4), Ausgabe …, Testmodus ….

## 0.56.0

- **Fixtures im Layout: Bauform und Drehung.** Im Formular eines Fixtures (Stage Config → Layout) gibt es jetzt **Drehung x y z** in Grad (um die Mitte des Fixtures; Ring, Säule, Matrix, Strip, Cube) und die Auswahl **Bauform** aus der Bibliothek. Fixtures aus einer Bauform zeigen keine eigenen Formfelder mehr (die Form ändert man im Reiter Fixtures für alle Exemplare zugleich); „(eigene Form)“ löst ein Fixture von der Bibliothek, seine Form bleibt. Neuer Knopf **Aus Bauform** legt ein Exemplar samt Patchvorschlag an. Das Verteilen der Exemplare auf der Bühne (Verschieben, Ausrichten, 3D-Ansicht) geht wie bisher.

## 0.55.0

- **Stage Config: neue Seite „Fixtures“ (Bibliothek der Bauformen).** Bauformen anlegen (2D-Matrix, 3D-Matrix, Kreis, Strip, Säule), Maße und Verkabelung einstellen, Vorschau in 3D und ein Test-Knopf, der ein Muster über die Form laufen lässt (Lauflicht, Balken x/y/z, Farbfolge). Ändern wirkt auf alle Exemplare im Layout. Neues Menü Stage Config → Fixtures …; das Fenster ist breiter. Die Seiten Layout (Verteilen und Drehen), Ausgabe und Test werden in 0.56 und 0.57 neu gebaut.

## 0.54.0

- **Datenmodell für das neue Stage-Config-Fenster.** Neue Formen **Ring** (`ring`) und **Säule** (`helix`), **Drehung** von Fixtures um drei Achsen (`rotate x y z`) und eine **Fixture-Bibliothek** im Projekt (`fixturetype "Name"` … `end`, Verwendung mit `type "Name"`). Ändert man eine Bauform, folgen alle Exemplare. Abtastung und Vorschau berücksichtigen die Drehung. Die Bedienung im Studio folgt mit den nächsten Versionen; bestehende Projekte laden unverändert.

## 0.53.0

- **Mausrad ändert keine Werte mehr.** Im ganzen Programm scrollt das Mausrad nur noch die Fläche; Zahlenfelder, Auswahlfelder und Schieber ändern ihren Wert nur durch Klick, Ziehen, Tippen oder Pfeiltasten. Der Rauchtest der CI prüft das.
- **Deutlichere Scrollbalken.** Breiter (16 Pixel), mit abgesetztem Griff und Hervorhebung unter der Maus.
- **„LED-Positionen importieren“ entfernt** (Menü Stage Config und Knopf im Layout). Das Einlesen von Punktwolken aus Projektdateien bleibt; der Import kehrt mit dem neuen Stage-Config-Fenster zurück.

## 0.52.0

- **Skalierung am Kabel.** Eine Verbindung kann ihren Wert umrechnen: `Wert × Faktor + Offset`, danach begrenzt auf den Bereich des Parameters. Im Parameter-Dock erscheint unter jedem gesteuerten Zahlenparameter eine Zeile „Kabel:“ mit Faktor und Offset; „An Bereich anpassen“ spannt einen Wert von 0 bis 1 über den ganzen Bereich (Winkel ohne Bereich: 0 bis 360), ↺ setzt zurück. Gespeichert wird `connect a.out -> b.angle scale 360 offset 0` (nur wenn nicht 1 und 0); Kopieren, Einfügen, Presets und Rückgängig nehmen die Skalierung mit. Ohne Angaben ändert sich nichts.

## 0.51.0

- **Ukrainisch als Sprache.** Oberfläche (alle Menüs, Dialoge und Hinweise) und eingebautes Tutorial gibt es jetzt auch auf Ukrainisch (Hilfe → Sprache / Language → Українська; `--lang uk`). Namen von Nodes, Parametern und Ports bleiben englisch.

## 0.50.1

- Alte Projekte: Die frühere automatische `behind`-Kette zwischen Output-Nodes wird beim Laden entfernt (zur Sicherheit auch beim Übersetzen aufgetrennt). Vorher riss dort ein Output ohne Quelle alle nachfolgenden mit ab.

## 0.50.0

- **Mehrere Outputs laufen echt parallel (unsichtbarer Adder).** Die gespeicherte Verkettung der Output-Nodes (`behind`) entfällt. Stattdessen fasst die Engine beim Übersetzen alle Output-Nodes mit angeschlossener `source` selbst über versteckte Add-Nodes zusammen; im Projekt und im Editor erscheint davon nichts. Nimmt man einer Kette den Generator weg, bleiben die anderen Outputs unberührt; ein frischer Output ohne Effekt bleibt dunkel. Überlappen sich zwei Ziele, addieren sich die Bilder (statt „später gewinnt“). Looks nehmen alle Outputs mit.
- Wird der Ausgang des Projekts gelöscht und war ein Output, rückt er auf einen anderen Output.

## 0.49.1

- Die automatische Verkettung der Output-Nodes wird nicht mehr als Kabel gezeichnet: Mehrere Outputs wirken im Editor unabhängig voneinander, bleiben aber alle aktiv.

## 0.49.0

- **Legende der Ports.** Hilfe → „Legende der Ports …“ öffnet ein schwebendes Fenster mit Farben und Formen aller Porttypen und ihrer Bedeutung (Ausgang gefüllt, Eingang als Ring, Ausgang des Projekts, überbrückt).
- **Mehrere Output-Nodes sind immer alle aktiv.** Bisher lief nur der, an dem der Ausgang hing. Jetzt verkettet das Studio jeden Output mit dem vorigen (`behind`) und hält den Ausgang am letzten; auch beim Laden alter Projekte.
- **Region-Dialog verständlich.** Prozentangaben statt 0 bis 1, Schnellwahl (Links, Rechts, Oben, Unten, Mitte) und eine kleine Bühnenvorschau mit dem Rechteck.
- **Nodes ohne Parameter im Parameter-Dock.** Nodes wie Add fehlten dort bisher; sie erscheinen jetzt mit dem Hinweis „Dieser Node hat keine einstellbaren Parameter“.

## 0.48.0

- **Mehrere Outputs mit Ziel.** Ein Output-Node kann Fixtures, Gruppen von Fixtures oder benannte Regionen der Bühne als Ziel haben (Rechtsklick auf den Output-Node → „Ziel zuweisen“). Sein Rechteck wird aus dem Ziel berechnet und folgt dem Layout (Verschieben, Umbenennen, Rückgängig). So gibt es getrennte Effektketten je Fixture oder Gruppe; LEDs ohne Output bleiben dunkel, bei Überlappung gewinnt der spätere Output.
- Im Layout der Knopf **Gruppen**: Gruppe aus gewählten Fixtures speichern, Region anlegen, beide löschen. Projektdatei: neue Zeilen `group`, `region` und `target` im node-Block.
- Der Import von LED-Positionen heißt jetzt „LED-Positionen importieren (CSV/JSON) …“ (vorher „Punktwolke importieren“).

## 0.47.0

- **Stage Config übersichtlicher.** Zusammengehörige Felder stehen nebeneinander (Spalten/Zeilen, Kabelstart/Richtung, Protokoll/Ziel, Universe/Kanal, Gamma/Helligkeit), Zahlenfelder sind schmal.
- **Verkabelung in der Vorschau.** Die kleine Vorschau neben dem Formular zeigt den Kabelweg mit Pfeilen, die Start-LED und die LED-Nummern; der Schalter „Verkabelung“ blendet alles aus.
- **LED anklicken.** Ein Klick auf eine LED zeigt Fixture, LED-Nummer, Protokoll, Ziel, Universe und Kanäle (z. B. „sACN Ziel 0 · Universe 2 · Kanal 109–111“). Die Rechnung nutzt dieselben Regeln wie die Ausgabe.
- **Fixture-Bibliothek.** Knopf „Vorlagen“ im Layout: ein Fixture als Vorlage speichern (Form, Maße, Verkabelung, Protokoll, Farbreihenfolge, Gamma, Helligkeit; ohne Universe und Kanal), später mit einem Klick neu anlegen, Vorlagen löschen. Die Vorlagen liegen als `.whfx`-Dateien im Ordner `fixtures` im Programmordner des Nutzers.
- CLI: alle 2 s eine Zeitmessung (echte fps, größte Lücke zwischen Frames, Rechenzeit je Frame) zur Fehlersuche bei ruckelnder Ausgabe.

## 0.46.0

- **Absturz- und Stillstandsbericht für Windows.** Bisher gab es den Wächter (#26) nur unter Linux. Jetzt schreibt das Studio unter Windows bei einem Absturz (nicht abgefangene Ausnahme, `abort()`, `std::terminate`) einen Textbericht (Version, Ausnahme, Adresse als Modul+Offset, Aufrufstapel, geladene Module, GPU-Adapter, Windows-Version) und einen Minidump (`.dmp`) nach `%LOCALAPPDATA%\WhiteHoleFX\cache\crash\`. Beim nächsten Start weist ein Hinweis darauf hin, mit Knopf „Ordner öffnen“. Steht der Hauptthread länger als 5 Sekunden, entsteht ebenfalls ein Bericht (`stall-…`), das Programm läuft weiter. `--crash-test` stürzt absichtlich ab (Selbsttest der CI). Die CI legt die `.pdb` als Artefakt „Symbole“ ab, damit sich Adressen später in Funktionsnamen auflösen lassen.

## 0.45.0

- **Audio: Kick, Snare und Hi-Hat getrennt (#121).** Die Analyse hat jetzt je Instrument einen Erkenner (`wh_audio/onset_detector.hpp`): eigener Frequenzbereich, relative Schwelle (auch leise Schläge werden erkannt), Auslösen schon beim Anstieg. Der neue Node **Kick / Snare / Hat** liefert Treffer, Pulse, Stärke und „Age“ (Sekunden seit dem Treffer).
- **Neue Nodes für dunkle, monochrome Treffer:** **Hit Flash** (Fläche, Scheibe, Rahmen oder Balken mit „Level“) und **Shockwave** (Ring, der nach dem Treffer nach außen läuft). Beispiel: `examples/audio/techno.whfx` (Weiß und Rot, sonst schwarz). Phrasen zählt der vorhandene Sequencer (Rezept in `docs/audio.md`).

## 0.44.0

- **Layout-Editor: Raster und Einrasten.** Unter den Anordnen-Knöpfen im Layout gibt es „Raster“ (Linien in der 2D-Vorschau, jede zehnte kräftiger), „Einrasten“ und die Rasterweite (0,1 bis 10 Layout-Einheiten). Beim Ziehen rastet die linke untere Ecke der gezogenen Fixtures am Raster ein, bei mehreren die der Gesamtbox; mit gedrückter Alt-Taste wird ohne Einrasten gezogen. Bei kleinem Zoom wird das Raster automatisch gröber. Die Einstellungen werden zwischen Sitzungen gemerkt. Die Rechnung steht in `preview/grid.hpp` und ist getestet; der Rauchtest prüft Linien und Einrasten.

## 0.43.0

- **Fixtures nebeneinander reihen:** „Verteilen“ im Layout ist ersetzt durch „Nebeneinander ↔“ und „Übereinander ↕“. Die gewählten Fixtures werden in der Reihenfolge ihrer Lage dicht aneinandergesetzt, das erste bleibt stehen; ein Feld „Abstand“ erlaubt eine Lücke. Es genügen zwei Fixtures, und es kann sich nichts mehr überlappen. Kern: `Player::packFixtures` (ersetzt `distributeFixtures`), getestet.
- CI: Der GCC-Job mit ASan baut mit 3 Prozessen, weil der Runner beim Bauen wiederholt beendet wurde.

## 0.42.0

- **Einrasten: Platzierung ohne Überlappung:** Ein in ein Kabel eingefügter Node sitzt jetzt mittig zwischen Quelle und Ziel; fehlt der Platz, rücken das Ziel und alles, was dahinter hängt, nach rechts. Links vor einem Zielnode steht der neue Node direkt davor, rechts dahinter direkt dahinter (auch hier machen Nachbarn bei Bedarf Platz). Das Verschieben gehört zum selben Schritt Rückgängig. Die Rechnung steht in `graph/node_placement.hpp` und ist getestet; der Rauchtest prüft, dass sich nach dem Einfügen kein Node überdeckt.
- **Space auf Auto:** Ripples, Metaballs, Starfield, Fading Pixels und Expanding Shapes haben wie Sphere und Box die Auswahl „Auto (by layout)“ als Standard: Auf flachen Matrizen rechnen sie in 2D, bei einem räumlichen Aufbau (LEDs mit Tiefe) in 3D. Gespeicherte Projekte mit gesetztem „Space“ bleiben unverändert; nur Nodes, die den Standard nutzten, folgen jetzt dem Aufbau.

## 0.41.0

- **#30 Info-Zeichen:** Jeder Node hat im Kopf ein kleines „i“. Mit der Maus darüber erscheint eine Beschreibung des Node-Typs als Tooltip, ein Klick zeigt sie sofort. Dieselbe Beschreibung steht im Tooltip der Palette. Die 69 Texte gibt es auf Deutsch und Englisch (Französisch und Spanisch zeigen vorerst den englischen Text). Der Rauchtest (`--exercise`) meldet jeden Node-Typ ohne Beschreibung, neue Typen brauchen also einen Eintrag in `node_info_texts.cpp`.

## 0.40.1

- **#32 Scale-Node:** Der Regler für „Factor“ und „Factor Z“ reichte von 0,01 bis 100 und war damit unbrauchbar (der sinnvolle Bereich lag auf den ersten Pixeln). Jetzt 0,1 bis 5 (10-fach verkleinern bis 5-fach vergrößern). Ältere Projekte mit Werten außerhalb werden auf den Bereich begrenzt.

## 0.40.0

- **Einrasten beim Ziehen aus der Palette:** Zieht man einen Node auf ein **Kabel**, wird er dazwischen eingefügt (A → neu → B). Zieht man ihn auf einen **Node**, speist er in der linken Hälfte einen Eingang des Ziels, in der rechten Hälfte hängt er hinter dessen Ausgang. Den Port wählt die Höhe der Maus; bei mehreren Eingängen gilt der erste freie des passenden Typs. Ein belegter Eingang bekommt den Node dazwischen. Vor dem Loslassen zeigt der Editor das Ziel hellblau (Rahmen, Kabel, Port) und die neuen Kabel gestrichelt. **Alt** gedrückt halten schaltet das Einrasten ab; Esc bricht den Drag ab. Alles ist ein Schritt Rückgängig. Der Ausgangs-Node rastet nirgends ein. Die Regeln stehen in `graph/node_docking.hpp` (`planDocking`) und sind getestet; `Player::addNodeDocked` führt sie in einem Schritt aus. Pfeiltasten zum Umschalten von Port und Seite folgen später (ein Qt-Drag reicht Tasten nicht durch).

## 0.39.0

- **Palette farbig:** Vor jedem Node in der Palette steht ein Punkt in der Farbe dessen, was er liefert (Bild blau, Wert grün, Farbe rosa, Audio himmelblau, sonst grau). Ansicht → „Palette farbig“ schaltet ihn ab.
- **Passende Nodes hervorheben:** Beim Ziehen eines Kabels aus einem Port werden in der Palette die Nodes abgedunkelt, die sich nicht anschließen lassen. Die Regeln stehen in `graph/port_family.hpp` und sind getestet.

## 0.38.1

- **Scalar → Integer:** Zahlen-Ausgänge (LFO, Math, Map …) lassen sich jetzt auch an ganzzahlige Eingänge und Auswahlfelder anschließen (türkise Rauten, z. B. Mode, Strands, Count). Der Wert wird gerundet und auf den Bereich des Parameters begrenzt; Werte, die keine Zahl sind, bleiben wirkungslos.

## 0.38.0

- **Node überbrücken (Bypass):** Field-Nodes haben im Kopf einen Schalter (Kreis mit Strich), dazu Taste B und Rechtsklick → „Überbrücken“. Der Node bleibt im Graphen, reicht aber seinen ersten Field-Eingang unverändert durch (Generatoren ohne Eingang: transparent). Überbrückte Nodes erscheinen abgeblendet mit gestrichelter Durchleitung. Mehrere ausgewählte Nodes lassen sich zugleich schalten, ein Schritt Rückgängig. Gespeichert wird es als Zeile `bypass` im node-Block der Projektdatei.
- **Kategorien:** „Modifier“ und „Transform“ sind jetzt eine Kategorie „Effects“ (nur die Anzeige im Palettenfenster; Node-Typen und gespeicherte Projekte bleiben unverändert).

## 0.37.8

- **#29:** Ein per Doppelklick (oder Suche, Menü, Palette) angelegter Node ist danach der einzige ausgewählte. Vorher blieben die bisher gewählten Nodes gewählt, und der nächste Zug mit der Maus verschob alle mit. Mit Prüfung im Rauchtest.
- **#27:** Das Studio merkt sich Fenstergröße, Lage und die Anordnung der Docks und Leisten und stellt sie beim nächsten Start wieder her. Ansicht → „Fensteranordnung zurücksetzen“ vergisst sie wieder. Im Live-Modus wird nur die Geometrie gemerkt.
- Intern: Node-Überbrückung (Bypass) im Kern, noch ohne Schalter im Editor (Zeile `bypass` in der Projektdatei).

## 0.37.7

- **Fix:** Node-Editor: Das Mausrad blockierte komplett, wenn die Ansicht außerhalb von 0,25..3-fach lag (etwa nach „Alles zeigen“ bei vielen Nodes). Jetzt zoomt es immer in Richtung dieses Bereichs. Neuer Zoom-Test im Rauchtest (`--exercise`).

## 0.37.6

- **Fehlersuche #26:** Der Wächter nimmt bei einem Stillstand nach einer Sekunde eine zweite Stapelprobe und zeigt, welche Qt-Ereignisse (Klasse und Typ) in dieser Zeit am häufigsten waren. Der erste Stapel aus dem Freeze zeigte eine Ereignisschleife beim Ein-/Ausblenden von Widgets; so lässt sich das betroffene Widget benennen.

## 0.37.5

- **Fehlersuche #26 (Freeze/Absturz):** Das Studio hat unter Linux einen Wächter. Steht die Hauptschleife länger als 2 s, gibt er den Aufrufstapel des Hauptthreads aus (stderr und `~/.cache/WhiteHoleFX/stack.txt`); dasselbe bei einem Absturz (SIGSEGV, SIGABRT …). Abschalten mit `WHITEHOLEFX_NO_WATCHDOG=1`. Selbsttest: `whitehole_studio --hang-test` (läuft auch in der CI).

## 0.37.4

- **Fix:** Sphere und Box zeigten auf einer 2D-Matrix nichts (dort ist `p.z = 0`, die Körper lagen bei `cz = 0.5`). Neuer Parameter `space` mit *Flat*, *Volume* und *Auto* (Standard): Auto wählt selbst — flache LED-Karte → Kreis/Rechteck, räumliche → Kugel/Quader.
- Intern: neuer Shader-Platzhalter `@volume` (aus `WhFrame.volume`, gesetzt von der Sampling-Stage, sobald eine LED z > 0 hat).

## 0.37.3

- **Build:** 0.37.2 ließ sich auf Ubuntu 22.04 nicht bauen (fehlendes `#include <QAction>` in `main_window_exercise.cpp`).

## 0.37.2

- **Strg+N gleich nach dem Start (#26):** Neue Prüfung `whitehole_studio --ctrl-n` löst „Neu“ ohne geladenes Projekt aus, wie ein
  Tastendruck direkt nach dem Start. Die CI führt es (auch mit Wiedergabe) aus und zeigt bei einem Absturz den Aufrufstapel.

## 0.37.1

- **Fehlersuche Einfrieren (#26):** Im Terminal meldet das Studio jetzt, wenn die Hauptschleife länger als 0,5 s stand, ein Frame
  länger als 0,25 s dauerte (getrennt nach Player und Bild/Panels) oder der Grafiktreiber länger als 0,3 s für einen Shader brauchte.

## 0.37.0

- **Fehlende Eingänge (#25):** Bei Blend, Mix, Crossfade, Add und Mask gilt ein unverbundener Eingang jetzt als durchsichtiges
  Schwarz statt die Ausgabe zu sperren (bei Blend/Over bleibt A sichtbar, wenn B fehlt). Bearbeiten → Strenge Eingänge schaltet
  das alte Verhalten wieder ein (`settings ... strict-inputs on`, ein Rückgängig-Schritt). Nodes mit nur einem Eingang bleiben streng.
  Ältere Programme lehnen Dateien mit `strict-inputs` ab.

## 0.36.5

- **Einfrieren (#26):** Der GPU-Start ist wieder genau wie vor 0.36.2: Ohne `WHITEHOLEFX_GPU_BACKEND` bekommt die Bibliothek keinen
  Instanz-Deskriptor mehr und wählt ihre Voreinstellung. 0.36.2 hatte immer einen (leeren) Deskriptor übergeben; das ist
  der einzige Unterschied beim GPU-Start zwischen 0.36.0 und 0.36.3, und das Studio fror erst danach ein.

## 0.36.4

- **Messvarianten für den Text-Node (#13):** `WHITEHOLEFX_TEXT_VARIANT` kennt jetzt zusätzlich `s` (feste Startposition), `g`
  (jedes Zeichen ein „A“), `h` (jeder Schriftpunkt gesetzt), `k` (ganzes Bild in der Textfarbe) und `q` (ganzes Bild rot).
  Auf einer GTX 760 (Vulkan) zeigten die Umschreibungen a, b und c keine Wirkung; so wird sichtbar, welche Stufe versagt.

## 0.36.3

- **Fehlersuche Text auf NVIDIA (#13):** Der Text-Node läuft auf Software-Vulkan und OpenGL, bleibt aber auf einer GeForce GTX 760
  (Vulkan) unsichtbar. Mit `WHITEHOLEFX_TEXT_VARIANT` (`a`, `b`, `c` oder kombiniert, zum Beispiel `abc`) lassen sich drei
  gleichwertige Umschreibungen des Shaders einschalten, um die Stelle zu finden, die der Treiber falsch übersetzt
  (siehe `builtin_text_variants.hpp`). `whitehole_cli --snapshot` zeigt jetzt die Zahl heller LEDs.

## 0.36.2

- **Grafikschnittstelle wählbar (#13):** Mit der Umgebungsvariable `WHITEHOLEFX_GPU_BACKEND` (`vulkan`, `gl`, `dx12`, `metal`)
  lässt sich die Schnittstelle erzwingen, zum Beispiel `WHITEHOLEFX_GPU_BACKEND=vulkan whitehole_studio`. Der Adapter-Name
  zeigt jetzt auch die Schnittstelle. Die CI führt die Text-Prüfung zusätzlich auf OpenGL aus.

## 0.36.1

- **GPU-Adapter sichtbar (#13):** Studio (Hilfe → Über und Terminal) und `whitehole_cli` zeigen, auf welchem Adapter gerechnet
  wird. Bei Fehlern, die nur auf echter Hardware auftreten (Schrift fehlt), ist das der erste Hinweis.

## 0.36.0

- **Random Pixels (#24):** Neuer Generator `wh.generator.randompixels`: leuchtet zufällige Pixel auf, ein Feld je LED
  (`columns` x `rows`, Standard 32 x 16), Dichte, Takt, Blink oder Fade, Farbe zwischen A und B.
- **Text (#13):** Die CI prüft jetzt das fertige Fensterbild der Laufschrift (`scripts/check_text_pixels.py`), nicht nur
  die Session. So zeigt sich, ob die Schrift auf dem Weg Player, Vorschau, Zeichnen verloren geht.

## 0.35.0

- **Cuelist (#21):** Looks in fester Reihenfolge mit Auslöser (GO, Zeit, Taste, DMX, Audio-Beat), Delay und Überblendzeit.
  Neues Dock Ansicht → Cuelist, `cue`-Zeilen in der Projektdatei, Beispiel `examples/einstieg/cuelist.whfx`.
  Hinweis: Ältere Programme lehnen Projekte mit Cues ab.
- **Build:** Die GPU- und Windows-Tests übersetzen wieder (Hilfsfunktion in `text_session_on_device_test.cpp` hieß wie ein
  Namensraum).

## 0.34.0

- **Master per DMX (#17):** Einstellungen → DMX-Eingang: Ein DMX-Kanal (Art-Net oder sACN, Universe, Kanal) steuert die
  Gesamthelligkeit. Der Master-Regler folgt dem Pult; fällt das Signal 5 Sekunden aus, hält der Master seinen Wert.
- **Farbwahl (#20):** Eine gewählte Farbe wird nicht mehr durchsichtig. Der Dialog startete bei Farben mit Alpha 0 (zum Beispiel
  dem Text-Hintergrund) mit Alpha 0 und gab es zurück. Jetzt startet er sichtbar, und ein Ergebnis mit Alpha 0 wird sichtbar;
  durchsichtig macht der Schalter „Transparent“. Das kann auch die „fehlende“ Schrift im Text-Node erklären (#13): eine so
  gewählte Textfarbe war unsichtbar.
- **Zahlenfelder (#22):** Felder im Parameter-Panel sowie bei Überblendzeit und Testmuster übernehmen den Wert erst bei Enter
  oder beim Verlassen. Vorher schrieb jede Taste den Wert zurück ins Feld, und halb getippte Zahlen („0,“) wurden mitten im
  Tippen überschrieben.
- **Value Noise (#19, #16):** ruhiger. Die höchste Geschwindigkeit sinkt von 8 auf 3 Zellen pro Sekunde, und `scale` beginnt bei 2
  statt 1 Zelle über die Leinwand, damit nicht die ganze Wand eine einzige Zelle ist und gemeinsam aufleuchtet.
- **Text-Node (#13):** Neue Prüfung auf dem GPU-Gerät, die das ganze Projekt (Layout 32 × 16, Standardwerte, Laufschrift über die
  Zeit) bis zu den LED-Farben fährt (`text_session_on_device_test.cpp`).

## 0.33.0

- **Schnellere 3D-Vorschau:** Der Schein (Glow) rechnet nur noch innerhalb seines Hofs (Vergleich der Abstandsquadrate statt Wurzel für
  jeden Pixel), überspringt schwarze LEDs und rundet ohne `lround`. Gemessen: etwa 2,5- bis 3-mal schneller bei 512 bis 14 000 LEDs, Bild unverändert.
- **Node-Verzeichnis im Terminal:** `whitehole_cli --list-nodes` und `--describe-node <typ>` zeigen Typen, Ports und Parameter
  (mit Bereich, Standardwert und der Bedeutung von Auswahlzahlen). Siehe [Projektdatei](docs/project-format.md).
- **Fehlersuche:** Neue Fuzz-Tests (zufällig veränderte Projektdateien, CSV-/JSON-Import, zufällige Editor-Aktionen mit
  Rückgängig/Wiederholen) und ein Benchmark `wh_bench` (`-DWHITEHOLEFX_BUILD_BENCH=ON`). Siehe [BUILDING.md](BUILDING.md).

## 0.32.0

- **Sprachen:** Die Oberfläche gibt es auf Deutsch, Englisch, Französisch und Spanisch. Umschalten unter *Hilfe → Sprache / Language*
  (der Titel ist absichtlich zweisprachig); das Programm bietet den nötigen Neustart an und öffnet dasselbe Projekt wieder.
  Ohne Wahl gilt die Sprache des Systems (sonst Englisch). `whitehole_studio --lang fr` überstimmt die Wahl. Die Kataloge sind
  gettext-Dateien (`apps/whitehole_studio/i18n/*.po`), gepflegt mit `scripts/i18n.py`; neue Texte sind dort sofort sichtbar und
  die CI prüft, dass nichts fehlt und die Platzhalter (%1) stimmen. Siehe [Sprachen und Übersetzen](docs/translating.md).
- **Tutorial:** Eingebaut unter *Hilfe → Tutorial*, in allen vier Sprachen (`docs/tutorial/`).
- **Fünf 3D-Beispiele** unter *Datei → Beispiele → 3D*: Ausrichtung prüfen, Plasma im Würfel, Scheibe durch den Cube, Regentropfen
  als Kugelschalen, Säule mit gewickeltem Strip. Die Beispiele liegen jetzt in Untergruppen (Einstieg, Effekte, Audio, 3D,
  Daten); das Menü zeigt sie als Untermenüs. Siehe [Beispielprojekte](docs/examples.md).

## 0.31.0

- **Port-Farben und -Formen:** Die zwölf Port-Typen haben neue, über den Farbkreis verteilte Farben. Dazu kommt eine Form
  (Kreis, Raute, Quadrat, Dreieck je Typfamilie), und **Ausgänge sind gefüllt, Eingänge nur ein Ring**. So lassen sich auch ähnliche
  Typen und Ein-/Ausgänge auf einen Blick auseinanderhalten.
- **Node-Palette:** Kategorien sind beim Start zugeklappt. Rechtsklick auf einen Node-Typ → *Zu Favoriten hinzufügen*; Favoriten stehen
  oben in der Gruppe „Favoriten“ (und mit Stern in ihrer Kategorie) und werden gespeichert. Beim Suchen sind die Treffer aufgeklappt.
- **Löschen überbrückt:** Wird ein Node aus einer Kette gelöscht (A → X → B), verbindet der Player A direkt mit B, statt die Kette
  zu zerreißen. Der Node gibt dafür seinen ersten belegten Eingang gleichen Typs an die Ziele seines Ausgangs weiter. Rückgängig stellt
  alles in einem Schritt wieder her.
- **Kein „Hold Last Look“ mehr bei fehlendem Eingang:** Fehlt auf dem Weg zum Ausgang ein Pflicht-Eingang (Kabel gelöscht oder
  getrennt), gehen die LEDs aus, statt das letzte Bild weiter zu zeigen. Sobald der Graph wieder vollständig ist, läuft der Effekt weiter.
  Bei anderen Fehlern (z. B. eine fehlerhafte Eingabe) bleibt das letzte gute Bild wie bisher.

## 0.30.0

- **3D-Vorschau lesbar:** Ausgeschaltete LEDs erschienen als graue Scheiben und verdeckten im Cube die leuchtenden dahinter.
  Neu im Menü Ansicht → 3D-Darstellung: *Aus-LEDs unsichtbar* (Vorgabe in 3D), *schwach* oder *grau* (wie 2D), dazu *Leuchten
  (additiv)*: leuchtende LEDs mit weichem Schein, Farben addieren sich, nichts verdeckt etwas. Die Wahl wird gespeichert und bleibt
  beim Wechsel zwischen den 3D-Ansichten. Die 2D-Ansicht ist unverändert.
- **Bühnenansicht im Stage Config:** Neue Seite „Ansicht“ mit der Anlage im Raum (drehbar, Front/Seite/Oben): eigener Farbton je Fixture,
  heller entlang des Kabels, **erste LED weiß**, Rahmen und Achsenkreuz (x rot, y grün, z blau), Auswahl hervorgehoben. Dazu eine kleine
  Ansicht oben im Tab „Fixtures“, die sich bei jedem Wert sofort ändert.

## 0.29.1

- **Farbauswahl erschien nicht (#18):** Unter Linux öffnete sich der Farbdialog bei manchen Fenstermanagern hinter dem
  Hauptfenster, unsichtbar, aber mit dem Fokus (sichtbar erst nach dem Verschieben des Fensters). Der Dialog hängt jetzt
  am Hauptfenster, wird von Qt selbst gezeichnet (nicht der native Dialog) und ausdrücklich nach vorn geholt.

## 0.29.0

- **3D-Nodes:** Die Nodes kennen jetzt die Tiefe. Alle neuen Regler stehen am Ende des Parameterfensters und sind so voreingestellt,
  dass 2D-Bilder und alte Projekte unverändert aussehen.
  - Linear Gradient und Scan Line: `elevation` kippt die Richtung zur z-Achse (Scan Line wird zur Scheibe durch den Würfel).
  - Plasma, Fire: `depth` legt die Wellen beziehungsweise das Rauschen durch z.
  - Ripples, Metaballs, Starfield, Fading Pixels, Expanding Shapes: Auswahlfeld `space` (Flat oder Volume); Volume rechnet
    Kugelschalen, Würfelzellen und z-Bahnen. Expanding Shapes hat dazu `cz`. Sine Lines: `frequencyZ`.
  - Scale (`factorZ`), Slide (`z`), Tile (`countZ`), Pixelate (`cellsZ`), Region (`zStart`, `zSize`) wirken auch in der Tiefe.
  - **Neu:** Box (Quader), Helix (Schraube), Position Test (x/y/z als Rot/Grün/Blau, zum Prüfen von Verkabelung und Startecke)
    und Twist (verdreht den Raum um eine Achse).
  - Neuer GPU-Test, der einen 3×2×4-Würfel abtastet und jede LED mit einer Rechnung auf der CPU vergleicht.

## 0.28.0

- **Menü „Stage Config“:** Die Bühne wird einmal am Anfang eingerichtet und danach selten angefasst, darum hat sie ein eigenes
  Menü (zwischen Bearbeiten und Ansicht): Fixture hinzufügen …, Punktwolke importieren …, Layout und Patch … (F4), Testmodus ….
  Das Fenster heißt jetzt „Stage Config“ (vorher „Stage Setup“, in Ansicht). Im Live-Modus ist das Menü gesperrt.
- **Dialog „Fixture hinzufügen“:** Statt „Matrix +“ und „Strip +“ auf Knopfdruck fragt ein Dialog alles in einem Schritt:
  Form (Matrix, Strip, 3D-Gitter), Anzahl LEDs in x y z, Abstand, Position, Verkabelung (Zählrichtung, Serpentine, Startecke)
  und Patch (automatisch, manuell, nicht senden). Eine Zeile zeigt LED-Zahl, Größe und Kanäle schon vor dem Anlegen.
- **Startecke bei 3D-Gittern:** `grid` kennt `flipx`, `flipy`, `flipz` (das Kabel beginnt am oberen Ende der Achse). Bisher
  begann der Strang immer links unten hinten. Alte Projekte laden unverändert. Das Formular des Fixtures hat dieselben Schalter.

## 0.27.0

- **Menüleiste aufgeräumt:** Die Menüs stehen in Betriebsreihenfolge: Datei, Bearbeiten, Ansicht, Wiedergabe, Live, Hilfe
  (Wiedergabe steht jetzt vor Live). Datei ist durch Trennlinien in Anlegen/Öffnen, Speichern, Neu laden und Beenden gegliedert;
  Blackout steht in Wiedergabe abgesetzt von Start und Senden.
- **Neues Menü Hilfe:** Dokumentation (F1), Fehler melden oder Idee vorschlagen (öffnet ein neues GitHub-Issue), Tastenkürzel
  (Liste, die aus den Aktionen des Fensters gelesen wird und deshalb nie vom Menü abweicht) und Über WhiteHoleFX (Version, Lizenz).
- **Ausgabe senden leuchtet rot**, solange wirklich Pakete ins Netz gehen.
- Tooltips der Schaltflächen nennen jetzt ihr Tastenkürzel.

## 0.26.0

- **Node-Standardwerte:** Neue Seite „Node-Standardwerte“ in den Einstellungen. Zu jedem Node-Typ lassen sich eigene Startwerte
  festlegen (z. B. Value Noise mit Dichte 0,3); neue Nodes starten damit statt mit dem Wert der Bibliothek. Links die Typen nach
  Kategorie mit Suchfeld (Typen mit eigenen Werten fett), rechts die Parameter mit denselben Bedienelementen wie im
  Parameterfenster; „↺“ stellt eine Zeile zurück, ein Wert gleich dem Bibliothekswert zählt nicht als eigener. Bestehende Nodes
  und geladene Projekte ändern sich nicht; die Werte gehören dem Nutzer (nicht dem Projekt) und werden gespeichert.
- Kern ohne Qt: `runtime::NodeDefaults` mit Text-Kodierung der Werte, `Player::setNodeDefaults`; unpassende Werte (falscher Typ,
  unbekannter Parameter) werden beim Anlegen übersprungen, Bereiche begrenzt der Graph.

## 0.25.0

- **Einstellungen:** Neuer Menüpunkt Bearbeiten → Einstellungen … (Strg+Komma). Das Menü „Eingang“ entfällt, seine Schalter
  wandern in den Dialog: Seite **Netzwerk** (Karte zum Senden und zum Empfangen), Seite **DMX-Eingang** (Art-Net und sACN
  einzeln, Ports) und Seite **Audio** (Aus, Standardgerät, Gerät, WAV-Datei). Die Wahl wird gespeichert; die bisherigen
  Schlüssel (`dmx/artnet`, `dmx/sacn`, `audio/source`) bleiben gültig.
- **Netzwerkkarte wählen:** `UdpSender::bindInterface` bindet das Senden an die Adresse einer Karte (Absender, Broadcast, Multicast),
  `DmxReceiverConfig::interfaceAddress` bestimmt die Karte für sACN-Multicast. Eine Karte, die es nicht gibt, wird abgelehnt
  und die bisherige Wahl bleibt bestehen.

## 0.24.0

- **Stage Setup als eigenes Fenster:** Layout (Fixtures, Patch, Ausgabe-Ziele) und Testmodus liegen nicht mehr als Docks
  rechts, sondern in einem Fenster, das sich über Ansicht > Stage Setup oder F4 öffnet. Es ist nicht modal, lässt sich auf einen
  zweiten Bildschirm ziehen und merkt sich Größe und Platz. Im Live-Modus wird es geschlossen und danach wieder geöffnet, wenn
  es offen war. Rechts bleibt nur das Dock „Parameter“, die Vorschau bekommt mehr Platz.

## 0.23.1

- **Value Noise überarbeitet (Issue #16):** Alle Regler gehen jetzt von 0 bis 1, damit ein LFO sie direkt fahren kann.
  Neu: `density` (Anteil der leuchtenden Pixel: 0 alles aus, 1 alles an, 0,5 die Hälfte, 0,25 ein Viertel; der Rauschwert wird dafür
  in seinen Rang umgerechnet, damit der Anteil wirklich stimmt), `softness` (harte Kante bis weich) sowie `colorA` (aus) und
  `colorB` (an) als Farbeingänge. **Achtung:** `scale` (vorher 0,01–256) und `speed` (vorher −20…20) haben neue Bereiche; ältere
  Projekte laden weiter, ihre Werte werden aber auf 0…1 begrenzt und sehen anders aus (Scale 8 war etwa 0,33, Speed 0,5 etwa 0,25).
  Die Rückwärtsrichtung von `speed` entfällt. Der Anteil stimmt in jeder Zeitphase auf etwa 2 % genau.

## 0.23.0

- **3D-Vorschau:** Die Vorschau lässt sich auf eine frei drehbare 3D-Ansicht umschalten (Leiste „Ansicht“, Strg+1…5):
  ziehen = drehen, Rechtsklick oder Umschalt+Ziehen = verschieben, Rad = zoomen, Doppelklick = zurücksetzen. Dazu feste
  Ansichten Front, Seite und Oben (maßhaltig). Die Ansicht kennt keine Fixture-Formen, nur LED-Positionen, und ist darum für
  Matrizen, Strips, Cubes und hängende Installationen aus vielen Strips gleichermaßen geeignet. Bearbeitet wird weiter in „2D“.
- Kern: `preview::Camera3d`, `Projector` und `renderPreview3d` (Orbit-Kamera, Perspektive oder Parallelprojektion, Tiefensortierung,
  Tiefenhinweis, Rahmen) in `wh_preview`, `Player::image3d`; 2D und 3D teilen sich das Zeichnen der LED-Scheiben.

## 0.22.2

- **Color Ramp (Issue #14):** Neuer Parameter `speed` (Umläufe pro Sekunde, Standard 0,2): Der Node wechselt die Farben von selbst,
  ohne dass ein LFO nötig ist. Die Phase wird aufsummiert und zu `position` addiert; ohne `loop` pendelt er zwischen erster
  und letzter Farbe. `speed` 0 verhält sich wie vorher.
- **Preset und Einfügen (Issue #15):** Ein eingefügter Output-Node (Preset, Kopie, Strg+V) wird jetzt wie ein neu angelegter zum
  Ausgang des Projekts und legt sich über den bisherigen. Vorher blieb er unverbunden, und das Projekt meldete „kein Ausgang“.

## 0.22.1

- **Fehlerbehebung (Issue #11):** Mit zwei Output-Nodes und getrennten Rechtecken gab nur einer Daten aus, weil der zweite
  außerhalb seines Rechtecks durchsichtig war und den ersten damit löschte. Der Output-Node hat jetzt einen optionalen
  Eingang „Behind (rest of canvas)“: Außerhalb des eigenen Rechtecks zeigt er das, was dort anliegt, und innerhalb legt er
  sich über das Dahinterliegende. Ein neu angelegter Output hängt den bisherigen automatisch an `behind`. Bestehende
  Projekte rendern unverändert (ohne Verbindung an `behind` ist es wie bisher durchsichtig).

## 0.22.0

- **Oberfläche aufgeräumt:** Looks-Dock links, Parameter/Layout/Testmodus als Reiter rechts; zwei Leisten („Datei“ und
  „Betrieb“) statt einer überladenen, Master und Blackout rechts außen; Look speichern per Strg+L. Im Live-Modus
  verschwindet die Datei-Leiste, Rückgängig ist gesperrt und die Leertaste ist GO (vorher hätten Start/Stopp und GO
  um die Taste gestritten).
- **Node-Prüfung:** Neue Regeltests für alle eingebauten Nodes (eindeutige Typ-IDs und Namen, Schlüssel in lowerCamelCase,
  Bereiche, Auswahlfelder, Ports); ein neuer Node mit leerem oder doppeltem Namen fällt sofort auf.
- **Messung (Release, ein Kern):** Einen Feldgraphen mit zehn Nodes zu übersetzen dauert rund 34 µs, ein Werte-Graph mit
  25 Nodes kostet rund 23 µs pro Frame. Beides ist gegenüber dem Frame-Budget (25 ms bei 40 fps) vernachlässigbar; Zeit
  kostet allein die GPU und die Ausgabe. Die Messungen laufen mit `wh_tests "[bench]"`.

- **Lizenz:** WhiteHoleFX steht ab dieser Version unter der PolyForm Shield License 1.0.0 (`LICENSE`, `NOTICE`, README,
  Debian-`copyright`). Jede Nutzung ist kostenlos erlaubt, auch auf bezahlten Veranstaltungen; ausgenommen ist, ein
  konkurrierendes Produkt oder einen Dienst daraus zu machen. Versionen bis 0.21.0 bleiben unter Apache-2.0.

## 0.21.0

- **Looks:** Ein fertiger Effekt (Generator, Modifier bis zum Ausgang) lässt sich als Look in einer Liste speichern, mit
  Überblendzeit, und steht mit dem Projekt in der Datei (`look "Name" fade 3` ... `endlook`). Dock „Looks“: speichern,
  überschreiben, in den Editor laden, umbenennen, ordnen, löschen.
- **Live-Modus (F9):** große Look-Knöpfe, Tasten 1 bis 9, Überblendung zwischen zwei Decks, optional „Mit GO“ (vorladen,
  dann Leertaste), eigene Überblendzeit, Fortschrittsbalken; Master und Blackout bleiben erreichbar.
- **Node-Palette:** Fenster mit allen Node-Typen nach Kategorien mit Suche; Nodes per Drag & Drop hinzufügen, durch
  Ziehen auf die Palette entfernen.
- **Leistung:** Änderungen reiner Zahlenwerte (Regler, Überblendung, Value-Nodes) übersetzen den Shader nicht mehr neu
  (`FieldPass::setParameters`); nur Struktur- und Textänderungen bauen die Pipeline neu.
- **Tests:** Parser/Writer-Rundlauf mit Looks, Deck-Überblendung, Player im Live-Betrieb, Parameter-Schnellweg;
  ASan/UBSan ohne Befund im eigenen Code. Neue Rauchtests für Palette und Live-Modus (nur in der CI mit Qt geprüft).

## 0.20.0

- **Neue Werte-Nodes** (Kategorie „Value“): `Math` (Add, Subtract, Multiply, Divide, Min, Max, Power, Modulo, Absolute,
  Quantize, Sine; Fehlerfälle wie Teilen durch 0 liefern 0 statt NaN), `Compare` (Schwelle mit Hysterese, Modus
  Above/Below, Ausgänge `out`, `rise`, `fall`), `Clock` (Tempo × Verhältnis 1/8 bis 8x, `tick`, `phase`, `count`,
  Stopp und Reset; die Phase wird aufsummiert und springt bei Tempowechsel nicht), `Envelope` (ADSR mit linearen
  Flanken, Retrigger vom aktuellen Pegel, Modus One-shot macht aus einem Trigger Anschlag und Ausklingen) und
  `Sequencer` (bis 8 Schritte, Richtung vorwärts/rückwärts/hin und her/zufällig, `value`, `step`, `trigger`, `hit`).
- **Zusammenspiel:** Clock.tick → Sequencer.advance → Envelope.gate → Helligkeit; Beispiel: `examples/takt.whfx`.
- **Intern:** `EdgeDetector` für Flanken, `hashToUnit` jetzt für alle Nodes gemeinsam (builtin_internal.hpp).

## 0.19.0

- **Neue Transformationen:** `Kaleidoscope` (Keile mit Spiegelung, Drehung, Zoom), `Tunnel` (Winkel und Tiefe als
  Koordinaten, Tempo, Drehung mit der Tiefe, Fade in der Mitte) und `Vortex` (Wirbel um einen Mittelpunkt). Alle drei
  haben `cx`, `cy` und `aspect` für Wände, die nicht quadratisch sind.
- **Neuer Modifier `Color Map`:** Helligkeit wird zu einem Farbverlauf aus 2–4 Farben (Falschfarben, Wärmebild), mit
  `shift` und Ringmodus; die Durchsichtigkeit bleibt erhalten.
- **Tests:** alle vier auf einem echten Gerät, Erwartungen vorab im WebGPU-Testaufbau (Chromium) gegengerechnet
  (Spiegelsymmetrie, Wirbelformel, Winkel als x-Koordinate, Palettenstufen).

## 0.18.0

- **Text / Laufschrift:** Neuer Node `Text` (`wh.generator.text`). Eine eingebaute 5×7-Punktschrift mit ASCII und
  ä ö ü Ä Ö Ü ß wird im Shader gezeichnet, ohne Textur. Modus *Scroll* (Tempo in Schriftpunkten pro Sekunde, beide
  Richtungen, Pause vor dem nächsten Durchlauf) oder *Static* (links, mittig, rechts); Höhe, Mitte, Buchstabenabstand,
  Farbe und Hintergrund einstellbar. Beispiel: `examples/laufschrift.whfx`.
- **Textparameter im Shader:** Der Feldcompiler kennt die Platzhalter `@text(schluessel)` und `@textlen(schluessel)`; der
  Text (UTF-8, höchstens 256 Zeichen) wird als Feld von Codepunkten in den Shader geschrieben. Dafür gibt es
  `core/utf8.hpp` (`decodeUtf8`, fehlertolerant). Ein neuer Text baut die Pipeline neu, alle anderen Parameter nicht.
- **Roadmap:** Die Home-Assistant-Integration ist aus der Liste gestrichen.

## 0.17.0

- **Oszilloskop:** Neue Nodes `Audio Waveform` (Kategorie „Audio“) und `Oscilloscope` (Generators). Der Analyse-Frame
  trägt jetzt die letzten 8192 Samples (`AudioFrame::recent`, `sampleRate`); `Audio Waveform` schneidet ein Fenster
  von `span` ms heraus, triggert wie ein Oszilloskop an einem steigenden Nulldurchgang und liefert 32 Punkte,
  `Oscilloscope` zeichnet sie als Linie (mit Glühen und optionaler Füllung). Beispiel: `examples/oszilloskop.whfx`.
- **Neuer Porttyp `AudioWave`:** Die 32 Punkte laufen auf demselben Kanal zur GPU wie das Spektrum (`KernelOutputs::setSpectrum`,
  Slot `Spectrum`, `@spectrum(port, i)`), sind aber ein eigener Typ: Wellenform und Spektrum lassen sich nicht
  vertauschen (`graph::carriesBands`).
- **Tests:** Samples im Frame, Fenster, Trigger, Glättung und Kein-Signal des Waveform-Nodes; Oszilloskop-Linie und
  Füllung auf einem echten Gerät (Erwartungen vorab im WebGPU-Testaufbau gegengerechnet).

## 0.16.0

- **Neue Nodes (Generators):** `Lightning` (Blitze mit Verästelung, Nachschlag und Flash, bis zu drei gleichzeitig)
  und `Particles` (Funken und Konfetti: Wurfparabeln mit Schwerkraft, Schweif, Fächer und Regenbogenpalette).
  Beide sind zustandslos: Zufall kommt aus einem Hash von Takt und Platz, nicht aus einer Partikelliste, das Bild
  ist bei jedem Abspielen gleich. Sie teilen ein Shader-Prelude (`builtin_sparks.cpp`).
- **Beispiele:** `examples/gewitter.whfx` und `examples/funken.whfx`; ein Test lädt und kompiliert sie, damit sie
  nicht still veralten.
- **Tests:** Lightning und Particles auf einem echten Gerät (Blitz hängt oben und endet bei `reach`, Flash,
  ruhendes Teilchen, Richtung, Schwerkraft, Regenbogen). Die Erwartungen sind vorab in einem WebGPU-Testaufbau
  (Chromium mit Software-Renderer) gegengerechnet.

## 0.15.0

- **Audio-Eingang:** Neues Modul `wh_audio` mit FFT, Pegel, 32-Band-Spektrum (40 Hz–16 kHz), Beat-Erkennung
  und Tempo-Schätzung. Quelle ist ein Aufnahmegerät (über miniaudio, wird beim Konfigurieren geladen; abschaltbar
  mit `-DWHITEHOLEFX_AUDIO_CAPTURE=OFF`) oder eine WAV-Datei in Schleife. Studio: Menü *Eingang → Audio*
  (standardmäßig aus, Wahl wird gemerkt, Statuszeile mit Pegel und BPM); Konsole: `--audio-in default|<Gerät>|<datei.wav>`
  und `--audio-devices`; der Prüfbericht nennt Audio-Nodes. Siehe `docs/audio.md`.
- **Neue Nodes (Kategorie „Audio“):** `Audio Level`, `Audio Band`, `Audio Spectrum`, `Beat`, `Tap Tempo` und der
  Effekt `Spectrum Bars` (Equalizer). Beispiel: `examples/audio.whfx`.
- **Spektrum auf der GPU:** Der Porttyp `AudioSpectrum` hat jetzt einen eigenen Weg vom CPU-Node bis in den
  Parameterpuffer (`KernelOutputs::setSpectrum`, `IValueLookup::spectrum`, Slot-Typ `Spectrum` mit 128 Bytes,
  Shader-Platzhalter `@spectrum(port, band)`).
- **Dateipfade:** WAV-Dateien werden auch unter Windows mit Umlauten im Pfad gelesen (UTF-8).

## 0.14.2

- **CI-Reparatur:** Der JSON-Leser kommt ohne `std::pair` mit unvollständigem Typ aus (Clang und
  GCC 11 lehnten ihn ab). Das Studio bindet `test_panel.hpp` in `main_window_file.cpp` ein. Der
  Test „Zuletzt geöffnet“ vergleicht Pfade normalisiert und läuft damit auch unter Windows

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
