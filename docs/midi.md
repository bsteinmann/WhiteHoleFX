# MIDI

Warum diese Datei: Sie beschreibt, was von der MIDI-Anbindung schon gebaut ist und wie sie sich verhält. Der Gesamtplan mit
den Entscheidungen steht in `entwurf-midi.md`.

## Stand: Stufe 1 (Grundlage)

- Einstellungen → **MIDI**: Eingangs- und Ausgangsport wählen (Standard: aus). Die Wahl wird über den **Namen** des Ports gemerkt
  und gehört zum Programm, nicht zum Projekt. Unter Linux hängt das System eine Nummer wie `20:0` an den Namen; sie ändert sich
  beim Anstecken, deshalb wird sie beim Wiederfinden ignoriert.
- Ein Port, der beim Start fehlt (Gerät nicht angesteckt), bleibt als „(nicht gefunden)“ gewählt; das Programm versucht es bei
  jedem Übernehmen der Einstellungen erneut.
- Die Seite zeigt die zuletzt empfangene Nachricht (Kanal, Note oder Regler, Wert). Verarbeitet werden Note an/aus und
  Control Change; Systemnachrichten (Takt, SysEx) werden ignoriert.

## Stufe 2: Nodes und „Lernen“

- **MIDI CC** (`wh.value.midicc`, Kategorie Input): Kanal 1..16, Reglernummer (CC) 0..127; Ausgänge `Value` (auf Out min/Out max umgerechnet),
  `On` (Wert ≥ Schwelle) und `Signal`.
- **MIDI Note** (`wh.value.midinote`): Kanal, Note 0..127; Ausgänge `On` (solange die Taste gedrückt ist), `Velocity` (0..1, die
  Anschlagstärke des letzten Drucks, bleibt nach dem Loslassen stehen) und `Signal`.
- **Signal** heißt: Für diesen Regler bzw. diese Taste kam schon eine Nachricht (und nicht länger her als das Zeitlimit). Ein Regler
  meldet sich nur bei Bewegung; darum ist das Zeitlimit hier standardmäßig **0 = nie**. „Hold“ hält den letzten Wert bei Signalverlust.
- **Lernen:** Im Parameterfenster des Nodes „Lernen“ drücken, dann am Controller den Regler bewegen bzw. die Taste drücken; Kanal und
  Nummer werden eingetragen. Ein zweiter Klick bricht ab. Voraussetzung: der Eingangsport ist unter Einstellungen → MIDI gewählt.
- Die Nodes lesen aus demselben Speicher wie die Anzeige „Letzte Nachricht“ der Einstellungen (`BuiltinLibrary::midi`).

## Stufe 3: Live-Modus auf dem Akai APC mini (mk1)

Solange das Live-Fenster offen ist, bedient der Controller den Live-Modus; außerhalb bleiben Pads und Fader frei (die
MIDI-Nodes lesen sie weiter). Dafür muss unter Einstellungen → MIDI der Eingang gewählt sein, für die LEDs auch der Ausgang.

- **Pads → Looks:** Raster wie im Live-Fenster, erste Zeile oben (Look 1 = Pad links oben). Ein Druck blendet zum Look über; mit
  „Mit GO“ im Live-Fenster lädt er ihn nur vor. Pads ohne Look tun nichts.
- **LEDs:** gelb = belegt, grün = läuft (auch beim Einblenden), gelb blinkend = vorgeladen, rot = Flash-Look (nicht aktiv); ohne Look bleibt das Pad aus. Gesendet wird nur, was sich ändert; beim
  Verlassen des Live-Modus und beim Beenden des Programms gehen alle aus.
- **Tasten** (Profil „Looks 4 x 8 mit GO und Fadern“): Seitentaste 1 (oben) = GO (blinkt, wenn etwas vorgeladen ist), Seitentaste 2 =
  Live beenden (leuchtet im Live-Modus).
- **Fader:** Fader 9 = Master-Helligkeit (der Regler im Studio zieht mit), Fader 1 = Überblendzeit 0–10 s (hat Vorrang vor der
  eigenen Zeit im Live-Fenster, sobald er einmal bewegt wurde), Fader 2 = **manuelle Überblendung**.
- **Manuelle Überblendung:** Look vorladen (am besten mit „Mit GO“), dann Fader 2 von unten nach oben ziehen: Bei ganz oben ist der
  Look sichtbar, zurück nach unten ist er wieder nur vorgeladen, die Zeit läuft dabei nicht mit. GO mittendrin blendet mit der Uhr zu
  Ende. **Mitnahme:** Ein neu vorgeladener Look folgt dem Fader erst, wenn dieser einmal unten war (Wert ≤ 3) - sonst würde das Bild
  springen. Nach einem vollständigen Übergang also erst wieder nach unten ziehen.
- **Profile** (Einstellungen → MIDI, Programmeinstellungen, nicht im Projekt): vorgefertigt sind „Looks 4 x 8 mit GO und Fadern“
  (Standard, 32 Looks, rechte Pad-Hälfte frei) und „Looks 8 x 8 ohne Steuertasten“ (64 Looks, Fader wie oben). Beide sind
  schreibgeschützt; „Kopieren …“ legt ein eigenes an, dessen Spalten, Tasten (Noten) und Fader (CC) sich ändern lassen
  (−1 = nicht belegt); eigene lassen sich umbenennen und löschen.
