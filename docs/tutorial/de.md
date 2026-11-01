# WhiteHoleFX – Tutorial

Dieses Tutorial führt dich in etwa 30 Minuten vom ersten Start bis zu einem eigenen Effekt auf deinen LEDs. Es gibt
dasselbe Tutorial auch auf [Englisch](en.md), [Französisch](fr.md) und [Spanisch](es.md). Im Programm findest du es unter
**Hilfe → Tutorial**, in der Sprache, die du unter **Hilfe → Sprache / Language** gewählt hast.

## 1. Was WhiteHoleFX macht

WhiteHoleFX steuert LED-Matrizen, Strips, Cubes und beliebige Pixel-Anordnungen. Du baust einen Effekt aus **Nodes**
(Bausteine wie Verlauf, Plasma, Drehen, Spiegeln), die du mit Kabeln verbindest. Der Effekt wird auf der Grafikkarte
berechnet, für jede LED an ihrer Position im Raum abgetastet und per **Art-Net**, **sACN** oder **DDP** an deine
Controller geschickt.

Drei Begriffe brauchst du:

- **Fixture:** ein Stück Hardware mit LEDs, zum Beispiel eine Matrix, ein Strip oder ein 3D-Gitter (Cube).
- **Patch:** wohin ein Fixture seine Daten schickt (Controller-Adresse, Universe, Kanal, Farbreihenfolge).
- **Look:** ein gespeicherter, fertiger Effekt, den du im Live-Modus per Klick abrufst.

## 2. Erster Start

1. Starte `whitehole_studio`.
2. Öffne ein Beispiel: **Datei → Beispiele → Einstieg → wand**. Das Beispiel besteht aus einer Matrix mit 16×8 LEDs und
   einem Strip.
3. Drücke die **Leertaste** (Start). In der Mitte siehst du die Vorschau mit dem laufenden Effekt.

**Wichtig:** Nach jedem Start ist die Netzwerkausgabe **aus**. Das Projekt läuft und zeigt sich in der Vorschau, aber kein
Paket verlässt deinen Rechner. Erst **Wiedergabe → Ausgabe senden** schickt wirklich Daten an die Controller; der Knopf
leuchtet dann rot. So kann nichts versehentlich auf einer fremden Anlage leuchten.

## 3. Die Bühne einrichten (Stage Config)

Wer neu anfängt, beginnt mit der Hardware. Öffne **Stage Config → Layout und Patch …** (F4). Das eigene Fenster hat drei
Seiten:

- **Fixtures:** Liste aller Fixtures. **Fixture hinzufügen …** öffnet einen Dialog, in dem du Typ (Matrix, Strip, 3D-Gitter),
  Größe in x, y und z, Abstand, Lage im Raum und die Verkabelung einträgst: die Startecke, ob das Kabel serpentinenförmig
  läuft und welche Achse am schnellsten zählt. Die kleine 3D-Ansicht oben zeigt dir bei jedem Wert sofort, wie es aussieht.
  Die erste LED am Kabel ist **weiß**, so erkennst du die Startecke.
- **Ziele (Controller):** die Adressen deiner Controller. Eine leere Adresse bedeutet Multicast.
- **Testmodus:** schickt Testbilder (Farbe, Laufbalken, Farbreihenfolge, Lauflicht) direkt an die LEDs, um Verkabelung und
  Patch zu prüfen, ohne dass ein Effekt laufen muss. **Achtung:** Weiß leuchtet mit voller Helligkeit; prüfe vorher die
  Netzteile.
- **Ansicht:** die Anlage im Raum, drehbar, mit eigener Farbe je Fixture und einem Achsenkreuz (x rot, y grün, z blau).

Einen Cube legst du als **3D-Gitter** an, zum Beispiel 8 × 8 × 8 LEDs. Für Strips, die zu einer Säule gewickelt sind, nimmst du
eine **Punktwolke**: **Stage Config → Punktwolke importieren …** liest die LED-Positionen aus einer CSV- oder JSON-Datei.

Tipp: Öffne **Datei → Beispiele → 3D → ausrichtung**. Jede LED zeigt ihre Lage als Farbe (Rot = x, Grün = y, Blau = z). Stimmt
das Bild nicht mit deinem Aufbau überein, änderst du Startecke und Reihenfolge im Fixture.

## 4. Einen Effekt bauen (Node-Editor)

Unten im Fenster liegt das Dock **Nodes**: links die **Palette**, rechts der **Editor**.

- **Node hinzufügen:** aus der Palette auf die Fläche ziehen, doppelklicken, Rechtsklick auf die freie Fläche oder
  **Strg+Leertaste** und tippen („rain“, „mix“, „blend add“).
- **Verbinden:** von einem Ausgang (gefüllt, rechts) zu einem Eingang (Ring, links) ziehen. Die Farbe und Form eines Ports
  zeigen seinen Typ, es lassen sich nur passende Ports verbinden. Ein belegter Eingang wird ersetzt.
- **Ausgang bestimmen:** Rechtsklick auf einen Node → **Als Ausgang setzen**. Dieser Node bestimmt, was auf den LEDs
  erscheint (orange umrandet).
