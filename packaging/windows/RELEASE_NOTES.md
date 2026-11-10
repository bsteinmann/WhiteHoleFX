**Vorabversion (0.x) für Windows (64 Bit) und Linux.** Alle Einzelheiten stehen in `CHANGELOG.md`.

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

### Neu seit v0.51.0

- **Node-Editor:** Rechtsklick auf einen Ein- oder Ausgang zeigt passende Nodes und verbindet sie sofort; Skalierung am Kabel
  (`Wert × Faktor + Offset`); Parameterfenster zeigt nur den gewählten Node, mit Suche und Anheften.
- **Neue Nodes:** sechs 1D-Generatoren für Strips (Achse wählbar), Strobe Gate.
- **Stage Config mit vier Reitern** (Fixtures, Layout, Ausgabe, Test): Bibliothek der Bauformen (Matrix, Kreis, Strip, Säule,
  Ring), Drehung von Fixtures, Achsen-Griff und Drehringe in der Bühnenansicht, Einrasten an anderen Fixtures,
  Controller-Bibliothek mit Protokoll (Art-Net, sACN, DDP) pro Controller.
- **Windows:** Programmsymbol und Startbildschirm, Node-Editor bleibt nach dem Ablegen aus der Palette sichtbar,
  Update-Benachrichtigung (Hilfe → Auf Updates prüfen …, erst nach Zustimmung).

### Hinweise

**Sicherheit:** Nach dem Start geht nichts ins Netz. Erst „Ausgabe senden“ schickt Art-Net/sACN/DDP an die im Projekt
eingetragenen Adressen. Die Suche nach Updates fragt GitHub nur, wenn du sie einschaltest oder anstößt.

**Stand:** Das Programm wird automatisch auf Linux und Windows gebaut und getestet, in der CI auch in einem
unsichtbaren Fenster bedient. Auf echter Hardware (GPU-Treiber, Controller im Netz, Maus und Bildschirm) ist es noch
wenig erprobt. **Rückmeldungen sind willkommen** (Issues): bitte mit Windows-Version, Grafikkarte und den Schritten
bis zum Problem.

**Bekannt:** Der Text-Node zeigt unter Vulkan auf NVIDIA-Karten der Kepler-Generation (GTX 600/700) keinen Text (#13);
dort nimmt WhiteHoleFX automatisch OpenGL. Bei anderen Karten mit dem gleichen Problem lohnt ein Versuch mit der
Umgebungsvariable `WHITEHOLEFX_GPU_BACKEND` (`vulkan`, `gl` oder `dx12`) und ein Issue mit Kartenname und Treiber.

**Zum Ausprobieren:** Datei → Beispiele → `looks` öffnen, „Start“ drücken, mit **F9** in den Live-Modus wechseln
und per Klick oder Tasten 1 bis 3 zwischen den Looks überblenden.

**Lizenz:** PolyForm Shield 1.0.0 — kostenlos für jeden Zweck, auch für DJs und Lichttechniker auf bezahlten
Veranstaltungen; nicht erlaubt ist, ein konkurrierendes Produkt oder einen Dienst daraus zu machen. Details in
`LICENSE` und `NOTICE`.
