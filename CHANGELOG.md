# Änderungen

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
