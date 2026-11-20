# Absturz- und Stillstands-Todoliste (Stand 2026-11-19)

Warum es diese Datei gibt: Der Nutzer hat am 19.11. neun Berichte samt Minidumps aus der Windows-Version (Dateien
`crash-…`/`stall-…`, Version 0.82.0 und 1.0.0, GPU GTX 1060) geschickt und will die Ursachen **später in einem Rutsch**
beheben lassen. Hier steht die Auswertung, damit eine spätere Sitzung ohne die Dumps starten kann. Nichts davon ist
umgesetzt. Stand der Sicherheit steht je Punkt dabei: Die Stapel zeigen nur Adressen der `whitehole_studio.exe`
(`+0x…`), keine Funktionsnamen, weil die `.pdb` in der Auswertungssitzung nicht verfügbar war (siehe Punkt 0).

Bei der Abarbeitung gelten die Regeln aus `CLAUDE.md` (kleine Schritte, Tests, Doku, i18n, vier lokale Testläufe,
Release-Workflow). Studio-Code (Qt) lässt sich lokal nicht bauen; die Prüfung läuft über die CI und beim Nutzer unter Windows.

## Umsetzungsstand (20.11., auf Branches, Pull Requests offen; Merge nach `main` erst nach grüner CI und Freigabe)

| Punkt | Branch |
|---|---|
| 1, 2, 6 (Fokus-Absturz, StallPause, Build-Kennung im Bericht) | `fix/crashes` |
| 7 (Vorlagen-Bauform „Matrix 16x8“) | `fix/fixture-template` |
| 8 (Testmodus-Knopf) | `feat/testmodus-button` |
| 9 (Output duplizieren, Variante a) | `fix/output-duplizieren` |
| Zellenzahl automatisch (Wunsch vom 20.11., kein Punkt dieser Liste) | `feat/cells-auto` |
| CI `paths-ignore`, Branch-Regeln, diese Liste | `chore/ci-docs-paths` |

Punkt 1 ist ohne passende `.pdb` nicht bestätigt (siehe Punkt 0); das Muster ist behoben, die Stelle nicht belegt. Punkt 9: der
Fehler ließ sich im Test nur als „Ausgang wechselt auf die Kopie“ nachstellen, nicht als schwarzes Bild (die Engine fasst
Outputs zusammen); Variante (a) ist umgesetzt.

## Überblick

| # | Art | Wann (lokal) | Version | Kern | Sicherheit der Ursache | Priorität |
|---|---|---|---|---|---|---|
| 0 | Voraussetzung | – | – | `.pdb` besorgen, Adressen auflösen | – | zuerst |
| 1 | Absturz ×2 | 17.11. 12:26:08, 19.11. 13:29:53 | 1.0.0 | Zugriff auf schon gelöschtes Widget beim Fokuswechsel | mittel (Muster eindeutig, Stelle offen) | hoch |
| 2 | falscher Stillstand | 19.11. 14:17:00 | 1.0.0 | Fixture-Export/-Import ohne `StallPause`, nativer Dialog | hoch (Stapel eindeutig) | hoch, klein |
| 3 | Stillstand ×2 | 17.11. 12:27:06, 12:27:24 | 1.0.0 | Verdrahtung zeichnen (`drawPolyline`) dauert > 5 s | mittel | **ignorieren** (Experiment) |
| 4 | Stillstand ×2 | 17.11. 12:26:49, 12:26:58 | 1.0.0 | Vorschau-Render (`lroundf`) dauert > 5 s | niedrig bis mittel | **ignorieren** (Experiment) |
| 5 | Absturz ×2 | 11.11. 08:30:08, 08:31:30 | 0.82.0 | Node aus Palette in leeren Editor ziehen | hoch | erledigt (0.82.1), nur gegenprüfen |
| 6 | Diagnose | – | – | Berichte verbessern | – | optional |
| 7 | Fehler (Bedienung) | 20.11. gemeldet | 1.0.0 | Stage Config → Fixtures: erstes Hinzufügen legt zusätzlich eine 16×8-Matrix an | mittel (Ursache sehr wahrscheinlich, noch nicht nachgestellt) | mittel, klein |
| 8 | Wunsch (Bedienung) | 20.11. gemeldet | 1.0.0 | Testmodus per Knopf neben den Ansichts-Umschaltern (2D/3D/Front/…) ein-/ausschalten | hoch (klein, Qt-Teil nur in der CI prüfbar) | mittel, klein |
| 9 | Fehler (Bedienung) | 20.11. gemeldet | 1.0.0 | Output-Node duplizieren (Strg+D/Einfügen) → Bild wird schwarz | hoch (Ursache im Code gefunden, noch nicht nachgestellt); Lösung (a) vom Nutzer gewählt | hoch, klein |

