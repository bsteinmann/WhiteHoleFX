// layout_panel_add.cpp: neue Fixtures anlegen (Matrix, Strip, Punktwolke aus einer Datei).
//
// Warum eine eigene Datei: layout_panel.cpp (Liste und Seitenaufbau) ist sonst über die 400-Zeilen-Grenze.
// Hier steht alles, was ein Fixture neu anlegt, samt dem Import von Positionen aus CSV/JSON
// (Formatlogik in wh_mapping, Datei lesen in wh_runtime).
#include "layout_panel.hpp"

#include <QCheckBox>
#include <QLabel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <variant>

#include "whitehole/runtime/point_file.hpp"

namespace whitehole::studio {

using form::fromStd;

namespace {

/// Grundname für ein Fixture ohne eigenen Namen.
QString baseNameOf(const mapping::FixtureShape& shape) {
    if (std::holds_alternative<mapping::MatrixShape>(shape)) {
        return QObject::tr("Matrix");
    }
    if (std::holds_alternative<mapping::StripShape>(shape)) {
        return QObject::tr("Strip");
    }
    if (std::holds_alternative<mapping::GridShape>(shape)) {
        return QObject::tr("Cube");
    }
    return QObject::tr("Punkte");
}

}  // namespace

void LayoutPanel::addFixture(const QString& baseName, mapping::FixtureShape shape) {
    NewFixture fixture;
    fixture.name = uniqueName(baseName);
    fixture.origin = nextOrigin();
    fixture.shape = std::move(shape);
    addFixture(fixture);
}

void LayoutPanel::addFromDialog() {
    AddFixtureDialog dialog{player_, nextOrigin(), this};
    if (dialog.exec() == QDialog::Accepted) {
        addFixture(dialog.result());
    }
}

void LayoutPanel::addFixture(const NewFixture& fixture) {
    // Ohne Projekt (Studio frisch gestartet) legt das Hauptfenster ein neues an. Dessen Vorlagen-
    // Matrix wird danach wieder entfernt, damit nur das Fixture übrig bleibt, das der Nutzer wollte.
    std::optional<mapping::FixtureId> template_;
    if (player_.project() == nullptr) {
        Q_EMIT projectNeeded();
        if (player_.project() == nullptr || player_.project()->layout.fixtures().empty()) {
            Q_EMIT failed(tr("Es ist kein Projekt geladen. Mit Strg+N ein neues anlegen oder mit Strg+O eins öffnen."));
            return;
        }
        template_ = player_.project()->layout.fixtures().front().id;
    }
    const std::uint64_t leds = mapping::ledCount(fixture.shape);
    const QString name = fixture.name.isEmpty() ? uniqueName(baseNameOf(fixture.shape)) : fixture.name;
    // Neben der Vorlage gilt vorläufig das nächste freie Universe (sonst stieße ein manueller Patch an die Vorlage);
    // den endgültigen Patch setzt der Abschnitt unten, nachdem die Vorlage weg ist.
    std::optional<output::FixturePatch> first;
    if (fixture.patchMode == PatchMode::Auto || template_) {
        first = player_.suggestPatch(leds);
    } else if (fixture.patchMode == PatchMode::Manual) {
        first = fixture.patch;
    }
    const auto id = player_.addFixture(name.toUtf8().toStdString(), fixture.origin, fixture.shape, first);
    if (!id.ok()) {
        Q_EMIT failed(fromStd(id.error().message));
        return;
    }
    if (template_) {
        // Das neue Fixture ist jetzt das einzige: Es bekommt das erste Universe, es sei denn, der Nutzer hat
        // einen Patch vorgegeben. "Nicht senden" geht hier nicht, ein Projekt braucht mindestens ein gepatchtes Fixture.
        Result<void> r = player_.removeFixture(*template_);
        if (r.ok()) {
            r = player_.moveFixture(id.value(), fixture.origin);
        }
        if (r.ok()) {
            r = player_.setPatch(id.value(), fixture.patchMode == PatchMode::Manual ? fixture.patch : output::FixturePatch{});
        }
        if (!r.ok()) {
            Q_EMIT failed(fromStd(r.error().message));
        }
    }
    fillList(id.value());
    Q_EMIT edited();
}

void LayoutPanel::addMatrix() {
    mapping::MatrixShape m;
    m.columns = 16;
    m.rows = 8;
    m.wiring.serpentine = true;
    addFixture(tr("Matrix"), m);
}

void LayoutPanel::addStrip() {
    mapping::StripShape s;
    s.count = 60;
    s.end = Vec3{60.0F, 0.0F, 0.0F};
    addFixture(tr("Strip"), s);
}

bool LayoutPanel::importPointsFile(const QString& path, const mapping::ImportOptions& options) {
    const std::string file = path.toUtf8().toStdString();
    auto points = runtime::loadPointsFile(file, options);
    if (!points.ok()) {
        Q_EMIT failed(fromStd(points.error().message));
        return false;
    }
    mapping::PointsShape shape;
    shape.points = std::move(points).value();
    const int before = list_->count();
    addFixture(fromStd(runtime::pointsFixtureName(file)), std::move(shape));
    return list_->count() > before;
}

void LayoutPanel::importPoints() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("LED-Positionen importieren"), QString{},
        tr("Positionen (*.csv *.txt *.tsv *.json);;Alle Dateien (*)"));
    if (path.isEmpty()) {
        return;
    }
    QDialog dialog{this};
    dialog.setWindowTitle(tr("Import: %1").arg(QDir::toNativeSeparators(path)));
    auto* form = new QFormLayout{&dialog};
    auto* scale = new QDoubleSpinBox;
    scale->setDecimals(4);
    scale->setRange(0.0001, 10000.0);
    scale->setValue(1.0);
    scale->setToolTip(tr("Faktor auf alle Koordinaten, z. B. 0,1 für Millimeter in Zentimeter"));
    auto* flipY = new QCheckBox{tr("y spiegeln (y zeigt in der Datei nach unten)")};
    auto* toOrigin = new QCheckBox{tr("kleinste Koordinate auf 0 schieben")};
    toOrigin->setChecked(true);
    form->addRow(tr("Maßstab"), scale);
    form->addRow(QString{}, flipY);
    form->addRow(QString{}, toOrigin);
    form->addRow(new QLabel{tr("Die Reihenfolge in der Datei ist die Reihenfolge am Datenkabel.")});
    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    mapping::ImportOptions options;
    options.scale = static_cast<float>(scale->value());
    options.flipY = flipY->isChecked();
    options.toOrigin = toOrigin->isChecked();
    importPointsFile(path, options);
}

