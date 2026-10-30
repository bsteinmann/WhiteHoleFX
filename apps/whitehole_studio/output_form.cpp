// output_form.cpp: Ziele und Einstellungen.
#include "output_form.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace whitehole::studio {

using form::fromStd;

OutputForm::OutputForm(runtime::Player& player, QWidget* parent) : QWidget{parent}, player_{player} {
    auto* outer = new QVBoxLayout{this};

    auto* destinations = new QGroupBox{tr("Ziele (Controller)")};
    auto* destinationLayout = new QVBoxLayout{destinations};
    table_ = new QTableWidget{0, 2};
    table_->setHorizontalHeaderLabels({tr("Adresse (leer = Multicast)"), tr("Port (0 = Standard)")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->setMinimumHeight(120);
    destinationLayout->addWidget(table_);
    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton{tr("Ziel +")};
    auto* remove = new QPushButton{tr("Letztes Ziel −")};
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    destinationLayout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, [this]() { addDestination(); });
    connect(remove, &QPushButton::clicked, this, [this]() { removeLastDestination(); });
    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem*) {
        if (!populating_) {
            applyDestinations();
        }
    });
    outer->addWidget(destinations);

    auto* settings = new QGroupBox{tr("Einstellungen")};
    auto* settingsLayout = new QFormLayout{settings};
    fps_ = form::doubleBox(40.0, 0.1, 1000.0, 1.0, 1);
    normalize_ = form::combo({tr("strecken (Stretch)"), tr("Seitenverhältnis halten (KeepAspect)")}, 0);
    priority_ = form::intBox(100, 0, 200);
    sourceName_ = new QLineEdit;
    settingsLayout->addRow(tr("Bildrate (fps)"), fps_);
    settingsLayout->addRow(tr("Layout auf Würfel abbilden"), normalize_);
    settingsLayout->addRow(tr("sACN-Priorität"), priority_);
    settingsLayout->addRow(tr("Quellname (sACN)"), sourceName_);
    connect(fps_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { applySettings(); });
    connect(normalize_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { applySettings(); });
    connect(priority_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { applySettings(); });
    connect(sourceName_, &QLineEdit::editingFinished, this, [this]() { applySettings(); });
    outer->addWidget(settings);
    outer->addStretch();
}

void OutputForm::rebuild() {
    const project::Project* project = player_.project();
    populating_ = true;
    {
        const QSignalBlocker blockTable{table_};
        table_->setRowCount(0);
        if (project != nullptr) {
            for (const output::Destination& d : project->destinations) {
                const int row = table_->rowCount();
                table_->insertRow(row);
                table_->setItem(row, 0, new QTableWidgetItem{fromStd(d.address)});
                table_->setItem(row, 1, new QTableWidgetItem{QString::number(d.port)});
            }
        }
    }
    if (project != nullptr) {
        const QSignalBlocker a{fps_};
        const QSignalBlocker b{normalize_};
        const QSignalBlocker c{priority_};
        const QSignalBlocker d{sourceName_};
        fps_->setValue(project->framesPerSecond);
        normalize_->setCurrentIndex(static_cast<int>(project->normalize));
        priority_->setValue(project->sacnPriority);
        sourceName_->setText(fromStd(project->sourceName));
    }
    populating_ = false;
}

void OutputForm::report(const Result<void>& result) {
    if (result.ok()) {
        Q_EMIT edited();
        return;
    }
    Q_EMIT failed(fromStd(result.error().message));
    QTimer::singleShot(0, this, [this]() { rebuild(); });
}

void OutputForm::applyDestinations() {
    std::vector<output::Destination> destinations;
    for (int row = 0; row < table_->rowCount(); ++row) {
        output::Destination d;
        if (const QTableWidgetItem* address = table_->item(row, 0)) {
            d.address = address->text().trimmed().toUtf8().toStdString();
        }
        bool ok = true;
        const QTableWidgetItem* port = table_->item(row, 1);
        const int value = port != nullptr ? port->text().trimmed().toInt(&ok) : 0;
        if (!ok || value < 0 || value > 65535) {
            report(Error{ErrorCode::InvalidArgument,
                         "Zeile " + std::to_string(row + 1) + ": Der Port muss eine Zahl von 0 bis 65535 sein"});
            return;
        }
        d.port = static_cast<std::uint16_t>(value);
        destinations.push_back(std::move(d));
    }
    report(player_.setDestinations(std::move(destinations)));
}

void OutputForm::addDestination() {
    if (player_.project() == nullptr) {
        return;
    }
    std::vector<output::Destination> destinations = player_.project()->destinations;
    output::Destination d;
    d.address = "192.168.0.100";
    destinations.push_back(std::move(d));
    const Result<void> r = player_.setDestinations(std::move(destinations));
    if (r.ok()) {
        rebuild();
    }
    report(r);
}

void OutputForm::removeLastDestination() {
    if (player_.project() == nullptr || player_.project()->destinations.empty()) {
        return;
    }
    std::vector<output::Destination> destinations = player_.project()->destinations;
    destinations.pop_back();
    const Result<void> r = player_.setDestinations(std::move(destinations));
    if (r.ok()) {
        rebuild();
    }
    report(r);
}

void OutputForm::applySettings() {
    if (populating_) {
        return;
    }
    runtime::Player::Settings s;
    s.framesPerSecond = fps_->value();
    s.normalize = static_cast<mapping::NormalizeMode>(normalize_->currentIndex());
    s.sacnPriority = static_cast<std::uint8_t>(priority_->value());
    s.sourceName = sourceName_->text().toUtf8().toStdString();
    report(player_.setSettings(s));
}

QString OutputForm::exercise() {
    if (player_.project() == nullptr) {
        return QStringLiteral("Ausgabe: kein Projekt");
    }
    fps_->setValue(33.0);
    if (std::abs(player_.framesPerSecond() - 33.0) > 1.0e-6) {
        return QStringLiteral("Ausgabe: Bildrate wurde nicht übernommen");
    }
    const std::size_t destinations = player_.project()->destinations.size();
    addDestination();
    if (player_.project()->destinations.size() != destinations + 1 ||
        table_->rowCount() != static_cast<int>(destinations + 1)) {
        return QStringLiteral("Ausgabe: Ziel wurde nicht angelegt");
    }
    // Adresse in der Tabelle ändern, wie es ein Mensch täte
    table_->item(static_cast<int>(destinations), 0)->setText(QStringLiteral("192.168.0.101"));
    if (player_.project()->destinations.back().address != "192.168.0.101") {
        return QStringLiteral("Ausgabe: Adresse wurde nicht übernommen");
    }
    removeLastDestination();
    if (player_.project()->destinations.size() != destinations) {
        return QStringLiteral("Ausgabe: Ziel wurde nicht entfernt");
    }
    return {};
}

}  // namespace whitehole::studio
