# Entwurf: LEDs als gefaltete 2D-Fläche abbilden (Würfel-Außenhaut)

Warum es diese Datei gibt: Der Nutzer will Effekte auf einem hohlen Würfel (sechs 32×32-Matrizen, `examples/3d/wuerfel-hohl.whfx`)
so sehen, als wäre der Würfel eine **gefaltete 2D-Form**. Heute gilt das Layout als 3D-Form; flächenbasierte Effekte
(Labyrinth, Text, Pong, …) sind dort großteils unsichtbar oder verzerrt. Dies ist ein **Entwurf zur Entscheidung**,
noch nichts davon ist gebaut (Stand 21.11.2026).

## 0. Entscheidungen des Nutzers (22.11.2026)

1. Beide Abbildungen: **`faces` und `box`** (zusätzlich zum bisherigen `volume`).
2. Name im Studio: **„Projektion“** (Einstellung `projection`).
3. **Pro Gruppe/Fixture**, nicht projektweit (siehe 2: die Einstellung hängt am Fixture, die Gruppe setzt sie für alle ihre Fixtures).
4. **Außenprojektion**, kein Netz mit gemeinsamer Leinwand. Ein Netz ist damit vom Tisch, solange der Nutzer es nicht neu anfragt.

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

## 2. Die Einstellung „Projektion“ (pro Fixture, per Gruppe setzbar)

Jedes Fixture bekommt ein Feld `projection`, in der Projektdatei eine Zeile im `fixture`-Block (Standard `volume`, wird dann
nicht geschrieben, alte Projekte bleiben bitgleich):

```
fixture "Vorne" at 0 0 0
  matrix 32 32
  projection box
end
```

| Wert | Bedeutung |
|---|---|
| `volume` | wie bisher: `unit` aus der 3D-Position (Standard) |
| `faces` | das Fixture bekommt sein **eigenes** 2D-Bild: `unit.xy` = Lage in der eigenen Matrix, `unit.z` = 0 |
| `box` | **von außen projiziert** (Box-Mapping), siehe 3. |

**Warum am Fixture und nicht an der Gruppe:** Eine Gruppe ist nur ein Name für eine Menge von Fixtures (z. B. zum Zielen von
Output-Nodes); ein Fixture kann in mehreren Gruppen stehen, und widersprüchliche Gruppeneinstellungen wären nicht eindeutig.
Autoritativ ist daher das Fixture. Für die Bedienung bekommt die Gruppe eine Aktion „Projektion für alle Fixtures der Gruppe
setzen“, die nur die Fixtures ändert. Das erfüllt „Gruppe/Fixture“ ohne Mehrdeutigkeit.

`projection` ändert nur `unit` (also `p` im Effekt) und was daraus folgt. Die **Weltpositionen** und damit die 3D-Vorschau,
die Ausgabe-Adressen und das Patchen bleiben unberührt.

Die Abbildung gehört in `Layout::bake` (`unit` wird dort schon pro LED berechnet, also ist ein Wert pro Fixture natürlich),
**nicht** in die einzelnen Nodes. Sonst müssten alle Nodes mit `space` angepasst werden.

**Mischanlagen werden damit möglich:** Ein Würfel mit `box` neben einer freistehenden Matrix mit `faces` neben einem
räumlichen Strip-Gewirr mit `volume`. Folgen davon:
- Der Bezug für `box` ist die **Box aller Fixtures mit `box`**, nicht der ganzen Anlage (sonst läge ein weit entfernter Strip im
  Bezugsrahmen des Würfels).
- `@volume` gilt global für den Frame: wahr, sobald irgendeine LED `p.z > 0` hat, also schon, wenn ein einziges Fixture `volume`
  ist und Tiefe hat. *Auto*-Nodes rechnen dann in 3D, mit `p.z = 0` auf den projizierten Fixtures. Ein 2D-Effekt sieht dort
  weiter richtig aus; ein 3D-Effekt (Kugel) schneidet die projizierten Flächen in der Ebene z = 0. Das ist so gewollt und gehört
  in die Doku.

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

