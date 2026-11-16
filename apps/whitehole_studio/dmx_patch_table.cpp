// dmx_patch_table.cpp: Umsetzung zu dmx_patch_table.hpp.
#include "dmx_patch_table.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "form_helpers.hpp"
#include "whitehole/output/artnet.hpp"
#include "whitehole/output/patch_protocol.hpp"

namespace whitehole::studio {

using form::fromStd;

namespace {

QString protocolName(output::Protocol protocol) {
    switch (protocol) {
        case output::Protocol::ArtNet: return QStringLiteral("Art-Net");
        case output::Protocol::Sacn: return QStringLiteral("sACN");
        case output::Protocol::Ddp: return QStringLiteral("DDP");
    }
    return {};
}

QTableWidgetItem* readOnly(const QString& text) {
    auto* item = new QTableWidgetItem{text};
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

}  // namespace

DmxPatchTable::DmxPatchTable(runtime::Player& player, QWidget* parent) : QWidget{parent}, player_{player} {
    auto* outer = new QVBoxLayout{this};
    table_ = new QTableWidget{0, Count};
    table_->setHorizontalHeaderLabels({tr("Pool-Kanal (von)"), tr("Anzahl"), tr("Controller"), tr("Protokoll"),
                                       tr("Universe"), tr("Erster Kanal")});
    table_->horizontalHeaderItem(PoolFrom)->setToolTip(
        tr("Erster Kanal im internen Kanal-Pool der DMX-Output-Nodes (1..512, siehe deren Parameter \"ch\")."));
    table_->horizontalHeaderItem(PoolCount)->setToolTip(tr("Wie viele Pool-Kanäle ab dieser Stelle hierher gehen."));
    table_->horizontalHeaderItem(Destination)->setToolTip(
        tr("Der Controller bestimmt das Protokoll. Pool-Kanäle ohne Patch werden nicht gesendet."));
    table_->horizontalHeaderItem(Universe)->setToolTip(tr("Art-Net zählt Universen ab 0 (bis 32767), sACN ab 1 (bis 63999)."));
    table_->horizontalHeaderItem(Channel)->setToolTip(tr("Erster echter DMX-Kanal am Ziel (1..512)."));
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setMinimumHeight(120);
    outer->addWidget(table_);

    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton{tr("Patch +")};
    auto* remove = new QPushButton{tr("Gewählten Patch −")};
    remove->setToolTip(tr("Entfernt den gewählten Patch (ohne Auswahl den letzten)."));
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    outer->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, [this]() { addPatch(); });
    connect(remove, &QPushButton::clicked, this, [this]() { removeSelectedPatch(); });
}

void DmxPatchTable::rebuild() {
    const project::Project* project = player_.project();
    populating_ = true;
    const QSignalBlocker blockTable{table_};
    table_->setRowCount(0);
    if (project != nullptr) {
        QStringList controllers;
        for (std::size_t i = 0; i < project->destinations.size(); ++i) {
            const output::Destination& d = project->destinations[i];
            const QString label = d.name.empty() ? (d.address.empty() ? tr("Multicast") : fromStd(d.address)) : fromStd(d.name);
            controllers << QStringLiteral("%1 – %2 (%3)").arg(i).arg(label, protocolName(d.protocol));
        }
        for (const output::DmxChannelPatch& p : project->dmxPatches) {
            const int row = table_->rowCount();
            table_->insertRow(row);
            table_->setCellWidget(row, PoolFrom, form::intBox(static_cast<int>(p.poolChannel), 1, 512));
            table_->setCellWidget(row, PoolCount, form::intBox(static_cast<int>(p.count), 1, 512));
            const int destinationIndex =
                std::min<int>(static_cast<int>(p.destination), std::max<int>(0, static_cast<int>(controllers.size()) - 1));
            auto* destination = form::combo(controllers, destinationIndex);
            destination->setEnabled(!controllers.isEmpty());
            table_->setCellWidget(row, Destination, destination);
            const output::Protocol shownProtocol =
                p.destination < project->destinations.size() ? project->destinations[p.destination].protocol : p.protocol;
            table_->setItem(row, Protocol, readOnly(protocolName(shownProtocol)));
            table_->setCellWidget(row, Universe, form::intBox(static_cast<int>(p.universe), output::firstUniverse(shownProtocol),
                                                              std::max<int>(output::lastUniverse(shownProtocol),
                                                                           output::firstUniverse(shownProtocol))));
            table_->setCellWidget(row, Channel, form::intBox(static_cast<int>(p.channel), 1, 512));
            for (const int column : {PoolFrom, PoolCount, Universe, Channel}) {
                connect(qobject_cast<QSpinBox*>(table_->cellWidget(row, column)), qOverload<int>(&QSpinBox::valueChanged),
                       this, [this](int) {
                           if (!populating_) {
                               applyPatches();
                           }
                       });
            }
            connect(destination, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
                if (!populating_) {
                    applyPatches();
                }
            });
        }
    }
    populating_ = false;
}

