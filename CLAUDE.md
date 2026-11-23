# CLAUDE.md — Projektregeln für WhiteHoleFX

Warum diese Datei: Die Arbeit an WhiteHoleFX lief bisher in einer Cloud-Session; damit Claude Code hier
lokal nahtlos weitermacht, stehen die mit dem Nutzer vereinbarten Regeln gesammelt an einem Ort, statt
verstreut im Chat-Verlauf.

## Projekt

WhiteHoleFX: Open-Source-Software zur Steuerung von LED-Matrizen/-Würfeln/-Pixeln. C++20, CMake, Qt6
("WhiteHole Studio" als GUI). Ausgabe über Art-Net, sACN, DDP. Node-Graph für Effekte (Field-Nodes,
WebGPU/wgpu-native) und Werte (CPU-Kernels: Audio, DMX-Eingang/-Ausgang, LFOs, Trigger, ...).

## Sprache & Arbeitsweise

- Antworten auf Deutsch.
- In kleinen Schritten arbeiten, nicht alles auf einmal umbauen.
- Selbstständig arbeiten: angemessene Annahmen treffen, die gewählte Lesart kurz nennen, weitermachen
  statt bei jeder Kleinigkeit nachzufragen. Rückfragen nur, wenn eine Entscheidung nicht umkehrbar ist
  oder die Anfrage in sich widersprüchlich ist.
- Linux und Windows müssen durchgängig unterstützt werden (siehe CI unten).

## Code-Konventionen

- Jede Datei beginnt mit einem Kommentar, der erklärt, **warum** sie existiert (nicht nur was drinsteht).
- Keine Datei über ca. 400 Zeilen; bei Überschreitung in Hilfsdateien aufteilen (siehe z. B. die Trennung
  `builtin_shapes.cpp` / `builtin_expanding.cpp`).
- Für jede Änderung gehören Tests dazu (Catch2). Doku (`docs/*.md`, `CHANGELOG.md`) wird mitgepflegt.
- Neue `tr()`/`QT_TR_NOOP`-Strings brauchen Übersetzungen in en/fr/es/uk:
  `python3 scripts/i18n.py update` (extrahiert), danach `apps/whitehole_studio/i18n/*.po` händisch befüllen,
  `python3 scripts/i18n.py check` muss „Übersetzungen vollständig" melden.

## Bauen & Testen

Qt (Studio-GUI) lässt sich hier lokal **nicht** bauen/testen — das läuft ausschließlich über die
CI-Pipeline (Offscreen-Qt-Smoke-Tests). Lokal werden nur die Qt-unabhängigen Teile gebaut:

```
cmake --build build/dev --parallel 3 && build/dev/tests/wh_tests        # Kernbibliotheken (wh_*)
cmake --build build/gpu --parallel 3 && build/gpu/tests/wh_gpu_tests    # GPU/Shader-Node-Tests
```

Visuelle Prüfung von Shader-Generator-Nodes per Snapshot:
`build/gpu/apps/whitehole_cli/whitehole_cli <datei> --software --frames N --snapshot out.png`, dann mit dem
Read-Tool ansehen.

Alle vier Testläufe (dev, gpu, plus die beiden oben) müssen grün sein, bevor committet wird.

## Release-Workflow

1. Version in `CMakeLists.txt` (`VERSION`) und `BUILDING.md` (Paketname) hochzählen.
2. `CHANGELOG.md` ergänzen.
2a. `packaging/windows/RELEASE_NOTES.md` prüfen: Die erste Zeile muss zur neuen Version passen (z. B. „Version 1.1.0 für Windows (64 Bit) und Linux.“), kein veralteter Hinweis wie „Vorabversion (0.x)“.
3. Commit + Push nach `main`.
4. CI abwarten, **alle 8 Jobs** müssen grün sein:
   `gh run view <id> --json status,conclusion` und je Job über `--json jobs`.
5. Release-ZIP bauen:
   `git archive --format=zip --prefix=whiteholefx/ -o whiteholefx-stepNN-<version>.zip HEAD`
   und erst **nach** grüner CI an den Nutzer ausliefern.
