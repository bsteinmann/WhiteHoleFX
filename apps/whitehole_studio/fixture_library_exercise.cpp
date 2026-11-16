// fixture_library_exercise.cpp: Rauchtest der Fixture-Bibliothek (FixtureLibraryPage::exercise) für die CI.
//
// Warum eine eigene Datei: der Test legt jede Art Bauform an, ändert, benennt um, vervielfältigt, prüft den Abgleich mit
// der Benutzerbibliothek und räumt auf. Das ist länger als die Seite selbst und soll sie nicht aufblähen.
#include <QByteArray>
#include <QDir>
#include <QListWidget>
#include <QTemporaryDir>

#include <iterator>

#include "fixture_library_page.hpp"
#include "shape_form.hpp"
#include "shape_preview.hpp"
#include "user_library.hpp"

namespace whitehole::studio {

namespace {

/// Lenkt die Benutzerbibliothek für die Dauer des Rauchtests in ein Wegwerfverzeichnis (die echte bleibt unberührt).
class ScopedLibraryPath {
public:
    ScopedLibraryPath() : had_{qEnvironmentVariableIsSet("WHITEHOLEFX_FIXTURE_LIBRARY")},
                          old_{qgetenv("WHITEHOLEFX_FIXTURE_LIBRARY")} {
        qputenv("WHITEHOLEFX_FIXTURE_LIBRARY", dir_.filePath(QStringLiteral("fixture-library.whfx")).toUtf8());
    }
    ~ScopedLibraryPath() {
        if (had_) {
            qputenv("WHITEHOLEFX_FIXTURE_LIBRARY", old_);
        } else {
            qunsetenv("WHITEHOLEFX_FIXTURE_LIBRARY");
        }
    }
    ScopedLibraryPath(const ScopedLibraryPath&) = delete;
    ScopedLibraryPath& operator=(const ScopedLibraryPath&) = delete;

private:
    QTemporaryDir dir_;
    bool had_;
    QByteArray old_;
};

}  // namespace

QString FixtureLibraryPage::exercise() {
    const ScopedLibraryPath libraryPath;
    rebuild();
    const std::size_t before = player_.fixtureTypes().size();
    const Kind all[] = {Kind::Matrix2d, Kind::Matrix3d, Kind::Circle, Kind::Strip, Kind::Column, Kind::Cylinder};
    const std::size_t kinds = std::size(all);
    for (const Kind kind : all) {
        if (!addType(kind)) {
            return QStringLiteral("Fixture-Bibliothek: Bauform %1 lässt sich nicht anlegen").arg(static_cast<int>(kind));
        }
        const QString formProblem = form_->exercise();
        if (selectedName().isEmpty() || !formProblem.isEmpty()) {
            return QStringLiteral("Fixture-Bibliothek: Maße der Bauform %1 lassen sich nicht ändern: %2")
                .arg(static_cast<int>(kind))
                .arg(formProblem);
        }
        if (preview_->ledCount() == 0) {
            return QStringLiteral("Fixture-Bibliothek: Vorschau der Bauform %1 ist leer").arg(static_cast<int>(kind));
        }
    }
    if (player_.fixtureTypes().size() != before + kinds) {
        return QStringLiteral("Fixture-Bibliothek: erwartet %1 Bauformen, es sind %2")
            .arg(before + kinds)
            .arg(player_.fixtureTypes().size());
    }
    if (const QString problem = preview_->exercise(); !problem.isEmpty()) {
        return problem;
    }
    renameSelected(QStringLiteral("Umbenannt"));
    if (selectedName() != QStringLiteral("Umbenannt")) {
        return QStringLiteral("Fixture-Bibliothek: Umbenennen wurde nicht übernommen");
    }
    duplicateSelected();
    if (player_.fixtureTypes().size() != before + kinds + 1) {
        return QStringLiteral("Fixture-Bibliothek: Duplizieren hat keine Bauform angelegt");
    }
    {
        // Bibliothek und Show sind getrennt: gespeichert wird nur auf Knopfdruck (die letzten 6 Bauformen dieses Tests).
        const auto registry = player_.registry();
        const auto none = userlib::load(registry);
        if (!none.ok() || !none.value().empty()) {
            return QStringLiteral("Fixture-Bibliothek: Bibliothek ist ohne Speichern nicht leer");
        }
        for (int row = list_->count() - static_cast<int>(kinds + 1); row < list_->count(); ++row) {
            list_->setCurrentRow(row);
            if (!saveSelectedToLibrary()) {
                return QStringLiteral("Fixture-Bibliothek: Speichern in die Bibliothek schlägt fehl");
            }
        }
        const auto stored = userlib::load(registry);
        if (!stored.ok() || stored.value().size() != kinds + 1 || stored_->count() != static_cast<int>(kinds + 1)) {
            return QStringLiteral("Fixture-Bibliothek: Bibliothek hat %1 statt %2 Bauformen")
                .arg(stored.ok() ? static_cast<int>(stored.value().size()) : -1)
                .arg(static_cast<int>(kinds + 1));
        }
        // Fehlt eine in der Show, holt „Alle übernehmen“ sie zurück; die Show-Liste bleibt von der Bibliothek getrennt
        const std::string gone = player_.fixtureTypes().back().name;
        if (!player_.removeFixtureType(gone).ok() || importFromUserLibrary() != 1 ||
            player_.fixtureTypes().size() != before + kinds + 1) {
            return QStringLiteral("Fixture-Bibliothek: Alle übernehmen schlägt fehl");
        }
        // Einzeln übernehmen: erst weg aus der Show, dann aus der gespeicherten Liste zurückholen; zweites Mal: Meldung
        if (!player_.removeFixtureType(gone).ok()) {
            return QStringLiteral("Fixture-Bibliothek: Entfernen aus der Show schlägt fehl");
        }
        fillList(QString{});
        // Wie der Nutzer: zuerst die gespeicherte Bauform anklicken (dann ist das Formular versteckt), dann übernehmen.
        fillStored(QString::fromStdString(gone));
        if (!takeOverStored(QString::fromStdString(gone)) || player_.fixtureTypes().size() != before + kinds + 1 ||
            takeOverStored(QString::fromStdString(gone))) {
            return QStringLiteral("Fixture-Bibliothek: In Show übernehmen schlägt fehl");
        }
        // Danach ist die geholte Bauform in der Show gewählt und ihr Formular zu sehen, auch nach einem rebuild()
        // (das die gespeicherte Liste neu füllt); vorher blieb die gespeicherte Zeile gewählt und versteckte es.
        rebuild();
        if (selectedName() != QString::fromStdString(gone) || !form_->isVisibleTo(this) || stored_->currentRow() >= 0) {
            return QStringLiteral("Fixture-Bibliothek: nach dem Übernehmen fehlen die Parameter der Bauform");
        }
        // Entfernen aus der Show lässt die Bibliothek stehen, Entfernen aus der Bibliothek lässt die Show stehen
        removeSelected();
        if (stored_->count() != static_cast<int>(kinds + 1)) {
            return QStringLiteral("Fixture-Bibliothek: Entfernen aus der Show hat die Bibliothek verändert");
        }
        const std::size_t inShow = player_.fixtureTypes().size();
        while (stored_->count() > 0) {
            stored_->setCurrentRow(0);
            removeStored(selectedStoredName());
        }
        const auto left = userlib::load(registry);
        if (!left.ok() || !left.value().empty() || player_.fixtureTypes().size() != inShow) {
            return QStringLiteral("Fixture-Bibliothek: Entfernen aus der Bibliothek ist fehlgeschlagen");
        }
    }
    // Aufräumen: alles, was dieser Test angelegt hat, wieder entfernen (der Rauchtest soll das Projekt nicht ändern).
    while (player_.fixtureTypes().size() > before) {
        list_->setCurrentRow(list_->count() - 1);
        removeSelected();
    }
    return {};
}

}  // namespace whitehole::studio
