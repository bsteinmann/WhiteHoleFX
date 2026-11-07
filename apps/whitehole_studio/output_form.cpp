// output_form.cpp: Ziele und Einstellungen.
#include "output_form.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QMenu>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

#include "user_library.hpp"

namespace whitehole::studio {

using form::fromStd;

OutputForm::OutputForm(runtime::Player& player, QWidget* parent) : QWidget{parent}, player_{player} {
    auto* outer = new QVBoxLayout{this};

    auto* destinations = new QGroupBox{tr("Controller")};
    auto* destinationLayout = new QVBoxLayout{destinations};
    table_ = new QTableWidget{0, Count};
    table_->setHorizontalHeaderLabels(
        {tr("Name"), tr("Protokoll"), tr("Adresse (leer = Multicast, nur sACN)"), tr("Port (leer = Standard)")});
    table_->horizontalHeader()->setToolTip(
        tr("Das Protokoll gehört zum Controller. Standardports: Art-Net 6454, sACN 5568, DDP 4048."));
    // Alle Spalten per Ziehen in der Breite änderbar; die Port-Spalte war bei "Stretch" nur ein schmaler Streifen.
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setColumnWidth(Name, 160);
    table_->setColumnWidth(Protocol, 110);
    table_->setColumnWidth(Address, 240);
    table_->setColumnWidth(Port, 160);
    table_->horizontalHeader()->setSectionsMovable(true);  // Spalten per Ziehen an der Kopfzeile umordnen
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setMinimumHeight(120);
    destinationLayout->addWidget(table_);
    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton{tr("Controller +")};
    auto* remove = new QPushButton{tr("Gewählten Controller −")};
    remove->setToolTip(tr("Entfernt den gewählten Controller (ohne Auswahl den letzten). Benutzte Controller bleiben stehen."));
    buttons->addWidget(add);
    buttons->addWidget(remove);
    // Bibliothek: gespeicherte Controller (Name, Protokoll, Adresse, Port) in jedem Projekt wiederverwenden
    auto* fromLibrary = new QPushButton{tr("Aus Bibliothek +")};
    auto* toLibrary = new QPushButton{tr("In Bibliothek speichern")};
    auto* deleteFromLibrary = new QPushButton{tr("Aus Bibliothek löschen")};
    fromLibrary->setToolTip(tr("Einen gespeicherten Controller als neuen Controller in dieses Projekt holen"));
    toLibrary->setToolTip(tr("Speichert den gewählten Controller unter seinem Namen in der Bibliothek (Benutzerverzeichnis), "
                             "damit du ihn in anderen Projekten wieder holen kannst. Gespeichert in: %1")
                              .arg(userlib::controllerPath()));
    deleteFromLibrary->setToolTip(tr("Entfernt einen Controller aus der Bibliothek (das Projekt bleibt unverändert)"));
    addMenu_ = new QMenu{fromLibrary};
    deleteMenu_ = new QMenu{deleteFromLibrary};
    fromLibrary->setMenu(addMenu_);
    deleteFromLibrary->setMenu(deleteMenu_);
    connect(addMenu_, &QMenu::aboutToShow, this, [this]() { fillLibraryMenu(addMenu_, true); });
    connect(deleteMenu_, &QMenu::aboutToShow, this, [this]() { fillLibraryMenu(deleteMenu_, false); });
    connect(toLibrary, &QPushButton::clicked, this, [this]() { saveSelectedToLibrary(); });
    buttons->addSpacing(16);
    buttons->addWidget(fromLibrary);
    buttons->addWidget(toLibrary);
    buttons->addWidget(deleteFromLibrary);
    buttons->addStretch();
    destinationLayout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, [this]() { addDestination(); });
    connect(remove, &QPushButton::clicked, this, [this]() { removeSelectedDestination(); });
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
    sacnLayout_ = settingsLayout;
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
                table_->setItem(row, Name, new QTableWidgetItem{fromStd(d.name)});
                auto* protocol = form::combo({QStringLiteral("Art-Net"), QStringLiteral("sACN"), QStringLiteral("DDP")},
                                             static_cast<int>(d.protocol));
                connect(protocol, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
                    if (!populating_) {
                        applyDestinations();
                    }
                });
                table_->setCellWidget(row, Protocol, protocol);
                table_->setItem(row, Address, new QTableWidgetItem{fromStd(d.address)});
                table_->setItem(row, Port, new QTableWidgetItem{d.port == 0 ? QString{} : QString::number(d.port)});
            }
        }
        QStringList numbers;  // Zielnummer wie im Patch: ab 0
        for (int row = 0; row < table_->rowCount(); ++row) {
            numbers.push_back(QString::number(row));
        }
        table_->setVerticalHeaderLabels(numbers);
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
    updateSacnVisibility();
}

