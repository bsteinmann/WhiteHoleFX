# Entwurf: MIDI-Eingang/-Ausgang und APC mini (mk1)

Warum diese Datei: Die MIDI-Umsetzung ist in drei Stufen geplant. Hier stehen die mit dem Nutzer am 23.11. besprochenen
Entscheidungen und der Plan, damit jede Stufe ohne den Chat-Verlauf nachvollziehbar bleibt. Es ist noch nichts gebaut.

## 0. Entscheidungen (Nutzer, 23.11.)

- Gerät: **Akai APC mini mk1** (USB). Der mk2 ist anders (RGB-Pads, andere Notennummern) und kommt als zweites Profil später.
- Reihenfolge: Stufe 1, 2, 3 nacheinander, damit früh am Gerät getestet werden kann.
- Looks-Raster auf dem APC = Raster im Live-Fenster: **4 Spalten**, Looks zeilenweise, erste Zeile **oben**. Bis 32 Looks
  (8 Zeilen). Die rechte Hälfte der Pads bleibt frei. Mehr Looks, Seiten und 8 Spalten kommen erst später.
- Steuertasten im Standardprofil: Seitentaste 1 = GO, Seitentaste 2 = Live beenden, Fader 9 = Master-Helligkeit,
  Fader 1 = Überblendzeit, Fader 2 = **manuelle Überblendung** (siehe 3.4).
- Vorgeladener Look (Modus „Mit GO“) blinkt gelb.
- Belegungen („Profile“) liegen **nicht im Projekt**, sondern bei den Programmeinstellungen (gehören zum Gerät, nicht zur
  Show). Zwei vorgefertigte Profile, eigene lassen sich speichern, umbenennen, löschen. Einzelne Pads umlegen: später.
- Außerhalb des Live-Modus lösen die Pads nichts aus; sie bleiben für die MIDI-Nodes frei.

## 1. Stufe 1: MIDI-Grundlage

- Neue Bibliothek `wh_midi` nach dem Muster von `wh_input`:
  - `MidiStore`: zuletzt empfangener Zustand (Kanal x CC, Notenzustand mit Velocity, Zeit der letzten Nachricht);
    threadsicher, weil der Rückruf auf einem eigenen Thread läuft.
  - Eine kleine Schnittstelle `IMidiBackend` (Ports auflisten, öffnen, Nachricht senden) mit zwei Umsetzungen: RtMidi
    (ALSA/WinMM, wird wie `miniaudio` geladen) und ein Fake für Tests.
- Einstellungen: neue Seite „MIDI“ mit Eingangs- und Ausgangsport (getrennt, Standard aus) und einer Zeile mit der letzten
  empfangenen Nachricht. Gehört zu `AppSettings`, nicht zum Projekt.
- Linux: das .deb braucht die ALSA-Bibliothek als Abhängigkeit; die CI-Jobs brauchen die Entwicklungsdateien dazu.
- Nicht reproduzierbar (wie DMX-Eingang): Offline-Rendern mit MIDI-Nodes hängt von der Eingabe ab; das steht in der Doku.

## 2. Stufe 2: Nodes und Zuweisung

- `wh.value.midicc`: Kanal, CC-Nummer, Out min/max; Ausgänge Wert und Signal.
- `wh.value.midinote`: Kanal, Note; Ausgänge On (Boolean) und Velocity (Scalar).
- Zuweisung per **„Lernen“-Knopf** im Node: nächste Nachricht trägt Kanal und Nummer ein.
- Signal-weg und „letzten Wert halten“ wie bei den DMX-Nodes.

## 3. Stufe 3: Live-Modus auf dem APC mini (mk1)

### 3.1 Geräteprofil mk1

- Pads: Noten 0..63, links unten = 0, zeilenweise nach oben. Das Raster wird intern gedreht, damit die erste Zeile des
  Live-Fensters oben liegt: Look i bei 4 Spalten liegt in Zeile `i / 4` von oben, Spalte `i % 4`.
- LED per Note-On-Velocity: 0 aus, 1 grün, 2 grün blinkend, 3 rot, 4 rot blinkend, 5 gelb, 6 gelb blinkend.
- Seitentasten (Scene) und untere Tasten (Track): nur an, aus, blinken (Noten und Farbe werden am Gerät geprüft).
- Fader: CC 48..56.

Die genauen Nummern stehen als kleine Tabelle im Code und werden am echten Gerät geprüft.

### 3.2 Anzeige (Player -> APC)

Aus `LiveStatus` und der Look-Liste: läuft = grün, vorgeladen = gelb blinkend, blendet gerade ein = gelb, sonst aus.
Es werden nur geänderte Pads gesendet (kein Dauerfeuer); beim Live-Start und beim Beenden wird das Raster einmal gesetzt
bzw. gelöscht.

### 3.3 Bedienung (APC -> Player)

Pad-Druck ruft `liveTrigger(index)` (Modus „Mit GO“: `liveArm`). Seitentaste 1 = `liveGo`, Seitentaste 2 = `stopLive`.
Fader 9 setzt den Master, Fader 1 die Überblendzeit (0..10 s, wie beim DMX-Kanal).

### 3.4 Manuelle Überblendung (Fader 2)

Der Live-Graph mischt über einen Crossfade-Node mit Parameter `amount`. Neu: `LiveDecks::setManualProgress(0..1)` setzt den
Fortschritt direkt. Ablauf: Look vorladen, dann den Fader von 0 nach 1 ziehen; bei 1 ist der Look sichtbar und der Fade
beendet. Solange der Fader mitten steht, läuft kein Zeit-Fade. Randfälle (Fader beim Vorladen nicht auf 0, Rückwärtsfahren)
werden in Stufe 3 mit Tests festgelegt.

### 3.5 Profile

- Ein Profil = Name, Gerät (mk1), Raster (Spalten, Richtung), Zuordnung der Steuertasten und Fader.
- Speicherort: Programmeinstellungen (Datei neben den übrigen Einstellungen), nicht im Projekt.
- Vorgefertigt: **„Looks 4 x 8 mit GO und Fadern“** (Standard) und **„Looks 8 x 8 ohne Steuertasten“** (alle 64 Pads für Looks).
- Speichern, Umbenennen, Löschen; vorgefertigte Profile sind schreibgeschützt (Kopie möglich).

## 4. Offen / später

- Pads einzeln umlegen, Seiten für mehr als 32/64 Looks, mk2-Profil (RGB), Cue-Sprung per MIDI (zusammen mit Teil 2 der
  DMX-Fernsteuerung), `wh.output.midinote`, MIDI-Clock als Tempoquelle, Hot-Plug.
- Zu prüfen in Stufe 1: ob RtMidi lokal ladbar ist (Netz) oder nur in der CI.
