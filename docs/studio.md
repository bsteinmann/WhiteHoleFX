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

## Noch nicht enthalten

Ein Node-Editor und ein Layout-Editor. Bis dahin entsteht die Struktur der Projektdatei von Hand
(siehe [Projektdatei](project-format.md)); „Neu laden“ übernimmt Änderungen sofort.
