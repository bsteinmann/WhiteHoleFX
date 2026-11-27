# Gemeinsame Tempo-Uhr

Tempo-Nodes wie **Clock**, **Tap Tempo** und **Beat** halten ihren Takt im eigenen Node. Im Live-Modus entfernt ein
Look-Wechsel die Nodes des alten Looks; ein Tap-Tempo im neuen Look beginnt dann von vorn. Die **Tempo-Uhr** lebt stattdessen
im Programm: sie überlebt Look- und Projektwechsel, und alle Looks teilen dasselbe Tempo und denselben Beat.

## Node „Tempo“ (`wh.value.tempo`)

| Ausgang | Bedeutung |
|---|---|
| `beat` (Trigger) | ein Beat beginnt (in jedem Bild höchstens einer, nie zweimal derselbe) |
| `pulse` | 1 beim Beat, klingt mit `decay` (Sekunden) ab |
| `phase` | Bruchteil des laufenden Beats, 0..1 |
| `bpm` | das aktuelle Tempo |
| `count` | Nummer des laufenden Beats |

Der Node hat keinen eigenen Zustand: ein neu geladener Look sieht sofort dasselbe Tempo, dieselbe Phase und denselben Zähler.

## Quellen

Einstellungen → **Steuerung**, Abschnitt Tempo-Uhr (Programmeinstellung `tempo/source`, `tempo/fixedBpm`, `tempo/dmxProtocol`,
`tempo/dmxUniverse`, `tempo/dmxChannel`; nicht im Projekt):

- **Tap:** im Takt tippen - Taste **TAP** im Live-Fenster (zeigt das Tempo) oder die Live-Funktion **Tempo-Tap** auf einer
  MIDI-Taste bzw. einem DMX-Kanal (Reiter Steuerung oder „Steuerung belegen“ im Live-Fenster, in der Belegung `tap`). Das Tempo
  ist der Mittelwert der letzten vier Abstände (30 bis 300 BPM); nach mehr als 2 s Pause beginnt eine neue Folge, das Tempo
  bleibt dann. Tappen wirkt auch außerhalb des Live-Modus.
- **Fester Wert:** ein Tempo in BPM.
- **Audio-Beat:** jeder Schlag, den der Beat-Erkenner des Audio-Eingangs findet, ist ein Beat (die Phase rastet ein wie bei
  einem Tap), das Tempo ist dessen Schätzung. Ohne Audio-Signal (länger als 0,5 s) läuft die Uhr mit dem letzten Tempo weiter.
  Braucht einen laufenden Audio-Eingang (Einstellungen → Audio).
- **DMX-Kanal:** der Wert eines Kanals (Art-Net oder sACN, Universe, Kanal) ist das Tempo in BPM, auf 30 bis 255 begrenzt;
  0 lässt das Tempo unverändert, ebenso ein Kanal ohne Signal (länger als 2 s). Nur das Tempo kommt über DMX, die Phase läuft
  stetig weiter (DMX kennt keinen Beat-Zeitpunkt).

- **MIDI-Clock:** eine DAW, ein Sequencer oder ein Lichtpult am MIDI-Eingang (Einstellungen → MIDI) gibt den Takt vor: 24
  Clock-Meldungen je Viertel, das Tempo kommt aus deren Abständen (gemittelt über einen Beat). Die erste Clock nach **Start** und
  danach jede 24. ist ein Beat (die Phase rastet ein); ohne Start zählt die Uhr ab der ersten empfangenen Clock. **Stop** oder
  länger als 0,5 s keine Clock: die Uhr läuft mit dem letzten Tempo weiter. Song Position wird nicht ausgewertet.

Wechselt die Quelle, läuft die Phase stetig weiter; mit dem nächsten Tap, Audio-Schlag oder Clock-Beat rastet sie ein. Die
Option „Globales Tempo“ an Clock, Beat und Shot folgt.

## Phase und Einrasten

Die Phase läuft stetig: ändert sich das Tempo, springt sie nicht. **Jeder Tap ist ein Beat:** die Phase rastet auf den Tap ein
(auf die nächste ganze Beat-Nummer, also höchstens einen halben Beat vor oder zurück). Damit Effekte nicht zucken, wird der
Unterschied nicht hart gezeigt, sondern in **0,25 s** angeglichen. Liegt der Tap knapp vor dem erwarteten Beat, kommt der Beat
sofort mit dem Tap (und nicht noch einmal danach); liegt er knapp danach, gibt es keinen zweiten Beat, nur der Puls beginnt neu.

## Hinweise

- Zeitbezug ist die Uhr des Players (in Tests vorgegeben), nicht die Projektzeit. Offline-Rendern mit dem Node „Tempo“ ist darum
  wie bei DMX- und MIDI-Nodes nicht reproduzierbar.
- Code: `values::TempoClock` (`libs/wh_values`, `externalBeat`/`externalBpm` für Audio und DMX), `BuiltinLibrary::tempo`,
  `Player::tempoTap`/`setTempoSource`/`setTempoDmx`/`advanceTempo`/`advanceMidiClock`, MIDI-Clock in
  `midi::MidiStore::applyRealtime`/`clock()` (`MidiRealtime`, eigener Weg neben Noten und Reglern)
  (`player_tempo.cpp`), Node in `builtin_tempo.cpp`.
