// dmx_output_monitor_panel.cpp: Umsetzung zu dmx_output_monitor_panel.hpp.
#include "dmx_output_monitor_panel.hpp"

#include <QAction>
#include <QBrush>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

#include "dmx_cell_colors.hpp"

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
    colorOverrides_.clear();  // galten für das vorige Protokoll/Universe, siehe Kommentar am Header
    refresh();  // berechnet jetzt auch die Tooltips/Farben neu, siehe dort
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
    // Wird jetzt aus refresh() heraus bei jedem Bild aufgerufen, solange der Reiter sichtbar ist (vom
    // Nutzer gemeldet: vorher nur bei Protokoll-/Universe-Wechsel, dadurch zeigte der Monitor nach einem
    // Patch/Aufheben anderswo die alte Farbe/den alten Tooltip, bis man neu wählte).
    const auto protocol = protocol_->currentIndex() == 1 ? output::Protocol::Sacn : output::Protocol::ArtNet;
    const auto universe = static_cast<std::uint32_t>(universe_->value());
    for (int channel = 1; channel <= static_cast<int>(output::kSentDmxChannels); ++channel) {
        const auto occupant = player_.dmxOutputOccupantAt(protocol, universe, static_cast<std::uint32_t>(channel));
        const int r = (channel - 1) / kColumns;
        const int c = (channel - 1) % kColumns;
        QTableWidgetItem* item = grid_->item(r, c);
        item->setToolTip(occupantTooltip(static_cast<std::uint32_t>(channel), occupant));
        const auto colorOverride = colorOverrides_.find(static_cast<std::uint32_t>(channel));
        if (colorOverride != colorOverrides_.end()) {
            item->setBackground(colorOverride->second);
        } else if (occupant) {
            item->setBackground(dmxCellColor(dmxCellCategory(*occupant)));
        } else {
            item->setBackground(QBrush{});  // keine Belegung: normaler Tabellenhintergrund
        }
    }
}

QMenu* DmxOutputMonitorPanel::buildContextMenu(std::uint32_t channel, output::Protocol protocol, std::uint32_t universe) {
    auto* menu = new QMenu{this};
    const auto occupant = player_.dmxOutputOccupantAt(protocol, universe, channel);
    if (occupant) {
        QAction* color = menu->addAction(tr("Farbe ändern…"));
        connect(color, &QAction::triggered, this, [this, channel]() { changeCellColor(channel); });
        if (occupant->kind == runtime::DmxOutputOccupant::Kind::Fixture) {
            QAction* edit = menu->addAction(tr("Fixture-Patch bearbeiten: %1…").arg(QString::fromStdString(occupant->name)));
            const mapping::FixtureId id = occupant->fixtureId;
            connect(edit, &QAction::triggered, this, [this, id]() { Q_EMIT requestEditFixturePatch(id); });
        } else {
            // Untermenü "Node" (wie bei der freien Zelle unten): "Patch bearbeiten…" springt zur DMX-Kanal-
            // Pool-Tabelle, "Patch aufheben" entfernt den Patch direkt (vom Nutzer gewünscht: "rechtklick ->
            // node -> patch aufheben").
            QMenu* nodeMenu = menu->addMenu(tr("Node"));
            const std::uint16_t poolChannel = occupant->poolChannel;
            QAction* edit = nodeMenu->addAction(tr("Patch bearbeiten (Pool-Kanal %1)…").arg(poolChannel));
            connect(edit, &QAction::triggered, this, [this, poolChannel]() { Q_EMIT requestEditPoolPatch(poolChannel); });
            QAction* remove = nodeMenu->addAction(tr("Patch aufheben"));
            connect(remove, &QAction::triggered, this, [this, poolChannel]() { Q_EMIT requestRemovePoolPatch(poolChannel); });
        }
    } else {
        // Untermenü "Node" (Gegenstück zum gleichnamigen Untermenü im Eingangs-Monitor, siehe
        // dmx_monitor_panel.hpp): patcht den Pool-Bereich eines schon im Projekt vorhandenen
        // wh.output.dmx/wh.output.dmxcolor-Nodes auf diesen Kanal. Fixtures laufen bewusst nicht über dieses
        // Menü, sondern ausschließlich über den Reiter Fixtures bzw. die Fixture-Patch-Tabelle.
        QMenu* nodeMenu = menu->addMenu(tr("Node"));
        const std::vector<runtime::DmxOutputValueNodeRef> nodes = player_.dmxOutputValueNodes();
        if (nodes.empty()) {
            QAction* none = nodeMenu->addAction(tr("Keine DMX-Ausgabe-Nodes im Projekt"));
            none->setEnabled(false);
        } else {
            for (const runtime::DmxOutputValueNodeRef& node : nodes) {
                QAction* patch = nodeMenu->addAction(QString::fromStdString(node.label));
                const std::uint16_t poolChannel = node.poolChannel;
                const std::uint16_t count = node.count;
                connect(patch, &QAction::triggered, this, [this, poolChannel, count, protocol, universe, channel]() {
                    Q_EMIT requestPatchOutputNode(poolChannel, count, protocol, universe, channel);
                });
            }
        }
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
    updateOccupantTooltips();  // jetzt bei jedem Aufruf, siehe Kommentar dort
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
        // Nur die Zahl (vom Nutzer gewünscht: der Name führte bei langen Namen zu abgeschnittenem Text in
        // der Zelle); wer die Zelle belegt, steht weiterhin im Tooltip (siehe occupantTooltip/updateOccupantTooltips).
        grid_->item(r, c)->setText(QString::number(frame->channels[static_cast<std::size_t>(i)]));
    }
    lastValues_ = frame->channels;
    hasData_ = true;
}

