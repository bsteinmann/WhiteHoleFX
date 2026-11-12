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
#include <optional>

#include "form_helpers.hpp"
#include "whitehole/output/patch_protocol.hpp"
#include "whitehole/output/patch_space.hpp"

namespace whitehole::studio {

using form::fromStd;

namespace {

constexpr int kIdRole = Qt::UserRole;
constexpr int kProtocolRole = Qt::UserRole + 1;

QString protocolName(output::Protocol protocol) {
    switch (protocol) {
        case output::Protocol::ArtNet: return QStringLiteral("Art-Net");
        case output::Protocol::Sacn: return QStringLiteral("sACN");
        case output::Protocol::Ddp: return QStringLiteral("DDP");
    }
    return {};
}

QSettings settings() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

QTableWidgetItem* readOnly(const QString& text) {
    auto* item = new QTableWidgetItem{text};
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

}  // namespace

PatchTable::PatchTable(runtime::Player& player, QWidget* parent) : QTableWidget{0, Count, parent}, player_{player} {
    setHorizontalHeaderLabels({tr("Fixture"), tr("Bauform"), tr("LEDs"), tr("Senden"), tr("Controller"), tr("Protokoll"),
                               tr("Universe (Art-Net ab 0, sACN ab 1)"), tr("Erster Kanal"), tr("Adressraum"), tr("DDP-Offset"),
                               tr("Vorweg überspringen"), tr("Überspringen nach je"), tr("Übersprungene Pixel"),
                               tr("Farbreihenfolge"), tr("Gamma"), tr("Helligkeit")});
    horizontalHeaderItem(SkipFirst)->setToolTip(tr("Pixel am Kabel vor der ersten LED, die nicht leuchten (zum Beispiel ein Pixel als "
                                                   "Signalverstärker). Sie bekommen Schwarz und belegen ihre Kanäle."));
    horizontalHeaderItem(SkipEvery)->setToolTip(tr("Nach je so vielen LEDs werden Pixel übersprungen (0 = nie), etwa ein Verstärker "
                                                   "am Ende jeder Reihe: hier die Spaltenzahl eintragen."));
    horizontalHeaderItem(SkipCount)->setToolTip(tr("Wie viele Pixel nach je N LEDs übersprungen werden."));
    horizontalHeaderItem(Destination)->setToolTip(tr("Der Controller bestimmt das Protokoll. Wechselst du auf einen "
                                                     "Controller mit anderem Protokoll, wird die Universe-Nummer "
                                                     "zwischen Art-Net und sACN um 1 verschoben."));
    horizontalHeaderItem(Universe)->setToolTip(tr("Art-Net zählt Universen ab 0 (bis 32767), sACN ab 1 (bis 63999). "
                                                  "Bei DDP gibt es keine Universen."));
    horizontalHeaderItem(Range)->setToolTip(tr("Welche Kanäle das Fixture belegt: Universe und erster bis letzter Kanal "
                                               "(eine LED belegt drei Kanäle). Läuft es über, steht das letzte Universe dahinter."));
    horizontalHeader()->setSectionsMovable(true);  // Spalten per Ziehen an der Kopfzeile umordnen
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setToolTip(tr("Spalten lassen sich an der Kopfzeile verschieben"));
    verticalHeader()->hide();
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setMinimumHeight(160);
    const QByteArray saved = settings().value(QStringLiteral("stageSetup/patchHeader4")).toByteArray();
    if (!saved.isEmpty()) {
        horizontalHeader()->restoreState(saved);
    }
    connect(horizontalHeader(), &QHeaderView::sectionMoved, this, [this](int, int, int) { saveHeader(); });
    connect(horizontalHeader(), &QHeaderView::sectionResized, this, [this](int, int, int) { saveHeader(); });
}

void PatchTable::saveHeader() const {
    if (!populating_) {
        settings().setValue(QStringLiteral("stageSetup/patchHeader4"), horizontalHeader()->saveState());
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

            // Controller zur Auswahl: "0 – Wand (Art-Net)"; das Protokoll folgt dem Controller.
            QStringList controllers;
            for (std::size_t i = 0; i < project->destinations.size(); ++i) {
                const output::Destination& d = project->destinations[i];
                const QString label = d.name.empty() ? (d.address.empty() ? tr("Multicast") : fromStd(d.address)) : fromStd(d.name);
                controllers << QStringLiteral("%1 – %2 (%3)").arg(i).arg(label, protocolName(d.protocol));
            }
            auto* destination = form::combo(controllers, std::min<int>(static_cast<int>(shown.destination),
                                                                       std::max<int>(0, static_cast<int>(controllers.size()) - 1)));
            destination->setEnabled(!controllers.isEmpty());
            const output::Protocol shownProtocol = shown.destination < project->destinations.size()
                                                       ? project->destinations[shown.destination].protocol
                                                       : shown.protocol;
            auto* protocolItem = readOnly(protocolName(shownProtocol));
            protocolItem->setData(kProtocolRole, static_cast<int>(shownProtocol));
            setItem(row, Protocol, protocolItem);
            auto* universe = form::intBox(static_cast<int>(shown.universe), output::firstUniverse(shownProtocol),
                                          std::max<int>(output::lastUniverse(shownProtocol), output::firstUniverse(shownProtocol)));
            auto* channel = form::intBox(static_cast<int>(shown.channel), 1, 510);
            auto* offset = form::intBox(static_cast<int>(shown.ddpOffset), 0, 2000000000);
            auto* skipFirst = form::intBox(static_cast<int>(shown.skipFirst), 0, 65535);
            auto* skipEvery = form::intBox(static_cast<int>(shown.skipEvery), 0, 1000000);
            auto* skipCount = form::intBox(static_cast<int>(shown.skipCount), 1, 65535);
            auto* order = form::combo({QStringLiteral("RGB"), QStringLiteral("RBG"), QStringLiteral("GRB"),
                                       QStringLiteral("GBR"), QStringLiteral("BRG"), QStringLiteral("BGR")},
                                      static_cast<int>(shown.color.order));
            auto* gamma = form::doubleBox(shown.color.gamma, 0.1, 10.0, 0.1, 2);
            auto* brightness = form::doubleBox(shown.color.brightness, 0.0, 1.0, 0.05, 2);
            setCellWidget(row, Destination, destination);
            setCellWidget(row, Universe, universe);
            setCellWidget(row, Channel, channel);
            setItem(row, Range, readOnly(QString{}));
            setCellWidget(row, DdpOffset, offset);
            setCellWidget(row, SkipFirst, skipFirst);
            setCellWidget(row, SkipEvery, skipEvery);
            setCellWidget(row, SkipCount, skipCount);
            setCellWidget(row, Order, order);
            setCellWidget(row, Gamma, gamma);
            setCellWidget(row, Brightness, brightness);
            // Anderer Controller: das Protokoll (und damit die Universe-Nummer) kann sich ändern, die Zeile wird neu gelesen.
            connect(destination, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, id](int) { commit(id, true); });
            connect(order, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, id](int) { commit(id); });
            for (QSpinBox* box : {universe, channel, offset, skipFirst, skipEvery, skipCount}) {
                connect(box, qOverload<int>(&QSpinBox::valueChanged), this, [this, id](int) { commit(id); });
            }
            for (QDoubleSpinBox* box : {gamma, brightness}) {
                connect(box, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, id](double) { commit(id); });
            }
            updateEnabled(row);
            updateRange(row);
        }
    }
    resizeColumnsToContents();
    populating_ = false;
}