**Stand 2026-11-20 (Nutzer):** Einer der Berichte geht auf ein Experiment mit 1920×1080 LEDs (rund 2 Millionen) zurück und soll
ignoriert werden. Gelesen als: die Stillstände aus Punkt 3 und 4 (alle vier stammen aus einem Prozess, und nur sie hängen
von der LED-Zahl ab; Punkt 1 und 2 nicht). Beide sind damit **zurückgestellt**, nichts davon umsetzen. Nur wenn ein
realistisches Layout (z. B. 100×100) das Studio ebenfalls ausbremst, wieder aufnehmen.

Zusammenhang: Punkt 3 und 4 sind vier Berichte aus **einem** Prozess innerhalb von 35 s (gleiche Modul-Basisadressen);
vermutlich dieselbe Ursachenfamilie (Hauptthread ist mit Zeichnen/Rendern eines großen Layouts ausgelastet). Der Absturz
12:26:08 (Punkt 1) war in der Sitzung davor, danach wurde das Studio neu gestartet.

## 0. Voraussetzung: `.pdb` besorgen und Adressen auflösen

**Stand 20.11. 14:30:** Der Nutzer hat eine `whitehole_studio.pdb` (25,4 MB) angehängt, aber sie **passt nicht** zu den Berichten:
PDB-GUID `{A05203DE-B411-4A37-98C1-12A9693C3600}`, die Dumps (alle drei geprüft: 17.11. Absturz, 19.11. Absturz, 19.11.
Stillstand) tragen `04B04DD3-7931-4F7A-8844-D43FE8F57A25`. Adressen **nicht** aufgelöst (mit falscher PDB wären sie Raten).
**Ergebnis 20.11. 14:50:** Die zweite PDB (aus Lauf `44056022233`) ist byte-identisch zur ersten (GUID `A05203DE-…`), kein
Download-Fehler. Die Dumps der 17./19.11. liegen unter `C:\Users\Acer\Downloads\WhiteHoleFX-Windows\` (ohne inneren Ordner
`WhiteHoleFX`, anders als die CI-Pakete vom 07./11.11.): vermutlich das **Release-ZIP** (release.yml baut einen eigenen
Build, verwirft die `.pdb` - `Move-Item ... symbole/`, aber kein Upload). Für diesen Build gibt es keine PDB. Außerdem
stehen im Absturz vom 19.11. nur `main`/Start-Frames der exe (`+0x2b3a0`, `+0x216390`, `+0x20b369`); der Absturz selbst liegt in
`QWidget::setFocus`/`QObject::inherits`. Die PDB würde die Stelle im eigenen Code also ohnehin nicht nennen. Punkt 1 braucht
sie nicht; er wird über das Muster (Widget synchron gelöscht) angegangen. Punkt 6 (Commit/GUID im Bericht) macht künftige
Berichte zuordenbar; `release.yml` sollte die `.pdb` hochladen (nur mit Freigabe).

**Klärung (20.11. 14:46, überholt durch das Ergebnis oben):** Das Studio stammt laut Nutzer aus einem **CI-Artefakt** (nicht aus dem Release-ZIP). Die einzige
1.0.0-Windows-Paket-Lauf ist `44056022233` (Dispatch, 17.11. 09:29; der ältere Dispatch `44042506383` ist noch 0.92.0). Die
passende PDB müsste daher dort liegen (Artefakt `14316144622`); die angehängte stammt wohl aus dem **falschen Lauf**
(vermutlich `44042506383`, Artefakt `14310656951`). Nutzer bittet, sie aus Lauf `44056022233` neu herunterzuladen.
(Ältere Vermutung, nur noch Rückfall:) Das laufende Studio (`C:\Users\Acer\Downloads\WhiteHoleFX-Windows\`) stammt aus einem anderen Build desselben
Stands `5a6da9d`, z. B. dem Release-Lauf `44063593479` (release.yml, 17.11. 10:18); dessen Lauf hat **kein** Symbole-Artefakt.
Möglichkeiten: (a) Nutzer klärt, woher sein Studio stammt; (b) release.yml lädt künftig die `.pdb` als Artefakt hoch
(Änderung an einem Workflow, nur mit Freigabe); (c) Nutzer testet mit einem frischen CI-Build samt passender `.pdb` neu.

- Die CI legt die `.pdb` als Artefakt `WhiteHoleFX-Windows-Symbole` ab (Lauf 44056022233, Artefakt 14316144622, 7,6 MB;
  das ist der 1.0.0-Windows-Build, den der Nutzer benutzt). **Läuft am 2026-12-01 ab** (`retention-days: 14` in `ci.yml`).
- Aus der Cloud-Sitzung nicht abrufbar: Der Download liegt auf `*.blob.core.windows.net`, das der Agent-Proxy sperrt
  (403, Richtlinie). Kein Umweg versuchen. Der Nutzer lädt das ZIP auf GitHub (Actions → Lauf → Artefakte) herunter und
  hängt `whitehole_studio.pdb` an. Passt das Image nicht: GUID im Dump ist `04B04DD3-7931-4F7A-8844-D43FE8F57A25`
  (Absturz 19.11.); dann den richtigen Lauf nennen lassen.
- Danach die anonymen Adressen der Punkte 1, 3, 4 auflösen (Werkzeug: `llvm-symbolizer`/`llvm-pdbutil`, falls vorhanden;
  `lldb -c <datei>.dmp` liest die Dumps unter Linux, Register und Threads gehen schon ohne `.pdb`). Ergebnis in diesen
  Punkten nachtragen und die Hypothesen bestätigen oder streichen, **bevor** gebaut wird.
- Wenn der Nutzer künftig Berichte schickt, immer die Versionsnummer **und** den Build (Lauf-Nummer) dazusagen lassen.

## 1. Absturz: Zugriff auf gelöschtes Widget beim Fokuswechsel per Mausklick (1.0.0, zweimal)

Befund (aus beiden Dumps, identisch):
- Stapel besteht nur aus Qt-Frames: Mausklick → `QApplication::notify` → `giveFocusAccordingToFocusPolicy` →
  `QWidget::setFocus` → `QObject::inherits` (Zugriffsverletzung beim Lesen von Adresse `0x9`). Kein Studio-Frame im Stapel.
- Register in beiden Dumps gleich: `rax = 1`, `rcx = rbx = rbp =` Heapzeiger, `rdi = 0`. `inherits` liest die Vtable des
  Objekts; die erste Speicherstelle des Objekts enthält `1` statt eines Vtable-Zeigers, also **freigegebener Speicher**.
  Qt arbeitet hier mit einem Widget (vorheriges oder neues Fokus-Widget), das schon gelöscht ist.
- Kein Studio-Frame im Stapel heißt: Die Ursache liegt früher, in einer Stelle, die Widgets **synchron** löscht, während
  Qt sie noch als Fokus-Widget kennt.

Hypothese (Sicherheit: mittel): Ein Klick nimmt einem Eingabefeld den Fokus (`FocusOut`, `editingFinished`/`valueChanged`).
Ein Slot baut daraufhin die Oberfläche neu und löscht den Widget-Baum mit genau diesem Feld mit `delete` oder
`QScrollArea::setWidget`, noch bevor Qt mit dem Fokuswechsel fertig ist. `FixtureForm` macht es schon richtig
(`removeWidget` + `hide` + `deleteLater`, `fixture_form.cpp:30-33`, Kommentar „wir können aus einem Signal dieses Inhalts
hierher kommen“), `node_defaults_page.cpp:150-154` ebenfalls.

Verdächtige Stellen, der Reihe nach prüfen (nach Auflösen der Adressen eingrenzen):
1. `apps/whitehole_studio/shape_form.cpp:41` `delete content_;` in `ShapeForm::build()`. Aufruf über `setShape()`, die
   `FixtureLibraryPage::showSelected()` (`fixture_library_page.cpp:252`) aus `currentRowChanged` ruft, also mitten im
   Klick auf die Liste, während ein Feld der Form den Fokus hat. Dort gibt es schon einen `QTimer::singleShot(0, …)`
   (Zeile 274) für einen ähnlichen Fall; derselbe Schutz fehlt hier.
2. `parameter_panel.cpp:394` `scroll_->setWidget(content)` („löscht den alten Inhalt“) in `ParameterPanel::rebuild()`,
   aufgerufen aus `main_window.cpp:175`. Die alten Zeilen haben Spinboxen mit `editingFinished` (z. B. Zeile 91-92) und
   Slots, die Änderungen weitergeben; läuft `rebuild()` aus so einem Signal, wird der Sender gelöscht.
3. Weitere synchrone Löschungen: `live_panel.cpp:111` (`delete button`), `node_editor_dock.cpp:154` (`delete line`, ein
   Grafik-Item, vermutlich harmlos), `dmx_*_monitor_panel.cpp` (Menüs, nach `exec()`, vermutlich harmlos).

Fix: Ein gemeinsamer Helfer `retireWidget(QWidget*)` (`removeWidget`/`takeWidget`, `hide()`, `clearFocus()` nur wenn nötig,
`deleteLater()`), alle Stellen aus 1 und 2 darauf umstellen; keine neuen synchronen `delete` auf Widgets, die Signale senden
können. Bei `ParameterPanel`: alten Inhalt mit `takeWidget()` herausnehmen und `deleteLater()`.

Test (nur in der CI/Offscreen-Qt möglich): Feld im `ShapeForm` bzw. in einer Parameterzeile fokussieren, Wert tippen, dann
per `QTest::mouseClick` einen Eintrag wählen, der `setShape()` bzw. `rebuild()` auslöst; das Programm darf nicht
abstürzen. Zusätzlich dasselbe mit `QTest::keyClick(Tab)`. Nach dem Fix den Nutzer bitten, die Schrittfolge von damals
nachzustellen (Welche Klicks vor den beiden Abstürzen? **offen, nachfragen**).

## 2. Falscher Stillstandsbericht beim Fixture-Export/-Import (1.0.0)

Befund: Stapel `QAbstractButton::clicked` → Studio-Lambda → `QFileDialog::getSaveFileName` → `QDialog::exec` → natives
Windows-Dialog (COM, wartet im `combase`). Der Hauptthread ist nicht eingefroren, er wartet im Dialog, aber der Wächter
(`stall_watchdog_win.cpp`) hält das für einen Stillstand.

Ursache (sicher): `fixture_library_page.cpp:149` (Export) und `:157` (Import) rufen die nativen Dialoge ohne `StallPause`
(`stall_watchdog.hpp`) auf. Der Fix von 0.61.0 hat nur `main_window_file.cpp:118/161` und `settings_dialog.cpp:232`
abgedeckt. Dazu kommt: In 0.84.1 wurde „Speichern unter“ auf den Qt-eigenen Dialog (`DontUseNativeDialog`) umgestellt, weil der
native Dialog unter Windows abstürzen kann (Shell-Erweiterungen); der neue Export (Commit 66c8022 vom 16.11.) nutzt den
nativen wieder.

Fix: Ein Helfer (`file_dialogs.hpp/cpp`) mit `saveFileName(...)`/`openFileName(...)`, der `StallPause` setzt und den Qt-Dialog
nimmt. Alle fünf Stellen (`main_window_file.cpp` ×2, `settings_dialog.cpp`, `fixture_library_page.cpp` ×2) darauf umstellen.
Test: Quelltext-Prüfung in der CI, die verbietet, dass `QFileDialog::get` außerhalb des Helfers vorkommt (einfacher Test
oder Skript); Doku/CHANGELOG ergänzen; neue `tr()`-Texte gibt es dabei nicht.

## 3. ZURÜCKGESTELLT: Stillstand: Verdrahtung zeichnen dauert über 5 s (1.0.0, zweimal im selben Prozess)

> Vom Nutzer am 20.11. als Experiment mit 1920×1080 LEDs eingeordnet und zu ignorieren. Text bleibt als Notiz stehen.

Befund: Stapel `paintEvent` → drei Studio-Frames → `QPainter::drawPolyline` → `QPaintEngineEx::stroke` →
`QRasterPaintEngine::fill` → `QVectorPath::convertToPainterPath`. Beide Stapel gleich (`exe+0xd2e0b` ← `0xd1719` ←
`0xc2d34`).

Hypothese (mittel): `StageView::paintWiring()` (`stage_view.cpp:~250-285`) baut je Kabel ein `QPolygonF` mit **einem Punkt
je LED**, zeichnet es antialiased mit 1,6 px Strich und tut das in **jedem** `paintEvent`, also auch bei laufender
Vorschau pro Frame. Bei einer großen Matrix (laut Screenshot vom 19.11. mehrere tausend LEDs) liegt das weit über 100 ms je Bild; das
Studio wirkt eingefroren. Nur zwei Stellen im Studio rufen `drawPolyline`: `paintWiring` und `paintRings`
(`stage_view_rings.cpp:104`, kurze Polylinien mit `kGizmoRingSegments`, unwahrscheinlich).

Fix-Ideen, in dieser Reihenfolge: (a) Punkte ausdünnen (mindestens 3 px Abstand, nur Sichtbares); (b) den Pfad cachen und nur
bei Layout-/Größen-/Auswahländerung neu bauen, nicht pro Bild; (c) Antialiasing bei vielen Punkten aus.
Test: Benchmark in `tests/bench` bzw. Rauchtest mit 100×100-Matrix und eingeschalteter Verdrahtung, Zeit für `paintWiring`
unter einem großzügigen Limit; Messung vorher/nachher in den CHANGELOG.

## 4. ZURÜCKGESTELLT: Stillstand: Vorschau-Render (`lroundf`) dauert über 5 s (1.0.0, zweimal im selben Prozess)

> Vom Nutzer am 20.11. als Experiment mit 1920×1080 LEDs eingeordnet und zu ignorieren. Text bleibt als Notiz stehen.

Befund: Beide Stapel enden in `ucrtbase!lroundf`/`roundf`, darüber 3 bis 4 Studio-Frames im Bereich `exe+0x134…` bis `0x183…`
(gleiche Codeecke). Auslöser: (a) Tippen in eine `QDoubleSpinBox` → Signalkette über drei Slots → Render; (b) Wechsel eines
Tabs → `Show`-Ereignis eines Widgets (`exe+0xd3c82`) → Render.

Hypothese (niedrig bis mittel): Synchroner Vorschau-Render auf dem Hauptthread (`libs/wh_preview`, `drawDisc`/`drawGlow`/
`view3d.cpp`; `lround` je Pixel und Kanal in `raster.hpp:84`, `:154`, `preview.cpp:66`, `view3d.cpp:205-212`) bei großem
Layout oder großem Bild. 5 s sind viel; entweder sehr viele LEDs/große Scheiben oder ein Fall mit riesigem Radius.
Ohne `.pdb` nicht belegt.

Vorgehen: erst Punkt 0 (Funktion bestätigen); dann reproduzieren mit einem großen Projekt (Nutzer um das Projekt bitten,
das beim Stillstand offen war: Größe, Ansicht 2D/3D, „Schein“ an/aus, Verdrahtung an/aus). Fix-Ideen: `lround` in den
Pixelschleifen durch `static_cast<int>(x + 0.5F)` ersetzen (so wie `drawGlow`, Kommentar in `raster.hpp:128`); den Render
bei schnellen Änderungen zusammenfassen (einmal je Ereignisschleife, Entprellen beim Tippen in Spinboxen); Obergrenze für
Pixel × LED-Fläche. Test: `wh_bench`-Szenario mit ≥ 20 000 LEDs, 2D und 3D mit Schein, mit Zeitbudget.

## 5. Erledigt: Node aus der Palette in den leeren Editor ziehen (0.82.0, zweimal)

Befund: Stapel `QDrag::exec` → `DoDragDrop` → `processDrop` → Event-Filter in der exe, Lesezugriff auf `0x88`
(`rcx = 0x88`): Nullzeiger auf das fehlende Projekt. Die beiden `.dmp`-Dateien (`69a77693`, `f72266ad`) und die beiden
Textberichte (`5f6bb465`, `de4647a6`) sind jeweils Duplikate.
Stand: Behoben in 0.82.1 (CHANGELOG, `node_editor_drag.cpp:88`, `node_editor_library.cpp:93`, CI-Test `--ctrl-n`).
Aufgabe: nur bei der nächsten Windows-Abnahme gegenprüfen, nichts programmieren.

## 6. Optional: Berichte besser machen

- **Version im Bericht (20.11., Nutzerfrage):** Die Textberichte nennen die Versionsnummer (Zeile 2, z. B. „WhiteHoleFX 1.0.0“), aber
  **keinen Commit und keine Build-Herkunft**. Mehrere Builds derselben Version (CI-Lauf, Release-Lauf) sind darin nicht
  unterscheidbar; nur die `.dmp` trägt die Build-GUID (hier `04B04DD3-…`). Fix-Idee: Commit-Hash und Build-GUID (oder Lauf-Nummer
  der CI) in die erste Zeilen der Berichte schreiben. **Arbeitsregel des Nutzers:** nur den neuesten Bericht zählen
  (19.11. 13:29:53 Absturz, 14:17:00 Stillstand); ältere können von anderen Versionen stammen.

- Anhaltende Stillstände zu **einem** Bericht je Episode zusammenfassen (die Episode aus Punkt 3 und 4 ergab vier Berichte in 35 s).
- In den Bericht aufnehmen: Build-/Lauf-Nummer oder Commit, Anzahl LEDs, Bildgröße und Ansicht (2D/3D), Verdrahtung an/aus,
  welcher Tab/Dock aktiv war, die zuletzt ausgelöste Menü-/Knopf-Aktion. Dann sind Stillstände wie 3 und 4 ohne `.pdb` zuordenbar.
- Stillstandsbericht: die Statistik „häufigste Qt-Ereignisse“ (`stallCountEvent`) steht in den Windows-Berichten nicht; prüfen, ob sie dort fehlt, und nachziehen.

## 7. Fehler: Leeres Projekt → Stage Config → Fixtures: beim ersten Hinzufügen erscheint zusätzlich eine 16×8-Matrix

Meldung des Nutzers (20.11., wörtlich): „leeres file -> stage config -> fixtures. liste ist leer. sobald ich eine neus fixture
hinzufüge erscheint automatisch zusätzlich eine 16*8 matrix, sowie das neu angelegte fixture“.

Erwartet: nach dem Hinzufügen steht genau das neue Fixture (bzw. sein Typ) in der Liste.

Ursache (vermutet, aus dem Code gelesen, noch nicht nachgestellt):
- Ohne Projekt ruft das Hinzufügen `projectNeeded` → `MainWindow::newProject(false)`. Das neue Projekt entsteht aus
  `kNewProject` (`main_window_file.cpp:28-60`): dort steht ein **Fixture-Typ „Matrix 16x8“** und ein Fixture „Matrix“ dieses Typs.
- `LayoutPanel::addFixture` (`layout_panel_add.cpp:81-125`) merkt sich das Vorlagen-Fixture (`template_`) und entfernt es danach
  mit `player_.removeFixture(*template_)`. Der **Typ „Matrix 16x8“ bleibt dabei in der Bibliothek stehen** (kein
  `removeFixtureType`). In der Fixtures-Liste/Typenliste taucht er deshalb zusätzlich zum neuen Eintrag auf.
- Gleiches Muster gilt für die beiden anderen Aufrufer von `projectNeeded`: `fixture_library_page.cpp:279` und
  `fixture_library_export.cpp:49` (Import) - dort prüfen, ob sie auch eine Vorlage übrig lassen.

Fix-Idee: Nach dem Entfernen des Vorlagen-Fixtures auch dessen Typ entfernen, sofern kein anderes Fixture ihn nutzt
(Typname vor `removeFixture` merken). Alternativ: `projectNeeded` legt ein wirklich leeres Projekt an - dann entfällt die
ganze Vorlagen-Logik, aber „ein Projekt braucht mindestens ein gepatchtes Fixture“ (Kommentar in `addFixture`) müsste dafür
geprüft werden.

Tests: Qt-Rauchtest (nur in der CI): kein Projekt → Fixture hinzufügen → genau ein Fixture und genau ein Typ in Layout und
Bibliothek, der Typ „Matrix 16x8“ nicht mehr vorhanden. Zusätzlich ein Test ohne Qt auf Player-Ebene (`wh_runtime`):
`removeFixture` auf das letzte Exemplar eines Typs + `removeFixtureType` hinterlässt keine Reste. Dasselbe für den Import-
und den Bibliotheks-Pfad. CHANGELOG-Eintrag nicht vergessen.

## 8. Wunsch: Testmodus per Knopf neben den Ansichts-Umschaltern ein-/ausschalten

Meldung des Nutzers (20.11., wörtlich): „testoutput lässt sich nur im testmenu wieder ausschalten, im preview window erscheint
oben rechts dann: Testmodus:*testmuster*. ich hätte gerne rechts neben den Ansichtsumschaltern, 2d, 3d vorne, 3d oben usw einen
button der den testmodus toggelt“.

Ist-Stand: Der Testmodus wird im Fenster Stage Config (Seite Testmodus, `test_panel.cpp`) ein- und mit „Testmodus beenden“
(`stop_`, setzt `kind_` auf Index 0) ausgeschaltet. In der Vorschau steht nur die Anzeige „Testmodus: <Muster>“ oben rechts.
Die Ansichts-Umschalter (2D, 3D, 3D Front, 3D Seite, 3D Oben) entstehen in `main_window_view3d.cpp` (Tabelle um Zeile 40).

Umsetzung (Vorschlag):
- Neben den Ansichts-Umschaltern einen checkbaren Knopf „Testmodus“ (Tooltip: „Testbild ein/aus“) einfügen. Eingeschaltet
  = Testmodus läuft; Klick schaltet aus (wie `stop_`). Beim Einschalten ohne gewähltes Muster: zuletzt benutztes Muster, sonst das
  erste der Liste (Lesart, ggf. mit dem Nutzer klären).
- Zustand nicht doppelt halten: Knopf spiegelt den Testmodus-Zustand des `Player`/`TestPanel` (Signal bei jeder Änderung, auch wenn
  im Stage-Config-Fenster geschaltet wird), damit beide Stellen nie auseinanderlaufen. Ggf. auch Menüeintrag/Tastenkürzel.
- Neue `tr()`-Strings in en/fr/es/uk übersetzen (`scripts/i18n.py`).

Tests: Qt-Rauchtest (CI): Knopf an → Testmodus aktiv, Anzeige in der Vorschau da; Knopf aus → Testmodus beendet; Umschalten im
Stage-Config-Fenster spiegelt sich im Knopf. Doku (`docs/studio.md`) und CHANGELOG ergänzen.

## 9. Fehler: Output duplizieren macht die Ausgabe schwarz

Meldung des Nutzers (20.11., wörtlich): „bugreport: output duplizieren verursacht das der orginale output von dem deupliziert
wurde schwatz wird.“ (Lesart: der Output-Node `wh.output.canvas` im Node-Editor.)

Ursache (aus dem Code gelesen, noch nicht nachgestellt): `Player::pasteNodes` (`libs/wh_runtime/src/player_paste.cpp:43-52`,
auch für Duplizieren/Presets genutzt) macht einen eingefügten Output-Node ohne Rückfrage zum **Ausgang des Projekts**
(`project_->target = {output, "out"}`). Beim Duplizieren wird nur der Node kopiert, **nicht** das Kabel, das in seinen Eingang
`source` führt (die Quelle liegt außerhalb der Auswahl). Die Kopie hat also keinen Eingang, ist aber jetzt der aktive Ausgang
→ schwarzes Bild, während der Original-Output unbenutzt daneben hängt. Die Sonderbehandlung (Zeile 47-48) verbindet nur, wenn
das bisherige Ziel **kein** Output-Node ist.

**Wichtiger Vorbehalt (20.11., beim Gegenlesen):** Outputs mit angeschlossener `source` werden bereits automatisch addiert
(`graph::mergeOutputs`, `output_merge.cpp`; siehe `docs/studio.md` „Mehrere Outputs“). Ein Output **ohne** `source` bleibt dort
dunkel, und ist er das Ziel, tritt der erste Output mit Effekt an seine Stelle. Nach dem Code müsste das Bild nach dem Duplizieren
also **nicht** schwarz werden - die obige Ursache ist daher unsicher. **Zuerst nachstellen** (Test: Verlauf → Output, Output
duplizieren, `mergeOutputs(graph, project.target)` prüfen und Bild rendern), bevor am Code gedreht wird; ggf. liegt die Ursache
woanders (z. B. Zusammenspiel mit `project_->target`, Live-Look `live_look.cpp`, Zielzuweisung/Rechteck der Kopie).

**Entscheidung des Nutzers (20.11.): Variante (a).** Der bestehende Ausgang bleibt beim Duplizieren unverändert, die Kopie bleibt ein zusätzlicher Node.

Fix-Ideen (Variante (a) gewählt, (b) verworfen):
- (a) Beim Duplizieren/Einfügen den bisherigen Ausgang **nicht** ersetzen, wenn schon ein Output-Node Ziel ist (Kopie bleibt
  zusätzlicher, unverbundener Node). Wohl die erwartbarste Lesart: „Duplizieren ändert nichts am laufenden Bild“.
- (b) Die Kopie bekommt dieselbe Quelle am `source`-Eingang wie das Original und wird Ausgang (dann kein Schwarzbild, aber
  das Original wird still abgelöst).
- Das „neuer Output wird Ausgang“ bleibt nur für Presets/neu angelegte Nodes ohne bestehenden Output-Ausgang (Zweck des
  Kommentars: „Projekt meldet kein Ausgang“).

Tests: `wh_runtime`-Test (Catch2, ohne Qt): Projekt mit Verlauf → Output; Output-Node kopieren/einfügen → `project.target`
zeigt weiter auf das Original bzw. die Kopie hat dieselbe Quelle (je nach Wahl), Bild nicht schwarz; Preset einfügen in Projekt
ohne Ausgang → bekommt weiterhin den Ausgang. Undo-Test: Rückgängig stellt das alte Ziel her. CHANGELOG ergänzen.

