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
- Noch keine Belegung für den Akai APC mini im Live-Modus (Stufe 3).

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

## Bauen

Die Geräteanbindung nutzt RtMidi (wird beim Konfigurieren geladen). Linux braucht dafür die ALSA-Entwicklungsdateien
(`libasound2-dev`); fehlen sie, baut das Programm ohne Geräteunterstützung (die Seite sagt es), alles andere läuft weiter.
Windows nutzt WinMM und braucht nichts. Mit `-DWHITEHOLEFX_MIDI_DEVICES=OFF` entfällt die Geräteanbindung ausdrücklich.

## Nicht reproduzierbar

Wie beim DMX-Eingang kommen die Werte von außen, nicht aus der Projektzeit. Offline-Rendern mit MIDI-Nodes (ab Stufe 2) liefert
je nach Eingabe unterschiedliche Ergebnisse.
