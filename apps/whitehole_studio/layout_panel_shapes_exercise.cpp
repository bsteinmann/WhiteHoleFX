// layout_panel_shapes_exercise.cpp: Rauchtest der Bauform-Bearbeitung im Reiter Layout (früher
// FixtureLibraryPage::exercise, siehe layout_panel_shapes.cpp).
//
// Eigene Datei wegen der 400-Zeilen-Grenze: der Test legt jede Art Bauform an, ändert, benennt um,
// vervielfältigt, prüft den Abgleich mit der Benutzerbibliothek (jetzt über die Fixture-Bibliothek,
// layout_panel_catalog.cpp, die ein Übernehmen immer mit einem Exemplar auf der Bühne verbindet) und räumt auf.
#include "layout_panel.hpp"

#include <QByteArray>
#include <QListWidget>
#include <QTemporaryDir>

#include <iterator>
#include <vector>

#include "shape_form.hpp"
#include "shape_preview.hpp"
#include "user_library.hpp"

namespace whitehole::studio {

namespace {

/// Lenkt die Benutzerbibliothek für die Dauer des Rauchtests in ein Wegwerfverzeichnis (die echte bleibt
/// unberührt) - dasselbe Muster wie layout_panel_catalog.cpp::exerciseCatalog.
class ScopedLibraryPath {
public:
    ScopedLibraryPath()
        : had_{qEnvironmentVariableIsSet("WHITEHOLEFX_FIXTURE_LIBRARY")}, old_{qgetenv("WHITEHOLEFX_FIXTURE_LIBRARY")} {
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

QString LayoutPanel::exerciseShapes() {
    const ScopedLibraryPath libraryPath;
    const std::size_t typesBefore = player_.fixtureTypes().size();
    const ShapeKind all[] = {ShapeKind::Matrix2d, ShapeKind::Matrix3d, ShapeKind::Circle,
                              ShapeKind::Strip,    ShapeKind::Column,  ShapeKind::Cylinder};
    const std::size_t kinds = std::size(all);
    for (const ShapeKind kind : all) {
        if (!addShapeType(kind)) {
            return QStringLiteral("Bauformen: Bauform %1 lässt sich nicht anlegen").arg(static_cast<int>(kind));
        }
        const QString formProblem = shapeForm_->exercise();
        if (selectedShapeName().isEmpty() || !formProblem.isEmpty()) {
            return QStringLiteral("Bauformen: Maße der Bauform %1 lassen sich nicht ändern: %2")
                .arg(static_cast<int>(kind))
                .arg(formProblem);
        }
        if (shapePreview_->ledCount() == 0) {
            return QStringLiteral("Bauformen: Vorschau der Bauform %1 ist leer").arg(static_cast<int>(kind));
        }
    }
    if (player_.fixtureTypes().size() != typesBefore + kinds) {
        return QStringLiteral("Bauformen: erwartet %1 Bauformen, es sind %2")
            .arg(typesBefore + kinds)
            .arg(player_.fixtureTypes().size());
    }
    if (const QString problem = shapePreview_->exercise(); !problem.isEmpty()) {
        return problem;
    }
    renameSelectedShape(QStringLiteral("Umbenannt"));
    if (selectedShapeName() != QStringLiteral("Umbenannt")) {
        return QStringLiteral("Bauformen: Umbenennen wurde nicht übernommen");
    }
    duplicateSelectedShape();
    if (player_.fixtureTypes().size() != typesBefore + kinds + 1) {
        return QStringLiteral("Bauformen: Duplizieren hat keine Bauform angelegt");
    }
    {
        // Bibliothek und Show sind getrennt: gespeichert wird nur auf Knopfdruck (die letzten 6 Bauformen dieses Tests).
        const auto registry = player_.registry();
        const auto none = userlib::load(registry);
        if (!none.ok() || !none.value().empty()) {
            return QStringLiteral("Bauformen: Bibliothek ist ohne Speichern nicht leer");
        }
        for (int row = shapeList_->count() - static_cast<int>(kinds + 1); row < shapeList_->count(); ++row) {
            shapeList_->setCurrentRow(row);
            if (!saveSelectedShapeToLibrary()) {
                return QStringLiteral("Bauformen: Speichern in die Bibliothek schlägt fehl");
            }
        }
        const auto stored = userlib::load(registry);
        if (!stored.ok() || stored.value().size() != kinds + 1 || catalog_->count() != static_cast<int>(kinds + 1)) {
            return QStringLiteral("Bauformen: Bibliothek hat %1 statt %2 Bauformen")
                .arg(stored.ok() ? static_cast<int>(stored.value().size()) : -1)
                .arg(static_cast<int>(kinds + 1));
        }
        // Fehlt eine in der Show, holt „Alle übernehmen“ die Bauform zurück (legt kein Exemplar an) - die
        // Show-Liste bleibt von der Bibliothek getrennt.
        const std::string gone = player_.fixtureTypes().back().name;
        const std::size_t fixturesBeforeGone = player_.project()->layout.fixtures().size();
        if (!player_.removeFixtureType(gone).ok() || importFromUserLibrary() != 1 ||
            player_.fixtureTypes().size() != typesBefore + kinds + 1 ||
            player_.project()->layout.fixtures().size() != fixturesBeforeGone) {
            return QStringLiteral("Bauformen: Alle übernehmen schlägt fehl (hat ungewollt ein Exemplar angelegt?)");
        }
        // Einzeln übernehmen: erst weg aus der Show, dann über die Fixture-Bibliothek zurückholen - anders als
        // „Alle übernehmen“ legt das jetzt immer auch ein Exemplar an (layout_panel_catalog.cpp), ein zweites Mal
        // also ein zweites Exemplar statt abgelehnt zu werden.
        if (!player_.removeFixtureType(gone).ok()) {
            return QStringLiteral("Bauformen: Entfernen aus der Show schlägt fehl");
        }
        fillShapeList(QString{});
        // Wie der Nutzer: zuerst die gespeicherte Bauform anklicken (dann ist das Formular versteckt), dann übernehmen.
        fillCatalogList();
        for (int row = 0; row < catalog_->count(); ++row) {
            if (catalog_->item(row)->data(Qt::UserRole).toString() == QString::fromStdString(gone)) {
                catalog_->setCurrentRow(row);
            }
        }
        const std::size_t fixturesBeforeTakeOver = player_.project()->layout.fixtures().size();
        if (!takeOverFromCatalog(QString::fromStdString(gone)) ||
            player_.fixtureTypes().size() != typesBefore + kinds + 1 ||
            player_.project()->layout.fixtures().size() != fixturesBeforeTakeOver + 1) {
            return QStringLiteral("Bauformen: In Show übernehmen schlägt fehl");
        }
        if (!takeOverFromCatalog(QString::fromStdString(gone)) ||
            player_.project()->layout.fixtures().size() != fixturesBeforeTakeOver + 2) {
            return QStringLiteral("Bauformen: zweites Übernehmen legt kein zweites Exemplar an");
        }
        // Aufräumen: die beiden eben angelegten Exemplare wieder entfernen, bevor es mit der Bauform weitergeht.
        std::vector<mapping::FixtureId> newInstances;
        const auto& fixtures = player_.project()->layout.fixtures();
        for (std::size_t i = fixturesBeforeTakeOver; i < fixtures.size(); ++i) {
            newInstances.push_back(fixtures[i].id);
        }
        for (const mapping::FixtureId id : newInstances) {
            removeFixture(id);
        }
        // Entfernen aus der Show lässt die Bibliothek stehen, Entfernen aus der Bibliothek lässt die Show stehen.
        removeSelectedShape();
        if (catalog_->count() != static_cast<int>(kinds + 1)) {
            return QStringLiteral("Bauformen: Entfernen aus der Show hat die Bibliothek verändert");
        }
        const std::size_t inShow = player_.fixtureTypes().size();
        while (catalog_->count() > 0) {
            catalog_->setCurrentRow(0);
            removeSelectedFromCatalog();
        }
        const auto left = userlib::load(registry);
        if (!left.ok() || !left.value().empty() || player_.fixtureTypes().size() != inShow) {
            return QStringLiteral("Bauformen: Entfernen aus der Bibliothek ist fehlgeschlagen");
        }
    }
    // Aufräumen: alles, was dieser Test angelegt hat, wieder entfernen (der Rauchtest soll das Projekt nicht ändern).
    while (player_.fixtureTypes().size() > typesBefore) {
        shapeList_->setCurrentRow(shapeList_->count() - 1);
        removeSelectedShape();
    }
    return {};
}

}  // namespace whitehole::studio
