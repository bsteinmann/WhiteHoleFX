// settings_dialog.cpp: Umsetzung zu settings_dialog.hpp. Das Fenster "Einstellungen" mit den Seiten Netzwerk, DMX-Eingang und Audio.
#include "settings_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <string>

#include "network_interfaces.hpp"
#include "whitehole/audio/audio_input.hpp"

namespace whitehole::studio {

namespace {

QLabel* note(const QString& text, QWidget* parent) {
    auto* label = new QLabel{text, parent};
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}

/// Füllt die Auswahl der Netzwerkkarten. Eine gemerkte Adresse, die es nicht mehr gibt (DHCP, Kabel
/// abgezogen), bleibt als eigener Eintrag stehen, damit die Wahl nicht stillschweigend verschwindet.
void fillCards(QComboBox* combo, const QString& current) {
    combo->addItem(QObject::tr("Automatisch (das System wählt)"), QString{});
    for (const NetworkCard& card : networkCards()) {
        combo->addItem(card.label(), card.address);
    }
    int index = combo->findData(current);
    if (index < 0) {
        combo->addItem(QObject::tr("%1 (nicht gefunden)").arg(current), current);
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(index);
}

QString toQ(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

SettingsDialog::SettingsDialog(const AppSettings& current, QWidget* parent) : QDialog{parent} {
    setWindowTitle(tr("Einstellungen"));
    auto* tabs = new QTabWidget{this};
    tabs->addTab(buildNetworkPage(current), tr("Netzwerk"));
    tabs->addTab(buildDmxPage(current), tr("DMX-Eingang"));
    tabs->addTab(buildAudioPage(current), tr("Audio"));
    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* box = new QVBoxLayout{this};
    box->addWidget(tabs);
    box->addWidget(buttons);
    resize(520, 380);
}

QWidget* SettingsDialog::buildNetworkPage(const AppSettings& current) {
    auto* page = new QWidget{this};
    auto* form = new QFormLayout{page};
    outputCard_ = new QComboBox{page};
    inputCard_ = new QComboBox{page};
    fillCards(outputCard_, current.outputInterface);
    fillCards(inputCard_, current.inputInterface);
    form->addRow(tr("Karte zum Senden:"), outputCard_);
    form->addRow(note(tr("Art-Net, sACN und DDP gehen über diese Karte hinaus. Bei „Automatisch“ entscheidet die "
                         "Routingtabelle des Systems, bei mehreren Karten oft die falsche. Eine Karte auswählen, wenn "
                         "die Lichtanlage an einem zweiten Netzwerkanschluss hängt."),
                     page));
    form->addRow(tr("Karte zum Empfangen:"), inputCard_);
    form->addRow(note(tr("Über diese Karte tritt der sACN-Empfang den Multicast-Gruppen bei. Art-Net-Broadcast und "
                         "Unicast-Pakete kommen weiterhin von jeder Karte an."),
                     page));
    form->addRow(note(tr("Die Änderung wirkt sofort nach „OK“."), page));
    return page;
}

QWidget* SettingsDialog::buildDmxPage(const AppSettings& current) {
    auto* page = new QWidget{this};
    auto* form = new QFormLayout{page};
    artnet_ = new QCheckBox{tr("Art-Net empfangen"), page};
    sacn_ = new QCheckBox{tr("sACN (E1.31) empfangen"), page};
    artnet_->setChecked(current.artnetIn);
    sacn_->setChecked(current.sacnIn);
    artnetPort_ = new QSpinBox{page};
    sacnPort_ = new QSpinBox{page};
    for (QSpinBox* port : {artnetPort_, sacnPort_}) {
        port->setRange(1, 65535);
    }
    artnetPort_->setValue(current.artnetPort > 0 ? current.artnetPort : 6454);
    sacnPort_->setValue(current.sacnPort > 0 ? current.sacnPort : 5568);
    form->addRow(artnet_);
    form->addRow(tr("Art-Net-Port (UDP):"), artnetPort_);
    form->addRow(sacn_);
    form->addRow(tr("sACN-Port (UDP):"), sacnPort_);
    form->addRow(note(tr("Die DMX-Nodes im Projekt lesen ihre Werte aus dem Empfang. Standard-Ports sind 6454 (Art-Net) "
                         "und 5568 (sACN); nur ändern, wenn ein anderes Programm sie belegt. Nach der Installation ist "
                         "der Empfang aus; unter Windows fragt die Firewall beim ersten Einschalten nach."),
                     page));
    return page;
}

QWidget* SettingsDialog::buildAudioPage(const AppSettings& current) {
    auto* page = new QWidget{this};
    auto* box = new QVBoxLayout{page};
    audioOff_ = new QRadioButton{tr("Aus"), page};
    audioDefault_ = new QRadioButton{tr("Standardgerät des Systems"), page};
    audioDevice_ = new QRadioButton{tr("Gerät:"), page};
    audioFile_ = new QRadioButton{tr("WAV-Datei in Schleife:"), page};
    audioDevices_ = new QComboBox{page};
    audioPath_ = new QLineEdit{page};
    auto* browse = new QPushButton{tr("Wählen …"), page};
    connect(browse, &QPushButton::clicked, this, &SettingsDialog::chooseWav);

    const QString wanted = current.audioSource.startsWith(QStringLiteral("device:")) ? current.audioSource.mid(7) : QString{};
    const auto devices = audio::listCaptureDevices();
    if (devices.ok()) {
        for (const audio::AudioDeviceInfo& device : devices.value()) {
            const QString name = toQ(device.name);
            audioDevices_->addItem(device.isDefault ? tr("%1 (Standard)").arg(name) : name, name);
        }
    }
    if (!wanted.isEmpty() && audioDevices_->findData(wanted) < 0) {
        audioDevices_->addItem(tr("%1 (nicht gefunden)").arg(wanted), wanted);
    }
    if (!wanted.isEmpty()) {
        audioDevices_->setCurrentIndex(audioDevices_->findData(wanted));
    }
    audioDevices_->setEnabled(audioDevices_->count() > 0);
    audioDevice_->setEnabled(audioDevices_->count() > 0);

    if (current.audioSource == QStringLiteral("default")) {
        audioDefault_->setChecked(true);
    } else if (!wanted.isEmpty()) {
        audioDevice_->setChecked(true);
    } else if (current.audioSource.startsWith(QStringLiteral("file:"))) {
        audioFile_->setChecked(true);
        audioPath_->setText(current.audioSource.mid(5));
    } else {
        audioOff_->setChecked(true);
    }

    box->addWidget(audioOff_);
    box->addWidget(audioDefault_);
    auto* deviceRow = new QHBoxLayout;
    deviceRow->addWidget(audioDevice_);
    deviceRow->addWidget(audioDevices_, 1);
    box->addLayout(deviceRow);
    auto* fileRow = new QHBoxLayout;
    fileRow->addWidget(audioFile_);
    fileRow->addWidget(audioPath_, 1);
    fileRow->addWidget(browse);
    box->addLayout(fileRow);
    if (!devices.ok()) {
        box->addWidget(note(tr("Aufnahmegeräte lassen sich nicht auflisten: %1").arg(toQ(devices.error().message)), page));
    } else if (devices.value().empty()) {
        box->addWidget(note(tr("Keine Aufnahmegeräte gefunden. Ein Audio-Interface anschließen und das Fenster neu öffnen."), page));
    }
    box->addWidget(note(tr("Die Quelle speist die Audio-Nodes (Pegel, Frequenzband, Beat, Spektrum)."), page));
    box->addStretch(1);
    return page;
}

void SettingsDialog::chooseWav() {
    const QString path = QFileDialog::getOpenFileName(this, tr("WAV-Datei wählen"), QString{}, tr("WAV-Dateien (*.wav)"));
    if (!path.isEmpty()) {
        audioPath_->setText(path);
        audioFile_->setChecked(true);
    }
}

AppSettings SettingsDialog::settings() const {
    AppSettings result;
    result.outputInterface = outputCard_->currentData().toString();
    result.inputInterface = inputCard_->currentData().toString();
    result.artnetIn = artnet_->isChecked();
    result.sacnIn = sacn_->isChecked();
    result.artnetPort = artnetPort_->value();
    result.sacnPort = sacnPort_->value();
    if (audioDefault_->isChecked()) {
        result.audioSource = QStringLiteral("default");
    } else if (audioDevice_->isChecked() && audioDevices_->currentIndex() >= 0) {
        result.audioSource = QStringLiteral("device:") + audioDevices_->currentData().toString();
    } else if (audioFile_->isChecked() && !audioPath_->text().isEmpty()) {
        result.audioSource = QStringLiteral("file:") + audioPath_->text();
    } else {
        result.audioSource = QStringLiteral("off");
    }
    return result;
}

}  // namespace whitehole::studio