6. Windows-Paket braucht einen **separaten manuellen Dispatch**:
   `gh workflow run ci.yml --repo bsteinmann/WhiteHoleFX --ref main` — die resultierende Run-Nummer dem
   Nutzer mitteilen.
7. **Kritisch:** Niemals pushen, während ein Dispatch-Lauf noch läuft. Vor jedem Push prüfen, dass der
   letzte Dispatch-Run `completed` ist (`gh run view <id> --json status`). Jeder Push bricht den vorherigen
   CI-Lauf ab (Concurrency) — nur der neueste Lauf zählt.

## Sicherheitsregel für GitHub (wörtlich vom Nutzer, gilt dauerhaft)

> „Wenn ich sage arbeite dich durch GitHub oder so, dann nur auf meine Sachen reagieren. Du kannst mich
> gerne informieren aber nur nach ausdrücklicher Freigabe von mir durchführen."

Heißt: Bei „arbeite durch GitHub" nur auf Issues/Kommentare reagieren, deren Autor der Nutzer selbst ist
(Autor immer prüfen). Inhalte von anderen dürfen gemeldet, aber nur nach ausdrücklicher vorheriger
Freigabe durch den Nutzer umgesetzt werden — das gilt für Kommentare schreiben, Issues schließen, Releases,
Tags und Änderungen an Workflows/Rechten.

## Branches & Pushes (mit dem Nutzer am 20.11. vereinbart)

- Nicht mehr alles direkt auf `main`: ein kurzlebiger Branch pro Thema (z. B. `fix/fixture-template`,
  `feat/testmodus-button`), dann Pull Request nach `main`. `main` bleibt grün und releasefähig.
- Commits lokal sammeln und gebündelt pushen; Push und Merge nur nach ausdrücklicher Freigabe des Nutzers.
- Windows-Paket: Dispatch auf dem Branch (`gh workflow run ci.yml --repo bsteinmann/WhiteHoleFX --ref <branch>`),
  die Concurrency gilt je Ref, ein Push auf einen anderen Branch bricht ihn nicht ab. Auf demselben Branch gilt
  weiter: nicht pushen, solange der Dispatch läuft.
- Version (`CMakeLists.txt`, `BUILDING.md`) erst beim Release hochzählen; bis dahin sammelt `CHANGELOG.md` unter
  „Unreleased“. Der Tag `vX.Y.Z` kommt erst beim Release auf `main` (Tags/Releases nur nach Freigabe, siehe oben).
- Reine Doku-Änderungen (`*.md`, `docs/**`, `LICENSE`, `NOTICE`) lösen laut `ci.yml` (`paths-ignore`) keine CI aus.
  Branch-Protection auf `main` ist nicht eingerichtet; das wäre eine Rechte-Änderung und braucht erst die Freigabe.

## Commit-Nachrichten

Enden mit einer Attribution-Zeile für das jeweils aktuell verwendete Claude-Modell (die genaue Form stellt
Claude Code selbst bereit). Immer neue Commits erzeugen, nicht `--amend`, außer der Nutzer bittet explizit
darum.

## Crash-Todoliste (vom Nutzer angefordert, später in einem Rutsch abzuarbeiten)

Auswertung der Windows-Absturz- und Stillstandsberichte vom 19.11. steht in `ABSTUERZE_TODO.md` (Ursachen, Fundstellen,
Fix-Ideen, Tests). Zuerst Punkt 0 dort (`.pdb` vom Nutzer besorgen, läuft am 2026-12-01 ab), dann Punkt 1, 2, 7, 8 und 9 (Punkt 3 und 4 sind laut Nutzer ein 1920×1080-Experiment und zurückgestellt).

## Offene Backlog-Punkte (nicht angefordert, nicht ohne Rückfrage angehen)

- **Color Ramp** (0.92.0 hat die vier konkreten UX-Punkte des Nutzers umgesetzt, siehe CHANGELOG): offen
  bleiben nur die größeren, später gewünschten Ideen — Live-Vorschaustreifen im Node-Körper; noch später,
  noch größer: echte Gradient-Stopp-Liste mit frei platzierbaren Stopps statt vier fester Slots.
