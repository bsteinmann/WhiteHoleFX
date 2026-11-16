// fixture_library_exercise.cpp: Rauchtest der Fixture-Seite (FixtureLibraryPage::exercise) für die CI.
//
// Warum eine eigene Datei: der Test legt jedes Fixture an, ändert, benennt um, vervielfältigt, prüft den Abgleich mit
// der Benutzerbibliothek (eine gemeinsame Liste, materialisiert beim Bearbeiten) und räumt auf. Das ist länger als
// die Seite selbst und soll sie nicht aufblähen.
//
// Die gemeinsame Liste ist alphabetisch sortiert (siehe fixture_catalog.hpp), nicht nach Anlegereihenfolge - der
// Test wählt seine eigenen Einträge deshalb über ihren bekannten Namen (selectType), nie über eine Zeilennummer.
#include <QByteArray>
#include <QDir>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>

#include <algorithm>
#include <iterator>
#include <vector>

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
    std::vector<QString> names;  // die von diesem Test angelegten Fixtures, in Anlegereihenfolge
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
        names.push_back(selectedName());
    }
    if (player_.fixtureTypes().size() != before + kinds) {
        return QStringLiteral("Fixtures: erwartet %1 Fixtures, es sind %2")
            .arg(before + kinds)
            .arg(player_.fixtureTypes().size());
    }
    if (const QString problem = preview_->exercise(); !problem.isEmpty()) {
        return problem;
    }
    // Das zuletzt angelegte (noch gewählte) umbenennen; names merkt sich den neuen Namen dafür.
    renameSelected(QStringLiteral("Umbenannt"));
    if (selectedName() != QStringLiteral("Umbenannt")) {
        return QStringLiteral("Fixtures: Umbenennen wurde nicht übernommen");
    }
    names.back() = QStringLiteral("Umbenannt");
    duplicateSelected();
    if (player_.fixtureTypes().size() != before + kinds + 1) {
        return QStringLiteral("Fixtures: Duplizieren hat kein Fixture angelegt");
    }
    names.push_back(selectedName());  // die Kopie, Name per uniqueName() vergeben
    {
        const auto registry = player_.registry();
        const auto none = userlib::load(registry);
        if (!none.ok() || !none.value().empty()) {
            return QStringLiteral("Fixtures: Bibliothek ist ohne Speichern nicht leer");
        }
        // Alle (kinds + 1) von diesem Test angelegten Fixtures in die Bibliothek speichern - über ihren Namen
        // ausgewählt, nicht über eine Zeilennummer (die gemeinsame Liste ist alphabetisch sortiert).
        for (const QString& name : names) {
            selectType(name);
            if (selectedName() != name || !saveSelectedToLibrary()) {
                return QStringLiteral("Fixtures: Speichern in die Bibliothek schlägt fehl (%1)").arg(name);
            }
        }
        const auto stored = userlib::load(registry);
        if (!stored.ok() || stored.value().size() != kinds + 1) {
            return QStringLiteral("Fixtures: Bibliothek hat %1 statt %2 Fixtures")
                .arg(stored.ok() ? static_cast<int>(stored.value().size()) : -1)
                .arg(static_cast<int>(kinds + 1));
        }
        // Eines aus der Show entfernen (die Bibliothek-Kopie bleibt) - der Eintrag bleibt in der gemeinsamen Liste
        // sichtbar, nur eben nur-in-Bibliothek.
        const QString gone = names.back();
        if (!player_.removeFixtureType(gone.toUtf8().toStdString()).ok()) {
            return QStringLiteral("Fixtures: Entfernen aus der Show schlägt fehl");
        }
        fillList(gone);
        if (selectedName() != gone) {
            return QStringLiteral("Fixtures: nur-in-Bibliothek-Fixture fehlt in der gemeinsamen Liste");
        }
        if (remove_->isEnabled()) {
            // remove_ muss deaktiviert sein, solange das Fixture nicht in der Show ist (inProject == false).
            return QStringLiteral("Fixtures: „Entfernen“ ist für ein nur-in-Bibliothek-Fixture fälschlich aktiv");
        }
        // Bearbeiten (hier: Umbenennen) holt es automatisch in die Show zurück (ensureInProject).
        const QString back = QStringLiteral("Zurückgeholt");
        renameSelected(back);
        if (selectedName() != back ||
            player_.fixtureTypes().size() != before + kinds + 1 ||
            !remove_->isEnabled()) {
            return QStringLiteral("Fixtures: Bearbeiten eines Bibliothek-Fixtures holt es nicht automatisch zurück");
        }
        // Entfernen aus der Show lässt die Bibliothek-Kopie stehen (unter dem alten Namen „gone“).
        removeSelected();
        const auto stillStored = userlib::load(registry);
        if (!stillStored.ok() || stillStored.value().size() != kinds + 1) {
            return QStringLiteral("Fixtures: Entfernen aus der Show hat die Bibliothek verändert");
        }
        fillList(gone);
        // Aus der Bibliothek löschen, jeweils über den bekannten Namen; ein Eintrag, der auch in der Show ist,
        // bleibt in der Liste stehen (nur ohne Bibliothek-Herkunft).
        const std::size_t inShow = player_.fixtureTypes().size();
        for (const QString& name : names) {
            selectType(name);
            if (selectedName() != name) {
                return QStringLiteral("Fixtures: „%1“ fehlt vor dem Löschen aus der Bibliothek").arg(name);
            }
            removeFromLibrary();
        }
        const auto left = userlib::load(registry);
        if (!left.ok() || !left.value().empty() || player_.fixtureTypes().size() != inShow) {
            return QStringLiteral("Fixtures: Entfernen aus der Bibliothek ist fehlgeschlagen");
        }
    }
    // Aufräumen: alles, was dieser Test angelegt hat, wieder entfernen (der Rauchtest soll das Projekt nicht ändern).
    for (const QString& name : names) {
        if (std::ranges::any_of(player_.fixtureTypes(),
                                 [&](const project::FixtureType& t) { return QString::fromStdString(t.name) == name; })) {
            selectType(name);
            removeSelected();
        }
    }
    if (player_.fixtureTypes().size() != before) {
        return QStringLiteral("Fixtures: Aufräumen scheitert");
    }
    return {};
}

}  // namespace whitehole::studio