void OutputForm::updateSacnVisibility() {
    // Priorität und Quellname gelten nur für sACN: ohne sACN-Controller weg damit.
    bool any = false;
    if (const project::Project* project = player_.project()) {
        for (const output::Destination& d : project->destinations) {
            any = any || d.protocol == output::Protocol::Sacn;
        }
    }
    for (QWidget* field : {static_cast<QWidget*>(priority_), static_cast<QWidget*>(sourceName_)}) {
        field->setVisible(any);
        if (QWidget* label = sacnLayout_->labelForField(field)) {
            label->setVisible(any);
        }
    }
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
        if (const QTableWidgetItem* name = table_->item(row, Name)) {
            d.name = name->text().trimmed().toUtf8().toStdString();
        }
        if (const auto* protocol = qobject_cast<QComboBox*>(table_->cellWidget(row, Protocol))) {
            d.protocol = static_cast<output::Protocol>(protocol->currentIndex());
        }
        if (const QTableWidgetItem* address = table_->item(row, Address)) {
            d.address = address->text().trimmed().toUtf8().toStdString();
        }
        bool ok = true;
        const QTableWidgetItem* port = table_->item(row, Port);
        const QString portText = port != nullptr ? port->text().trimmed() : QString{};
        const int value = portText.isEmpty() ? 0 : portText.toInt(&ok);
        if (!ok || value < 0 || value > 65535) {
            report(Error{ErrorCode::InvalidArgument,
                         "Zeile " + std::to_string(row + 1) + ": Der Port muss eine Zahl von 0 bis 65535 sein"});
            return;
        }
        d.port = static_cast<std::uint16_t>(value);
        destinations.push_back(std::move(d));
    }
    // Merken, was ein Protokollwechsel mit den Patches macht, um es in der Statuszeile zu sagen.
    QStringList notes;
    if (const project::Project* project = player_.project()) {
        for (std::size_t i = 0; i < destinations.size() && i < project->destinations.size(); ++i) {
            const output::Protocol from = project->destinations[i].protocol;
            const output::Protocol to = destinations[i].protocol;
            if (from == to) {
                continue;
            }
            int patched = 0;
            for (const output::FixturePatch& patch : project->patches) {
                patched += patch.destination == i ? 1 : 0;
            }
            const bool artnetSacn = (from == output::Protocol::ArtNet && to == output::Protocol::Sacn) ||
                                    (from == output::Protocol::Sacn && to == output::Protocol::ArtNet);
            if (patched > 0 && artnetSacn) {
                notes << tr("Controller %1: Universe von %2 Patch(es) um %3 verschoben (Art-Net zählt ab 0, sACN ab 1).")
                             .arg(i)
                             .arg(patched)
                             .arg(to == output::Protocol::Sacn ? QStringLiteral("+1") : QStringLiteral("−1"));
            }
        }
    }
    const Result<void> result = player_.setDestinations(std::move(destinations));
    report(result);
    if (result.ok()) {
        updateSacnVisibility();
        if (!notes.isEmpty()) {
            Q_EMIT notice(notes.join(QLatin1Char(' ')));
        }
    }
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

void OutputForm::removeSelectedDestination() {
    if (player_.project() == nullptr || player_.project()->destinations.empty()) {
        return;
    }
    const int row = table_->currentRow() >= 0 ? table_->currentRow()
                                              : static_cast<int>(player_.project()->destinations.size()) - 1;
    const Result<void> r = player_.removeDestination(static_cast<std::size_t>(row));
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
    table_->item(static_cast<int>(destinations), Address)->setText(QStringLiteral("192.168.0.101"));
    if (player_.project()->destinations.back().address != "192.168.0.101") {
        return QStringLiteral("Ausgabe: Adresse wurde nicht übernommen");
    }
    // Protokoll des neuen Controllers wechseln: sACN zeigt Priorität und Quellname, Art-Net blendet sie wieder aus
    {
        auto* protocol = qobject_cast<QComboBox*>(table_->cellWidget(static_cast<int>(destinations), Protocol));
        if (protocol == nullptr) {
            return QStringLiteral("Ausgabe: keine Protokoll-Auswahl in der Tabelle");
        }
        protocol->setCurrentIndex(static_cast<int>(output::Protocol::Sacn));
        if (player_.project()->destinations.back().protocol != output::Protocol::Sacn) {
            return QStringLiteral("Ausgabe: Protokoll wurde nicht übernommen");
        }
        if (priority_->isHidden()) {
            return QStringLiteral("Ausgabe: sACN-Priorität bleibt trotz sACN-Controller verborgen");
        }
        qobject_cast<QComboBox*>(table_->cellWidget(static_cast<int>(destinations), Protocol))
            ->setCurrentIndex(static_cast<int>(output::Protocol::ArtNet));
        table_->item(static_cast<int>(destinations), Name)->setText(QStringLiteral("Test"));
        if (player_.project()->destinations.back().name != "Test") {
            return QStringLiteral("Ausgabe: Name wurde nicht übernommen");
        }
    }
    // Ein mittleres Ziel entfernen: das davor angelegte Ziel (192.168.0.100) soll bleiben, das gewählte verschwinden
    addDestination();
    table_->setCurrentCell(static_cast<int>(destinations), Address);
    removeSelectedDestination();
    if (player_.project()->destinations.size() != destinations + 1 ||
        player_.project()->destinations.back().address != "192.168.0.100") {
        return QStringLiteral("Ausgabe: das gewählte Ziel wurde nicht entfernt");
    }
    table_->clearSelection();
    table_->setCurrentCell(-1, -1);
    removeSelectedDestination();
    if (player_.project()->destinations.size() != destinations) {
        return QStringLiteral("Ausgabe: Ziel wurde nicht entfernt");
    }
    return exerciseLibrary();
}

}  // namespace whitehole::studio
