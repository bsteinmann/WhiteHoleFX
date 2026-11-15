// dmx_output_monitor_panel.cpp: Umsetzung zu dmx_output_monitor_panel.hpp.
#include "dmx_output_monitor_panel.hpp"

#include <QAction>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include <vector>

namespace whitehole::studio {

namespace {

constexpr int kColumns = 16;
constexpr int kRows = static_cast<int>(output::kSentDmxChannels) / kColumns;  // 512 / 16 = 32
constexpr double kStaleAfterSeconds = 5.0;  // wie kMasterDmxTimeoutSeconds/kLiveDmxTimeoutSeconds

}  // namespace

DmxOutputMonitorPanel::DmxOutputMonitorPanel(std::shared_ptr<output::SentDmxStore> store, runtime::Player& player,
                                              QWidget* parent)
    : QWidget{parent}, store_{std::move(store)}, player_{player} {
    auto* layout = new QVBoxLayout{this};

    auto* form = new QFormLayout{};
    protocol_ = new QComboBox{this};
    protocol_->addItem(tr("Art-Net"));
    protocol_->addItem(tr("sACN"));
    form->addRow(tr("Protokoll:"), protocol_);
    universe_ = new QSpinBox{this};
    universe_->setRange(0, 32767);
    form->addRow(tr("Universe:"), universe_);
    layout->addLayout(form);

    status_ = new QLabel{this};
    layout->addWidget(status_);

    grid_ = new QTableWidget{kRows, kColumns, this};
    QStringList columnHeaders;
    for (int c = 0; c < kColumns; ++c) {
        columnHeaders << QString::number(c + 1);
    }
    grid_->setHorizontalHeaderLabels(columnHeaders);
    QStringList rowHeaders;
    for (int r = 0; r < kRows; ++r) {
        rowHeaders << QString::number(r * kColumns + 1);
    }
    grid_->setVerticalHeaderLabels(rowHeaders);
    grid_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    grid_->setEditTriggers(QTableWidget::NoEditTriggers);
    grid_->setSelectionMode(QTableWidget::NoSelection);
    grid_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(grid_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) { showContextMenu(pos); });
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kColumns; ++c) {
            auto* item = new QTableWidgetItem{QStringLiteral("–")};
            item->setTextAlignment(Qt::AlignCenter);
            const int channel = r * kColumns + c + 1;
            item->setToolTip(occupantTooltip(static_cast<std::uint32_t>(channel), std::nullopt));
            grid_->setItem(r, c, item);
        }
    }
    layout->addWidget(grid_);

    connect(protocol_, &QComboBox::currentIndexChanged, this, [this](int) { onSelectionChanged(); });
    connect(universe_, &QSpinBox::valueChanged, this, [this](int) { onSelectionChanged(); });
    onSelectionChanged();
}

void DmxOutputMonitorPanel::onSelectionChanged() {
    // Anderes Protokoll/Universe: alte Werte sind sofort ungültig. Das Gitter selbst leeren statt nur
    // hasData_ zurückzusetzen - sonst überspringt refresh() das Leeren, weil sein "kein Signal"-Zweig
    // genau an hasData_ hängt (der war ja schon false) und die alten Zellen stehen bleiben würden.
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kColumns; ++c) {
            grid_->item(r, c)->setText(QStringLiteral("–"));
        }
    }
    hasData_ = false;
    updateOccupantTooltips();
    refresh();
}

QString DmxOutputMonitorPanel::occupantTooltip(std::uint32_t channel, const std::optional<runtime::DmxOutputOccupant>& occupant) {
    if (!occupant) {
        return tr("Kanal %1").arg(channel);
    }
    return occupant->kind == runtime::DmxOutputOccupant::Kind::Fixture
               ? tr("Kanal %1 — belegt von Fixture: %2").arg(channel).arg(QString::fromStdString(occupant->name))
               : tr("Kanal %1 — belegt vom DMX-Kanal-Pool (Pool-Kanal %2)").arg(channel).arg(occupant->poolChannel);
}

