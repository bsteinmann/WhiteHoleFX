# Projektdatei und Konsolen-Runner

Ein Projekt (Layout, Effektgraph, Ausgabe) liegt in einer Textdatei, üblicherweise `*.whfx`.
Sie ist von Hand les- und schreibbar und lässt sich gut in Git vergleichen. Die GUI wird
dieselbe Datei schreiben. Beispiel: [`examples/wand.whfx`](../examples/wand.whfx).

## Schnellstart

```
whitehole_cli examples/wand.whfx --check            # prüfen, ohne GPU, ohne Netzwerk
whitehole_cli examples/wand.whfx                    # rendern und senden (braucht das GPU-Backend)
whitehole_cli examples/wand.whfx --frames 200 --fps 30 --software
```

`--check` baut denselben Ausgabeplan wie der Betrieb und beschreibt ihn: wie viele LEDs, welche
Pakete an welche Adresse und in welches Universe gehen. Doppelt belegte Kanäle, fehlende Ziele
oder zu lange Strips fallen so vor dem ersten Paket auf.

Ohne `--frames` läuft der Runner bis Strg+C. Er beendet geordnet: laufende Frames werden noch
ausgeliefert, dann erscheint eine Zusammenfassung (Frames, verworfene Frames, Pakete, Fehler).

## Vorschau als Bild

```
whitehole_cli wand.whfx --check --snapshot layout.png     # Layout, ohne GPU
whitehole_cli wand.whfx --frames 60 --snapshot bild.png   # der 60. Frame, wie ihn die LEDs zeigen
whitehole_cli wand.whfx --snapshot bild.png --size 1280x720
```

Jede LED erscheint als Punkt an ihrer Position (von vorn, y nach oben; hat das Layout Tiefe,
schräg von vorn oben). Das Bild zeigt die Farben vor der Ausgabeumrechnung (Gamma, Helligkeit,
Farbreihenfolge), also so, wie der Effekt sie meint. Ausgeschaltete LEDs bleiben als dunkle
Punkte sichtbar.

Die **Layout-Vorschau** (`--check --snapshot`) färbt jedes Fixture mit einem eigenen Farbton und
macht es entlang des Kabels heller: Die erste LED am Kabel ist die dunkelste. So sieht man auf
einen Blick, ob eine Matrix richtig herum liegt und wo die Serpentine umkehrt.

Ohne `--frames` rendert `--snapshot` genau einen Frame. Die Dateien sind unkomprimierte PNGs
(einige hundert KB bis wenige MB). In der CI liegt das Bild des Beispielprojekts als Artefakt
„vorschau“ am Lauf.

## Aufbau der Datei

Zeilenweise; Wörter durch Leerzeichen getrennt; `#` beginnt einen Kommentar; Wörter mit
Leerzeichen stehen in `"Anführungszeichen"` (darin gelten `\"`, `\\`, `\n`). Zahlen mit Punkt
(`1.5`), keine Tausendertrenner. Die erste Zeile ist immer `whitehole-project 1`.

```
whitehole-project 1

settings fps 40 normalize stretch sacn-priority 100 source-name "WhiteHoleFX"

destination 0 192.168.1.50            # Nummer, IPv4 oder "multicast", optional Port
destination 1 multicast
destination 2 10.0.0.7 6455

fixture "Panel links" at 0 0 0        # "at x y z" ist optional (Standard 0 0 0)
  matrix 16 8 pitch 1 1 start top-left first rows serpentine
  patch artnet destination 0 universe 3 channel 1 order GRB gamma 2.2 brightness 1
end

node a wh.generator.gradient
  set angle 30                        # Wert nach dem Typ des Parameters
  set colorA 1 0 0 1
end
node b wh.transform.mirror
end
connect a.out -> b.source
output b.out                          # dieser Ausgang wird auf die LEDs abgetastet
```

### `settings` (optional, Schlüssel in beliebiger Reihenfolge)

| Schlüssel | Bedeutung | Standard |
|---|---|---|
| `fps <zahl>` | Bildrate, größer 0, höchstens 1000 | 40 |
| `normalize stretch\|keep-aspect` | wie das Layout auf den Einheitswürfel abgebildet wird | stretch |
| `sacn-priority <0..200>` | sACN-Priorität | 100 |
| `source-name <text>` | Name der sACN-Quelle; aus ihm wird auch die stabile Sender-ID abgeleitet | WhiteHoleFX |