void DmxPatchTable::selectPoolChannel(std::uint16_t poolChannel) {
    for (int row = 0; row < table_->rowCount(); ++row) {
        if (qobject_cast<QSpinBox*>(table_->cellWidget(row, PoolFrom))->value() == static_cast<int>(poolChannel)) {
            table_->setCurrentCell(row, PoolFrom);
            table_->scrollTo(table_->model()->index(row, PoolFrom));
            return;
        }
    }
}

void DmxPatchTable::report(const Result<void>& result) {
    if (result.ok()) {
        Q_EMIT edited();
        return;
    }
    Q_EMIT failed(fromStd(result.error().message));
    QTimer::singleShot(0, this, [this]() { rebuild(); });
}

void DmxPatchTable::applyPatches() {
    const project::Project* project = player_.project();
    if (project == nullptr) {
        return;
    }
    std::vector<output::DmxChannelPatch> patches;
    for (int row = 0; row < table_->rowCount(); ++row) {
        output::DmxChannelPatch p;
        p.poolChannel = static_cast<std::uint16_t>(qobject_cast<QSpinBox*>(table_->cellWidget(row, PoolFrom))->value());
        p.count = static_cast<std::uint16_t>(qobject_cast<QSpinBox*>(table_->cellWidget(row, PoolCount))->value());
        const auto* destination = qobject_cast<QComboBox*>(table_->cellWidget(row, Destination));
        p.destination = static_cast<std::uint32_t>(std::max(0, destination != nullptr ? destination->currentIndex() : 0));
        // Das Protokoll gehört zum gewählten Controller (wie beim Fixture-Patch), nicht zur Zeile selbst.
        p.protocol = p.destination < project->destinations.size() ? project->destinations[p.destination].protocol
                                                                   : output::Protocol::ArtNet;
        p.universe = static_cast<std::uint16_t>(qobject_cast<QSpinBox*>(table_->cellWidget(row, Universe))->value());
        p.channel = static_cast<std::uint16_t>(qobject_cast<QSpinBox*>(table_->cellWidget(row, Channel))->value());
        patches.push_back(p);
    }
    report(player_.setDmxPatches(std::move(patches)));
}

void DmxPatchTable::addPatch() {
    if (player_.project() == nullptr) {
        return;
    }
    std::vector<output::DmxChannelPatch> patches = player_.project()->dmxPatches;
    output::DmxChannelPatch next;
    // Nächster freier Pool-Kanal: der Standard (poolChannel 1, siehe DmxChannelPatch) ist nur beim
    // allerersten Patch frei. Ohne diese Suche überschneidet sich jeder weitere Klick auf "Patch +" mit
    // dem schon bestehenden ersten Patch, setDmxPatches scheitert leise an der Überschneidung (siehe
    // OutputPipeline::create) und der Button scheint ab dem zweiten Klick nichts mehr zu tun.
    std::uint32_t freeFrom = 1;
    for (const output::DmxChannelPatch& p : patches) {
        freeFrom = std::max<std::uint32_t>(freeFrom, static_cast<std::uint32_t>(p.poolChannel) + p.count);
    }
    if (freeFrom <= output::kDmxMaxChannels) {
        next.poolChannel = static_cast<std::uint16_t>(freeFrom);
    }
    // sonst: Pool ist voll - next behält den Standard, setDmxPatches meldet dann regulär "Pool voll".
    patches.push_back(next);
    const Result<void> r = player_.setDmxPatches(std::move(patches));
    if (r.ok()) {
        rebuild();
    }
    report(r);
}