- **Echte Leinwandgröße als eingebauter Shader-Wert** (analog zu `@time`/`@volume`), damit Nodes das
  tatsächliche Breite/Höhe-Verhältnis kennen, statt es per Hand im Parameter `aspect` nachpflegen zu
  müssen. Auslöser: Nutzer bemerkte, dass **Labyrinth** sich bei falsch gepflegtem `aspect` je nach
  Auflösung verzieht (quadratische Zellen werden zu Rechtecken) — das ist aber kein Einzelfall, sondern
  betraf alle Nodes mit manuellem `aspect`-Parameter: **Kaleidoscope, Tunnel, Vortex** (`builtin_warps.cpp`),
  **Text** (`builtin_text.cpp`), **Pong** (`builtin_pong.cpp`) und **Labyrinth** (`builtin_maze.cpp`).
  **Erledigt (0.92.0):** `@aspect`/`@aspectZ` existieren (Compiler-Kern), und alle sechs genannten Nodes
  nutzen sie jetzt automatisch (`aspect` = 0, neuer Standard); ein Wert > 0 bleibt als manueller Override
  erhalten (siehe CHANGELOG, beide Einträge). **Ebenfalls erledigt (0.92.0):** `@columns`/`@rows`
  existieren (Compiler-Kern, Unbekannt-Konvention 0 statt 1 wie bei `@aspect`), und Random Pixels
  (`builtin_shapes.cpp`, beide Parameter `columns`/`rows`) sowie Digital Rain (`builtin_generators.cpp`,
  `columns`) nutzen sie jetzt automatisch; ein Wert > 0 bleibt als manueller Override erhalten. Digital
  Rain hat dabei wie gewünscht automatisch 1 Pixel pro Spalte als neuen Standard (siehe CHANGELOG). Damit
  ist dieser gesamte Backlog-Punkt abgeschlossen — alle acht betroffenen Nodes (Kaleidoscope, Tunnel,
  Vortex, Text, Pong, Labyrinth, Random Pixels, Digital Rain) sind umgestellt. **Spectrum Bars**
  (`builtin_audio_field.cpp`) war von Anfang an **nicht** betroffen — kein `aspect`, keine quadratischen
  Zellen; Balkenbreite/-lücke sind relativ zur Spaltenbreite definiert, Höhe nutzt `p.y` direkt.
- **DMX-Output-Nodes** (0.88.0, vom Nutzer am 15.11. zur konkreten Umsetzung freigegeben): Anzeige als
  Zeilen in der Patch-Tabelle, Live-Wertanzeige am Node. **Erledigt (0.92.0), beide Teile:** Live-Wertanzeige
  — beide Nodes (`wh.output.dmx`, `wh.output.dmxcolor`) zeigen den Anzeigestreifen im Node-Körper (siehe
  CHANGELOG, `graph::LiveView::Channels`). Eigene Tabelle für den internen Kanal-Pool — da der Pool nicht an
  ein Fixture gebunden ist (anders als `output::FixturePatch` in der bestehenden, fixture-basierten
  `patch_table.cpp`), bekamen die `dmxpatch`-Einträge (`output::DmxChannelPatch`) eine eigene Tabelle
  darunter (`dmx_patch_table.hpp/cpp`, in `output_page.cpp` eingebunden, neuer `Player::setDmxPatches`) statt
  weiterhin nur über die Projektdatei von Hand gepflegt zu werden. Damit ist dieser Backlog-Punkt
  abgeschlossen.
- **Sequencer: ungenutzte Ausgänge (`trigN`/`onN` im Node-Editor) ausblenden** — der Teil, der noch fehlt
  (0.92.0 hat bereits die Step-Regler im Parameterfenster ausgeblendet, siehe CHANGELOG). Die Ports selbst
  sind weiterhin feste Graph-Verbindungspunkte (`NodeDescriptor::ports`); sie nur im Editor optisch
  auszugrauen wäre einfach, sie wirklich aus dem Node-Körper zu entfernen stellt die Frage, was mit
  bestehenden Verbindungen auf einem dann wegfallenden Port passiert (z. B. `steps` nachträglich verkleinert) —
  noch nicht entschieden, bewusst zurückgestellt.
