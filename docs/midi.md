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
- Noch **keine** MIDI-Nodes und noch keine Belegung für den Akai APC mini - das sind Stufe 2 und 3.

## Bauen

Die Geräteanbindung nutzt RtMidi (wird beim Konfigurieren geladen). Linux braucht dafür die ALSA-Entwicklungsdateien
(`libasound2-dev`); fehlen sie, baut das Programm ohne Geräteunterstützung (die Seite sagt es), alles andere läuft weiter.
Windows nutzt WinMM und braucht nichts. Mit `-DWHITEHOLEFX_MIDI_DEVICES=OFF` entfällt die Geräteanbindung ausdrücklich.

## Nicht reproduzierbar

Wie beim DMX-Eingang kommen die Werte von außen, nicht aus der Projektzeit. Offline-Rendern mit MIDI-Nodes (ab Stufe 2) liefert
je nach Eingabe unterschiedliche Ergebnisse.
