// patch_table.cpp: Umsetzung zu patch_table.hpp.
#include "patch_table.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>

#include <algorithm>

#include "form_helpers.hpp"

namespace whitehole::studio {

using form::fromStd;

namespace {

constexpr int kIdRole = Qt::UserRole;

QSettings settings() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

QTableWidgetItem* readOnly(const QString& text) {
    auto* item = new QTableWidgetItem{text};
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

}  // namespace

PatchTable::PatchTable(runtime::Player& player, QWidget* parent) : QTableWidget{0, Count, parent}, player_{player} {
    setHorizontalHeaderLabels({tr("Fixture"), tr("Bauform"), tr("LEDs"), tr("Senden"), tr("Protokoll"), tr("Ziel Nr."),
                               tr("Universe"), tr("Erster Kanal"), tr("DDP-Offset"), tr("Farbreihenfolge"), tr("Gamma"),
                               tr("Helligkeit")});
    horizontalHeader()->setSectionsMovable(true);  // Spalten per Ziehen an der Kopfzeile umordnen
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setToolTip(tr("Spalten lassen sich an der Kopfzeile verschieben"));
    verticalHeader()->hide();
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setMinimumHeight(160);
    const QByteArray saved = settings().value(QStringLiteral("stageSetup/patchHeader")).toByteArray();
    if (!saved.isEmpty()) {
        horizontalHeader()->restoreState(saved);
    }
    connect(horizontalHeader(), &QHeaderView::sectionMoved, this, [this](int, int, int) { saveHeader(); });
    connect(horizontalHeader(), &QHeaderView::sectionResized, this, [this](int, int, int) { saveHeader(); });
}

void PatchTable::saveHeader() const {
    if (!populating_) {
        settings().setValue(QStringLiteral("stageSetup/patchHeader"), horizontalHeader()->saveState());
    }
}

int PatchTable::rowOf(mapping::FixtureId id) const {
    for (int row = 0; row < rowCount(); ++row) {
        if (idAt(row) == id) {
            return row;
        }
    }
    return -1;
}

mapping::FixtureId PatchTable::idAt(int row) const {
    const QTableWidgetItem* item = this->item(row, Name);
    return item != nullptr ? mapping::FixtureId{item->data(kIdRole).toULongLong()} : mapping::FixtureId{};
}

void PatchTable::rebuild() {
    populating_ = true;
    clearContents();
    setRowCount(0);
    const project::Project* project = player_.project();
    if (project != nullptr) {
        for (const mapping::Fixture& fixture : project->layout.fixtures()) {
            const int row = rowCount();
            insertRow(row);
            const mapping::FixtureId id = fixture.id;
            auto* name = readOnly(fromStd(fixture.name));
            name->setData(kIdRole, static_cast<qulonglong>(id.value()));
            setItem(row, Name, name);
            setItem(row, Type, readOnly(fixture.type.empty() ? tr("(eigene Form)") : fromStd(fixture.type)));
            setItem(row, Leds, readOnly(QString::number(mapping::ledCount(fixture.shape))));

            const output::FixturePatch* patch = nullptr;
            for (const output::FixturePatch& candidate : project->patches) {
                if (candidate.fixture == id) {
                    patch = &candidate;
                }
            }
            const output::FixturePatch shown = patch != nullptr ? *patch : output::FixturePatch{};

            auto* send = new QCheckBox;
            send->setChecked(patch != nullptr);
            send->setToolTip(tr("Fixture an die Ausgabe schicken"));
            connect(send, &QCheckBox::toggled, this, [this, id](bool on) { toggle(id, on); });
            setCellWidget(row, Send, send);

            auto* protocol = form::combo({QStringLiteral("Art-Net"), QStringLiteral("sACN"), QStringLiteral("DDP")},
                                         static_cast<int>(shown.protocol));
            auto* destination = form::intBox(static_cast<int>(shown.destination), 0, 255);
            auto* universe = form::intBox(static_cast<int>(shown.universe), 0, 63999);
            auto* channel = form::intBox(static_cast<int>(shown.channel), 1, 510);
            auto* offset = form::intBox(static_cast<int>(shown.ddpOffset), 0, 2000000000);
            auto* order = form::combo({QStringLiteral("RGB"), QStringLiteral("RBG"), QStringLiteral("GRB"),
                                       QStringLiteral("GBR"), QStringLiteral("BRG"), QStringLiteral("BGR")},
                                      static_cast<int>(shown.color.order));
            auto* gamma = form::doubleBox(shown.color.gamma, 0.1, 10.0, 0.1, 2);
            auto* brightness = form::doubleBox(shown.color.brightness, 0.0, 1.0, 0.05, 2);
            setCellWidget(row, Protocol, protocol);
            setCellWidget(row, Destination, destination);
            setCellWidget(row, Universe, universe);
            setCellWidget(row, Channel, channel);
            setCellWidget(row, DdpOffset, offset);
            setCellWidget(row, Order, order);
            setCellWidget(row, Gamma, gamma);
            setCellWidget(row, Brightness, brightness);
            for (QComboBox* box : {protocol, order}) {
                connect(box, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, id](int) {
                    updateEnabled(rowOf(id));
                    commit(id);
                });
            }
            for (QSpinBox* box : {destination, universe, channel, offset}) {
                connect(box, qOverload<int>(&QSpinBox::valueChanged), this, [this, id](int) { commit(id); });
            }
            for (QDoubleSpinBox* box : {gamma, brightness}) {
                connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, id](double) { commit(id); });
            }
            updateEnabled(row);
        }
    }
    resizeColumnsToContents();
    populating_ = false;
}