- **DMX-Fernsteuerung für Live-Modus** (vom Nutzer gewünscht, am 15.11. zur Umsetzung freigegeben): Steuerung
  per eingehendem DMX (nicht Ausgabe) — ein Kanal wählt den aktiven Look anhand seines Werts (je DMX-Wert
  ein Look), ein zweiter Kanal legt die Überblendzeit fest. Dazu: in der Cuelist per DMX (oder allgemein) zu
  einem bestimmten Cue springen können, nicht nur linear vor/zurück.
  **Erledigt (0.92.0), Teil 1 (Live-Modus-Look-Auswahl):** Neue Dateien `live_dmx.hpp/cpp`
  (`runtime::LiveDmxSource`, reine Binning-Funktion `lookIndexFromDmxValue`) und `player_live_dmx.cpp`
  (`Player::setLiveDmx`/`pollLiveDmx`, Muster des bestehenden Masters über DMX, `player_master.cpp`): ein
  Auswahl-Kanal teilt sich in so viele gleich große Stufen auf, wie das Projekt Looks hat (0 = kein Wechsel),
  ein optionaler zweiter Kanal setzt die Überblendzeit (0..255 -> 0..10 s). Ausgelöst wird nur bei einem
  Wechsel des berechneten Index (Umschalter, kein Dauer-Verhalten wie beim Master), wirkt nur im laufenden
  Live-Betrieb. UI: neue Felder auf der Einstellungen-Seite „DMX-Eingang" (`settings_dialog.cpp`,
  `AppSettings.liveDmx*`, `MainWindow::applyLiveDmx`), gehört wie der Master zum Auftritt, nicht zum
  Projekt (nicht gespeichert). **Noch offen (Teil 2):** per DMX direkt zu einem bestimmten Cue springen,
  nicht nur linear — berührt das bestehende Cue-System (`cue_dialog.cpp`, `cue_panel.cpp`); dafür bietet
  sich `CueRunner::fire()`/`Player::cueFire()` an (existiert schon für den Doppelklick auf einen Cue), nur
  die DMX-Wert-zu-Cue-Zuordnung fehlt noch — noch nicht untersucht, wie genau das mit der bestehenden
  Cue-Reihenfolge zusammenspielt.