**Bewusst nicht Teil des Entwurfs (Entscheidung 4):** ein *Netz* (Kreuz-Layout auf einer gemeinsamen 2D-Leinwand mit Übergang über
die Kanten). Es bräuchte pro Fixture eine Lage in der Leinwand und ist deutlich größer.

## 4. Folgen für `@columns`, `@rows`, `@aspect`

Heute aus der Karte geschätzt. Bei `faces`/`box`:
- `@aspect` = 1 für quadratische Flächen; allgemein Breite/Höhe der **Fläche** (nicht der Gesamtbox).
- `@columns`/`@rows` = Spalten/Zeilen der Matrizen, wenn **alle** projizierten Fixtures (`faces`/`box`) gleich sind (hier 32 × 32);
  sonst 0 (unbekannt), gleiche Konvention wie bisher („0 = unbekannt“, Nodes mit `cells = 0` fallen dann auf ihren Standard zurück).
  Das sind Frame-weite Werte (ein Wert pro Bild), nicht pro Fixture: Bei gemischten Größen muss man den Parameter von Hand setzen.
- `@aspectZ` = 1.

## 5. Umsetzung in Schritten (alle klein, einzeln mergebar)

1. `mapping::Projection` (`volume|faces|box`) als Feld in `Fixture`; `Layout::bake` rechnet `unit` pro Fixture. Tests in
   `tests/mapping/`: sechs Matrizen als hohler Würfel mit `box` bzw. `faces` → alle `unit.z == 0`, jede Fläche deckt 0..1 ab;
   `volume` bleibt bitgleich zu heute; Mischfall (Würfel `box` + Matrix `faces`).
2. Projektdatei: `projection` im `fixture`-Block lesen/schreiben (`project_parser`, `project_writer`), Rundlauf-Test,
   Fehlerfall (unbekannter Wert), `docs/project-format.md`.
3. Durchreichen an `sampling_render_stage` (`@columns/@rows/@aspect`), GPU-Test nach dem Muster von `cells_auto` auf dem Würfel.
4. Befehle im `Player` (`setFixtureProjection`, Gruppe → alle Fixtures, ein Undo-Schritt), Tests.
5. Studio: Auswahl „Projektion“ im Fixture-Formular und Aktion an der Gruppe, neue `tr()`-Strings mit Übersetzungen
   en/fr/es/uk, Rauchtest (nur über die CI prüfbar).
6. Beispiel `examples/3d/wuerfel-hohl.whfx` bekommt `projection box` an allen Flächen, Doku `docs/studio.md`,
   `docs/field-nodes.md`, CHANGELOG.

Schritt 1 bis 4 sind reiner Kern ohne Qt (lokal prüfbar), Schritt 5 lässt sich nur über die CI prüfen.
Hinweis zum Aufwand: Schritt 1 bis 4 sind überschaubar (mittleres Modell, mittlerer Denkaufwand); mehr Denkaufwand lohnt für
`@columns/@rows/@aspect` in Schritt 3.

## 6. Risiken

- **Alte Projekte:** bleiben unverändert (`volume` als Standard).
- **Verzerrte Bilder:** `@aspect`/`@columns` sind der heikle Teil (siehe 4); dafür der GPU-Test in Schritt 3.
- **Vorschau:** Die 3D-Vorschau zeigt weiter den Würfel; die Abbildung sieht man nur im Bild auf den LEDs. Ein Hinweis in der
  Layout-Seite wäre hilfreich.
- **Mischanlagen:** siehe 2 (Bezugsbox für `box`, `@volume` global). Ungewohnt bleibt, dass ein 3D-Effekt projizierte Flächen in z = 0 schneidet.

## 7. Noch offen (kleine Fragen für den Start von Schritt 1)

1. Bezug von `box`: die Box der Fixtures mit `box` (hier vorgeschlagen) — oder lieber die Box der ganzen Anlage?
2. Reihenfolge der Studio-Bedienung: Auswahl im Fixture-Formular zuerst, die Gruppen-Aktion danach (Schritt 5 in zwei Teilen)?
