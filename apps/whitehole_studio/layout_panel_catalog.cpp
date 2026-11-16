// layout_panel_catalog.cpp: Fixture-Bibliothek im Reiter Layout: alle gespeicherten Bauformen in diese Show übernehmen.
//
// Warum eine eigene Datei: layout_panel.cpp und layout_panel_add.cpp sind an der 400-Zeilen-Grenze. Anlegen,
// Bearbeiten und Speichern einer Bauform bleibt Aufgabe des Reiters Fixtures (siehe fixture_library_user.cpp); hier
// steht nur das Lesen der Benutzerbibliothek (user_library.hpp) und das Übernehmen einer gewählten Bauform in diese
// Show - dieselbe Bibliotheksdatei wie dort, nur von einer zweiten Stelle aus erreichbar.
#include "layout_panel.hpp"

#include <QByteArray>
#include <QListWidget>
#include <QSignalBlocker>
#include <QTemporaryDir>

#include <algorithm>

#include "fixture_shape_label.hpp"
#include "user_library.hpp"

namespace whitehole::studio {

using form::fromStd;

void LayoutPanel::fillCatalogList() {
    const QString keep = selectedCatalogName();
    const QSignalBlocker blocker{catalog_};
    catalog_->clear();
    const auto stored = userlib::load(player_.registry());
    if (!stored.ok()) {
        Q_EMIT failed(fromStd(stored.error().message));
        return;
    }
    for (const project::FixtureType& type : stored.value()) {
        const QString name = fromStd(type.name);
        // Name zuerst (frei vergeben), dahinter die Original-Bezeichnung der Form: sonst sieht man einem Namen wie
        // "Kachel 5x3" nicht mehr an, ob dahinter wirklich eine Matrix steckt (siehe fixture_shape_label.hpp).
        auto* item = new QListWidgetItem{
            tr("%1 — %2 (%3 LEDs)").arg(name, shapeKindLabel(type.shape)).arg(mapping::ledCount(type.shape))};
        item->setData(Qt::UserRole, name);
        catalog_->addItem(item);
        if (name == keep) {
            catalog_->setCurrentItem(item);
        }
    }
    if (catalog_->count() == 0) {
        auto* none = new QListWidgetItem{tr("Noch keine gespeicherten Fixtures (Stage Config → Fixtures)")};
        none->setFlags(Qt::NoItemFlags);
        catalog_->addItem(none);
    }
}

QString LayoutPanel::selectedCatalogName() const {
    const QListWidgetItem* item = catalog_->currentItem();
    return item != nullptr ? item->data(Qt::UserRole).toString() : QString{};
}

bool LayoutPanel::takeOverFromCatalog(const QString& name) {
    if (name.isEmpty()) {
        return false;  // nichts gewählt (zum Beispiel der Platzhalter „Noch keine gespeicherten Fixtures“)
    }
    if (player_.project() == nullptr) {
        Q_EMIT projectNeeded();
        if (player_.project() == nullptr) {
            return false;
        }
    }
    const auto stored = userlib::load(player_.registry());
    if (!stored.ok()) {
        Q_EMIT failed(fromStd(stored.error().message));
        return false;
    }
    const std::string key = name.toUtf8().toStdString();
    const auto found = std::ranges::find(stored.value(), key, &project::FixtureType::name);
    if (found == stored.value().end()) {
        Q_EMIT failed(tr("Es gibt keine gespeicherte Bauform \"%1\"").arg(name));
        return false;
    }
    if (std::ranges::any_of(player_.fixtureTypes(), [&](const project::FixtureType& t) { return t.name == found->name; })) {
        Q_EMIT failed(tr("In dieser Show gibt es schon eine Bauform „%1“.").arg(name));
        return false;
    }
    const Result<void> added = player_.addFixtureType(found->name, found->shape);
    if (!added.ok()) {
        Q_EMIT failed(fromStd(added.error().message));
        return false;
    }
    fillTypeList();
    Q_EMIT edited();
    return true;
}

namespace {

/// Lenkt die Benutzerbibliothek für die Dauer des Rauchtests in ein Wegwerfverzeichnis (die echte bleibt
/// unberührt) - dasselbe Muster wie FixtureLibraryPage::exercise (fixture_library_exercise.cpp).
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

QString LayoutPanel::exerciseCatalog() {
    const ScopedLibraryPath libraryPath;
    mapping::RingShape ring;
    ring.count = 8;
    const project::FixtureType type{"Rauchtest-Bibliothek", ring};
    if (!userlib::upsert(player_.registry(), type).ok()) {
        return QStringLiteral("Fixture-Bibliothek: Testbauform lässt sich nicht speichern");
    }
    fillCatalogList();
    bool found = false;
    for (int row = 0; row < catalog_->count(); ++row) {
        if (catalog_->item(row)->data(Qt::UserRole).toString() == QStringLiteral("Rauchtest-Bibliothek")) {
            catalog_->setCurrentRow(row);
            found = true;
        }
    }
    if (!found) {
        return QStringLiteral("Fixture-Bibliothek: Testbauform erscheint nicht in der Liste");
    }
    const std::size_t before = player_.fixtureTypes().size();
    if (!takeOverFromCatalog(selectedCatalogName()) || player_.fixtureTypes().size() != before + 1) {
        return QStringLiteral("Fixture-Bibliothek: Übernehmen in die Show schlägt fehl");
    }
    if (takeOverFromCatalog(selectedCatalogName())) {
        return QStringLiteral("Fixture-Bibliothek: doppeltes Übernehmen hätte abgelehnt werden müssen");
    }
    if (!player_.removeFixtureType("Rauchtest-Bibliothek").ok() || player_.fixtureTypes().size() != before) {
        return QStringLiteral("Fixture-Bibliothek: Aufräumen in der Show schlägt fehl");
    }
    return {};
}

}  // namespace whitehole::studio
