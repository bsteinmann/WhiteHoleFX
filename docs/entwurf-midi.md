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
werden in Stufe 3 mit Tests festgelegt. **Festgelegt (umgesetzt):** Mitnahme - ein neu vorgeladener Look folgt dem Fader erst,
wenn dieser unten war (≤ 3); rückwärts senkt den Fortschritt, auf 0 ist der Look wieder nur vorgeladen; GO mittendrin blendet mit
der Uhr von der erreichten Stelle zu Ende (`tests/runtime/live_manual_fade_test.cpp`, `tests/runtime/midi_live_test.cpp`).

### 3.5 Profile

- Ein Profil = Name, Gerät (mk1), Raster (Spalten, Richtung), Zuordnung der Steuertasten und Fader.
- Speicherort: Programmeinstellungen (Datei neben den übrigen Einstellungen), nicht im Projekt.
- Vorgefertigt: **„Looks 4 x 8 mit GO und Fadern“** (Standard) und **„Looks 8 x 8 ohne Steuertasten“** (alle 64 Pads für Looks).
- Speichern, Umbenennen, Löschen; vorgefertigte Profile sind schreibgeschützt (Kopie möglich).

## 4. Offen / später

- Pads einzeln umlegen, Seiten für mehr als 32/64 Looks, mk2-Profil (RGB), `wh.output.midinote`, MIDI-Clock als Tempoquelle, Hot-Plug.
- Erledigt: Cue-Sprung per MIDI und DMX (Pads/Auswahl-Kanal feuern Cues, solange die Cuelist läuft); APC mini (mk1) auf Windows getestet.
- Zu prüfen in Stufe 1: ob RtMidi lokal ladbar ist (Netz) oder nur in der CI.

## 5. Looks auslösen: umschalten und flashen (Entscheidung vom 23.11.)

Ziel: Jeder Look legt selbst fest, wie er ausgelöst wird. Das gilt für alle Eingänge (MIDI, DMX, Taste im Fenster), nicht nur für MIDI.

### 5.1 Einstellungen je Look

- **Auslösen:** `umschalten` (Standard, wie bisher: Druck blendet dauerhaft um) oder `flashen` (läuft nur, solange gehalten).
- **Überblendzeit** (Hinlauf): wie bisher, der Wert des Looks.
- **Zurückblenden** (nur Flash): wahlweise
  - `Zeit des Looks` (Standard: die Überblendzeit des Looks, der gerade geflasht wird), oder
  - `frei` (eigene Zeit in Sekunden, 0 = hart).
- Speicherort: am Look selbst, in der Projektdatei (`look "Name" fade 3 trigger flash in 0.2 return 0.5`; `trigger`, `in` und `return` werden nur geschrieben, wenn nicht Standard; `in` = Zeit zum Einblenden beim Drücken, Nachtrag 23.11.). Bearbeitbar im Live-Fenster (Rechtsklick auf den Knopf).

### 5.2 Vorrang

- **Flash hat Vorrang vor „Mit GO".** Ein flashender Look blendet beim Drücken sofort ein und beim Loslassen sofort zurück, GO wird dabei nicht gebraucht.
- Ein umschaltender Look folgt weiter dem Schalter „Mit GO" im Live-Fenster.

### 5.3 Ablauf Flash

1. Drücken: Der aktuell sichtbare Look wird gemerkt (`returnTo`). Der geflashte Look wird wie beim Umschalten geladen und eingeblendet.
2. Loslassen: Zurück zum gemerkten Look, mit der gewählten Zeit (siehe 5.1).
3. Ist ein anderer Look inzwischen durch ein Pad oder GO umgeschaltet worden, bleibt er stehen; das Zurückblenden betrifft nur den Fall, dass nichts anderes passiert ist.
4. Mehrere Flash-Pads gleichzeitig: Der zuletzt gedrückte gewinnt. Erst beim Loslassen des letzten Flash-Pads geht es zum ursprünglichen Look zurück.

### 5.4 Schnittstelle (protokollunabhängig)

- `Player::liveDown(index)` und `Player::liveUp(index)`. Die Logik (merken, zurückblenden, Vorrang) steckt nur im Player.
- MIDI: Note-On ruft `liveDown`, Note-Off ruft `liveUp` auf. Heute wird Note-Off ignoriert.
- DMX: ein Schwellwert (z. B. ab 128 = unten) ruft dieselben Aufrufe. Noch nicht Teil dieses Schritts.
- Umschalt-Looks ignorieren `liveUp`.

### 5.5 Schritte

