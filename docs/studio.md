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
| Neues Projekt (Vorlage: 16×8-Matrix mit Farbverlauf, Ausgabe auf den eigenen Rechner) | Strg+N |
| „Matrix +“ oder „Strip +“ ohne geladenes Projekt | legt ein neues Projekt an, dessen erstes Fixture das gewünschte ist |
| Projekt öffnen (auch per Drag & Drop einer `.whfx`-Datei) | Strg+O |
| Speichern (ohne Dateinamen: fragt nach dem Namen) | Strg+S |
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

- Zahlen mit Wertebereich: Regler plus Zahlenfeld, ohne Bereich nur Zahlenfeld. Die Pfeile des
  Zahlenfelds gehen in sinnvollen Schritten (ein Hundertstel der Spanne gerundet auf 1, 2 oder 5;
  ohne Bereich 1, bei Winkeln in Grad 5); gedrückt halten wird schneller
- Bei „Time“ und „LFO“ stellen die Parameter „… slider min“ und „… slider max“ den Bereich des
  Reglers ein (Geschwindigkeit −4 bis 4, Frequenz 0 bis 5 Hz sind die Voreinstellungen). Das ist
  nur eine Bedienhilfe: im Zahlenfeld lassen sich auch Werte außerhalb eintippen. Allgemein gilt:
  Hat ein Node die Zahlen `<name>`, `<name>Min` und `<name>Max`, benutzt der Regler von `<name>` die
  beiden anderen als Bereich
- Ganzzahlen, Ja/Nein, Vektoren (2D/3D), Text
- Farben: Farbfeld (mit Schachbrett dahinter, damit man Durchsichtiges sieht), öffnet den Farbdialog
  (mit Alpha-Regler; gespeichert wird der Wert so, wie man ihn auf dem Bildschirm sieht, siehe „Helligkeit“). Das Kästchen „Transparent“ setzt
  Alpha auf 0, die Farbe bleibt gemerkt; abhaken stellt Alpha 1 her (siehe „Transparenz“)
- ↺ setzt auf den Standardwert zurück
- Parameter, die von einem anderen Node gesteuert werden (z. B. ein LFO), sind ausgegraut

Änderungen wirken im nächsten Frame. Werte werden auf den erlaubten Bereich begrenzt.
„Speichern“ (Strg+S) und „Speichern unter“ (Strg+Umschalt+S) schreiben die Projektdatei neu. Achtung: Der Writer erzeugt die
Datei kanonisch, Kommentare und Formatierung einer von Hand geschriebenen Datei gehen dabei
verloren. Ein Beispiel zum Ausprobieren: `examples/regler.whfx`.

Die CI verstellt mit `--exercise` jeden Regler einmal und prüft, dass der Player keinen Fehler meldet.

### Helligkeit und Verläufe

Effekt-Werte sind so gemeint, wie man sie sieht: 0,5 ist ein mittleres Grau, ein Verlauf von 0 bis 1
sieht gleichmäßig aus. Die Ausgabe potenziert die Werte mit dem Gamma des Patches (Standard 2,2),
weil eine LED linear strahlt (siehe `docs/output.md`); die Vorschau macht dasselbe nach und
zeigt das Licht als sRGB-Bild. Früher behandelte die Vorschau die Werte als Licht und zeigte
Verläufe viel zu hell und „nicht linear“.

### Transparenz

Jede Farbe hat einen Alpha-Wert (0 = durchsichtig, 1 = deckend). Verläufe, Kugel, Welle sowie die
Nodes Mix und Add rechnen damit „vormultipliziert“: Ein Verlauf von Transparent nach Weiß blendet
also Weiß ein, er wird nicht grau. Beim Senden und in der Vorschau gilt: Die Farbe wird mit Alpha
multipliziert, **transparent heißt LED aus**. Ein Hintergrund, über dem etwas liegt, scheint durch
Transparent hindurch, sobald die Ebene darunter mit Mix oder Add dazugerechnet wird.

### Node anklicken

Klickt man im Node-Editor einen Node an, springt das Dock „Parameter“ nach vorn, scrollt zu seinen
Parametern und umrandet sie orange. Auch nach Änderungen im Graphen (Kabel, neue Nodes) bleibt die
Auswahl erhalten. Ein Node ohne Parameter (z. B. Rotate ohne Regler) meldet das in der Statuszeile.

