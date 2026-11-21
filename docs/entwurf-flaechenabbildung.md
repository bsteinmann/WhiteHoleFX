# Entwurf: LEDs als gefaltete 2D-Fläche abbilden (Würfel-Außenhaut)

Warum es diese Datei gibt: Der Nutzer will Effekte auf einem hohlen Würfel (sechs 32×32-Matrizen, `examples/3d/wuerfel-hohl.whfx`)
so sehen, als wäre der Würfel eine **gefaltete 2D-Form**. Heute gilt das Layout als 3D-Form; flächenbasierte Effekte
(Labyrinth, Text, Pong, …) sind dort großteils unsichtbar oder verzerrt. Dies ist ein **Entwurf zur Entscheidung**,
noch nichts davon ist gebaut (Stand 21.11.2026).

## 1. Ausgangslage (im Code geprüft)

- `Layout::bake` (`libs/wh_mapping/src/layout.cpp`) berechnet für jede LED die Weltposition `world` und daraus die
  Koordinate `unit` im Einheitswürfel [0,1]³ (`NormalizeMode`: `stretch` oder `keep-aspect`). Genau diese `unit`-Werte
  sieht der Effekt als `p`.
- Eine Achse ohne Ausdehnung ergibt 0. Räumlich gilt die Karte, sobald irgendeine LED `p.z > 0` hat
  (`sampling_render_stage.cpp`, `setVolume`); daraus wird `@volume`.
- Nodes mit Parameter `space` (*Flat / Volume / Auto*) wählen damit 2D oder 3D. *Flat* rechnet mit `p.xy`.
- Beim hohlen Würfel ist `p.z` ungleich 0, also 3D. Wählt man *Flat*, sieht jede Fläche nur die Projektion von vorn:
  Vorder- und Rückseite zeigen das Bild, Seiten und Oben/Unten zeigen Streifen (eine Achse ist dort konstant).
- `@columns`, `@rows`, `@aspect`, `@aspectZ` werden aus der LED-Karte abgeleitet (`sampling_render_stage.cpp`). Sie müssen
  zur gewählten Abbildung passen, sonst verzerrt sich z. B. das Labyrinth wieder.

## 2. Vorschlag: eine projektweite Einstellung „Abbildung“

Neue Projekt-Einstellung in der `settings`-Zeile, neben `normalize`:

```
settings ... projection volume|faces|box
```

| Wert | Bedeutung | Stand |
|---|---|---|
| `volume` | wie bisher: `unit` aus der 3D-Position | Standard, alte Projekte unverändert |
| `faces` | jedes Fixture bekommt sein **eigenes** 2D-Bild: `unit.xy` = Lage in der eigenen Matrix, `unit.z` = 0 | neu |
| `box` | **von außen projiziert** (Box-Mapping): Fläche nach Richtung der Außennormalen, siehe 3. | neu |

`projection` ändert nur `unit` (also `p` im Effekt) und was daraus folgt. Die **Weltpositionen** und damit die
3D-Vorschau, die Ausgabe-Adressen und das Patchen bleiben unberührt. `@volume` wird bei `faces`/`box` falsch,
alle *Auto*-Nodes wählen dadurch von selbst ihre 2D-Form.

Die Abbildung gehört in `Layout::bake` (ein zusätzlicher Parameter neben `NormalizeMode`), **nicht** in die einzelnen
Nodes. Sonst müssten alle Nodes mit `space` angepasst werden.

## 3. Die beiden neuen Abbildungen im Detail

**`faces` (jede Matrix eigenes Bild).** `unit.xy` = (Spalte, Zeile) des Fixtures, normiert auf 0..1. Einfach, robust,
unabhängig von der Lage im Raum, funktioniert für jede Anordnung von Matrizen. Nachteil: Alle sechs Würfelflächen zeigen
**dasselbe** Bild (ein Plasma sähe auf jeder Seite gleich aus), und es gibt keinen Zusammenhang über die Kanten.

**`box` (von außen projiziert).** Jede LED bekommt die Fläche nach ihrer Richtung vom Mittelpunkt der Gesamtbox aus
(größte Achse der Abweichung gewinnt):

| Fläche | 2D-Koordinate |
|---|---|
| vorne/hinten (±z) | (x, y) |
| links/rechts (±x) | (z, y) |
| oben/unten (±y) | (x, z) |

