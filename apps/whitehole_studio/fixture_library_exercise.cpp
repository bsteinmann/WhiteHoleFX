// fixture_library_exercise.cpp: Rauchtest der Fixture-Bibliothek (FixtureLibraryPage::exercise) für die CI.
//
// Warum eine eigene Datei: der Test legt jede Art Bauform an, ändert, benennt um, vervielfältigt, prüft den Abgleich mit
// der Benutzerbibliothek und räumt auf. Das ist länger als die Seite selbst und soll sie nicht aufblähen.
#include <QByteArray>
#include <QDir>
#include <QListWidget>
#include <QTemporaryDir>

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
    const Kind all[] = {Kind::Matrix2d, Kind::Matrix3d, Kind::Circle, Kind::Strip, Kind::Column};
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
    if (player_.fixtureTypes().size() != before + 5) {
        return QStringLiteral("Fixture-Bibliothek: erwartet %1 Bauformen, es sind %2")
            .arg(before + 5)
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
    if (player_.fixtureTypes().size() != before + 6) {
        return QStringLiteral("Fixture-Bibliothek: Duplizieren hat keine Bauform angelegt");
    }
    {
        // Die Benutzerbibliothek spiegelt alles mit: 6 Bauformen auf der Platte; fehlt eine im Projekt, holt der Knopf sie.
        const auto stored = userlib::load(player_.project()->graph.registryPtr());
        if (!stored.ok() || stored.value().size() != 6) {
            return QStringLiteral("Fixture-Bibliothek: Benutzerbibliothek hat %1 statt 6 Bauformen")
                .arg(stored.ok() ? static_cast<int>(stored.value().size()) : -1);
        }
        const std::string gone = player_.fixtureTypes().back().name;
        if (!player_.removeFixtureType(gone).ok() || importFromUserLibrary() != 1 ||
            player_.fixtureTypes().size() != before + 6) {
            return QStringLiteral("Fixture-Bibliothek: Ergänzen aus der Benutzerbibliothek schlägt fehl");
        }
    }
    removeSelected();
    // Aufräumen: alles, was dieser Test angelegt hat, wieder entfernen (der Rauchtest soll das Projekt nicht ändern).
    while (player_.fixtureTypes().size() > before) {
        list_->setCurrentRow(list_->count() - 1);
        removeSelected();
    }
    const auto left = userlib::load(player_.project()->graph.registryPtr());
    if (!left.ok() || !left.value().empty()) {
        return QStringLiteral("Fixture-Bibliothek: Entfernen hat die Benutzerbibliothek nicht geleert");
    }
    return {};
}

}  // namespace whitehole::studio