1. Datenmodell und Projektdatei: Felder `trigger`, `return` am Look, Lesen/Schreiben, Tests.
2. Player: `liveDown`/`liveUp`, Vorrang, Rückblenden, Tests (Einzel-Flash, Zwei-Flash, Flash mit GO, Rückblenden während eines anderen Fades).
3. MIDI: Note-Off durchreichen, Pad-Druck ruft `liveDown`.
4. Live-Fenster: Einstellungsdialog pro Look.
5. Doku, CHANGELOG, Übersetzungen.

## 6. Steuerung frei belegbar, mit „Lernen" (Wunsch vom 23.11.)

Lesart: Jede Steuerfunktion im Live-Modus (GO, Live beenden, Master, Überblendzeit, manuelle Überblendung, und die Look-Pads
selbst) bekommt eine Belegung, die man nicht mehr von Hand in Zahlen eingibt, sondern per **Lernen** auf dem Controller
drückt bzw. bewegt. Das ersetzt die festen Nummern aus den Profilen als Standard, Profile bleiben als Vorlagen.

- **Einstellungen → eigener Reiter „Steuerung":** eine Zeile je Funktion mit Name, aktueller Belegung und Knopf „Lernen".
  Pads für Looks werden über ihren Ort im Raster gelernt (ein Druck auf das gewünschte Pad belegt das Raster-Feld) oder
  bleiben am Profil-Raster.
- **Live-Fenster:** neben den Knöpfen für GO, Live beenden und Überblendzeit ebenfalls „Lernen"; dasselbe Ergebnis wie im
  Reiter, weil beides dieselbe Belegung schreibt. Nachtrag 23.11.: Die Zeile steht nicht mehr dauerhaft unten (verwirrend),
  sondern hinter der Schaltfläche „Einstellungen“ oben rechts, zusammen mit dem Haken „Flash“ je Look und der Menüleiste;
  im Live-Modus ist alles zunächst ausgeblendet.
- **Flash und Umschalten** bekommen je Look eine Taste (oder das Pad selbst), gelernt im Look-Dialog (siehe Abschnitt 5).
- Gespeichert: Belegungen gehören zum Auftritt (Programmeinstellungen), nicht zum Projekt. Ausnahme: der Look-Auslöse-Modus
  aus Abschnitt 5 liegt im Projekt.
- Lernen wird wie bei den MIDI-Nodes umgesetzt (`midi::MidiLearn`): nur eine neue Nachricht zählt, Abbruch per zweitem Klick.

Offene Punkte für die Umsetzung:
1. Reiter „Steuerung" und Live-Fenster schreiben dieselbe Belegung, damit es keine zwei Wahrheiten gibt.
2. Profil bleibt als Startbelegung, Lernen überschreibt sie; „Auf Profil zurücksetzen" pro Funktion.
3. Welche Nachrichtenart beim Lernen gilt: Note für Tasten, Regler für Fader; beim Lernen eines Pads wird die Art automatisch gewählt.

### 6.1 Entscheidungen vom 23.11. (Nachtrag)

- **Reiter und Live-Fenster schreiben dieselbe Belegung.** Bestätigt.
- **DMX genauso:** Jede Funktion kann auch auf einen DMX-Kanal (Quelle, Universe, Kanal) gelernt werden. „Lernen" heißt hier:
  Knopf drücken, dann den Kanal am Pult bewegen; der erste Kanal, der sich ändert, wird übernommen. Dafür braucht der
  DMX-Empfang eine Lernfunktion analog zu `midi::MidiLearn` (Änderung seit dem Scharfstellen, nicht der Wert selbst).
- Eine Funktion hat **eine Quelle pro Eingang**: Ein Pad und ein DMX-Kanal können dieselbe Funktion steuern, beide Eingänge
  wirken gleichzeitig. Die Belegungstabelle hält je Funktion eine Liste von Quellen (MIDI oder DMX).
- Offen (unverändert): Pad-Direktlernen, Profil als Startbelegung mit „Auf Profil zurücksetzen".
- **Das Pad-Raster bleibt fest** (Entscheidung vom 23.11.): Pads werden nicht einzeln einem Look zugewiesen. Look i liegt
  immer am Raster-Feld i. Gelernt werden nur die Funktionen (GO, Ende, Fader, Flash-/Umschalt-Tasten).
  Das Lernen von Pads entfällt damit.
- **Profil bleibt Startbelegung** (bestätigt am 23.11.): Ein Profil legt die Anfangsbelegung der Funktionen fest. Lernen
  überschreibt sie. Pro Funktion gibt es „Auf Profil zurücksetzen"; „Alle auf Profil zurücksetzen" im Reiter „Steuerung".
- **Profil bleibt Startbelegung** (Entscheidung vom 23.11.): Gelernte Belegungen überschreiben das Profil je Funktion;
  „Auf Profil zurücksetzen" je Funktion stellt den Profilwert wieder her.
