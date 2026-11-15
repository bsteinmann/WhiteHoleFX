// dmx_monitor_panel.cpp: Umsetzung zu dmx_monitor_panel.hpp.
#include "dmx_monitor_panel.hpp"

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
constexpr int kRows = static_cast<int>(input::kDmxChannels) / kColumns;  // 512 / 16 = 32
constexpr double kStaleAfterSeconds = 5.0;  // wie kMasterDmxTimeoutSeconds/kLiveDmxTimeoutSeconds

}  // namespace

DmxMonitorPanel::DmxMonitorPanel(std::shared_ptr<input::DmxStore> store, runtime::Player& player, QWidget* parent)
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
            item->setToolTip(listenerTooltip(static_cast<std::uint32_t>(channel), std::nullopt));
            grid_->setItem(r, c, item);
        }
    }
    layout->addWidget(grid_);

    connect(protocol_, &QComboBox::currentIndexChanged, this, [this](int) { onSelectionChanged(); });
    connect(universe_, &QSpinBox::valueChanged, this, [this](int) { onSelectionChanged(); });
    onSelectionChanged();
}

void DmxMonitorPanel::onSelectionChanged() {
    // Anderes Protokoll/Universe: alte Werte sind sofort ungültig. Das Gitter selbst leeren statt nur
    // hasData_ zurückzusetzen - sonst überspringt refresh() das Leeren, weil sein "kein Signal"-Zweig
    // genau an hasData_ hängt (der war ja schon false) und die alten Zellen stehen bleiben würden.
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kColumns; ++c) {
            grid_->item(r, c)->setText(QStringLiteral("–"));
        }
    }
    hasData_ = false;
    updateListenerTooltips();
    refresh();
}

QString DmxMonitorPanel::listenerTooltip(std::uint32_t channel, const std::optional<runtime::DmxInputListener>& listener) {
    if (!listener) {
        return tr("Kanal %1").arg(channel);
    }
    return tr("Kanal %1 — gehört zu: %2").arg(channel).arg(QString::fromStdString(listener->name));
}

void DmxMonitorPanel::updateListenerTooltips() {
    // Nur bei Protokoll-/Universe-Wechsel (siehe Kommentar am Header), nicht in refresh(): ein Durchlauf
    // über alle Nodes pro Zelle wäre für jeden Frame unnötig teuer.
    const auto protocol = protocol_->currentIndex() == 1 ? input::DmxProtocol::Sacn : input::DmxProtocol::ArtNet;
    const auto universe = static_cast<std::uint32_t>(universe_->value());
    for (int channel = 1; channel <= static_cast<int>(input::kDmxChannels); ++channel) {
        const auto listener = player_.dmxListenerAt(protocol, universe, static_cast<std::uint32_t>(channel));
        const int r = (channel - 1) / kColumns;
        const int c = (channel - 1) % kColumns;
        grid_->item(r, c)->setToolTip(listenerTooltip(static_cast<std::uint32_t>(channel), listener));
    }
}

QMenu* DmxMonitorPanel::buildContextMenu(std::uint32_t channel, input::DmxProtocol protocol, std::uint32_t universe) {
    auto* menu = new QMenu{this};
    const auto listener = player_.dmxListenerAt(protocol, universe, channel);
    if (listener) {
        QAction* info = menu->addAction(listenerTooltip(channel, listener));
        info->setEnabled(false);
        return menu;
    }
    // Drei Kategorien statt einer flachen Liste (nachträglich freigegebene Erweiterung): "Master" und
    // "Live Kanal" zeigen nur die schon bestehenden Zuordnungen aus den Auftritts-Einstellungen (keine neue
    // Funktion, nur neu gegliedert); "DMX Out" patcht stattdessen die Adresse eines Nodes.
    QMenu* masterMenu = menu->addMenu(tr("Master"));
    QAction* masterDimmer = masterMenu->addAction(tr("Als Masterdimmer-Kanal verwenden"));
    connect(masterDimmer, &QAction::triggered, this,
            [this, protocol, universe, channel]() { Q_EMIT requestMasterDmx(protocol, universe, channel); });

    QMenu* liveMenu = menu->addMenu(tr("Live Kanal"));
    QAction* select = liveMenu->addAction(tr("Als Live-Look-Auswahl-Kanal verwenden"));
    connect(select, &QAction::triggered, this,
            [this, protocol, universe, channel]() { Q_EMIT requestLiveDmxSelect(protocol, universe, channel); });
    QAction* fade = liveMenu->addAction(tr("Als Live-Überblendzeit-Kanal verwenden"));
    connect(fade, &QAction::triggered, this,
            [this, protocol, universe, channel]() { Q_EMIT requestLiveDmxFade(protocol, universe, channel); });

    QMenu* dmxOutMenu = menu->addMenu(tr("DMX Out"));
    const std::vector<runtime::DmxValueNodeRef> nodes = player_.dmxValueNodes();
    if (nodes.empty()) {
        QAction* none = dmxOutMenu->addAction(tr("Keine DMX-Wert-Nodes im Projekt"));
        none->setEnabled(false);
    } else {
        for (const runtime::DmxValueNodeRef& node : nodes) {
            QAction* patch = dmxOutMenu->addAction(QString::fromStdString(node.label));
            const NodeId id = node.id;
            connect(patch, &QAction::triggered, this,
                    [this, id, protocol, universe, channel]() { Q_EMIT requestPatchNode(id, protocol, universe, channel); });
        }
    }
    return menu;
}