### `destination <nummer> <adresse|multicast> [port]`

Die Nummern müssen der Reihe nach bei 0 beginnen. Port 0 oder weggelassen: Standardport des
Protokolls (Art-Net 6454, sACN 5568, DDP 4048). `multicast` gilt nur für sACN
(Gruppe `239.255.<Universe hoch>.<Universe tief>`). Nur IPv4-Zahlen, keine Hostnamen.

### `fixture "<name>" [at x y z]` … `end`

Genau eine Form pro Fixture, beliebig viele Patches:

| Anweisung | Bedeutung |
|---|---|
| `matrix <spalten> <zeilen> [pitch px py] [start <ecke>] [first rows\|columns] [serpentine]` | Ecke: `top-left`, `top-right`, `bottom-left`, `bottom-right` |
| `strip <anzahl> from x y z to x y z` | gerader Strip |
| `grid <nx> <ny> <nz> [pitch x y z] [order XYZ\|XZY\|YXZ\|YZX\|ZXY\|ZYX] [serpentine]` | 3D-Gitter (Cube) |
| `point x y z` | eine LED einer Punktwolke, in Kabelreihenfolge; beliebig oft |
| `patch <artnet\|sacn\|ddp> destination <n> [universe u] [channel c] [offset o] [order RGB…] [gamma g] [brightness b]` | wohin das Fixture geht |

Koordinaten und Reihenfolge der LEDs: siehe [Mapping](mapping.md). Verteilung auf Universen
und Grenzen (170 LEDs je Universe): siehe [Ausgabe](output.md). `offset` gilt nur für DDP,
`universe` und `channel` nur für Art-Net und sACN.

### `node <name> <typ>` … `end`

`<name>` ist frei wählbar (ohne Punkt) und gilt nur in dieser Datei. Innerhalb des Blocks setzt
`set <parameter> <werte…>` einen Parameter; wie viele Werte folgen, bestimmt der Typ des
Parameters: Wahrheitswert (`true`/`false`), Ganzzahl, Zahl, 2 Zahlen, 3 Zahlen, Farbe (4 Zahlen
r g b a, linear) oder Text. Nicht gesetzte Parameter behalten ihren Standardwert.

`at <x> <y>` legt die Lage des Nodes im Node-Editor fest. Die Zeile ist optional und hat keinen
Einfluss auf das Ergebnis. Fehlt sie bei allen Nodes, ordnet der Editor sie selbst in Spalten an
(Quellen links, Ausgang rechts). Der Writer schreibt `at` immer. Ältere Programmversionen kennen
die Zeile nicht und lehnen solche Dateien ab; die Formatversion bleibt 1, weil neuere Programme
alte Dateien unverändert lesen.

### `connect <node>.<ausgang> -> <node>.<eingang>` und `output <node>.<ausgang>`

Verbindungen werden beim Lesen geprüft (Typen, Zyklen, Eingang schon belegt). `output` ist
Pflicht zum Rendern, nicht zum Prüfen.

## Fehlermeldungen

Jede Meldung nennt die Zeile: `Zeile 7: Node-Typ "wh.x" hat keinen Parameter "tempo"`. Fehler in
einem `fixture`-Block nennen die Zeile des Blockanfangs und den Namen. Was nur mit einer GPU
auffällt (Shader lässt sich nicht übersetzen), meldet der Runner beim Start.

## Speichern

`writeProject` (in `wh_project`) schreibt dasselbe Format. Die Ausgabe ist deterministisch:
Nodes heißen `n1`, `n2`, … in Graph-Reihenfolge, alle Parameter werden ausgeschrieben, damit
sich eine Datei nicht ändert, wenn ein späteres Release einen Standardwert ändert.
Lesen–Schreiben–Lesen ergibt denselben Text; das prüft ein Test.

## Noch nicht in Version 1

Lage und Beschriftung der Nodes im Editor (kommt mit der GUI), Drehung von Fixtures, mehrere
Szenen, Hostnamen als Ziel. Eine neue Formatversion wird die Zahl in der ersten Zeile erhöhen;
ältere Programme melden dann „Formatversion N wird nicht unterstützt“.