void DmxOutputMonitorPanel::changeCellColor(std::uint32_t channel) {
    const int r = static_cast<int>(channel - 1) / kColumns;
    const int c = static_cast<int>(channel - 1) % kColumns;
    QTableWidgetItem* item = grid_->item(r, c);
    const QColor initial = item->background().color();
    const QColor chosen = QColorDialog::getColor(initial.isValid() ? initial : Qt::white, this, tr("Farbe für Kanal %1").arg(channel));
    if (!chosen.isValid()) {
        return;  // abgebrochen
    }
    colorOverrides_[channel] = chosen;
    item->setBackground(chosen);
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

    // Rechtsklick auf eine freie Zelle (Universe 30000 ist nicht gepatcht): Kontextmenü bietet nur das
    // Untermenü "Node" an ("Neues Fixture hier patchen…" gab es hier früher zusätzlich, vom Nutzer wieder
    // entfernt). menu.exec() selbst wird nicht aufgerufen (würde im Rauchtest blockieren).
    QMenu* menu = buildContextMenu(1, output::Protocol::ArtNet, 30000);
    if (menu->actions().size() != 1 || menu->actions()[0]->menu() == nullptr) {
        delete menu;
        return QStringLiteral("DMX-Ausgangs-Monitor: Kontextmenü für eine freie Zelle bietet nicht \"Node\" an");
    }
    // Ohne wh.output.dmx/wh.output.dmxcolor-Node im Projekt: "Node" zeigt nur einen deaktivierten Hinweis.
    QMenu* emptyNodeMenu = menu->actions()[0]->menu();
    const bool emptyNodeOk =
        emptyNodeMenu != nullptr && emptyNodeMenu->actions().size() == 1 && !emptyNodeMenu->actions().first()->isEnabled();
    delete menu;
    if (!emptyNodeOk) {
        return QStringLiteral("DMX-Ausgangs-Monitor: \"Node\" zeigt ohne passenden Node nicht den deaktivierten Hinweis");
    }

    // Mit einem wh.output.dmx-Node im Projekt: "Node" bietet ihn an, und das Auslösen patcht wirklich den
    // Pool-Bereich dieses Nodes auf die angeklickte Adresse (requestPatchOutputNode ist mit
    // MainWindow::patchDmxOutputValueNode verbunden, siehe buildDmxMonitor) - den testweise angelegten Node
    // und den dadurch entstandenen Patch am Ende wieder entfernen.
    const std::vector<output::DmxChannelPatch> dmxPatchesBeforeNode = player_.project()->dmxPatches;
    const Result<NodeId> addedNode = player_.addNode("wh.output.dmx", Vec2{});
    if (!addedNode.ok()) {
        return QStringLiteral("DMX-Ausgangs-Monitor: testweiser DMX-Ausgabe-Node für den Rauchtest lässt sich nicht anlegen");
    }
    const auto outputNodes = player_.dmxOutputValueNodes();
    const auto match = std::find_if(outputNodes.begin(), outputNodes.end(),
                                     [&](const runtime::DmxOutputValueNodeRef& ref) { return ref.id == addedNode.value(); });
    if (match == outputNodes.end()) {
        player_.removeNode(addedNode.value());
        return QStringLiteral("DMX-Ausgangs-Monitor: der testweise angelegte Node steht nicht in dmxOutputValueNodes()");
    }
    bool requestedOutputPatch = false;
    std::uint16_t requestedOutputPoolChannel = 0;
    std::uint16_t requestedCount = 0;
    const QMetaObject::Connection outputPatchProbe = connect(
        this, &DmxOutputMonitorPanel::requestPatchOutputNode, this,
        [&](std::uint16_t poolChannel, std::uint16_t count, output::Protocol, std::uint32_t, std::uint32_t) {
            requestedOutputPatch = true;
            requestedOutputPoolChannel = poolChannel;
            requestedCount = count;
        });
    QMenu* filledMenu = buildContextMenu(2, output::Protocol::ArtNet, 30000);
    const bool filledMenuOk = filledMenu->actions().size() == 1 && filledMenu->actions()[0]->menu() != nullptr;
    QMenu* filledNodeMenu = filledMenuOk ? filledMenu->actions()[0]->menu() : nullptr;
    const bool filledOk = filledNodeMenu != nullptr && filledNodeMenu->actions().size() == 1;
    if (filledOk) {
        filledNodeMenu->actions().first()->trigger();
    }
    delete filledMenu;
    disconnect(outputPatchProbe);
    if (!filledOk) {
        player_.removeNode(addedNode.value());
        return QStringLiteral("DMX-Ausgangs-Monitor: \"Node\" bietet den angelegten Node nicht an");
    }
    if (!requestedOutputPatch || requestedOutputPoolChannel != match->poolChannel || requestedCount != match->count) {
        player_.removeNode(addedNode.value());
        return QStringLiteral("DMX-Ausgangs-Monitor: Kontextmenü löst requestPatchOutputNode nicht mit dem richtigen Pool-Bereich aus");
    }
    // Das Auslösen der Aktion hat schon die echte requestPatchOutputNode-Verbindung ausgelöst (siehe
    // buildDmxMonitor: MainWindow::patchDmxOutputValueNode bleibt, anders als requestManualPatch, während
    // exercise() verbunden - kein modaler Dialog dahinter) und damit wirklich einen DmxChannelPatch angelegt.
    const auto nodeOccupant = player_.dmxOutputOccupantAt(output::Protocol::ArtNet, 30000, 2);
    player_.removeNode(addedNode.value());
    player_.setDmxPatches(dmxPatchesBeforeNode);  // Rauchtest-Projekt wieder in den Ausgangszustand bringen
    if (!nodeOccupant || nodeOccupant->kind != runtime::DmxOutputOccupant::Kind::Pool) {
        return QStringLiteral("DMX-Ausgangs-Monitor: \"Node\" patcht die Adresse des Nodes nicht wirklich (MainWindow-Verbindung?)");
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
        if (editMenu->actions().size() != 2 || !editMenu->actions()[0]->text().contains(QStringLiteral("Farbe ändern")) ||
            !editMenu->actions()[1]->text().contains(QStringLiteral("bearbeiten"))) {
            delete editMenu;
            disconnect(editProbe);
            return QStringLiteral(
                "DMX-Ausgangs-Monitor: Kontextmenü für eine belegte Fixture-Zelle bietet nicht \"Farbe ändern…\" und \"bearbeiten\" an");
        }
        editMenu->actions()[1]->trigger();
        delete editMenu;
        disconnect(editProbe);
        // Zellenfarbe: Fixture-Belegung bekommt die Fixture-Farbe. Dafür kurz auf das Protokoll/Universe
        // des Patches wechseln (Farben werden wie Tooltips nur bei einem solchen Wechsel neu berechnet,
        // siehe Kommentar am Header), prüfen, danach zurück auf ArtNet/30000 für die restlichen Tests.
        protocol_->setCurrentIndex(patch.protocol == output::Protocol::Sacn ? 1 : 0);
        universe_->setValue(static_cast<int>(patch.universe));
        const int r = static_cast<int>(patch.channel - 1) / kColumns;
        const int c = static_cast<int>(patch.channel - 1) % kColumns;
        const bool fixtureColorOk = grid_->item(r, c)->background().color() == dmxCellColor(DmxCellCategory::Fixture);
        protocol_->setCurrentIndex(0);
        universe_->setValue(30000);
        if (!fixtureColorOk) {
            return QStringLiteral("DMX-Ausgangs-Monitor: Fixture-Belegung bekommt nicht die Fixture-Farbe");
        }
        // Zellentext bleibt die reine Zahl (siehe Kommentar am Header), der Fixture-Name steht nur im
        // Tooltip - braucht dafür einen echten gesendeten Frame auf diesem Universe, sonst bleibt die
        // Zelle beim "–"-Platzhalter.
        protocol_->setCurrentIndex(patch.protocol == output::Protocol::Sacn ? 1 : 0);
        universe_->setValue(static_cast<int>(patch.universe));
        std::vector<std::uint8_t> fixturePacket(512, 0);
        fixturePacket[patch.channel - 1] = 7;
        store_->write(patch.protocol, patch.universe, fixturePacket);
        refresh();
        const bool fixtureNameOk = grid_->item(r, c)->text() == QStringLiteral("7") &&
                                    grid_->item(r, c)->toolTip().contains(QString::fromStdString(occupant->name));
        protocol_->setCurrentIndex(0);
        universe_->setValue(30000);
        if (!fixtureNameOk) {
            return QStringLiteral("DMX-Ausgangs-Monitor: Zelle zeigt nicht die reine Zahl oder der Tooltip nennt den Fixture-Namen nicht");
        }
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
    const bool poolMenuOk = poolMenu->actions().size() == 2 && poolMenu->actions()[0]->text().contains(QStringLiteral("Farbe ändern")) &&
                             poolMenu->actions()[1]->menu() != nullptr;
    QMenu* poolNodeMenu = poolMenuOk ? poolMenu->actions()[1]->menu() : nullptr;
    const bool poolNodeMenuOk = poolNodeMenu != nullptr && poolNodeMenu->actions().size() == 2 &&
                                 poolNodeMenu->actions()[0]->text().contains(QStringLiteral("bearbeiten")) &&
                                 poolNodeMenu->actions()[1]->text().contains(QStringLiteral("aufheben"));
    if (poolNodeMenuOk) {
        poolNodeMenu->actions()[0]->trigger();
    }
    delete poolMenu;
    disconnect(poolProbe);
    if (!poolMenuOk || !poolNodeMenuOk) {
        player_.setDmxPatches(dmxPatchesBefore);
        return QStringLiteral(
            "DMX-Ausgangs-Monitor: Kontextmenü für eine belegte Pool-Zelle bietet nicht \"Farbe ändern…\" und das Untermenü "
            "\"Node\" mit \"Patch bearbeiten…\"/\"Patch aufheben\" an");
    }
    if (!requestedPoolEdit || requestedPoolChannel != 77) {
        player_.setDmxPatches(dmxPatchesBefore);
        return QStringLiteral("DMX-Ausgangs-Monitor: Kontextmenü löst requestEditPoolPatch nicht mit dem richtigen Pool-Kanal aus");
    }
    // Zellenfarbe: Pool-Belegung bekommt die Pool-Farbe (deckt zugleich die Farbzuordnung für beide
    // occupant.kind-Werte ab - Fixture siehe oben, derselbe Code-Pfad, nur anderer Zweig). Vor "Patch
    // aufheben" prüfen: danach ist die Zelle nicht mehr belegt (siehe unten).
    universe_->setValue(30002);
    const bool poolColorOk = grid_->item(0, 0)->background().color() == dmxCellColor(DmxCellCategory::Pool);
    // Zellentext bleibt die reine Zahl; ohne echten Namen (DmxOutputOccupant::name ist bei Kind::Pool leer)
    // nennt der Tooltip stattdessen den Pool-Kanal - braucht dafür einen echten gesendeten Frame auf diesem
    // Universe, sonst bleibt die Zelle beim "–"-Platzhalter ohne Text.
    std::vector<std::uint8_t> poolPacket(512, 0);
    poolPacket[0] = 5;
    store_->write(output::Protocol::ArtNet, 30002, poolPacket);
    refresh();
    const bool poolNameOk =
        grid_->item(0, 0)->text() == QStringLiteral("5") && grid_->item(0, 0)->toolTip().contains(QStringLiteral("Pool-Kanal 77"));
    universe_->setValue(30000);
    if (!poolColorOk) {
        player_.setDmxPatches(dmxPatchesBefore);
        return QStringLiteral("DMX-Ausgangs-Monitor: Pool-Belegung bekommt nicht die Pool-Farbe");
    }
    if (!poolNameOk) {
        player_.setDmxPatches(dmxPatchesBefore);
        return QStringLiteral("DMX-Ausgangs-Monitor: Zelle zeigt nicht die reine Zahl oder der Tooltip nennt den Pool-Kanal nicht");
    }

    // "Patch aufheben": requestRemovePoolPatch ist in MainWindow::buildDmxMonitor echt verbunden (anders als
    // bei den übrigen Sonden hier, die nur lokal mitzählen) - das Auslösen entfernt den Patch wirklich, kein
    // Aufräumen danach mehr nötig außer der Sicherheit halber (dmxPatchesBefore enthält ihn ja ohnehin nicht).
    QMenu* removeMenu = buildContextMenu(1, output::Protocol::ArtNet, 30002);
    QMenu* removeNodeMenu = removeMenu->actions().size() == 2 ? removeMenu->actions()[1]->menu() : nullptr;
    const bool removeMenuOk = removeNodeMenu != nullptr && removeNodeMenu->actions().size() == 2 &&
                               removeNodeMenu->actions()[1]->text().contains(QStringLiteral("aufheben"));
    if (removeMenuOk) {
        removeNodeMenu->actions()[1]->trigger();
    }
    delete removeMenu;
    if (!removeMenuOk) {
        player_.setDmxPatches(dmxPatchesBefore);
        return QStringLiteral("DMX-Ausgangs-Monitor: Kontextmenü bietet \"Patch aufheben\" nicht mehr an");
    }
    const auto removedOccupant = player_.dmxOutputOccupantAt(output::Protocol::ArtNet, 30002, 1);
    player_.setDmxPatches(dmxPatchesBefore);  // Rauchtest-Projekt wieder in den Ausgangszustand bringen (Sicherheit)
    if (removedOccupant) {
        return QStringLiteral("DMX-Ausgangs-Monitor: \"Patch aufheben\" entfernt den Patch nicht wirklich (MainWindow-Verbindung?)");
    }
    return {};
}

}  // namespace whitehole::studio
