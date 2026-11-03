**Testversion für Windows (64 Bit).** ZIP herunterladen, in einen eigenen Ordner entpacken, `whitehole_studio.exe` starten.
Windows warnt vor einem nicht signierten Programm: „Weitere Informationen“ → „Trotzdem ausführen“.

**Neu seit v0.22.0** (alle Einzelheiten in `CHANGELOG.md`):

- **Node-Editor:** Nodes lassen sich **überbrücken** (Schalter im Kopf, Taste B), jeder Node hat ein **Info-„i“** mit Beschreibung,
  beim Ziehen aus der Palette **rastet** ein Node auf Kabeln und Nodes **ein** (Alt schaltet das ab), die Palette zeigt
  farbige Punkte und hebt beim Kabelziehen die passenden Nodes hervor. Zahlen-Ausgänge (LFO, Math ...) passen jetzt
  auch an Ganzzahl-Eingänge und Auswahlfelder.
- **Neue Nodes und Effekte:** Text, Random Pixels, Value Noise, Color Ramp, Cuelist, Master per DMX, Sphere und Box
  wählen auf flachen Matrizen automatisch 2D, in Volumen 3D; mehrere 3D-Nodes und fünf 3D-Beispiele.
- **Oberfläche:** Fensteranordnung wird gemerkt (#27), Stage Setup und Einstellungen als eigene Fenster, Sprachen
  Deutsch, Englisch, Französisch und Spanisch, schnellere 3D-Vorschau, Tutorial und Menü Hilfe.
- **Fehlersuche:** Das Studio erkennt, wenn es hängt, und schreibt einen Bericht in den Cache-Ordner (#26).

**Bekannt:** Der **Text-Node** zeigt auf manchen Grafikkarten (etwa NVIDIA GTX 760 mit Vulkan) noch keinen Text (#13);
dort lohnt ein Versuch mit der Umgebungsvariable `WHITEHOLEFX_GPU_BACKEND` (`vulkan`, `gl` oder `dx12`). Neu und nur in der CI, nicht von Hand
auf Windows geprüft: Einrasten, Info-Zeichen, Bypass und farbige Palette - bitte melden, wenn etwas hakt. Die
Node-Beschreibungen sind auf Französisch und Spanisch noch englisch.

**Zum Ausprobieren:** Datei → Beispiele → `looks` öffnen, „Start“ drücken, mit **F9** in den Live-Modus wechseln
und per Klick oder Tasten 1 bis 3 zwischen den Looks überblenden.

**Sicherheit:** Nach dem Start geht nichts ins Netz. Erst „Ausgabe senden“ schickt Art-Net/sACN/DDP an die im Projekt
eingetragenen Adressen (die Beispiele zeigen auf den eigenen Rechner).

**Stand:** Das Programm wird automatisch auf Linux und Windows gebaut und getestet, in der CI auch in einem
unsichtbaren Fenster bedient. Auf echter Hardware (GPU-Treiber, Controller im Netz, Maus und Bildschirm) ist es noch
wenig erprobt. **Rückmeldungen sind willkommen** (Issues): bitte mit Windows-Version, Grafikkarte und den Schritten
bis zum Problem.

**Lizenz:** PolyForm Shield 1.0.0 — kostenlos für jeden Zweck, auch für DJs und Lichttechniker auf bezahlten
Veranstaltungen; nicht erlaubt ist, ein konkurrierendes Produkt oder einen Dienst daraus zu machen. Details in
`LICENSE` und `NOTICE`. Änderungen: `CHANGELOG.md`.
