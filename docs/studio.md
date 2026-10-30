# Oberfläche (whitehole_studio)

Qt-6-Fenster mit Live-Vorschau. Gebaut wird es nur auf Wunsch, weil Qt vorhanden sein muss:

```bash
# Debian/Ubuntu: sudo apt install qt6-base-dev libgl1-mesa-dev
cmake -S . -B build -DWHITEHOLEFX_BUILD_STUDIO=ON -DWHITEHOLEFX_WEBGPU=ON -DWGPU_NATIVE_ROOT=/pfad/zu/wgpu
cmake --build build --target whitehole_studio
./build/apps/whitehole_studio/whitehole_studio examples/wand.whfx
```

Ohne `WHITEHOLEFX_WEBGPU` zeigt das Fenster nur das Layout, rendern geht dann nicht.

## Bedienung

| Aktion | Taste |
|---|---|
| Projekt öffnen (auch per Drag & Drop einer `.whfx`-Datei) | Strg+O |
| Neu laden (Datei hat sich geändert; läuft es, startet es danach wieder) | F5 |
| Start / Stopp | Leertaste |
| Ausgabe senden | Menü Wiedergabe |

**Sicherheit:** Die Netzwerkausgabe ist nach jedem Start **aus**. Das Projekt läuft und zeigt sich
in der Vorschau, aber kein Paket verlässt den Rechner, bis „Ausgabe senden“ eingeschaltet ist. Die
Statuszeile zeigt rechts, ob die Ausgabe an oder aus ist und wie viele Pakete unterdrückt wurden.

Die Vorschau zeigt vor dem Start das Layout (Farbton je Fixture, heller entlang des Kabels), danach
die Farben des laufenden Effekts. Fehler beim Laden erscheinen als Dialog mit der Zeilennummer aus
der Datei.

## Aufbau

Fenster und Menüs liegen in `apps/whitehole_studio` und enthalten keine Logik. Laden, Starten,
Frames und Vorschaubild erledigt `runtime::Player` (Modul `wh_runtime`, ohne Qt, ohne Bildschirm
testbar). `runtime::GatedPacketSink` ist der Schalter für die Netzwerkausgabe.

## Prüfung ohne Bildschirm

```
QT_QPA_PLATFORM=offscreen whitehole_studio wand.whfx --run --software --frames 40 --grab fenster.png
```

öffnet das Fenster unsichtbar, lässt 40 Frames laufen, speichert ein Bild des Fensters und beendet
sich. Die CI macht das und legt die Bilder als Artefakt „fenster“ ab.

## Parameterregler

Das Dock „Parameter“ (Menü Ansicht) listet jeden Node mit seinen Parametern:

- Zahlen mit Wertebereich: Regler plus Zahlenfeld, ohne Bereich nur Zahlenfeld
- Ganzzahlen, Ja/Nein, Vektoren (2D/3D), Text
- Farben: Farbfeld, öffnet den Farbdialog (Eingabe in sRGB, gespeichert wird linear)
- ↺ setzt auf den Standardwert zurück
- Parameter, die von einem anderen Node gesteuert werden (z. B. ein LFO), sind ausgegraut

Änderungen wirken im nächsten Frame. Werte werden auf den erlaubten Bereich begrenzt.
„Speichern unter“ (Strg+Umschalt+S) schreibt die Projektdatei neu. Achtung: Der Writer erzeugt die
Datei kanonisch, Kommentare und Formatierung einer von Hand geschriebenen Datei gehen dabei
verloren. Ein Beispiel zum Ausprobieren: `examples/regler.whfx`.

Die CI verstellt mit `--exercise` jeden Regler einmal und prüft, dass der Player keinen Fehler meldet.

## Node-Editor

Das Dock „Nodes“ (unten, Menü Ansicht) zeigt den Effektgraphen. Eingänge stehen links, Ausgänge
rechts; die Farbe eines Ports zeigt den Typ (Blau Field, Grün Zahl, Rosa Farbe, …). Parameter
stehen nicht im Node, sondern im Dock „Parameter“; beide benutzen dieselbe Nummerierung
(„Linear Gradient 2“).

