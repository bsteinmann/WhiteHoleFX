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

#include <filesystem>
#include <fstream>
#include <optional>

#include "whitehole/runtime/point_file.hpp"

namespace whitehole::studio {

using form::fromStd;

void LayoutPanel::addFixture(const QString& baseName, mapping::FixtureShape shape) {
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
    const std::uint64_t leds = mapping::ledCount(shape);
    const auto id = player_.addFixture(uniqueName(baseName).toUtf8().toStdString(), nextOrigin(), std::move(shape),
                                       player_.suggestPatch(leds));
    if (!id.ok()) {
        Q_EMIT failed(fromStd(id.error().message));
        return;
    }
    if (template_) {
        // Das neue Fixture rückt an die Stelle der Vorlage: Ursprung 0 und erstes freies Universe.
        Result<void> r = player_.removeFixture(*template_);
        if (r.ok()) {
            r = player_.moveFixture(id.value(), Vec3{});
        }
        if (r.ok()) {
            r = player_.setPatch(id.value(), player_.suggestPatch(leds));
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

}  // namespace whitehole::studio