void DmxOutputMonitorPanel::updateOccupantTooltips() {
    const auto protocol = protocol_->currentIndex() == 1 ? output::Protocol::Sacn : output::Protocol::ArtNet;
    const auto universe = static_cast<std::uint32_t>(universe_->value());
    for (int channel = 1; channel <= static_cast<int>(output::kSentDmxChannels); ++channel) {
        const auto occupant = player_.dmxOutputOccupantAt(protocol, universe, static_cast<std::uint32_t>(channel));
        const int r = (channel - 1) / kColumns;
        const int c = (channel - 1) % kColumns;
        grid_->item(r, c)->setToolTip(occupantTooltip(static_cast<std::uint32_t>(channel), occupant));
    }
}

QMenu* DmxOutputMonitorPanel::buildContextMenu(std::uint32_t channel, output::Protocol protocol, std::uint32_t universe) {
    auto* menu = new QMenu{this};
    const auto occupant = player_.dmxOutputOccupantAt(protocol, universe, channel);
    if (occupant) {
        if (occupant->kind == runtime::DmxOutputOccupant::Kind::Fixture) {
            QAction* edit = menu->addAction(tr("Fixture-Patch bearbeiten: %1…").arg(QString::fromStdString(occupant->name)));
            const mapping::FixtureId id = occupant->fixtureId;
            connect(edit, &QAction::triggered, this, [this, id]() { Q_EMIT requestEditFixturePatch(id); });
        } else {
            QAction* edit = menu->addAction(tr("DMX-Kanal-Pool-Patch bearbeiten (Pool-Kanal %1)…").arg(occupant->poolChannel));
            const std::uint16_t poolChannel = occupant->poolChannel;
            connect(edit, &QAction::triggered, this, [this, poolChannel]() { Q_EMIT requestEditPoolPatch(poolChannel); });
        }
    } else {
        QAction* add = menu->addAction(tr("Neues Fixture hier patchen…"));
        connect(add, &QAction::triggered, this,
                [this, protocol, universe, channel]() { Q_EMIT requestManualPatch(protocol, universe, channel); });
    }
    return menu;
}

void DmxOutputMonitorPanel::showContextMenu(const QPoint& pos) {
    const QTableWidgetItem* item = grid_->itemAt(pos);
    if (item == nullptr) {
        return;
    }
    const auto channel = static_cast<std::uint32_t>(item->row() * kColumns + item->column() + 1);  // 1-basiert
    const auto protocol = protocol_->currentIndex() == 1 ? output::Protocol::Sacn : output::Protocol::ArtNet;
    const auto universe = static_cast<std::uint32_t>(universe_->value());

    QMenu* menu = buildContextMenu(channel, protocol, universe);
    menu->exec(grid_->viewport()->mapToGlobal(pos));
    menu->deleteLater();
}

void DmxOutputMonitorPanel::refresh() {
    const auto protocol = protocol_->currentIndex() == 1 ? output::Protocol::Sacn : output::Protocol::ArtNet;
    const auto universe = static_cast<std::uint32_t>(universe_->value());
    const std::optional<output::SentDmxFrame> frame = store_->readAll(protocol, universe);
    if (!frame) {
        if (hasData_) {
            for (int r = 0; r < kRows; ++r) {
                for (int c = 0; c < kColumns; ++c) {
                    grid_->item(r, c)->setText(QStringLiteral("–"));
                }
            }
            hasData_ = false;
        }
        status_->setText(tr("Es wurde noch nichts auf dieses Universe gesendet."));
        return;
    }
    status_->setText(frame->ageSeconds > kStaleAfterSeconds
                          ? tr("Letzter gesendeter Frame vor %1 s (veraltet).").arg(frame->ageSeconds, 0, 'f', 1)
                          : tr("Aktiv (letzter gesendeter Frame vor %1 s).").arg(frame->ageSeconds, 0, 'f', 1));
    for (int i = 0; i < static_cast<int>(output::kSentDmxChannels); ++i) {
        if (hasData_ && frame->channels[static_cast<std::size_t>(i)] == lastValues_[static_cast<std::size_t>(i)]) {
            continue;
        }
        const int r = i / kColumns;
        const int c = i % kColumns;
        grid_->item(r, c)->setText(QString::number(frame->channels[static_cast<std::size_t>(i)]));
    }
    lastValues_ = frame->channels;
    hasData_ = true;
}

