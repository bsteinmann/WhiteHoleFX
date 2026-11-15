// dmx_monitor_panel.cpp: Umsetzung zu dmx_monitor_panel.hpp.
#include "dmx_monitor_panel.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
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

DmxMonitorPanel::DmxMonitorPanel(std::shared_ptr<input::DmxStore> store, QWidget* parent)
    : QWidget{parent}, store_{std::move(store)} {
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
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kColumns; ++c) {
            auto* item = new QTableWidgetItem{QStringLiteral("–")};
            item->setTextAlignment(Qt::AlignCenter);
            const int channel = r * kColumns + c + 1;
            item->setToolTip(tr("Kanal %1").arg(channel));
            grid_->setItem(r, c, item);
        }
    }
    layout->addWidget(grid_);

    connect(protocol_, &QComboBox::currentIndexChanged, this, [this](int) { onSelectionChanged(); });
    connect(universe_, &QSpinBox::valueChanged, this, [this](int) { onSelectionChanged(); });
    onSelectionChanged();
}

void DmxMonitorPanel::onSelectionChanged() {
    hasData_ = false;  // anderes Universe: alte Werte sind nicht mehr gültig, Gitter beim nächsten refresh() neu füllen
    refresh();
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
    universe_->setValue(9);
    refresh();
    if (status_->text() != tr("Kein Signal auf diesem Universe.")) {
        return QStringLiteral("DMX-Monitor: zeigt ohne Paket kein \"kein Signal\"");
    }
    std::vector<std::uint8_t> packet(512, 0);
    packet[0] = 42;
    packet[511] = 200;
    store_->write(input::DmxProtocol::ArtNet, 9, packet);
    refresh();
    if (grid_->item(0, 0)->text() != QStringLiteral("42") || grid_->item(31, 15)->text() != QStringLiteral("200")) {
        return QStringLiteral("DMX-Monitor: zeigt die geschriebenen Werte nicht an den richtigen Zellen");
    }
    universe_->setValue(10);  // anderes Universe: muss wieder "-" zeigen, nicht die alten Werte von Universe 9
    refresh();
    if (grid_->item(0, 0)->text() != QStringLiteral("–")) {
        return QStringLiteral("DMX-Monitor: Wechsel des Universe behält alte Werte");
    }
    universe_->setValue(9);
    refresh();
    return {};
}

}  // namespace whitehole::studio