## Node-Editor

Das Dock „Nodes“ (unten, Menü Ansicht) zeigt den Effektgraphen. Eingänge stehen links, Ausgänge
rechts; die Farbe eines Ports zeigt den Typ (Blau Field, Grün Zahl, Rosa Farbe, …). Parameter
stehen nicht im Node, sondern im Dock „Parameter“; beide benutzen dieselbe Nummerierung
(„Linear Gradient 2“).

| Aktion | Bedienung |
|---|---|
| Node hinzufügen | aus der **Palette** links im Dock ziehen oder doppelklicken (siehe unten); Rechtsklick auf freie Fläche, Menü nach Kategorien; schneller: Strg+Leertaste, tippen („rain“, „mix“, „blend add“), Pfeiltasten, Enter. Der Node entsteht an der Maus |
| Node verschieben | ziehen; die Lage wird in der Projektdatei gespeichert (`at`) |
| verbinden | von einem Port zu einem passenden anderen ziehen (in beide Richtungen); ein belegter Eingang wird ersetzt |
| löschen | anklicken (Strg oder Rahmen für mehrere), Entf; oder Rechtsklick; oder den Node auf die Palette ziehen und loslassen (sie färbt sich rot) |
| kopieren, einfügen, duplizieren | Strg+C, Strg+V (an der Mausposition), Strg+D; auch im Rechtsklick-Menü. Kabel zwischen den ausgewählten Nodes kommen mit, Kabel nach draußen nicht. Die Kopie ist danach ausgewählt, ein Schritt Rückgängig nimmt sie komplett zurück |
| Ausgang wählen | Rechtsklick auf einen Field-Ausgang, „Als Ausgang setzen“; der Node ist dann orange umrandet |
| Ausgangs-Node | Node „Output“ (Kategorie Output) einfügen: er wird automatisch zum Ausgang und hängt sich hinter den bisherigen. Mit Rechteck bespielt er nur einen Teil der Leinwand |
| Ansicht | Mausrad zoomt, mittlere Taste verschiebt, Taste F zeigt alles |

Änderungen wirken sofort, auch während das Projekt läuft. Ist der Graph zwischendurch unvollständig
oder ungültig (Typen passen nicht, Zyklus), meldet die Statuszeile den Grund und der letzte gute
Effekt läuft weiter. Löschst du den Node, an dem der Ausgang hängt, rückt der Ausgang auf den Node
davor (die Statuszeile sagt es). Gibt es keinen, bleibt das Projekt ohne Ausgang und „Start“ meldet
das, bis du per Rechtsklick einen neuen setzt. Ein Wechsel des Ausgangs baut die Ausgabe einmal neu
auf (kurze Unterbrechung).

Ungespeicherte Änderungen zeigt ein Stern im Fenstertitel; beim Neu, Öffnen, Neuladen und Beenden wird
nachgefragt. Dateien ohne Positionen (von Hand geschrieben) ordnet der Editor selbst in Spalten
an. Die Anordnung ist erst gespeichert, wenn du „Speichern unter“ wählst.

Die CI prüft mit `--exercise` auch den Editor: Zwei Nodes anlegen, ein Kabel auf dem Weg der Maus
ziehen (Treffer auf Ports), verschieben, Kabel und Nodes wieder löschen.

## Layout-Editor

Die Bühne wird einmal am Anfang eingerichtet und danach selten angefasst. Deshalb hat sie ein eigenes Menü **Stage Config**
(zwischen Bearbeiten und Ansicht) mit: Fixture hinzufügen …, Punktwolke importieren …, Layout und Patch … (F4) und Testmodus ….
Layout, Testmodus und Ansicht liegen im eigenen Fenster **Stage Config**. Es ist kein Dock: es öffnet sich bei Bedarf, lässt sich auf
einen zweiten Bildschirm ziehen und merkt sich Größe und Platz. Es ist nicht modal, die Vorschau im Hauptfenster bleibt
bedienbar. Im Live-Modus ist das Menü gesperrt.