output::Protocol PatchTable::protocolAt(int row) const {
    const QTableWidgetItem* item = this->item(row, Protocol);
    return item != nullptr ? static_cast<output::Protocol>(item->data(kProtocolRole).toInt()) : output::Protocol::ArtNet;
}

void PatchTable::updateRange(int row) {
    QTableWidgetItem* cell = row >= 0 ? item(row, Range) : nullptr;
    const project::Project* project = player_.project();
    if (cell == nullptr || project == nullptr) {
        return;
    }
    const mapping::FixtureId id = idAt(row);
    const mapping::Fixture* fixture = project->layout.find(id);
    const auto patch = std::find_if(project->patches.begin(), project->patches.end(),
                                    [&](const output::FixturePatch& candidate) { return candidate.fixture == id; });
    const auto span = fixture != nullptr && patch != project->patches.end()
                          ? output::addressSpan(*patch, static_cast<std::uint32_t>(mapping::ledCount(fixture->shape)))
                          : std::nullopt;
    if (!span) {
        cell->setText(QStringLiteral("–"));
    } else if (span->protocol == output::Protocol::Ddp) {
        cell->setText(tr("Byte %1–%2").arg(span->firstChannel).arg(span->lastChannel));
    } else if (span->firstUniverse == span->lastUniverse) {
        cell->setText(tr("U%1: %2–%3").arg(span->firstUniverse).arg(span->firstChannel).arg(span->lastChannel));
    } else {
        cell->setText(tr("U%1: %2 – U%3: %4")
                          .arg(span->firstUniverse).arg(span->firstChannel).arg(span->lastUniverse).arg(span->lastChannel));
    }
}

