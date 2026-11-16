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

#include <algorithm>
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

/// Nächster freier Bauform-Name (analog zu LayoutPanel::uniqueName, aber gegen die Bauform-Bibliothek
/// geprüft statt gegen die Fixture-Liste): "Typ wird Pflicht" (siehe CLAUDE.md) - jedes neu angelegte
/// Fixture bekommt jetzt immer eine eigene Bauform, auch aus dem Dialog "Fixture hinzufügen" und beim
/// Punktwolken-Import, die vorher mit einem typlosen Fixture endeten.
QString uniqueTypeName(const runtime::Player& player, const QString& base) {
    for (int i = 1;; ++i) {
        const QString candidate = QStringLiteral("%1 %2").arg(base).arg(i);
        const std::string name = candidate.toUtf8().toStdString();
        const bool taken = std::ranges::any_of(player.fixtureTypes(), [&](const project::FixtureType& t) { return t.name == name; });
        if (!taken) {
            return candidate;
        }
    }
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

void LayoutPanel::addFromDialogAt(output::Protocol protocol, std::uint32_t universe, std::uint32_t channel) {
    AddFixtureDialog dialog{player_, nextOrigin(), this};
    dialog.prefillManualPatch(protocol, universe, channel);
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
    // "Typ wird Pflicht" (siehe CLAUDE.md): die Form entsteht hier als neue Bauform in der Bibliothek, das
    // Fixture ist von Anfang an nur noch ein Exemplar davon - nie mehr ein typloses Fixture mit "eigener
    // Form", die sich nur noch im Reiter Fixtures bearbeiten lässt.
    const QString typeName = uniqueTypeName(player_, baseNameOf(fixture.shape));
    const std::string typeNameStd = typeName.toUtf8().toStdString();
    const Result<void> addedType = player_.addFixtureType(typeNameStd, fixture.shape);
    if (!addedType.ok()) {
        Q_EMIT failed(fromStd(addedType.error().message));
        return;
    }
    const auto id = player_.addFixtureOfType(name.toUtf8().toStdString(), typeNameStd, fixture.origin, Vec3{}, first);
    if (!id.ok()) {
        player_.removeFixtureType(typeNameStd);  // keine Bauform ohne Exemplar liegen lassen
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
    // "Typ wird Pflicht" (siehe CLAUDE.md): auch ein Punktwolken-Import bekommt sofort eine eigene Bauform.
    if (added.type.empty()) {
        return QStringLiteral("Layout: importiertes Fixture hat keine Bauform bekommen");
    }
    const std::string typeName = added.type;
    removeSelected();
    if (player_.project()->layout.fixtures().size() != before) {
        return QStringLiteral("Layout: importiertes Fixture ließ sich nicht entfernen");
    }
    if (!player_.removeFixtureType(typeName).ok()) {
        return QStringLiteral("Layout: die beim Import angelegte Bauform ließ sich nicht wieder entfernen");
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
    // "Typ wird Pflicht" (siehe CLAUDE.md): auch aus dem Dialog "Fixture hinzufügen" entsteht sofort eine
    // eigene Bauform statt eines typlosen Fixtures.
    if (added.type.empty()) {
        return QStringLiteral("Hinzufügen-Dialog: das Gitter hat keine Bauform bekommen");
    }
    const std::string typeName = added.type;
    setSelectedIds({added.id});
    removeSelected();
    if (player_.project()->layout.fixtures().size() != before) {
        return QStringLiteral("Hinzufügen-Dialog: das Gitter lässt sich nicht wieder entfernen");
    }
    if (!player_.removeFixtureType(typeName).ok()) {
        return QStringLiteral("Hinzufügen-Dialog: die angelegte Bauform ließ sich nicht wieder entfernen");
    }
    return {};
}

}  // namespace whitehole::studio