**Bühnenansicht:** Die Seite „Ansicht“ zeigt die Anlage im Raum, drehbar wie die 3D-Vorschau (Ziehen = drehen, Rad = Zoom,
Doppelklick = zurück) mit Knöpfen für Perspektive, Front, Seite und Oben. Jedes Fixture hat einen eigenen Farbton und wird
entlang des Kabels heller; die **erste LED am Kabel ist weiß**, so erkennst du Anfang und Richtung des Strangs. Rahmen und
Achsenkreuz (x rot, y grün, z blau, an der Ecke mit den kleinsten Werten) helfen beim Ausrichten. Gewählte Fixtures bleiben
kräftig, die übrigen werden dunkel. Dieselbe Ansicht gibt es klein oben im Tab „Fixtures“, neben dem Formular: Wert ändern,
sofort sehen. Beide zeigen nur die Anlage, nicht den laufenden Effekt, und aktualisieren sich bei jeder Änderung.

Die Seite „Layout“ hat drei Teile.

**Fixture hinzufügen:** Der Dialog (Menü Stage Config oder Knopf „Hinzufügen …“ in der Liste) fragt alles in einem Schritt:

- **Form:** Matrix (Fläche), Strip (Linie) oder 3D-Gitter (Cube, Vorhang, Säule).
- **Größe:** beim Gitter die Anzahl LEDs in x, y und z, dazu der Abstand je Achse. Eine Zeile darunter nennt LED-Zahl, Größe
  in Welteinheiten, Kanäle und Universes. Ist die Form ungültig oder zu groß (mehr als 4 Mio. LEDs), sagt sie warum und
  „Hinzufügen“ ist gesperrt.
- **Position:** x y z im Raum (vorbelegt: rechts neben dem letzten Fixture).
- **Verkabelung:** Matrix: Ecke des Kabelanfangs, Laufrichtung, Serpentine. Gitter: Zählrichtung (welche Achse am schnellsten
  läuft, z. B. x → y → z), Serpentine und Startecke (rechts, oben, vorn statt links, unten, hinten).
- **Patch:** automatisch (nächstes freies Universe), manuell (Protokoll, Ziel, Universe, erster Kanal, Farbreihenfolge) oder
  nicht senden.

**Fixtures:** Liste mit „Hinzufügen …“, „Import …“ und „Entfernen“. Ein Fixture aus dem Dialog landet dort, wo der Dialog es
hinsetzt; ohne eigene Angabe bekommt es das erste freie Universe (Art-Net, Ziel 0, 170 LEDs je Universe). Für das
gewählte Fixture gibt es ein Formular:

- Name und Ursprung (x y z)
- Matrix: Spalten, Zeilen, Abstand, Ecke des Kabelanfangs, Laufrichtung, Serpentine
- Strip: LED-Zahl, Anfang, Ende
- 3D-Gitter: Anzahl je Achse, Abstand, Achsenreihenfolge, Serpentine, Startecke (rechts/oben/vorn)
- Punktwolken lassen sich nur in der Datei ändern
- Patch: Protokoll (Art-Net, sACN, DDP), Ziel, Universe, erster Kanal, DDP-Offset,
  Farbreihenfolge, Gamma, Helligkeit; „an die Ausgabe schicken“ schaltet den Patch ein und aus

**Import von Positionen:** „Import …“ lädt LED-Positionen aus einer CSV- oder JSON-Datei als Punktwolke
(Format und Optionen: `docs/mapping.md`, Abschnitt „Import von Positionen“). Die Reihenfolge in der Datei
ist die Reihenfolge am Kabel. Beispiel: `examples/bogen.csv`.

**Mehrere Fixtures:** Die Liste erlaubt mehrere Einträge (Strg oder Umschalt). Die Knöpfe unter der
Liste richten die gewählten Fixtures aus (links, rechts, oben, unten, waagrecht oder senkrecht
mittig; Bezug ist die Gesamtbox der Auswahl) und verteilen drei oder mehr mit gleichen Lücken.
Jeder Knopf ist ein Schritt für Rückgängig.

**Ausgabe:** Tabelle der Ziele (Adresse, Port; leere Adresse = Multicast, Port 0 = Standard) mit
„Ziel +“ und „Letztes Ziel −“, darunter Bildrate, Abbildung auf den Würfel (Stretch oder
KeepAspect), sACN-Priorität und Quellname.