void PatchTable::updateEnabled(int row) {
    if (row < 0) {
        return;
    }
    const auto* send = qobject_cast<QCheckBox*>(cellWidget(row, Send));
    if (send == nullptr) {
        return;
    }
    const bool on = send->isChecked();
    const bool ddp = protocolAt(row) == output::Protocol::Ddp;
    for (const int column : {Order, Gamma, Brightness}) {
        cellWidget(row, column)->setEnabled(on);
    }
    cellWidget(row, Destination)->setEnabled(on && qobject_cast<QComboBox*>(cellWidget(row, Destination))->count() > 0);
    // Universe und Kanal gibt es bei Art-Net und sACN, den Offset nur bei DDP.
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

void PatchTable::commit(mapping::FixtureId id, bool rebuildAfter) {
    const int row = rowOf(id);
    if (populating_ || row < 0 || !qobject_cast<QCheckBox*>(cellWidget(row, Send))->isChecked()) {
        return;
    }
    const auto spin = [&](int column) { return qobject_cast<QSpinBox*>(cellWidget(row, column))->value(); };
    const auto dbl = [&](int column) { return qobject_cast<QDoubleSpinBox*>(cellWidget(row, column))->value(); };
    output::FixturePatch patch;
    // Das Protokoll der Zeile ist noch das alte; der Player stellt den Patch auf das des gewählten Controllers um.
    patch.protocol = protocolAt(row);
    patch.destination = static_cast<std::uint32_t>(std::max(0, qobject_cast<QComboBox*>(cellWidget(row, Destination))->currentIndex()));
    patch.universe = static_cast<std::uint16_t>(spin(Universe));
    patch.channel = static_cast<std::uint16_t>(spin(Channel));
    patch.ddpOffset = static_cast<std::uint32_t>(spin(DdpOffset));
    patch.skipFirst = static_cast<std::uint16_t>(spin(SkipFirst));
    patch.skipEvery = static_cast<std::uint32_t>(spin(SkipEvery));
    patch.skipCount = static_cast<std::uint16_t>(spin(SkipCount));
    patch.color.order = static_cast<output::ColorOrder>(qobject_cast<QComboBox*>(cellWidget(row, Order))->currentIndex());
    patch.color.gamma = static_cast<float>(dbl(Gamma));
    patch.color.brightness = static_cast<float>(dbl(Brightness));
    // Wechselt der Fixture-Patch Universe oder Controller und der Kanal ist dort schon belegt, kommt er hinter das andere
    // Fixture (Matrix und dahinter ein einzelner Pixel im selben Universe), statt dass die Änderung abgelehnt wird.
    const project::Project* project = player_.project();
    const output::FixturePatch* stored = nullptr;
    if (project != nullptr) {
        for (const output::FixturePatch& candidate : project->patches) {
            if (candidate.fixture == id) {
                stored = &candidate;
            }
        }
    }
    if (stored != nullptr && patch.protocol != output::Protocol::Ddp &&
        (stored->universe != patch.universe || stored->destination != patch.destination)) {
        const output::FixturePatch fitted = player_.fitPatch(id, patch);
        if (fitted.channel != patch.channel) {
            patch.channel = fitted.channel;
            populating_ = true;
            qobject_cast<QSpinBox*>(cellWidget(row, Channel))->setValue(patch.channel);
            populating_ = false;
        }
    }
    report(player_.setPatch(id, patch), rebuildAfter);
    updateRange(rowOf(id));
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
    // Der Adressraum eines gepatchten Fixtures steht in der Zelle ("U1: 1–297"), nie nur der Strich
    if (item(row, Range) == nullptr || item(row, Range)->text().isEmpty() || item(row, Range)->text() == QStringLiteral("–")) {
        return QStringLiteral("Patch-Tabelle: der Adressraum fehlt");
    }
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
    // Übersprungene Pixel: ein Verstärkerpixel vorweg verschiebt den Adressraum um drei Kanäle
    const QString rangeBefore = item(row, Range)->text();
    qobject_cast<QSpinBox*>(cellWidget(row, SkipFirst))->setValue(1);
    if (patchOf() == nullptr || patchOf()->skipFirst != 1 || item(row, Range)->text() == rangeBefore) {
        return QStringLiteral("Patch-Tabelle: Überspringen wurde nicht übernommen oder ändert den Adressraum nicht");
    }
    qobject_cast<QSpinBox*>(cellWidget(row, SkipFirst))->setValue(0);
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

    // Auf einen Controller mit anderem Protokoll umpatchen: der Patch folgt ihm (Art-Net -> sACN: Universe + 1), die
    // Zelle "Universe" bleibt aktiv, der DDP-Offset grau. Danach alles zurück.
    const std::vector<output::Destination> destinationsBefore = player_.project()->destinations;
    const output::Protocol protocolBefore = patchOf()->protocol;
    const std::uint32_t destinationBefore = patchOf()->destination;
    const std::uint16_t universeBefore = patchOf()->universe;
    std::vector<output::Destination> withExtra = destinationsBefore;
    const output::Protocol other = protocolBefore == output::Protocol::Sacn ? output::Protocol::ArtNet : output::Protocol::Sacn;
    withExtra.emplace_back("10.9.9.9", 0, other);
    if (!player_.setDestinations(withExtra).ok()) {
        return QStringLiteral("Patch-Tabelle: zweiter Controller lässt sich nicht anlegen");
    }
    rebuild();
    row = rowOf(id);
    qobject_cast<QComboBox*>(cellWidget(row, Destination))->setCurrentIndex(static_cast<int>(destinationsBefore.size()));
    if (patchOf() == nullptr || patchOf()->protocol != other || patchOf()->destination != destinationsBefore.size()) {
        return QStringLiteral("Patch-Tabelle: der Patch folgt dem Protokoll des neuen Controllers nicht");
    }
    const int expectedShift = (protocolBefore == output::Protocol::ArtNet && other == output::Protocol::Sacn)   ? 1
                              : (protocolBefore == output::Protocol::Sacn && other == output::Protocol::ArtNet) ? -1
                                                                                                                : 0;
    if (static_cast<int>(patchOf()->universe) != static_cast<int>(universeBefore) + expectedShift &&
        protocolBefore != output::Protocol::Ddp) {
        return QStringLiteral("Patch-Tabelle: Universe wurde beim Protokollwechsel nicht verschoben");
    }
    rebuild();
    row = rowOf(id);
    qobject_cast<QComboBox*>(cellWidget(row, Destination))->setCurrentIndex(static_cast<int>(destinationBefore));
    if (patchOf() == nullptr || patchOf()->protocol != protocolBefore ||
        (protocolBefore != output::Protocol::Ddp && patchOf()->universe != universeBefore)) {
        return QStringLiteral("Patch-Tabelle: Rückweg des Protokollwechsels stellt Universe nicht wieder her");
    }
    if (!player_.setDestinations(destinationsBefore).ok()) {
        return QStringLiteral("Patch-Tabelle: zweiter Controller lässt sich nicht entfernen");
    }
    rebuild();
    return {};
}

}  // namespace whitehole::studio
