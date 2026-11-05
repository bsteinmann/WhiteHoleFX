// layout_panel_library.cpp: die persönliche Fixture-Bibliothek im Layout (Vorlagen speichern, anlegen, löschen).
//
// Warum eine eigene Datei: layout_panel.cpp und layout_panel_add.cpp sind sonst über der 400-Zeilen-Grenze.
// Die Vorlagen liegen als Dateien im Programmordner des Nutzers (AppData/fixtures); Lesen und Schreiben
// macht wh_project (FixtureLibrary), hier steht nur die Bedienung.
#include <QInputDialog>
#include <QMenu>
#include <QStandardPaths>

#include <filesystem>
#include <string>

#include "layout_panel.hpp"
#include "whitehole/project/fixture_template.hpp"

namespace whitehole::studio {

using form::fromStd;

namespace {

project::FixtureLibrary library() {
    const QByteArray folder = (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/fixtures")).toUtf8();
    return project::FixtureLibrary{std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t*>(folder.constData()), static_cast<std::size_t>(folder.size())}}};
}

}  // namespace

void LayoutPanel::fillLibraryMenu(QMenu* menu) {
    menu->clear();
    QAction* save = menu->addAction(tr("Gewähltes Fixture als Vorlage speichern …"));
    save->setEnabled(!selectedIds().empty());
    connect(save, &QAction::triggered, this, [this]() { saveAsTemplate(); });
    menu->addSeparator();
    const std::vector<project::FixtureTemplate> all = library().list();
    if (all.empty()) {
        menu->addAction(tr("Noch keine Vorlagen"))->setEnabled(false);
        return;
    }
    for (const project::FixtureTemplate& tpl : all) {
        connect(menu->addAction(fromStd(tpl.name)), &QAction::triggered, this, [this, tpl]() { addFromTemplate(tpl); });
    }
    QMenu* remove = menu->addMenu(tr("Vorlage löschen"));
    for (const project::FixtureTemplate& tpl : all) {
        connect(remove->addAction(fromStd(tpl.name)), &QAction::triggered, this, [this, name = tpl.name]() {
            if (const Result<void> r = library().remove(name); !r.ok()) {
                Q_EMIT failed(fromStd(r.error().message));
            }
        });
    }
}

void LayoutPanel::saveAsTemplate() {
    if (player_.project() == nullptr) {
        return;
    }
    const mapping::FixtureId id = selectedId();
    for (const mapping::Fixture& fixture : player_.project()->layout.fixtures()) {
        if (fixture.id != id) {
            continue;
        }
        bool accepted = false;
        const QString name = QInputDialog::getText(this, tr("Als Vorlage speichern"), tr("Name der Vorlage"),
                                                   QLineEdit::Normal, fromStd(fixture.name), &accepted)
                                 .trimmed();
        if (!accepted || name.isEmpty()) {
            return;
        }
        project::FixtureTemplate tpl;
        tpl.name = name.toUtf8().toStdString();
        tpl.shape = fixture.shape;
        for (const output::FixturePatch& patch : player_.project()->patches) {
            if (patch.fixture == fixture.id) {
                tpl.patch = patch;
            }
        }
        if (const Result<void> saved = library().save(tpl); !saved.ok()) {
            Q_EMIT failed(fromStd(saved.error().message));
        }
        return;
    }
    Q_EMIT failed(tr("Wähle zuerst ein Fixture in der Liste."));
}

void LayoutPanel::addFromTemplate(const project::FixtureTemplate& tpl) {
    NewFixture fixture;
    fixture.name = uniqueName(fromStd(tpl.name));
    fixture.origin = nextOrigin();
    fixture.shape = tpl.shape;
    if (tpl.patch) {
        // Universe und Kanal vergibt das Projekt (nächstes freies), Protokoll und Farben kommen aus der Vorlage.
        output::FixturePatch patch = player_.suggestPatch(mapping::ledCount(tpl.shape));
        patch.protocol = tpl.patch->protocol;
        patch.destination = tpl.patch->destination;
        patch.color = tpl.patch->color;
        if (patch.protocol == output::Protocol::Sacn && patch.universe == 0) {
            patch.universe = 1;
        }
        fixture.patchMode = PatchMode::Manual;
        fixture.patch = patch;
    }
    addFixture(fixture);
}

}  // namespace whitehole::studio