Zahlenfelder übernehmen einen Wert bei Enter, Pfeiltasten oder Fokuswechsel. Jede Änderung wird
samt Ausgabeplan geprüft. Passt etwas nicht (Universe schon belegt, Ziel gibt es nicht, ungültiges
Maß, doppelter Name), zeigt die Statuszeile den Grund und das Feld springt auf den alten Wert
zurück. Das letzte gepatchte Fixture lässt sich nicht entfernen, und ein Ziel, auf das ein Patch
zeigt, nicht löschen. Laufende Ausgabe wird nach einer Änderung einmal neu aufgebaut (kurze
Unterbrechung). Alle Schritte lassen sich zurücknehmen.

### Bedienung in der Vorschau

Die Vorschau ist eine Arbeitsfläche für das Layout:

| Aktion | Wirkung |
|---|---|
| Klick auf ein Fixture | wählt es (gestrichelter blauer Rahmen); die Liste im Fenster Stage Config folgt |
| Strg oder Umschalt + Klick | Fixture zur Auswahl hinzufügen oder entfernen |
| Ziehen auf einem Fixture | verschiebt alle gewählten Fixtures (Raster 0,1) |
| Ziehen auf leerer Fläche | Auswahlrahmen („Gummiband“) |
| Pfeiltasten | gewählte Fixtures um 1 schieben (Umschalt 10, Strg 0,1) |

**Region- und Output-Node:** Ist im Node-Editor ein Region- oder Output-Node gewählt, zeigt die Vorschau sein Rechteck
orange, die ganze Leinwand gepunktet. Ziehen an einer Ecke ändert die Größe, Ziehen am Kreis in der
Mitte verschiebt das Rechteck, Pfeiltasten schieben es um 1 % (Umschalt 10 %), solange kein Fixture
gewählt ist. Die Regler Left, Top, Width und Height im Parameter-Panel ziehen mit. Während des
Ziehens bleibt der Bildausschnitt stehen, danach passt er sich wieder an. Jede Ziehbewegung ist ein
Schritt für Rückgängig.

### 3D-Vorschau

Die Leiste „Ansicht“ (und das Menü Ansicht) schaltet die Vorschau um:

| Knopf | Kürzel | Was du siehst |
|---|---|---|
| 2D (bearbeiten) | Strg+1 | die Ansicht von vorn, in der du Fixtures verschiebst und den Bereich aufziehst (wie bisher) |
| 3D | Strg+2 | frei drehbare Perspektive um die Mitte der Installation |
| 3D Front, Seite, Oben | Strg+3, 4, 5 | feste, maßhaltige Ansichten (Parallelprojektion) von vorn, von rechts und von oben |

In den 3D-Ansichten gehört die Maus der Kamera; bearbeitet wird nur in „2D“.

| Aktion | Wirkung |
|---|---|
| Ziehen mit links | um die Installation drehen (aus einer festen Ansicht heraus wird es die freie 3D-Ansicht) |
| Rechtsklick, Mitteltaste oder Umschalt + Ziehen | Bild verschieben |
| Mausrad | zoomen |
| Doppelklick | Zoom und Verschiebung zurücksetzen |
| Pfeiltasten | in 5-Grad-Schritten drehen |

Die 3D-Ansicht kennt keine Fixture-Formen, sie zeichnet jede LED als Punkt an ihrer Position im Raum. Sie
funktioniert deshalb für Matrizen, Strips, Punktwolken und Mischungen gleichermaßen, auch für eine hängende
Installation aus vielen Strips. Ein feiner Rahmen um die Installation hilft bei der Orientierung; entferntere
LEDs sind leicht abgedunkelt. Achsen wie im Layout: x nach rechts, y nach oben, z zum
Betrachter. Die Farben sind dieselben wie in der 2D-Vorschau (Gamma je Fixture, Master und Blackout wirken mit).
Die gewählte Ansicht wird nicht im Projekt gespeichert.

**3D-Darstellung (Menü Ansicht → 3D-Darstellung):** In einem Cube verdecken graue Scheiben ausgeschalteter LEDs sonst die
leuchtenden dahinter. Darum gilt in 3D:

| Einstellung | Wirkung |
|---|---|
| Aus-LEDs unsichtbar (Vorgabe) | nur leuchtende LEDs werden gezeichnet, man sieht durch den Cube hindurch |
| Aus-LEDs schwach | ausgeschaltete LEDs als sehr dunkle Punkte, nur als Orientierung |
| Aus-LEDs grau | wie in der 2D-Ansicht (graue Punkte der vorderen Schicht verdecken LEDs dahinter) |
| Leuchten (additiv, Vorgabe) | leuchtende LEDs haben einen weichen Schein, Farben addieren sich, nichts verdeckt etwas; ohne Haken zeichnet die Ansicht Scheiben, nahe vor fernen |

Die Wahl wird gespeichert und gilt für alle 3D-Ansichten. Die 2D-Ansicht bleibt wie sie war. Der Kern (Kamera, Projektion, Zeichnen) liegt ohne Qt in
`libs/wh_preview/include/whitehole/preview/view3d.hpp` und ist unit-getestet; `Player::image3d` liefert das Bild.

### Node-Palette

Links im Dock „Nodes“ steht die Palette: alle verfügbaren Node-Typen nach Kategorien (Generator, Modifier,
Value, Audio ...), oben ein Suchfeld. Getippt wird nach Name, Kategorie und Typ-ID, jedes Wort muss passen
(„blend add“). Ein Typ lässt sich in den Editor **ziehen** (er entsteht unter der Maus) oder per Doppelklick
oder Enter in die Mitte der Ansicht legen. Zum **Entfernen** ziehst du einen Node im Editor auf die
Palette; sie wird rot, beim Loslassen ist der Node (samt Kabeln) weg. Alles ist ein Schritt Rückgängig.
Neue Node-Typen erscheinen automatisch, die Palette liest die Registrierung.

## Aufbau des Fensters

- **Oben:** die Menüs Datei, Bearbeiten, Stage Config, Ansicht, Wiedergabe, Live und Hilfe (siehe unten), darunter die Leiste „Datei“
  (Neu, Öffnen, Speichern) und die Leiste „Betrieb“: Start/Stopp (Leertaste), Ausgabe senden, Look speichern (Strg+L),
  Live-Modus (F9); rechts außen Master und Blackout (Strg+B). Der Knopf „Ausgabe senden“ leuchtet rot, solange wirklich
  Pakete ins Netz gehen. Die Tooltips der Schaltflächen nennen ihr Tastenkürzel.
- **Links:** Dock „Looks“ (immer in Reichweite).
- **Mitte:** die Vorschau; die Leiste „Ansicht“ schaltet zwischen 2D (bearbeiten) und den 3D-Ansichten um.
- **Rechts:** das Dock Parameter. Layout und Testmodus öffnen sich im Fenster Stage Config (Menü Stage Config, F4).
- **Unten:** Dock „Nodes“ mit Palette und Editor. Alle Docks lassen sich über das Menü Ansicht ein- und ausblenden.

### Menüs

| Menü | Inhalt |
| --- | --- |
| Datei | Neu, Öffnen, Zuletzt geöffnet, Beispiele; Speichern, Speichern unter; Neu laden (F5); Beenden |
| Bearbeiten | Rückgängig, Wiederholen; Einstellungen … (Strg+Komma) |
| Stage Config | Fixture hinzufügen …; Punktwolke importieren …; Layout und Patch … (F4); Testmodus … (die Seite „Ansicht“ liegt im selben Fenster) |
| Ansicht | Docks ein- und ausblenden; 2D/3D-Umschalter (Strg+1 bis 5); 3D-Darstellung (Aus-LEDs, Leuchten) |
| Wiedergabe | Start/Stopp, Ausgabe senden; Blackout |
| Live | Live-Modus (F9), Look speichern |
| Hilfe | Dokumentation (F1, öffnet die Anleitung im Browser), Fehler melden oder Idee vorschlagen (öffnet ein neues Issue auf GitHub), Tastenkürzel (Liste aller Kürzel, direkt aus den Aktionen gelesen), Über WhiteHoleFX (Version, Lizenz) |

## Looks und Live-Betrieb

Ein **Look** ist ein fertiger Effekt: alles, was am Ausgang hängt (vom Generator über die Modifier bis zum
Ausgangs-Node). Looks stehen in der Projektdatei (`look ... endlook`, siehe [Projektdatei](project-format.md)).