- **Löschen:** Node anklicken, **Entf**. Steckt er in einer Kette (A → X → B), verbindet der Editor A **direkt mit B**, die
  Kette bleibt also heil. Mit **Strg+Z** nimmst du alles zurück.
- **Fehlt ein Eingang,** bleiben die LEDs **dunkel** (die Statuszeile meldet „EINGANG FEHLT“), statt das letzte Bild
  festzuhalten. Sobald der Graph wieder vollständig ist, läuft der Effekt weiter.

**Favoriten:** Die Kategorien der Palette sind beim Start zugeklappt. Per Rechtsklick auf einen Node-Typ nimmst du ihn in die
Gruppe **Favoriten** auf; sie steht oben und wird gespeichert. Beim Suchen klappen die Treffer auf.

Ein kleines Beispiel: Ziehe **Linear Gradient** und **Rotate** auf die Fläche, verbinde `out` des Verlaufs mit `source`
des Rotate, setze Rotate als Ausgang. Dann ziehe **Time** dazu und verbinde `seconds` mit dem Eingang `angle` von Rotate: Der
Verlauf dreht sich.

## 5. Parameter einstellen

Im Dock **Parameter** (rechts) hat jeder Node seine Regler: Zahlen, Auswahlfelder, Ja/Nein-Schalter und Farben. Ein Klick auf
einen Node im Editor zeigt seine Parameter. Regler, die an eine Verbindung angeschlossen sind (zum Beispiel an einen LFO),
sind ausgegraut. Das Symbol ↺ stellt den Standardwert wieder her. Unter **Bearbeiten → Einstellungen → Node-Standardwerte**
legst du fest, mit welchen Werten neue Nodes starten.

## 6. 3D

Alle Generatoren kennen die Tiefe (z). In 2D (eine Matrix) ändert sich dadurch nichts; in einem Cube füllt der Effekt den Raum.
Probiere die fünf 3D-Beispiele unter **Datei → Beispiele → 3D**:

- **ausrichtung:** prüft Achsen und Verkabelung (Position Test).
- **plasma-wuerfel:** Plasma, das durch die Tiefe läuft, mit Farbverlauf und Drehung.
- **scheibe:** eine Scheibe fährt durch den Würfel.
- **regentropfen:** Tropfen als Kugelschalen im Raum.
- **saeule:** ein Strip, schraubenförmig um eine Säule gewickelt.

Die Leiste **Ansicht** wechselt zwischen **2D (bearbeiten)** und den 3D-Ansichten (frei drehbar, Front, Seite, Oben). In 3D drehst du
mit gedrückter Maustaste, Rechtsklick verschiebt, das Mausrad zoomt. Unter **Ansicht → 3D-Darstellung** schaltest du aus-
geschaltete LEDs unsichtbar (Standard, damit du durch den Cube siehst) und das additive Leuchten ein.

## 7. Looks und Live-Modus

Ein Effekt, der dir gefällt, wird mit **Strg+L** als **Look** gespeichert (er steht im Dock **Looks** und in der Projektdatei).
Mit **F9** startest du den **Live-Modus**: Der Editor verschwindet, für jeden Look gibt es einen großen Knopf. Ein Klick (oder
die Taste 1 bis 9) blendet in der eingestellten Zeit über. Mit „Mit GO“ lädst du einen Look nur vor und startest die
Überblendung mit der **Leertaste**. **Esc** beendet den Live-Modus. **Master** regelt die Gesamthelligkeit, **Strg+B** ist der
Blackout.

## 8. Audio und DMX als Eingang

Unter **Bearbeiten → Einstellungen** wählst du Netzwerkkarte, Audiogerät und DMX-Eingang. Audio-Nodes (Pegel, Frequenzband,
Beat, Spektrum, Oszilloskop) reagieren dann auf Musik; DMX-Nodes lesen Werte von einem Lichtpult. Beispiele findest du unter
**Datei → Beispiele → Audio**.

## 9. Sprache wechseln

**Hilfe → Sprache / Language** bietet Deutsch, English, Français und Español an. Die Wahl wird gespeichert; die Oberfläche
startet auf Wunsch sofort neu und öffnet dein Projekt wieder. Namen von Nodes und Parametern bleiben englisch, damit
Anleitungen und Projekte in allen Sprachen zusammenpassen.

## 10. Wenn etwas nicht klappt

- **Kein Licht auf den LEDs:** Ist **Ausgabe senden** an? Stimmt die Controller-Adresse unter Ziele? Hilft der Testmodus?
- **Falsche Farben:** Farbreihenfolge im Patch (RGB, GRB …) ändern, oder im Testmodus „Farbreihenfolge“ laufen lassen.
- **Bild steht Kopf oder ist gespiegelt:** Startecke und `flip`-Optionen im Fixture ändern.
- **Ruckeln:** Bildrate verringern oder das Fenster verkleinern; bei Live-Wechseln „Mit GO“ vorladen.
- **Etwas anderes:** **Hilfe → Fehler melden oder Idee vorschlagen …** öffnet GitHub.

Weiter geht es in den ausführlichen Dokumenten im Ordner `docs/`: Oberfläche, Projektdatei, Field-Nodes, Mapping, Ausgabe,
Audio und Value-Graph.