QString LayoutPanel::exerciseImport() {
    // Eine Punktwolke aus einer CSV-Datei: 5 Punkte, y gespiegelt; sie muss als Fixture erscheinen.
    const auto file = std::filesystem::temp_directory_path() / "whfx_exercise_points.csv";
    {
        std::ofstream out{file, std::ios::binary};
        out << "x;y\n0;0\n1,5;0\n3;0\n4,5;1\n6;2\n";
    }
    const std::size_t before = player_.project()->layout.fixtures().size();
    mapping::ImportOptions options;
    options.flipY = true;
    const bool ok = importPointsFile(QString::fromStdString(file.string()), options);
    std::filesystem::remove(file);
    if (!ok || player_.project()->layout.fixtures().size() != before + 1) {
        return QStringLiteral("Layout: der Import einer CSV-Datei hat kein Fixture angelegt");
    }
    const mapping::Fixture& added = player_.project()->layout.fixtures().back();
    const auto* shape = std::get_if<mapping::PointsShape>(&added.shape);
    if (shape == nullptr || shape->points.size() != 5 || shape->points[3].x < 4.4F || shape->points[3].x > 4.6F) {
        return QStringLiteral("Layout: importierte Punkte stimmen nicht");
    }
    removeSelected();
    if (player_.project()->layout.fixtures().size() != before) {
        return QStringLiteral("Layout: importiertes Fixture ließ sich nicht entfernen");
    }
    return {};
}

QString LayoutPanel::exerciseAddDialog() {
    // Der Dialog füllt ein 4×3×2-Gitter aus (Startecke rechts und vorn); das Panel legt es an.
    AddFixtureDialog dialog{player_, nextOrigin(), this};
    if (const QString note = dialog.exercise(); !note.isEmpty()) {
        return note;
    }
    const std::size_t before = player_.project()->layout.fixtures().size();
    addFixture(dialog.result());
    const auto& fixtures = player_.project()->layout.fixtures();
    if (fixtures.size() != before + 1) {
        return QStringLiteral("Hinzufügen-Dialog: das Gitter wurde nicht angelegt");
    }
    const mapping::Fixture& added = fixtures.back();
    const auto* grid = std::get_if<mapping::GridShape>(&added.shape);
    if (grid == nullptr || mapping::ledCount(added.shape) != 24 || !grid->wiring.flipX || !grid->wiring.flipZ ||
        added.origin.x != 10.0F) {
        return QStringLiteral("Hinzufügen-Dialog: das angelegte Gitter hat nicht die Werte aus dem Dialog");
    }
    // Die erste LED am Kabel sitzt in der Startecke: x max, y min, z max (Abstand 2, Zellmitte).
    const Vec3 first = mapping::localPositions(added.shape).front();
    if (std::abs(first.x - 7.0F) > 1e-4F || std::abs(first.y - 1.0F) > 1e-4F || std::abs(first.z - 3.0F) > 1e-4F) {
        return QStringLiteral("Hinzufügen-Dialog: die erste LED sitzt nicht in der gewählten Startecke");
    }
    bool patched = false;
    for (const output::FixturePatch& patch : player_.project()->patches) {
        patched = patched || patch.fixture == added.id;
    }
    if (!patched) {
        return QStringLiteral("Hinzufügen-Dialog: das Gitter hat keinen Patch bekommen");
    }
    setSelectedIds({added.id});
    removeSelected();
    if (player_.project()->layout.fixtures().size() != before) {
        return QStringLiteral("Hinzufügen-Dialog: das Gitter lässt sich nicht wieder entfernen");
    }
    return {};
}

}  // namespace whitehole::studio
