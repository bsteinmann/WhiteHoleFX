**Version 1.2.0 für Windows (64 Bit) und Linux.** Alle Einzelheiten stehen in `CHANGELOG.md`.

**Windows:** ZIP herunterladen, in einen eigenen Ordner entpacken, `whitehole_studio.exe` starten.

**Linux (Ubuntu 22.04 und neuer, Debian):** `.deb` herunterladen und mit `sudo apt install ./whiteholefx_*.deb` installieren,
dann `whitehole_studio` starten (oder aus dem Anwendungsmenü).

### Warum warnt Windows? (Programm nicht signiert)

Die `.exe` ist noch **nicht digital signiert**. Windows zeigt deshalb beim ersten Start „Der Computer wurde durch Windows
geschützt“ (SmartScreen) oder „Unbekannter Herausgeber“. Das heißt nur: Das Programm ist neu und hat bei Microsoft noch
keinen Ruf, nicht dass etwas daran auffällig wäre. So gehst du vor:

1. „Weitere Informationen“ → „Trotzdem ausführen“.
2. Wer sichergehen will, prüft vorher die **Prüfsumme** (SHA-256). Sie steht in der Datei `…sha256` neben dem Download
   und muss mit der Datei übereinstimmen, die du heruntergeladen hast. Das Programm wird ausschließlich von GitHub Actions
   aus dem öffentlichen Quelltext gebaut, der Bau ist in den Actions des Projekts nachzulesen.

   Windows (PowerShell, im Ordner der Datei):
   `Get-FileHash .\WhiteHoleFX-<version>-windows-x64.zip -Algorithm SHA256`
   Linux: `sha256sum -c whiteholefx_*.deb.sha256`

Stimmt die Prüfsumme nicht, bitte nicht starten und melden.

### Neu in 1.2.0

- **MIDI:** Eingang und Ausgang (Einstellungen → MIDI), neue Nodes **MIDI CC** und **MIDI Note** mit Knopf „Lernen“, Akai APC mini (mk1) im Live-Modus mit Pad-Farben, Tasten und Fadern.
- **Live-Modus:** festes Raster mit frei belegbaren Feldern, Looks flashen (nur solange gehalten), Vorschaubilder auf den Knöpfen, geteilte Vorschau, manuelle Überblendung, frei belegbare Steuerung per MIDI oder DMX (auch Cue-Sprung), Schaltfläche „Einstellungen“ im Live-Fenster.
- **Benchmark:** `whitehole_benchmark` (Doppelklick, ohne Projekt) und `whitehole_cli --benchmark` messen Projektion und LED-Ausgabe.
- **Fehlerbehebungen:** Kein Stocken mehr beim Drücken, Loslassen und Vorladen von Looks. Animationen (z. B. Labyrinth) und die Werte-Nodes Time, LFO und Random springen nicht mehr, wenn man Tempo, Frequenz oder Rate verstellt. Dazu die Korrekturen aus 1.1.1 (Node-Editor-Stillstand, scrollendes Look-Raster).

### Hinweise

**Sicherheit:** Nach dem Start geht nichts ins Netz. Erst „Ausgabe senden“ schickt Art-Net/sACN/DDP an die im Projekt
eingetragenen Adressen. Die Suche nach Updates fragt GitHub nur, wenn du sie einschaltest oder anstößt.

**Stand:** Das Programm wird automatisch auf Linux und Windows gebaut und getestet, in der CI auch in einem
unsichtbaren Fenster bedient. GPU-Rendering und die Ausgabe an echte Controller (Art-Net/sACN/DDP) sind erprobt und
laufen. **Rückmeldungen sind weiterhin willkommen** (Issues): bitte mit Windows-Version, Grafikkarte und den
Schritten bis zum Problem.

**Haftungsausschluss:** Die Software wird ohne jede Gewährleistung bereitgestellt ("as is"), soweit gesetzlich
zulässig. Für Schäden, die aus der Nutzung entstehen — etwa Ausfälle während einer Veranstaltung, fehlerhafte
DMX-Ausgabe oder Datenverlust —, wird keine Haftung übernommen (siehe auch `LICENSE`, Abschnitt „No Liability“).
Einsatz auf eigenes Risiko; vor einem Live-Einsatz gründlich testen.

**Bekannt:** Der Text-Node zeigt unter Vulkan auf NVIDIA-Karten der Kepler-Generation (GTX 600/700) keinen Text (#13);
dort nimmt WhiteHoleFX automatisch OpenGL. Bei anderen Karten mit dem gleichen Problem lohnt ein Versuch mit der
Umgebungsvariable `WHITEHOLEFX_GPU_BACKEND` (`vulkan`, `gl` oder `dx12`) und ein Issue mit Kartenname und Treiber.

**Zum Ausprobieren:** Datei → Beispiele → `looks` öffnen, „Start“ drücken, mit **F9** in den Live-Modus wechseln
und per Klick oder Tasten 1 bis 3 zwischen den Looks überblenden.

**Lizenz:** GNU General Public License Version 3 (GPL-3.0) — kostenlos für jeden Zweck, auch kommerziell; wer
weitergibt, muss den Quellcode mitgeben und das Ergebnis ebenfalls unter der GPL-3.0 stellen. Details in
`LICENSE` und `NOTICE`.