**Dock „Looks“ (zum Bearbeiten):**

- *Aktuellen Effekt als Look speichern ...* legt einen Look an; ein gleichnamiger wird nach Rückfrage ersetzt.
- Die Liste zeigt Nummer, Name und Überblendzeit. Darunter: Überblendzeit einstellen (0 = hart),
  mit aktuellem Effekt überschreiben, **in den Editor laden** (auch Doppelklick; ein Schritt Rückgängig),
  umbenennen, löschen, nach oben/unten.
- Gespeichert wird mit dem Projekt (Strg+S).

**Live-Modus (F9 oder Menü Live):** versteckt Editor, Parameter, das Fenster Stage Config, Looks-Dock und die
Datei-Leiste (Neu/Öffnen/Speichern) und zeigt für jeden Look einen großen Knopf. Master und Blackout (Strg+B)
bleiben in der Leiste. Rückgängig ist gesperrt, und die Leertaste ist GO statt Start/Stopp, damit auf der Bühne
nichts versehentlich angehalten oder geändert wird.

| Bedienung | Wirkung |
|---|---|
| Klick auf einen Look oder Taste 1 bis 9 | blendet in der Zeit des Looks zu ihm über |
| Kästchen „Eigene Überblendzeit“ | gilt für alle Klicks statt der Zeit des Looks (0 = hart) |
| Kästchen „Mit GO“ | ein Klick lädt den Look nur vor (orange); GO oder Leertaste startet die Überblendung |
| Esc oder „Zurück zum Editor“ | beendet den Live-Modus |

Farben: grün läuft, orange vorgeladen, gelb blendet gerade ein. Ein Balken zeigt den Fortschritt.

Technik: Zwei Decks und ein Crossfade. Beim **Vorladen** wird der Look ins verdeckte Deck kopiert; wenn
sich dadurch die Struktur ändert, entsteht einmal eine neue Shader-Pipeline, das kann einen kurzen Ruckler
geben. Der **Fade selbst** ändert nur einen Zahlenwert und läuft ohne Neuübersetzung. Wer es auf der Bühne
ruckelfrei braucht, nutzt „Mit GO“ und lädt vorher. Der Editor-Graph ruht im Live-Betrieb; Rückgängig ist
gesperrt. Ein Projekt zu laden beendet den Live-Modus.

## Master, Blackout, LED-Info, Zuletzt geöffnet

- **Master:** Der Regler „Master“ in der Leiste „Betrieb“ ist die Gesamthelligkeit von 0 bis 100 %. Er
  multipliziert die Farben aller LEDs vor dem Gamma der Ausgabe und wirkt auf Vorschau und Ausgabe,
  auch auf Testbilder. Der Wert gehört zum Betrieb, nicht zum Projekt: nicht gespeichert, nicht im
  Undo, er bleibt beim Laden eines anderen Projekts stehen. Für eine feste Helligkeit im Projekt gibt
  es `brightness` am Ausgangs-Node und am Patch.
- **Blackout** (Strg+B, auch Menü Wiedergabe): alle LEDs aus. Die Pakete laufen weiter, die Controller
  bleiben also verbunden und der Effekt läuft unsichtbar weiter. Blackout gewinnt gegen Master und
  Testbild. Ein rotes Banner „BLACKOUT“ in der Vorschau und die Statuszeile zeigen ihn an.
- **LED-Info:** Fährt man mit der Maus über eine LED der Vorschau (ohne Taste), erscheint ein Tooltip:
  Fixture, die wievielte LED am Kabel, Position, Ziel mit Universe und Kanal (bei DDP Byte-Bereich) und
  die aktuelle Farbe. Nicht gepatchte Fixtures sind als solche markiert.
- **Zuletzt geöffnet:** Menü Datei, die letzten zehn Projekte (auch nach „Speichern unter“). Eine Datei,
  die es nicht mehr gibt, fliegt beim Versuch, sie zu öffnen, aus der Liste; „Liste leeren“ löscht alles.

## Node-Presets

Eine Kombination von Nodes, die sich bewährt hat, lässt sich als Preset merken:

- **Speichern:** Nodes im Node-Editor auswählen, Rechtsklick auf einen davon, „Als Preset speichern …“,
  Namen eingeben. Ein gleichnamiges Preset wird nach Rückfrage ersetzt. Gemerkt werden die Nodes mit
  ihren Parametern und die Kabel zwischen ihnen; Kabel nach draußen und der Ausgang gehören nicht dazu.
- **Einfügen:** Rechtsklick auf freie Fläche, „Preset einfügen“, Namen wählen. Die Nodes erscheinen an
  der Maus als ganz normale Nodes (ausgewählt, ein Schritt für Rückgängig) und sind danach nicht mehr
  mit dem Preset verbunden. Presets gelten für alle Projekte.
- **Löschen:** Rechtsklick auf freie Fläche, „Preset löschen“.

Presets liegen als Textdateien (`<Name>.whfxnodes`) im Ordner `presets` des Datenordners des Programms
(Linux `~/.local/share/WhiteHoleFX/presets`, Windows `%APPDATA%\WhiteHoleFX\presets`). Das Format ist das
Projektformat nur mit `node`- und `connect`-Zeilen (siehe [Projektdatei](project-format.md)), man kann es
also im Texteditor lesen, ändern und weitergeben: Datei in den Ordner legen, fertig. Namen dürfen keines
von `\ / : * ? " < > |` enthalten und höchstens 64 Zeichen lang sein.

## Testmodus

Die Seite „Testmodus“ im Fenster Stage Config (Menü Stage Config) ersetzt die Farben des
Effekts durch ein Testbild, um eine Wand in Betrieb zu nehmen. Die Wiedergabe muss laufen. Das Bild
erscheint in der Vorschau und, wenn „Ausgabe senden“ an ist, auf den LEDs; ohne „Ausgabe senden“
bleibt es bei der Vorschau. Solange ein Testbild aktiv ist, steht oben rechts in der Vorschau ein rotes
Banner „TESTMODUS“ und in der Statuszeile „TESTMODUS“. Das Testbild gehört nicht zum Projekt: Es wird
nicht gespeichert, nicht rückgängig gemacht und beim Öffnen eines anderen Projekts abgeschaltet.

| Testbild | Zeigt | Prüft |
|---|---|---|
| Farbe | alle LEDs in Weiß, Rot, Grün, Blau, Gelb, Cyan oder Magenta | Netzteile, Farbtreue |
| Auswahl hervorheben | die in der Layout-Liste (oder per Klick in der Vorschau) gewählten Fixtures zu 100 % Weiß, der Rest aus | welches Fixture ist welches |
| Laufbalken | ein Balken läuft entlang x, y oder z | Lage und Ausrichtung im Raum |
| Farbreihenfolge | Rot, Grün, Blau, Weiß nacheinander | stimmt die Farbreihenfolge des Patches? Zeigt die Wand etwas anderes als die Vorschau, ist sie falsch eingestellt |
| Lauflicht | ein Lichtpunkt mit Schweif läuft je Fixture in Kabelreihenfolge | Richtung des Kabels, defekte oder vertauschte Abschnitte |

„Nur auf gewählten Fixtures“ beschränkt Farbe, Laufbalken, Farbreihenfolge und Lauflicht auf die
Auswahl; ohne Auswahl leuchtet dann nichts. „Testmodus beenden“ (oder die Art „Aus“) stellt den
Effekt wieder her. Achtung: Weiß und Farbe leuchten mit voller Helligkeit; vorher prüfen, ob die
Netzteile die ganze Wand auf 100 % tragen.

## Einstellungen

**Bearbeiten → Einstellungen …** (Strg+Komma) öffnet das Fenster mit allem, was zum Rechner und zum Auftritt gehört, nicht
zum Projekt. Die Wahl wird beim Nutzer gespeichert und beim nächsten Start wieder umgesetzt; „OK“ wendet sie sofort an.

- **Netzwerk:** die Karte zum **Senden** (Art-Net, sACN, DDP gehen darüber hinaus; „Automatisch“ überlässt es der
  Routingtabelle des Systems, was bei zwei Anschlüssen oft die falsche Karte ist) und die Karte zum **Empfangen**
  (über sie tritt sACN den Multicast-Gruppen bei; Art-Net-Broadcast und Unicast kommen weiterhin von jeder Karte an).
  Die Liste zeigt die aktiven Karten mit ihrer IPv4-Adresse. Eine gemerkte Karte, die es gerade nicht gibt (Kabel
  gezogen, neue DHCP-Adresse), bleibt als „(nicht gefunden)“ stehen; das Senden läuft dann automatisch weiter und die
  Statuszeile meldet es.