void DmxMonitorPanel::showContextMenu(const QPoint& pos) {
    const QTableWidgetItem* item = grid_->itemAt(pos);
    if (item == nullptr) {
        return;
    }
    const auto channel = static_cast<std::uint32_t>(item->row() * kColumns + item->column() + 1);  // 1-basiert
    const auto protocol = protocol_->currentIndex() == 1 ? input::DmxProtocol::Sacn : input::DmxProtocol::ArtNet;
    const auto universe = static_cast<std::uint32_t>(universe_->value());

    QMenu* menu = buildContextMenu(channel, protocol, universe);
    menu->exec(grid_->viewport()->mapToGlobal(pos));
    menu->deleteLater();
}

void DmxMonitorPanel::refresh() {
    const auto protocol = protocol_->currentIndex() == 1 ? input::DmxProtocol::Sacn : input::DmxProtocol::ArtNet;
    const auto universe = static_cast<std::uint32_t>(universe_->value());
    const std::optional<input::DmxUniverseFrame> frame = store_->readAll(protocol, universe);
    if (!frame) {
        if (hasData_) {
            for (int r = 0; r < kRows; ++r) {
                for (int c = 0; c < kColumns; ++c) {
                    grid_->item(r, c)->setText(QStringLiteral("–"));
                }
            }
            hasData_ = false;
        }
        status_->setText(tr("Kein Signal auf diesem Universe."));
        return;
    }
    status_->setText(frame->ageSeconds > kStaleAfterSeconds
                          ? tr("Letztes Paket vor %1 s (veraltet).").arg(frame->ageSeconds, 0, 'f', 1)
                          : tr("Aktiv (letztes Paket vor %1 s).").arg(frame->ageSeconds, 0, 'f', 1));
    for (int i = 0; i < static_cast<int>(input::kDmxChannels); ++i) {
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

QString DmxMonitorPanel::exercise() {
    protocol_->setCurrentIndex(0);
    universe_->setValue(777);
    refresh();
    if (status_->text() != tr("Kein Signal auf diesem Universe.")) {
        return QStringLiteral("DMX-Monitor: zeigt ohne Paket kein \"kein Signal\"");
    }
    std::vector<std::uint8_t> packet(512, 0);
    packet[0] = 42;
    packet[511] = 200;
    store_->write(input::DmxProtocol::ArtNet, 777, packet);
    refresh();
    if (grid_->item(0, 0)->text() != QStringLiteral("42") || grid_->item(31, 15)->text() != QStringLiteral("200")) {
        return QStringLiteral("DMX-Monitor: zeigt die geschriebenen Werte nicht an den richtigen Zellen");
    }
    universe_->setValue(778);  // anderes Universe: muss wieder "-" zeigen, nicht die alten Werte von Universe 777
    refresh();
    if (grid_->item(0, 0)->text() != QStringLiteral("–")) {
        return QStringLiteral("DMX-Monitor: Wechsel des Universe behält alte Werte");
    }
    universe_->setValue(777);
    refresh();

    // "Wer hört zu": ohne Hörer der normale Tooltip; onSelectionChanged() (durch setValue oben ausgelöst)
    // hat ihn schon neu berechnet.
    if (grid_->item(0, 0)->toolTip() != tr("Kanal %1").arg(1)) {
        return QStringLiteral("DMX-Monitor: Tooltip zeigt ohne Hörer nicht einfach \"Kanal N\"");
    }

    // Rechtsklick auf eine freie Zelle: drei Kategorien ("Master", "Live Kanal", "DMX Out"), der Masterdimmer-
    // Eintrag im ersten Untermenü löst aus. menu.exec() selbst wird nicht aufgerufen (würde im Rauchtest blockieren).
    bool requested = false;
    input::DmxProtocol requestedProtocol{};
    std::uint32_t requestedUniverse = 0;
    std::uint32_t requestedChannel = 0;
    // Verbindung merken und am Ende wieder lösen: sie hängt sonst über das Ende dieser Funktion hinaus an
    // `this` und griffe bei einem echten späteren Rechtsklick auf die hier längst ungültigen Stack-Referenzen zu.
    const QMetaObject::Connection probe =
        connect(this, &DmxMonitorPanel::requestMasterDmx, this,
                [&](input::DmxProtocol protocol, std::uint32_t universe, std::uint32_t channel) {
                    requested = true;
                    requestedProtocol = protocol;
                    requestedUniverse = universe;
                    requestedChannel = channel;
                });
    QMenu* menu = buildContextMenu(1, input::DmxProtocol::ArtNet, 777);
    if (menu->actions().size() != 3 || menu->actions()[0]->menu() == nullptr || menu->actions()[1]->menu() == nullptr ||
        menu->actions()[2]->menu() == nullptr) {
        delete menu;
        disconnect(probe);
        return QStringLiteral("DMX-Monitor: Kontextmenü für eine freie Zelle bietet nicht die drei Kategorien an");
    }
    menu->actions()[0]->menu()->actions().first()->trigger();  // "Master" -> "Als Masterdimmer-Kanal verwenden"
    delete menu;
    disconnect(probe);
    if (!requested || requestedProtocol != input::DmxProtocol::ArtNet || requestedUniverse != 777 || requestedChannel != 1) {
        return QStringLiteral("DMX-Monitor: Kontextmenü löst requestMasterDmx nicht mit den richtigen Werten aus");
    }

    // "DMX Out" ohne passenden Node: nur ein deaktivierter Hinweis.
    QMenu* emptyMenu = buildContextMenu(1, input::DmxProtocol::ArtNet, 777);
    QMenu* emptyDmxOut = emptyMenu->actions()[2]->menu();
    const bool emptyOk =
        emptyDmxOut != nullptr && emptyDmxOut->actions().size() == 1 && !emptyDmxOut->actions().first()->isEnabled();
    delete emptyMenu;
    if (!emptyOk) {
        return QStringLiteral("DMX-Monitor: \"DMX Out\" zeigt ohne passenden Node nicht den deaktivierten Hinweis");
    }

    // Mit einem DMX-Wert-Node im Graphen: "DMX Out" bietet ihn an, und das Auslösen patcht wirklich seine
    // Adresse (requestPatchNode ist mit MainWindow::patchDmxValueNode verbunden, siehe buildDmxMonitor) -
    // den testweise angelegten Node am Ende wieder entfernen.
    const Result<NodeId> added = player_.addNode("wh.value.dmx", Vec2{});
    if (!added.ok()) {
        return QStringLiteral("DMX-Monitor: testweiser DMX-Wert-Node für den Rauchtest lässt sich nicht anlegen");
    }
    bool requestedPatch = false;
    NodeId requestedNode{};
    const QMetaObject::Connection patchProbe = connect(
        this, &DmxMonitorPanel::requestPatchNode, this,
        [&](NodeId node, input::DmxProtocol, std::uint32_t, std::uint32_t) {
            requestedPatch = true;
            requestedNode = node;
        });
    QMenu* filledMenu = buildContextMenu(1, input::DmxProtocol::ArtNet, 777);
    QMenu* filledDmxOut = filledMenu->actions()[2]->menu();
    const bool filledOk = filledDmxOut != nullptr && filledDmxOut->actions().size() == 1;
    if (filledOk) {
        filledDmxOut->actions().first()->trigger();
    }
    delete filledMenu;
    disconnect(patchProbe);
    if (!filledOk) {
        player_.removeNode(added.value());
        return QStringLiteral("DMX-Monitor: \"DMX Out\" bietet den angelegten Node nicht an");
    }
    if (!requestedPatch || requestedNode != added.value()) {
        player_.removeNode(added.value());
        return QStringLiteral("DMX-Monitor: Kontextmenü löst requestPatchNode nicht mit dem richtigen Node aus");
    }
    const auto patched = player_.dmxListenerAt(input::DmxProtocol::ArtNet, 777, 1);
    player_.removeNode(added.value());
    if (!patched) {
        return QStringLiteral("DMX-Monitor: \"DMX Out\" patcht die Adresse des Nodes nicht wirklich (MainWindow-Verbindung?)");
    }
    return {};
}

}  // namespace whitehole::studio
