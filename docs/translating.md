# Sprachen und Übersetzen

Die Oberfläche (`whitehole_studio`) gibt es auf Deutsch (Quelltext), Englisch, Französisch und Spanisch.

## Benutzer

- **Hilfe → Sprache / Language** wählt die Sprache. Die Wahl wird gespeichert (`ui/language`) und gilt nach einem Neustart,
  den das Programm anbietet; das Projekt wird dabei wieder geöffnet.
- Ohne Wahl gilt die Sprache des Systems, wenn sie unterstützt wird, sonst Englisch.
- `whitehole_studio --lang de|en|fr|es` überstimmt die Wahl für einen Start. Mit `--grab` (Rauchtests) ist die Vorgabe Deutsch.
- Namen von Nodes, Parametern und Ports bleiben englisch, damit Anleitungen und Projekte in allen Sprachen zusammenpassen.
  Meldungen aus den Bibliotheken (zum Beispiel Fehler beim Laden einer Datei) sind deutsch.

## Wie die Texte entstehen

Alle sichtbaren Texte stehen deutsch im Quelltext, eingehüllt in `tr("…")` (Konstanten in Tabellen mit `QT_TR_NOOP("…")`).
Für jede Sprache gibt es eine gettext-Datei in `apps/whitehole_studio/i18n/` (`en.po`, `fr.po`, `es.po`, dazu die Vorlage
`messages.pot`). Sie sind im Programm eingebettet (`studio.qrc`) und werden von `wh_i18n` gelesen; `lupdate`, `lrelease` oder
Qt Linguist sind nicht nötig. Ein Eintrag ohne Übersetzung fällt auf den deutschen Text zurück.

## Neue oder geänderte Texte

```bash
python3 scripts/i18n.py update   # neue Texte (leeres msgstr) in messages.pot und die .po-Dateien eintragen
# msgstr in en.po, fr.po und es.po ausfüllen (jedes Werkzeug für .po-Dateien geht, auch ein Texteditor)
python3 scripts/i18n.py check    # Exit 1 bei fehlenden Übersetzungen oder abweichenden Platzhaltern (%1, %2 …)
```

Regeln: Platzhalter `%1`, `%2` bleiben erhalten (die Reihenfolge darf wechseln), Beschleuniger `&` ebenso (ein `&` pro Menütext).
Der Testfall `Kataloge: en, fr und es sind vollständig …` und die CI-Prüfung fallen auf, wenn etwas fehlt. Texte in
`exercise…`-Funktionen (Meldungen der Rauchtests) werden nicht übersetzt.

## Eine neue Sprache hinzufügen

1. Eintrag in `libs/wh_i18n/include/whitehole/i18n/language.hpp` (`kLanguages`) und `LANGS` in `scripts/i18n.py`.
2. `python3 scripts/i18n.py update` legt die Datei an; übersetzen; `<file alias="i18n/xx.po">` in `studio.qrc`.
3. Tutorial als `docs/tutorial/xx.md` (und in `studio.qrc`).
4. Die Tests in `tests/i18n/po_catalog_test.cpp` durchlaufen die Liste der Sprachen (dort `{"en","fr","es"}` ergänzen).