- **DMX-Eingang:** Art-Net und sACN einzeln einschalten, dazu die Ports (Standard 6454 und 5568).
- **Audio:** die Quelle für die Audio-Nodes (siehe unten).

## DMX-Eingang

Einstellungen, Seite **DMX-Eingang**: „Art-Net empfangen“ (UDP 6454) und „sACN empfangen“ (UDP 5568). Beide sind nach der
Installation aus; erst der Schalter öffnet den Port (Windows fragt dann die Firewall). Die Wahl wird
gemerkt. Die Statuszeile zeigt „DMX-Eingang Art-Net + sACN: N Pakete“; enthält das Projekt DMX-Nodes,
der Empfang ist aber aus, steht dort ein Hinweis.

Für sACN tritt das Studio den Multicast-Gruppen genau der Universen bei, die das Projekt liest; Unicast-
Pakete kommen immer an. Ist der Port belegt, bleibt der Empfang aus und eine Meldung nennt den Grund.
Die Nodes selbst: `docs/value-graph.md`, Abschnitt „DMX-Eingang“.

## Audio-Eingang

Einstellungen, Seite **Audio**: *Aus*, *Standardgerät*, ein Gerät aus der Liste oder *WAV-Datei in Schleife*.
Nach der Installation ist der Eingang aus; das Mikrofon öffnet erst die ausdrückliche Wahl. Die Wahl wird
gemerkt (scheitert das Öffnen beim nächsten Start, bleibt der Eingang aus). Die Statuszeile zeigt „Audio:
Quelle, Pegel, BPM“; enthält das Projekt Audio-Nodes, der Eingang ist aber aus, steht dort ein Hinweis.
Die Nodes und die Technik dahinter: [docs/audio.md](audio.md).

Nicht enthalten: Art-Net-Erkennung (ArtPoll/ArtPollReply), das Studio meldet sich also nicht in der
Geräteliste von Pulten an; Pulte müssen die IP-Adresse dieses Rechners direkt oder per Broadcast ansprechen.

## Node-Standardwerte

Einstellungen, Seite **Node-Standardwerte**: Wer Nodes immer wieder mit denselben Werten braucht, stellt sie hier einmal ein.
Links stehen die Node-Typen nach Kategorie (Suchfeld darüber), rechts die Parameter des gewählten Typs mit denselben
Bedienelementen wie im Parameterfenster. Typen mit eigenen Werten sind fett. Das „↺“ in einer Zeile stellt den Wert der
Bibliothek wieder her; ein Wert, der ihm gleicht, zählt nicht als eigener. „Alle Standardwerte zurücksetzen“ leert die ganze Liste.

Ein neu angelegter Node (Palette, Menü, Doppelklick) startet mit diesen Werten. Bestehende Nodes, geladene Projekte und
Looks bleiben unverändert. Die Werte gehören zum Nutzer, nicht zum Projekt: sie werden mit den übrigen Einstellungen auf
dem Rechner gespeichert (`nodeDefaults/<Typ>/<Parameter>`) und wandern nicht mit einer Projektdatei zu anderen.

## Rückgängig und Wiederholen

Menü Bearbeiten, Strg+Z und Strg+Umschalt+Z (Windows auch Strg+Y). Zurückgenommen werden
Parameter, Nodes, Kabel, Ausgang und Lage; bis zu 200 Schritte. Viele Änderungen hintereinander am
selben Regler oder am selben Node (Ziehen) zählen als ein Schritt. Laden leert die Listen,
Speichern nicht. Der Stern im Titel bleibt nach einem Undo stehen, auch wenn der gespeicherte Stand
erreicht ist.

## Noch nicht enthalten

Punktwolken bearbeiten (importieren geht), Fixtures drehen, Layout in der 3D-Ansicht bearbeiten (sie ist nur zum Ansehen), Kopieren/Einfügen von Fixtures. Solches entsteht bis dahin in der Projektdatei (siehe
[Projektdatei](project-format.md)); „Neu laden“ übernimmt Änderungen sofort.