| Aktion | Bedienung |
|---|---|
| Node hinzufügen | Rechtsklick auf freie Fläche, Menü nach Kategorien |
| Node verschieben | ziehen; die Lage wird in der Projektdatei gespeichert (`at`) |
| verbinden | von einem Port zu einem passenden anderen ziehen (in beide Richtungen); ein belegter Eingang wird ersetzt |
| löschen | anklicken (Strg oder Rahmen für mehrere), Entf; oder Rechtsklick |
| Ausgang wählen | Rechtsklick auf einen Field-Ausgang, „Als Ausgang setzen“; der Node ist dann orange umrandet |
| Ansicht | Mausrad zoomt, mittlere Taste verschiebt, Taste F zeigt alles |

Änderungen wirken sofort, auch während das Projekt läuft. Ist der Graph zwischendurch unvollständig
oder ungültig (Typen passen nicht, Zyklus), meldet die Statuszeile den Grund und der letzte gute
Effekt läuft weiter. Der Node, an dem der Ausgang hängt, lässt sich nicht löschen; setze zuerst
einen anderen. Ein Wechsel des Ausgangs baut die Ausgabe einmal neu auf (kurze Unterbrechung).

Ungespeicherte Änderungen zeigt ein Stern im Fenstertitel; beim Öffnen, Neuladen und Beenden wird
nachgefragt. Dateien ohne Positionen (von Hand geschrieben) ordnet der Editor selbst in Spalten
an. Die Anordnung ist erst gespeichert, wenn du „Speichern unter“ wählst.

Die CI prüft mit `--exercise` auch den Editor: Zwei Nodes anlegen, ein Kabel auf dem Weg der Maus
ziehen (Treffer auf Ports), verschieben, Kabel und Nodes wieder löschen.

## Layout-Editor

Das Dock „Layout“ (rechts, teilt sich den Platz mit „Parameter“) hat zwei Seiten.

**Fixtures:** Liste mit „Matrix +“, „Strip +“ und „Entfernen“. Ein neues Fixture bekommt einen freien
Platz im Layout und das erste freie Universe (Art-Net, Ziel 0, 170 LEDs je Universe). Für das
gewählte Fixture gibt es ein Formular:

- Name und Ursprung (x y z)
- Matrix: Spalten, Zeilen, Abstand, Ecke des Kabelanfangs, Laufrichtung, Serpentine
- Strip: LED-Zahl, Anfang, Ende
- 3D-Gitter: Anzahl je Achse, Abstand, Achsenreihenfolge, Serpentine
- Punktwolken lassen sich nur in der Datei ändern
- Patch: Protokoll (Art-Net, sACN, DDP), Ziel, Universe, erster Kanal, DDP-Offset,
  Farbreihenfolge, Gamma, Helligkeit; „an die Ausgabe schicken“ schaltet den Patch ein und aus

**Ausgabe:** Tabelle der Ziele (Adresse, Port; leere Adresse = Multicast, Port 0 = Standard) mit
„Ziel +“ und „Letztes Ziel −“, darunter Bildrate, Abbildung auf den Würfel (Stretch oder
KeepAspect), sACN-Priorität und Quellname.

Zahlenfelder übernehmen einen Wert bei Enter, Pfeiltasten oder Fokuswechsel. Jede Änderung wird
samt Ausgabeplan geprüft. Passt etwas nicht (Universe schon belegt, Ziel gibt es nicht, ungültiges
Maß, doppelter Name), zeigt die Statuszeile den Grund und das Feld springt auf den alten Wert
zurück. Das letzte gepatchte Fixture lässt sich nicht entfernen, und ein Ziel, auf das ein Patch
zeigt, nicht löschen. Laufende Ausgabe wird nach einer Änderung einmal neu aufgebaut (kurze
Unterbrechung). Alle Schritte lassen sich zurücknehmen.

## Rückgängig und Wiederholen

Menü Bearbeiten, Strg+Z und Strg+Umschalt+Z (Windows auch Strg+Y). Zurückgenommen werden
Parameter, Nodes, Kabel, Ausgang und Lage; bis zu 200 Schritte. Viele Änderungen hintereinander am
selben Regler oder am selben Node (Ziehen) zählen als ein Schritt. Laden leert die Listen,
Speichern nicht. Der Stern im Titel bleibt nach einem Undo stehen, auch wenn der gespeicherte Stand
erreicht ist.

## Noch nicht enthalten

Punktwolken bearbeiten, Fixtures im Vorschaubild verschieben oder drehen, Kopieren/Einfügen von
Nodes und Fixtures. Solches entsteht bis dahin in der Projektdatei (siehe
[Projektdatei](project-format.md)); „Neu laden“ übernimmt Änderungen sofort.
