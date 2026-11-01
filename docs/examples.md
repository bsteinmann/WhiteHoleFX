# Beispielprojekte

Die Beispiele liegen in `examples/`, nach Themen in Untergruppen. Im Studio erscheinen dieselben Gruppen als Untermenüs unter
**Datei → Beispiele**. Prüfen ohne GPU geht mit `whitehole_cli <datei> --check`, rendern mit `--software --frames 20`.

| Gruppe | Datei | Zeigt |
| --- | --- | --- |
| Einstieg | `einstieg/wand.whfx` | 16×8-Matrix und Strip mit drehendem Verlauf; kleinstes sinnvolles Projekt |
| Einstieg | `einstieg/regler.whfx` | Plasma mit Spiegeln, Zoom und LFO-Drehung; alle Reglertypen |
| Einstieg | `einstieg/looks.whfx` | drei gespeicherte Looks für den Live-Modus |
| Effekte (2D) | `effekte/gewitter.whfx` | Blitze über Plasma-Wolken |
| Effekte (2D) | `effekte/funken.whfx` | Funken (Particles) |
| Effekte (2D) | `effekte/kaleidoskop.whfx` | Metaballs im Kaleidoskop |
| Effekte (2D) | `effekte/laufschrift.whfx` | Laufschrift |
| Audio | `audio/audio.whfx` | Equalizer mit pulsierendem Hintergrund |
| Audio | `audio/takt.whfx` | Effekte im Takt der Musik |
| Audio | `audio/oszilloskop.whfx` | Wellenform als Oszilloskop |
| 3D | `3d/ausrichtung.whfx` | Position Test: Achsen, Startecke und Verkabelung eines 6×4×8-Quaders prüfen |
| 3D | `3d/plasma-wuerfel.whfx` | Plasma durch die Tiefe eines 8×8×8-Cubes, Color Map, Rotate 3D |
| 3D | `3d/scheibe.whfx` | Scan Line als Scheibe, die ein LFO durch den Cube fährt |
| 3D | `3d/regentropfen.whfx` | Ripples als Kugelschalen (Volume) und blinkende Würfel (Fading Pixels, Volume) |
| 3D | `3d/saeule.whfx` | ein Strip als Schraube um eine Säule (Punktwolke) mit Helix und Wellenfront |
| Daten | `daten/bogen.csv` | LED-Positionen auf einem Bogen zum Import (Stage Config → Punktwolke importieren) |

Neue Beispiele kommen in die passende Gruppe oder in eine neue Unterordner (der Name erscheint dann großgeschrieben im Menü;
für einen übersetzten Namen gibt es `groupTitle` in `apps/whitehole_studio/main_window_actions.cpp`). Der Test
`Beispiele: jede Datei in examples/ …` lädt und kompiliert jede `.whfx`-Datei, auch in Unterordnern.