void DmxPatchTable::removeSelectedPatch() {
    if (player_.project() == nullptr || player_.project()->dmxPatches.empty()) {
        return;
    }
    std::vector<output::DmxChannelPatch> patches = player_.project()->dmxPatches;
    const int row = table_->currentRow() >= 0 ? table_->currentRow() : static_cast<int>(patches.size()) - 1;
    patches.erase(patches.begin() + row);
    const Result<void> r = player_.setDmxPatches(std::move(patches));
    if (r.ok()) {
        rebuild();
    }
    report(r);
}

QString DmxPatchTable::exercise() {
    if (player_.project() == nullptr) {
        return QStringLiteral("DMX-Kanal-Pool: kein Projekt");
    }
    const std::size_t before = player_.project()->dmxPatches.size();
    addPatch();
    if (player_.project()->dmxPatches.size() != before + 1 || table_->rowCount() != static_cast<int>(before + 1)) {
        return QStringLiteral("DMX-Kanal-Pool: Patch wurde nicht angelegt");
    }
    // Zweimal hintereinander "Patch +" ohne etwas zu bearbeiten (vom Nutzer gemeldet: der Knopf tat ab dem
    // zweiten Klick nichts mehr, weil beide Einträge sonst denselben Standard-Pool-Kanal 1 bekamen und sich
    // überschnitten - siehe CHANGELOG).
    addPatch();
    if (player_.project()->dmxPatches.size() != before + 2 || table_->rowCount() != static_cast<int>(before + 2)) {
        return QStringLiteral("DMX-Kanal-Pool: zweiter unbearbeiteter Patch wurde nicht angelegt (Standardwerte überschneiden sich?)");
    }
    removeSelectedPatch();  // letzte Zeile (keine Auswahl) wieder weg, ab hier wie zuvor mit genau einem Patch weitermachen
    if (player_.project()->dmxPatches.size() != before + 1) {
        return QStringLiteral("DMX-Kanal-Pool: der zweite Patch ließ sich nicht wieder entfernen");
    }
    const int row = static_cast<int>(before);
    qobject_cast<QSpinBox*>(table_->cellWidget(row, PoolFrom))->setValue(10);
    if (player_.project()->dmxPatches.back().poolChannel != 10) {
        return QStringLiteral("DMX-Kanal-Pool: Pool-Kanal wurde nicht übernommen");
    }
    qobject_cast<QSpinBox*>(table_->cellWidget(row, PoolCount))->setValue(4);
    if (player_.project()->dmxPatches.back().count != 4) {
        return QStringLiteral("DMX-Kanal-Pool: Anzahl wurde nicht übernommen");
    }
    addPatch();
    table_->setCurrentCell(row + 1, PoolFrom);
    removeSelectedPatch();
    if (player_.project()->dmxPatches.size() != before + 1) {
        return QStringLiteral("DMX-Kanal-Pool: der gewählte Patch wurde nicht entfernt");
    }
    table_->clearSelection();
    table_->setCurrentCell(-1, -1);
    removeSelectedPatch();
    if (player_.project()->dmxPatches.size() != before) {
        return QStringLiteral("DMX-Kanal-Pool: Patch wurde nicht entfernt");
    }
    return {};
}

}  // namespace whitehole::studio