QString DmxOutputMonitorPanel::exercise() {
    // Universe weit außerhalb von allem, was Vorlagen/Beispiele im übrigen Rauchtest patchen (0, 1, 4, 7),
    // damit echte Frames aus anderen Tests dieses Universe nicht schon vorher befüllt haben (siehe
    // DmxMonitorPanel::exercise, dieselbe Art von Kollision - dort war es Universe 9 aus exerciseControls).
    protocol_->setCurrentIndex(0);
    universe_->setValue(30000);
    refresh();
    if (status_->text() != tr("Es wurde noch nichts auf dieses Universe gesendet.")) {
        return QStringLiteral("DMX-Ausgangs-Monitor: zeigt ohne gesendeten Frame nicht \"noch nichts gesendet\"");
    }
    std::vector<std::uint8_t> packet(512, 0);
    packet[0] = 42;
    packet[511] = 200;
    store_->write(output::Protocol::ArtNet, 30000, packet);
    refresh();
    if (grid_->item(0, 0)->text() != QStringLiteral("42") || grid_->item(31, 15)->text() != QStringLiteral("200")) {
        return QStringLiteral("DMX-Ausgangs-Monitor: zeigt die gesendeten Werte nicht an den richtigen Zellen");
    }
    universe_->setValue(30001);  // anderes Universe: muss wieder "-" zeigen, nicht die alten Werte von 30000
    refresh();
    if (grid_->item(0, 0)->text() != QStringLiteral("–")) {
        return QStringLiteral("DMX-Ausgangs-Monitor: Wechsel des Universe behält alte Werte");
    }
    universe_->setValue(30000);
    refresh();

    // "Wer hört zu": ohne Belegung der normale Tooltip; onSelectionChanged() (durch setValue oben ausgelöst)
    // hat ihn schon neu berechnet.
    if (grid_->item(0, 0)->toolTip() != occupantTooltip(1, std::nullopt)) {
        return QStringLiteral("DMX-Ausgangs-Monitor: Tooltip zeigt ohne Belegung nicht einfach \"Kanal N\"");
    }

    // Rechtsklick auf eine freie Zelle (Universe 30000 ist nicht gepatcht): Kontextmenü bietet das Patchen
    // an, und löst es aus. menu.exec() selbst wird nicht aufgerufen (würde im Rauchtest blockieren).
    bool requested = false;
    output::Protocol requestedProtocol{};
    std::uint32_t requestedUniverse = 0;
    std::uint32_t requestedChannel = 0;
    // Verbindung merken und am Ende wieder lösen: sie hängt sonst über das Ende dieser Funktion hinaus an
    // `this` und griffe bei einem echten späteren Rechtsklick auf die hier längst ungültigen Stack-Referenzen zu.
    const QMetaObject::Connection probe =
        connect(this, &DmxOutputMonitorPanel::requestManualPatch, this,
                [&](output::Protocol protocol, std::uint32_t universe, std::uint32_t channel) {
                    requested = true;
                    requestedProtocol = protocol;
                    requestedUniverse = universe;
                    requestedChannel = channel;
                });
    QMenu* menu = buildContextMenu(1, output::Protocol::ArtNet, 30000);
    if (menu->actions().size() != 1 || !menu->actions().first()->text().contains(QStringLiteral("Neues Fixture"))) {
        delete menu;
        disconnect(probe);
        return QStringLiteral("DMX-Ausgangs-Monitor: Kontextmenü für eine freie Zelle bietet nicht \"Neues Fixture hier patchen\" an");
    }
    menu->actions().first()->trigger();
    delete menu;
    disconnect(probe);
    if (!requested || requestedProtocol != output::Protocol::ArtNet || requestedUniverse != 30000 || requestedChannel != 1) {
        return QStringLiteral("DMX-Ausgangs-Monitor: Kontextmenü löst requestManualPatch nicht mit den richtigen Werten aus");
    }

    // Rechtsklick auf eine belegte Fixture-Zelle: nutzt den ersten echten Fixture-Patch des Rauchtest-
    // Projekts (exerciseStageSetup lief schon vorher und hat mindestens ein gepatchtes Fixture angelegt).
    if (!player_.project()->patches.empty()) {
        const output::FixturePatch& patch = player_.project()->patches.front();
        const auto occupant = player_.dmxOutputOccupantAt(patch.protocol, patch.universe, patch.channel);
        if (!occupant || occupant->kind != runtime::DmxOutputOccupant::Kind::Fixture) {
            return QStringLiteral("DMX-Ausgangs-Monitor: belegter Fixture-Kanal wird nicht als solcher erkannt");
        }
        bool requestedEdit = false;
        mapping::FixtureId requestedId{};
        const QMetaObject::Connection editProbe =
            connect(this, &DmxOutputMonitorPanel::requestEditFixturePatch, this,
                    [&](mapping::FixtureId id) {
                        requestedEdit = true;
                        requestedId = id;
                    });
        QMenu* editMenu = buildContextMenu(patch.channel, patch.protocol, patch.universe);
        if (editMenu->actions().size() != 1 || !editMenu->actions().first()->text().contains(QStringLiteral("bearbeiten"))) {
            delete editMenu;
            disconnect(editProbe);
            return QStringLiteral("DMX-Ausgangs-Monitor: Kontextmenü für eine belegte Fixture-Zelle bietet nicht \"bearbeiten\" an");
        }
        editMenu->actions().first()->trigger();
        delete editMenu;
        disconnect(editProbe);
        if (!requestedEdit || requestedId != occupant->fixtureId) {
            return QStringLiteral("DMX-Ausgangs-Monitor: Kontextmenü löst requestEditFixturePatch nicht mit dem richtigen Fixture aus");
        }
    }

    // Rechtsklick auf eine belegte Pool-Zelle: vorübergehend einen Pool-Patch auf ein Universe weit weg von
    // allem anderen anlegen, die "bearbeiten"-Aktion prüfen, danach den ursprünglichen Stand wiederherstellen.
    const std::vector<output::DmxChannelPatch> dmxPatchesBefore = player_.project()->dmxPatches;
    std::vector<output::DmxChannelPatch> withPool = dmxPatchesBefore;
    output::DmxChannelPatch poolPatch;
    poolPatch.poolChannel = 77;
    poolPatch.count = 3;
    poolPatch.universe = 30002;
    poolPatch.channel = 1;
    withPool.push_back(poolPatch);
    if (!player_.setDmxPatches(withPool).ok()) {
        return QStringLiteral("DMX-Ausgangs-Monitor: vorübergehender DMX-Kanal-Pool-Patch für den Rauchtest lässt sich nicht anlegen");
    }
    const auto poolOccupant = player_.dmxOutputOccupantAt(output::Protocol::ArtNet, 30002, 1);
    if (!poolOccupant || poolOccupant->kind != runtime::DmxOutputOccupant::Kind::Pool || poolOccupant->poolChannel != 77) {
        player_.setDmxPatches(dmxPatchesBefore);
        return QStringLiteral("DMX-Ausgangs-Monitor: belegter Pool-Kanal wird nicht als solcher erkannt");
    }
    bool requestedPoolEdit = false;
    std::uint16_t requestedPoolChannel = 0;
    const QMetaObject::Connection poolProbe =
        connect(this, &DmxOutputMonitorPanel::requestEditPoolPatch, this,
                [&](std::uint16_t poolChannel) {
                    requestedPoolEdit = true;
                    requestedPoolChannel = poolChannel;
                });
    QMenu* poolMenu = buildContextMenu(1, output::Protocol::ArtNet, 30002);
    const bool poolMenuOk =
        poolMenu->actions().size() == 1 && poolMenu->actions().first()->text().contains(QStringLiteral("bearbeiten"));
    if (poolMenuOk) {
        poolMenu->actions().first()->trigger();
    }
    delete poolMenu;
    disconnect(poolProbe);
    player_.setDmxPatches(dmxPatchesBefore);  // Rauchtest-Projekt wieder in den Ausgangszustand bringen
    if (!poolMenuOk) {
        return QStringLiteral("DMX-Ausgangs-Monitor: Kontextmenü für eine belegte Pool-Zelle bietet nicht \"bearbeiten\" an");
    }
    if (!requestedPoolEdit || requestedPoolChannel != 77) {
        return QStringLiteral("DMX-Ausgangs-Monitor: Kontextmenü löst requestEditPoolPatch nicht mit dem richtigen Pool-Kanal aus");
    }
    return {};
}

}  // namespace whitehole::studio
