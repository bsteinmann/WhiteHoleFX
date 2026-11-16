// fixture_library_exercise.cpp: Rauchtest der Fixture-Seite (FixtureLibraryPage::exercise) für die CI.
//
// Warum eine eigene Datei: der Test legt jedes Fixture an, ändert, benennt um, vervielfältigt, exportiert und
// importiert es wieder und räumt auf. Das ist länger als die Seite selbst und soll sie nicht aufblähen.
#include <QListWidget>
#include <QTemporaryDir>

#include <algorithm>
#include <iterator>

#include "fixture_library_page.hpp"
#include "shape_form.hpp"
#include "shape_preview.hpp"

namespace whitehole::studio {

QString FixtureLibraryPage::exercise() {
    const QTemporaryDir exportDir;
    rebuild();
    const std::size_t before = player_.fixtureTypes().size();
    const Kind all[] = {Kind::Matrix2d, Kind::Matrix3d, Kind::Circle, Kind::Strip, Kind::Column, Kind::Cylinder};
    const std::size_t kinds = std::size(all);
    for (const Kind kind : all) {
        if (!addType(kind)) {
            return QStringLiteral("Fixtures: Fixture %1 lässt sich nicht anlegen").arg(static_cast<int>(kind));
        }
        const QString formProblem = form_->exercise();
        if (selectedName().isEmpty() || !formProblem.isEmpty()) {
            return QStringLiteral("Fixtures: Maße des Fixtures %1 lassen sich nicht ändern: %2")
                .arg(static_cast<int>(kind))
                .arg(formProblem);
        }
        if (preview_->ledCount() == 0) {
            return QStringLiteral("Fixtures: Vorschau des Fixtures %1 ist leer").arg(static_cast<int>(kind));
        }
    }
    if (player_.fixtureTypes().size() != before + kinds) {
        return QStringLiteral("Fixtures: erwartet %1 Fixtures, es sind %2")
            .arg(before + kinds)
            .arg(player_.fixtureTypes().size());
    }
    if (const QString problem = preview_->exercise(); !problem.isEmpty()) {
        return problem;
    }
    renameSelected(QStringLiteral("Umbenannt"));
    if (selectedName() != QStringLiteral("Umbenannt")) {
        return QStringLiteral("Fixtures: Umbenennen wurde nicht übernommen");
    }
    duplicateSelected();
    if (player_.fixtureTypes().size() != before + kinds + 1) {
        return QStringLiteral("Fixtures: Duplizieren hat kein Fixture angelegt");
    }
    // Export: das gewählte (gerade duplizierte) Fixture in eine eigene Datei schreiben.
    const QString exported = selectedName();
    const QString exportPath = exportDir.filePath(QStringLiteral("export.whfx-fixture"));
    if (!exportSelectedTo(exportPath)) {
        return QStringLiteral("Fixtures: Export schlägt fehl");
    }
    // Entfernen und wieder importieren: der importierte Eintrag bekommt denselben Namen zurück (frei, da entfernt).
    removeSelected();
    if (std::ranges::any_of(player_.fixtureTypes(),
                            [&](const project::FixtureType& t) { return QString::fromStdString(t.name) == exported; })) {
        return QStringLiteral("Fixtures: Entfernen vor dem Re-Import schlägt fehl");
    }
    if (importFrom(exportPath) != 1) {
        return QStringLiteral("Fixtures: Import schlägt fehl");
    }
    if (selectedName() != exported) {
        return QStringLiteral("Fixtures: importiertes Fixture heißt nicht wie exportiert");
    }
    if (player_.fixtureTypes().size() != before + kinds + 1) {
        return QStringLiteral("Fixtures: Import hat nicht genau ein Fixture angelegt");
    }
    // Erneuter Import derselben Datei darf den Namen nicht überschreiben, sondern eindeutig machen.
    if (importFrom(exportPath) != 1 || player_.fixtureTypes().size() != before + kinds + 2) {
        return QStringLiteral("Fixtures: zweiter Import überschreibt statt eindeutig zu benennen");
    }
    if (selectedName() == exported) {
        return QStringLiteral("Fixtures: zweiter Import hat denselben Namen wie der erste");
    }
    // Aufräumen: alles, was dieser Test angelegt hat, wieder entfernen (der Rauchtest soll das Projekt nicht ändern).
    while (player_.fixtureTypes().size() > before) {
        list_->setCurrentRow(list_->count() - 1);
        removeSelected();
    }
    return {};
}

}  // namespace whitehole::studio