Auf einem Würfel sieht man damit jede Seite als eigenen Ausschnitt desselben Raums. Die Kanten passen **nicht**
nahtlos (das ist bei jeder 2D-Abbildung eines Würfels so, nur ein Netz mit gemeinsamer Leinwand könnte das lösen).

Eckfälle: Eine LED, die genau auf der Kante läge, kommt bei Gleichstand der Achsen an die Fläche der
Achse mit der Reihenfolge x vor y vor z. Auf einer echten Hülle mit Matrizen mit Pitch 1 und Abstand zur Kante liegen
LEDs nie genau auf der Kante, der Fall ist also nur theoretisch.

**Nicht Teil dieses Entwurfs, bewusst:** ein *Netz* (Kreuz-Layout auf einer gemeinsamen 2D-Leinwand mit Übergang über
die Kanten). Es bräuchte pro Fixture eine Lage in der Leinwand (neues Feld am Fixture, neuer Dialog) und ist deutlich größer.
Falls gewünscht, ist es ein eigener Schritt nach `box`.

## 4. Folgen für `@columns`, `@rows`, `@aspect`

Heute aus der Karte geschätzt. Bei `faces`/`box`:
- `@aspect` = 1 für quadratische Flächen; allgemein Breite/Höhe der **Fläche** (nicht der Gesamtbox).
- `@columns`/`@rows` = Spalten/Zeilen der Matrizen, wenn **alle** Fixtures gleich sind (hier 32 × 32); sonst 0
  (unbekannt), gleiche Konvention wie bisher („0 = unbekannt“, Nodes mit `cells = 0` fallen dann auf ihren Standard zurück).
- `@aspectZ` = 1.

## 5. Umsetzung in Schritten (alle klein, einzeln mergebar)

1. `mapping::Projection`-Enum und Parameter in `Layout::bake`; Tests in `tests/mapping/`: sechs Matrizen als hohler Würfel
   → alle `unit.z == 0`, jede Fläche deckt 0..1 ab (`faces` bzw. `box`), `volume` bleibt bitgleich zu heute.
2. Projektdatei: `settings … projection …` lesen/schreiben (`project_parser`, `project_writer`), Test mit Rundlauf,
   `docs/project-format.md`.
3. Durchreichen an `sampling_render_stage` (`@columns/@rows/@aspect`), GPU-Test nach dem Muster von `cells_auto` auf dem Würfel.
4. Studio: Auswahl in Stage Config (Layout-Seite), neue `tr()`-Strings mit Übersetzungen en/fr/es/uk, Rauchtest.
5. Beispiel `examples/3d/wuerfel-hohl.whfx` bekommt `projection box`, Doku `docs/studio.md`, `docs/field-nodes.md`, CHANGELOG.

Schritt 1 bis 3 sind reiner Kern ohne Qt (lokal prüfbar), Schritt 4 lässt sich nur über die CI prüfen.

## 6. Risiken

- **Alte Projekte:** bleiben unverändert (`volume` als Standard).
- **Verzerrte Bilder:** `@aspect`/`@columns` sind der heikle Teil (siehe 4); dafür der GPU-Test in Schritt 3.
- **Vorschau:** Die 3D-Vorschau zeigt weiter den Würfel; die Abbildung sieht man nur im Bild auf den LEDs. Ein Hinweis in der
  Layout-Seite wäre hilfreich.
- **Mischanlagen** (Würfel plus freistehende Matrix): `box` nimmt dann die Gesamtbox als Bezug. Für solche Fälle wäre `faces`
  die bessere Wahl. Eine Einstellung pro Gruppe wäre denkbar, ist hier bewusst nicht vorgesehen.

## 7. Entscheidungen, die der Nutzer treffen muss

1. Reicht für den ersten Schritt `faces` + `box`, oder soll zuerst nur `box` (der gewünschte Fall) gebaut werden?
2. Name der Einstellung im Studio: „Abbildung“, „Projektion“ oder anders?
3. Soll die Einstellung **projektweit** (wie vorgeschlagen) oder pro Gruppe/Fixture sein?
4. Ist ein **Netz** (gemeinsame Leinwand über die Kanten) später gewünscht, oder reicht die Außenprojektion?