- Gerätedaten mk1 (Kanal 1): Pads Noten 0..63 (links unten 0), Seitentasten Noten 82..89 (oben 82), untere Tasten 64..71, Fader
  CC 48..56. Pad-LED über die Velocity: 0 aus, 1 grün, 2 grün blinkend, 3 rot, 4 rot blinkend, 5 gelb, 6 gelb blinkend.

Code: `midi::LiveProfile` (`live_profile.hpp`), `runtime::MidiLiveController` (`midi_live.hpp`), `LiveDecks::setManualProgress`,
`Player::liveManualFade`; im Studio `main_window_midi_live.cpp` und `midi_profile_box.cpp`.

## Looks umschalten oder flashen

Jeder Look legt fest, wie er ausgelöst wird (im Live-Fenster unter „Einstellungen“ der Haken **Flash** unter dem Look, oder per
Rechtsklick auf den Look; in der Datei `look "Name" fade 3 trigger flash in 0.2 return 0.5`): **Umschalten** (Standard) bleibt an; **Flashen** läuft nur, solange Pad oder Knopf gehalten werden, und blendet beim
Loslassen zum vorherigen Look zurück. Beide Zeiten sind je Look einstellbar: **Rein** (`in`, beim Drücken) und **Zurück**
(`return`, beim Loslassen), jeweils die Überblendzeit des Looks oder eine eigene Zeit (0 = hart). Ohne eigene Einblendzeit gilt beim
Drücken die Zeit des Looks (im Live-Fenster ggf. die eigene Überblendzeit); eine Einblendzeit am Look geht vor. Stellst du ein Feld neu auf Flash, tragen Ein- und Zurückblendzeit zunächst **0,1 s** ein (ein kurzer Stoß; beide bleiben je Feld einstellbar, auch zurück auf „Zeit des Looks“). Schon gespeicherte Flash-Felder behalten ihre Zeiten (auch „Zeit des Looks“). Flash hat Vorrang vor
„Mit GO“. Mehrere gehaltene Flash-Looks stapeln sich, erst das letzte Loslassen kehrt zurück; wer inzwischen umschaltet (Pad, GO,
Cue), beendet das Flash und der neue Look bleibt stehen. Die Logik steckt im Player (`liveDown`/`liveUp`), damit sie für alle
Eingänge gleich gilt.

## Cues per Pad abrufen

Solange die Cuelist läuft (Cues → Start), feuern die Pads Cues statt Looks: Pad n ruft Cue n auf (gleiches Raster wie für Looks), GO ruft den
nächsten Cue auf. Der Cue behält seine eigene Verzögerung und Überblendzeit. LEDs: aktueller Cue grün, in Verzögerung grün blinkend,
nächster Cue gelb blinkend, übrige gelb; GO blinkt, solange ein nächster Cue existiert. Per DMX gilt dasselbe für den Auswahl-Kanal der
Live-Steuerung (siehe `docs/studio.md`). Code: `MidiLiveController::handle`/`updateLeds`, `Player::cueFire`.

## Steuerung frei belegen (MIDI und DMX)

GO, Live beenden, Master, Überblendzeit und manuelle Überblendung lassen sich per **Lernen** auf beliebige Tasten, Regler oder
DMX-Kanäle legen - im Reiter Einstellungen → **Steuerung** und in der Zeile „Steuerung belegen“ des Live-Fensters (erscheint mit der
Schaltfläche **Einstellungen** oben rechts); beide schreiben dieselbe Belegung (`runtime::LiveBindings`, Programmeinstellung `midi/liveBindings`). Ohne eigene MIDI-Belegung gilt das Profil;
„Profil“ setzt zurück. DMX: Tasten lösen bei steigender Flanke über 127 aus, Fader wirken bei jeder Änderung; der erste gelesene
Wert löst nichts aus. Master über DMX ist die vorhandene Einstellung der Seite DMX-Eingang. Solange ein Lernen-Knopf wartet, wirkt
der Controller im Live-Modus nicht (die zu lernende Taste löst nicht ihre alte Funktion aus).

## Bauen

Die Geräteanbindung nutzt RtMidi (wird beim Konfigurieren geladen). Linux braucht dafür die ALSA-Entwicklungsdateien
(`libasound2-dev`); fehlen sie, baut das Programm ohne Geräteunterstützung (die Seite sagt es), alles andere läuft weiter.
Windows nutzt WinMM und braucht nichts. Mit `-DWHITEHOLEFX_MIDI_DEVICES=OFF` entfällt die Geräteanbindung ausdrücklich.

## Nicht reproduzierbar

Wie beim DMX-Eingang kommen die Werte von außen, nicht aus der Projektzeit. Offline-Rendern mit MIDI-Nodes (ab Stufe 2) liefert
je nach Eingabe unterschiedliche Ergebnisse.