void PatchTable::updateEnabled(int row) {
    if (row < 0) {
        return;
    }
    const auto* send = qobject_cast<QCheckBox*>(cellWidget(row, Send));
    const auto* protocol = qobject_cast<QComboBox*>(cellWidget(row, Protocol));
    if (send == nullptr || protocol == nullptr) {
        return;
    }
    const bool on = send->isChecked();
    const bool ddp = protocol->currentIndex() == static_cast<int>(output::Protocol::Ddp);
    for (const int column : {Protocol, Destination, Order, Gamma, Brightness}) {
        cellWidget(row, column)->setEnabled(on);
    }
    for (const int column : {Universe, Channel}) {
        cellWidget(row, column)->setEnabled(on && !ddp);
    }
    cellWidget(row, DdpOffset)->setEnabled(on && ddp);
}

void PatchTable::report(const Result<void>& result, bool rebuildAfter) {
    if (result.ok()) {
        Q_EMIT edited();
        if (rebuildAfter) {
            QTimer::singleShot(0, this, [this]() { rebuild(); });
        }
        return;
    }
    Q_EMIT failed(fromStd(result.error().message));
    QTimer::singleShot(0, this, [this]() { rebuild(); });  // Anzeige auf den gespeicherten Wert zurück
}

void PatchTable::toggle(mapping::FixtureId id, bool on) {
    if (populating_ || player_.project() == nullptr) {
        return;
    }
    if (!on) {
        report(player_.setPatch(id, std::nullopt), true);
        return;
    }
    const mapping::Fixture* fixture = player_.project()->layout.find(id);
    if (fixture != nullptr) {
        report(player_.setPatch(id, player_.suggestPatch(mapping::ledCount(fixture->shape))), true);
    }
}

void PatchTable::commit(mapping::FixtureId id) {
    const int row = rowOf(id);
    if (populating_ || row < 0 || !qobject_cast<QCheckBox*>(cellWidget(row, Send))->isChecked()) {
        return;
    }
    const auto spin = [&](int column) { return qobject_cast<QSpinBox*>(cellWidget(row, column))->value(); };
    const auto dbl = [&](int column) { return qobject_cast<QDoubleSpinBox*>(cellWidget(row, column))->value(); };
    output::FixturePatch patch;
    patch.protocol = static_cast<output::Protocol>(qobject_cast<QComboBox*>(cellWidget(row, Protocol))->currentIndex());
    patch.destination = static_cast<std::uint32_t>(spin(Destination));
    patch.universe = static_cast<std::uint16_t>(spin(Universe));
    patch.channel = static_cast<std::uint16_t>(spin(Channel));
    patch.ddpOffset = static_cast<std::uint32_t>(spin(DdpOffset));
    patch.color.order = static_cast<output::ColorOrder>(qobject_cast<QComboBox*>(cellWidget(row, Order))->currentIndex());
    patch.color.gamma = static_cast<float>(dbl(Gamma));
    patch.color.brightness = static_cast<float>(dbl(Brightness));
    report(player_.setPatch(id, patch), false);
}

QString PatchTable::exercise() {
    rebuild();
    if (player_.project() == nullptr || rowCount() == 0) {
        return QStringLiteral("Patch-Tabelle: keine Zeilen");
    }
    // Eine Zeile mit Patch suchen und die Farbreihenfolge wie ein Mensch ändern
    int row = -1;
    for (int r = 0; r < rowCount(); ++r) {
        if (qobject_cast<QCheckBox*>(cellWidget(r, Send))->isChecked()) {
            row = r;
        }
    }
    if (row < 0) {
        return QStringLiteral("Patch-Tabelle: kein Fixture mit Patch");
    }
    const mapping::FixtureId id = idAt(row);
    qobject_cast<QComboBox*>(cellWidget(row, Order))->setCurrentIndex(static_cast<int>(output::ColorOrder::GRB));
    const auto patchOf = [&]() -> const output::FixturePatch* {
        for (const output::FixturePatch& patch : player_.project()->patches) {
            if (patch.fixture == id) {
                return &patch;
            }
        }
        return nullptr;
    };
    if (patchOf() == nullptr || patchOf()->color.order != output::ColorOrder::GRB) {
        return QStringLiteral("Patch-Tabelle: Farbreihenfolge wurde nicht übernommen");
    }
    // Spalte verschieben: die logischen Spalten bleiben, die Zellen finden ihre Widgets weiter
    const int originalVisual = horizontalHeader()->visualIndex(Order);
    horizontalHeader()->moveSection(originalVisual, 0);
    if (horizontalHeader()->logicalIndex(0) != Order) {
        return QStringLiteral("Patch-Tabelle: Spalte lässt sich nicht verschieben");
    }
    horizontalHeader()->moveSection(0, originalVisual);  // zurück an die Ausgangsstelle
    qobject_cast<QComboBox*>(cellWidget(row, Order))->setCurrentIndex(static_cast<int>(output::ColorOrder::RGB));
    if (patchOf() == nullptr || patchOf()->color.order != output::ColorOrder::RGB) {
        return QStringLiteral("Patch-Tabelle: Farbreihenfolge nach dem Verschieben nicht übernommen");
    }
    return {};
}

}  // namespace whitehole::studio