- **DMX-Patch/Monitor-Gitter** (vom Nutzer vorgeschlagen anhand eines Excel-Mockups: grafische Darstellung
  des DMX-Patch, Rechtsklick auf einen Kanal weist ihm einen Output zu (umgekehrtes Patchen), dasselbe für
  den Input (inkl. Master-Helligkeit usw.), grafische Anzeige belegter Kanäle, soll auch als DMX-Monitor
  dienen; vom Nutzer am 15.11. mit "fahre so fort" für den vorgeschlagenen 4-Schritte-Plan freigegeben: 1.
  reiner Eingangs-Monitor, 2. dasselbe für den Ausgang, 3. Zuweisung per Rechtsklick (Fixture-Patch und
  Kanal-Pool), 4. "wer hört zu"-Overlay auf der Eingangsseite).
  **Erledigt (0.92.0), Schritt 1 (reiner Eingangs-Monitor):** Neues Dock "DMX-Monitor" (Ansicht-Menü,
  `dmx_monitor_panel.hpp/cpp`, Dock-Aufbau in `main_window_dmx_monitor.cpp`): Gitter aller 512 Kanäle eines
  wählbaren Protokoll+Universe (16 Spalten × 32 Zeilen), Zellwert = letzter empfangener Wert oder „–"; Status
  zeigt aktiv/veraltet (> 5 s). Beim Start versteckt, aktualisiert sich nur, solange sichtbar
  (`MainWindow::tick()`, Muster von `editor_->refreshLive()`). Dafür neues `DmxStore::readAll(protocol,
  universe)` (ganzes Universe in einer Sperre statt 128 einzelner `read()`-Aufrufe), eigene Tests
  (`tests/input/dmx_store_test.cpp`).
  **Erledigt (0.92.0), Schritt 2 (Ausgangs-Monitor):** Neues Dock "DMX-Ausgangs-Monitor" (dieselbe Stelle,
  `dmx_output_monitor_panel.hpp/cpp`), strukturell dasselbe Gitter wie Schritt 1, aber für die zuletzt
  **gesendeten** statt empfangenen Art-Net/sACN-Werte. Dafür neues `output::SentDmxStore` in `wh_output`
  (eigene Datei, nicht in `BuiltinLibrary`/`wh_nodes` — reines UI-Anzeige-Anliegen, keine neue
  `wh_nodes`→`wh_output`-Abhängigkeit nötig), geschrieben vom einzigen Sende-Aufruf in
  `OutputPipeline::sendSlot` über ein neues, optionales `OutputConfig::sentDmx`-Feld; DDP wird dabei
  bewusst übersprungen (byteweise statt per Universe adressiert, kein Universe-Raster). Durchgereicht über
  `SessionDeps`/`buildOutput` (neuer optionaler Parameter, alle anderen `buildOutput()`-Aufrufer bleiben
  über den Default unverändert) bis zum `Player`, der den Speicher session-übergreifend als
  `std::shared_ptr` hält (`Player::outputMonitor()`) — anders als die `OutputPipeline` selbst überlebt er
  einen Projekt-/Session-Neuaufbau. Eigene Tests (`tests/output/sent_dmx_store_test.cpp`,
  `output_pipeline_test.cpp`).
  **Erledigt (0.92.0), Schritt 3 (Rechtsklick-Zuweisung) und Schritt 4 ("Wer hört zu"-Overlay), zusammen
  umgesetzt:** Eingang (`DmxMonitorPanel`): Tooltip jeder Zelle nennt per neuem `Player::dmxListenerAt`
  (→ `runtime::findDmxListenerInGraph` für Nodes `wh.value.dmx`/`wh.value.dmxcolor`, plus Player-seitig
  `masterDmx()`/`liveDmx()` für die beiden Einstellungs-Kanäle ohne eigenen Node), wer liest - neu
  berechnet bei Protokoll-/Universe-Wechsel, nicht jeden Frame (ein Nodes-Durchlauf pro Zelle pro Frame
  wäre unnötig teuer). Rechtsklick auf eine freie Zelle bietet die drei Zuweisungen (Master-Helligkeit,
  Live-Look-Auswahl, Live-Überblendzeit) an, die direkt `AppSettings`/`applySettings` setzen - dieselbe
  Stelle wie der Einstellungen-Dialog; eine belegte Zelle zeigt nur den Tooltip-Text, deaktiviert.
  Ausgang (`DmxOutputMonitorPanel`): Rechtsklick nutzt neues `Player::dmxOutputOccupantAt` (→
  `runtime::findDmxOutputOccupant`, scannt `FixturePatch`/`DmxChannelPatch` gegen das Layout, folgt dabei
  demselben Universe-Überlauf wie `OutputPlan::create`) - eine schon gepatchte Zelle zeigt (deaktiviert),
  was dort sendet, eine freie Zelle bietet "Neues Fixture hier patchen…" an und öffnet den bestehenden
  Dialog "Fixture hinzufügen" vorbefüllt (neues `AddFixtureDialog::prefillManualPatch`,
  `LayoutPanel::addFromDialogAt`) statt das Patchen hier noch einmal nachzubauen. Beide Rechtsklick-Signale
  sind dauerhaft mit echten Programm-Aktionen verbunden (Einstellungen übernehmen bzw. den modalen Dialog
  öffnen); die Rauchtests der beiden Panels lösen die Menüpunkte deshalb wirklich aus, rufen aber nie
  `exec()` eines Menüs oder Dialogs selbst auf (würde ohne Publikum blockieren) - `MainWindow` löst dafür
  die Dialog-Verbindung während ihres eigenen Rauchtests kurz, bzw. sichert/stellt bei der Einstellungen-
  Verbindung den vorherigen Stand wieder her, nach demselben Muster wie `exerciseSettings`. Damit ist der
  ursprünglich vereinbarte 4-Schritte-Plan abgeschlossen.
  **Erledigt (0.92.0), zwei nachträglich freigegebene Erweiterungen** (vom Nutzer am 15.11. mit "beide
  genehmigt" freigegeben, nachdem der 4-Schritte-Plan selbst schon fertig war): dieselbe Rechtsklick-
  Zuordnung jetzt auch für einen schon bestehenden Ausgangs-Patch (statt nur für ein neues Fixture) und ein
  "Wer hört zu"-Overlay auch für die Ausgabe. Tooltip jeder Zelle im `DmxOutputMonitorPanel` nennt jetzt per
  `Player::dmxOutputOccupantAt`, was dort gepatcht ist (Fixture-Name oder Pool-Kanal), neu berechnet nur bei
  Protokoll-/Universe-Wechsel wie beim Eingang. Rechtsklick auf eine belegte Zelle bietet jetzt "Fixture-
  Patch bearbeiten …" bzw. "DMX-Kanal-Pool-Patch bearbeiten …" an statt nur die Belegung anzuzeigen: öffnet
  Stage Config auf der Seite Ausgabe und springt zur passenden Zeile (neue
  `PatchTable::selectFixture`/`DmxPatchTable::selectPoolChannel`, `OutputPage::selectFixture`/
  `selectPoolChannel`, `StageSetupWindow::openOutputFixture`/`openOutputPool`). Dafür trägt
  `runtime::DmxOutputOccupant` jetzt zusätzlich die `FixtureId` des gefundenen Fixtures. Rauchtest legt für
  den Pool-Fall vorübergehend einen Patch auf einem sonst ungenutzten Universe an und entfernt ihn danach
  wieder (`player_.setDmxPatches`, Stand vorher/nachher).
- **DMX-Monitor-Vorschlag vom 15.11., zweiter Teil ("Master dimmer usw. hierher patchen"): erledigt.**
  Der Nutzer klärte in einem längeren Hin und Her, dass damit das Rechtsklick-Menü auf einer freien Zelle
  im (Eingangs-)DMX-Monitor gemeint war, neu gegliedert in drei Kategorien: 1. Master (nur Masterdimmer —
  eine zunächst erwogene, dann ausdrücklich verworfene "Masterfade"-Einstellung ist **nicht** entstanden,
  siehe unten), 2. Live Kanal (Look-Auswahl/Überblendzeit, wie bisher), 3. DMX Out (**neu**: patcht die
  Adresse eines schon vorhandenen `wh.value.dmx`/`wh.value.dmxcolor`-Nodes direkt von der Zelle aus, statt
  nur zu ihm zu navigieren). Umgesetzt wie vom Nutzer mit "jetzt" freigegeben; Details siehe CHANGELOG.
  **Zwischenstand, nicht mehr aktuell:** Auf dem Weg dahin wurde eine "Masterfade"-Einstellung (neuer
  Überblendzeit-Kanal für den Masterdimmer, analog zu Live-DMX) komplett gebaut (Backend, Tests), nach
  einem Rückfragen-Hin-und-Her vom Nutzer aber ausdrücklich wieder verworfen ("Nein, nur Bestehendes") und
  vollständig zurückgenommen — die Kategorie "Master" zeigt nur die schon bestehende Masterdimmer-Zuordnung.
- **DMX-Monitor-Vorschlag vom 15.11., erster Teil ("farbig hinterlegen"): erledigt.** Nachträgliche
  Klärung (derselbe Tag): beide Monitore bekommen Zellenfarben, eine Farbe je Belegungsart (Eingang:
  Master, Live Kanal, Node; Ausgang: Fixture, DMX-Kanal-Pool) — automatisch gesetzt, bei einer schon
  belegten Zelle per Rechtsklick → "Farbe ändern…" überschreibbar (reine Anzeige, keine Projekt- oder
  Einstellungsdaten). Vom Nutzer mit "jetzt umsetzen" freigegeben und umgesetzt; Details siehe CHANGELOG.
  Damit sind beide Teile des 15.11.-Vorschlags zum DMX-Patch/Monitor-Gitter abgeschlossen.
- **MIDI-Eingang/-Ausgang** (z. B. Akai APC Mini): **vom Nutzer am 23.11. zur Umsetzung freigegeben**, Plan und Entscheidungen
  in `docs/entwurf-midi.md` (Stufe 1 Grundlage, 2 Nodes, 3 APC mini mk1 im Live-Modus). **Stand 23.11.:** Stufe 1 gemergt (PR #49), Stufe 2 als PR #50, Stufe 3 auf `feat/midi-stufe3`; offen bleiben die Punkte unter „4. Offen / später“ im Entwurf. Ursprüngliche Überlegungen: Überlegungen aus dem
  Gespräch:
  - *Bibliothek:* RtMidi — C++, cross-platform (ALSA/WinMM), passt zum bestehenden Muster, mit dem
    `wh_audio` schon `miniaudio` einbindet. Ein neues `wh_midi` mit einem `MidiStore` (threadsicherer
    Zwischenspeicher, analog zu `AudioStore`/`DmxStore`) würde demselben Schema folgen. MIDI-Callbacks
    laufen auf einem eigenen RtMidi-Thread; Übergabe zum Engine-Thread über denselben
    Lock-free/Atomic-Mechanismus, der für Audio/DMX schon existiert.
  - *Zwei Richtungen, wie bei DMX:* Eingang — `wh.value.midicc` (Channel, CC-Nummer, min/max) als Scalar;
    `wh.value.midinote` (Channel, Notennummer) als Trigger/Boolean plus Velocity als Scalar. Ausgang —
    analog zu `wh.output.dmx` ein `wh.output.midinote`, das Note+Velocity zurückschickt, damit Pads passend
    zum aktuellen Zustand leuchten (aktiver Cue, Preview-Farbe etc.).
  - *Haken — Pad-Farben sind herstellerspezifisch:* Ein APC Mini hat keine echte RGB-LED pro Pad, sondern
    eine kleine Palette, die über die Velocity-Zahl im Note-On gewählt wird. Launchpad-Geräte machen das
    wieder anders (teils SysEx, echtes RGB). Ein generischer „MIDI Note Out"-Node mit freier Velocity ist
    machbar und ehrlich, aber eine komfortable „schick dem Pad diese Farbe"-Abstraktion bräuchte pro
    Geräteklasse eine eigene Übersetzungstabelle/Profil — das ist der aufwendigere Teil, nicht das Protokoll
    selbst.
  - *Nicht reproduzierbar:* genau wie bei DMX-Eingang kommen Werte aus externer Hardware, nicht aus der
    Projektzeit. Offline-Rendern mit MIDI-Nodes liefert je nach Eingabe unterschiedliche Ergebnisse —
    müsste genauso dokumentiert werden.
  - *Geräteauswahl:* analog zur „DMX-Eingang"-Seite eine „MIDI"-Seite in den Einstellungen, Dropdown für
    Eingangs- und (separat) Ausgangsport, standardmäßig aus. USB-Controller können jederzeit ab- und wieder
    angesteckt werden — Hot-Plug bräuchte Polling oder einen RtMidi-Callback für Port-Änderungen.
  - *MIDI-Learn:* statt Channel/CC-Nummer von Hand einzutippen — einen Parameter im Studio anklicken, am
    Controller den Regler bewegen, fertig verbunden. Deutlich mehr UI-Aufwand als die Nodes selbst, aber
    der Unterschied zwischen „technisch möglich" und „macht wirklich Spaß zu benutzen".
  - *Bonus, quasi geschenkt:* MIDI-Clock/MTC als Tempoquelle für den bestehenden Clock-Node — eine DAW oder
    Lichtsoftware könnte den Takt vorgeben, ohne Tap-Tempo.
  - *Fazit:* technisch ein überschaubarer, dem DMX-Output sehr ähnlicher Baustein (Protokoll, Store, zwei/
    drei Nodes). Der Aufwand steckt im Drumherum — Geräteprofile für Pad-Farben und MIDI-Learn —, nicht im
    Kern.
- „Zonen"-Node, Projektions-/HDMI-Ausgabe, Remote-Web-UI, Headless-Player/Pi-Support, ukrainische
  Muttersprachler-Durchsicht der Übersetzungen, GitHub-Releases ab 0.71.0 und Issue #13 schließen — alle
  diese Punkte brauchen vorher die ausdrückliche Freigabe des Nutzers (siehe Sicherheitsregel oben).
