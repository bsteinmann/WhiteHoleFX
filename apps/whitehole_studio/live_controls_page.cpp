// live_controls_page.cpp: Umsetzung zu live_controls_page.hpp.
#include "live_controls_page.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "live_learn.hpp"
#include "midi_profile_box.hpp"

namespace whitehole::studio {

namespace {

std::size_t at(runtime::LiveFunction function) { return static_cast<std::size_t>(function); }

}  // namespace

LiveControlsPage::LiveControlsPage(const AppSettings& current, std::shared_ptr<midi::MidiStore> midi,
                                   std::shared_ptr<const input::DmxStore> dmx, QWidget* parent)
    : QWidget{parent}, profile_{findLiveProfile(current.midiProfiles, current.midiProfile)} {
    std::vector<std::string> lines;
    for (const QString& line : current.liveBindings) {
        lines.push_back(line.toStdString());
    }
    bindings_ = runtime::parseBindings(lines);
    if (current.masterDmx) {
        master_ = input::DmxAddress{current.masterDmxProtocol == 1 ? input::DmxProtocol::Sacn : input::DmxProtocol::ArtNet,
                                    static_cast<std::uint32_t>(current.masterDmxUniverse),
                                    static_cast<std::uint32_t>(current.masterDmxChannel)};
    }

    auto* layout = new QVBoxLayout{this};
    auto* intro = new QLabel{tr("Wer im Live-Modus GO, Ende, Master und Überblendung steuert. „Lernen“ wartet auf die nächste "
                                "Taste, den nächsten Regler oder einen DMX-Kanal, der sich bewegt. Ohne eigene Belegung gilt "
                                "das MIDI-Profil (Seite MIDI). Dieselbe Belegung lässt sich auch im Live-Fenster lernen."),
                             this};
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto* grid = new QGridLayout;
    grid->addWidget(new QLabel{tr("<b>Funktion</b>"), this}, 0, 0);
    grid->addWidget(new QLabel{tr("<b>MIDI</b>"), this}, 0, 1);
    grid->addWidget(new QLabel{tr("<b>DMX</b>"), this}, 0, 4);
    int row = 1;
    for (const runtime::LiveFunction function : runtime::kLiveFunctions) {
        grid->addWidget(new QLabel{liveFunctionName(function), this}, row, 0);
        midiLabels_[at(function)] = new QLabel{this};
        dmxLabels_[at(function)] = new QLabel{this};
        grid->addWidget(midiLabels_[at(function)], row, 1);
        grid->addWidget(makeLiveLearnButton(function, midi, nullptr,
                                            [this, function](const LearnedSource& found) {
                                                if (found.midi) {
                                                    learnMidi(function, *found.midi);
                                                }
                                            },
                                            this),
                        row, 2);
        auto* reset = new QPushButton{tr("Profil"), this};
        reset->setToolTip(tr("Auf die Belegung des MIDI-Profils zurücksetzen"));
        connect(reset, &QPushButton::clicked, this, [this, function]() { resetMidi(function); });
        grid->addWidget(reset, row, 3);
        grid->addWidget(dmxLabels_[at(function)], row, 4);
        grid->addWidget(makeLiveLearnButton(function, nullptr, dmx,
                                            [this, function](const LearnedSource& found) {
                                                if (found.dmx) {
                                                    learnDmx(function, *found.dmx);
                                                }
                                            },
                                            this),
                        row, 5);
        auto* off = new QPushButton{tr("Aus"), this};
        off->setToolTip(tr("Keinen DMX-Kanal für diese Funktion"));
        connect(off, &QPushButton::clicked, this, [this, function]() { clearDmx(function); });
        grid->addWidget(off, row, 6);
        showRow(function);
        ++row;
    }
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(4, 1);
    layout->addLayout(grid);
    auto* note = new QLabel{tr("Master über DMX ist dieselbe Einstellung wie auf der Seite DMX-Eingang. "
                               "DMX-Tasten lösen aus, wenn der Kanal über 50 % steigt."),
                            this};
    note->setWordWrap(true);
    layout->addWidget(note);

    // Gemeinsame Tempo-Uhr (Node "Tempo"): überlebt Look-Wechsel, alle Looks teilen BPM und Beat
    auto* tempo = new QFormLayout;
    tempoSource_ = new QComboBox{this};
    tempoSource_->addItem(tr("Tap (Taste TAP oder Funktion „Tempo-Tap“)"));
    tempoSource_->addItem(tr("Fester Wert"));
    tempoSource_->addItem(tr("Audio-Beat (jeder erkannte Schlag ist ein Beat)"));
    tempoSource_->addItem(tr("DMX-Kanal (Wert = BPM, 0 = unverändert)"));
    tempoSource_->setCurrentIndex(std::clamp(current.tempoSource, 0, 3));
    tempoSource_->setToolTip(tr("Woher die gemeinsame Tempo-Uhr ihr Tempo nimmt (Node „Tempo“)"));
    tempoBpm_ = new QDoubleSpinBox{this};
    tempoBpm_->setRange(30.0, 300.0);
    tempoBpm_->setDecimals(1);
    tempoBpm_->setSuffix(tr(" BPM"));
    tempoBpm_->setValue(current.tempoFixedBpm);
    tempoDmxProtocol_ = new QComboBox{this};
    tempoDmxProtocol_->addItem(QStringLiteral("Art-Net"));
    tempoDmxProtocol_->addItem(QStringLiteral("sACN"));
    tempoDmxProtocol_->setCurrentIndex(std::clamp(current.tempoDmxProtocol, 0, 1));
    tempoDmxUniverse_ = new QSpinBox{this};
    tempoDmxUniverse_->setRange(0, 63999);
    tempoDmxUniverse_->setValue(current.tempoDmxUniverse);
    tempoDmxChannel_ = new QSpinBox{this};
    tempoDmxChannel_->setRange(1, 512);
    tempoDmxChannel_->setValue(current.tempoDmxChannel);
    const auto enable = [this](int index) {
        tempoBpm_->setEnabled(index == 1);
        for (QWidget* w : std::initializer_list<QWidget*>{tempoDmxProtocol_, tempoDmxUniverse_, tempoDmxChannel_}) {
            w->setEnabled(index == 3);
        }
    };
    enable(tempoSource_->currentIndex());
    connect(tempoSource_, &QComboBox::currentIndexChanged, this, enable);
    auto* dmxRow = new QHBoxLayout;
    dmxRow->addWidget(tempoDmxProtocol_);
    dmxRow->addWidget(new QLabel{tr("Universe"), this});
    dmxRow->addWidget(tempoDmxUniverse_);
    dmxRow->addWidget(new QLabel{tr("Kanal"), this});
    dmxRow->addWidget(tempoDmxChannel_);
    dmxRow->addStretch(1);
    tempo->addRow(tr("Tempo-Uhr:"), tempoSource_);
    tempo->addRow(tr("Fester Wert:"), tempoBpm_);
    tempo->addRow(tr("Tempo über DMX:"), dmxRow);
    layout->addLayout(tempo);
    layout->addStretch(1);
}

int LiveControlsPage::tempoSource() const { return tempoSource_->currentIndex(); }

double LiveControlsPage::tempoFixedBpm() const { return tempoBpm_->value(); }

int LiveControlsPage::tempoDmxProtocol() const { return tempoDmxProtocol_->currentIndex(); }

int LiveControlsPage::tempoDmxUniverse() const { return tempoDmxUniverse_->value(); }

int LiveControlsPage::tempoDmxChannel() const { return tempoDmxChannel_->value(); }

void LiveControlsPage::setMasterDmx(std::optional<input::DmxAddress> address) {
    master_ = address;
    showRow(runtime::LiveFunction::Master);
}

void LiveControlsPage::learnMidi(runtime::LiveFunction function, runtime::MidiBinding binding) {
    bindings_.midiOf(function) = binding;
    showRow(function);
}

void LiveControlsPage::learnDmx(runtime::LiveFunction function, input::DmxAddress address) {
    if (runtime::hasDmxBinding(function)) {
        bindings_.dmxOf(function) = address;
    } else {
        master_ = address;
        Q_EMIT masterDmxChanged(address);
    }
    showRow(function);
}

void LiveControlsPage::resetMidi(runtime::LiveFunction function) {
    bindings_.midiOf(function).reset();
    showRow(function);
}

void LiveControlsPage::clearDmx(runtime::LiveFunction function) {
    if (runtime::hasDmxBinding(function)) {
        bindings_.dmxOf(function).reset();
    } else {
        master_.reset();
        Q_EMIT masterDmxChanged(std::nullopt);
    }
    showRow(function);
}

QString LiveControlsPage::midiText(runtime::LiveFunction function) const {
    const QString text = describeMidi(runtime::effectiveMidi(bindings_, profile_, function), function);
    return bindings_.midiOf(function) ? text : tr("%1 (Profil)").arg(text);
}

QString LiveControlsPage::dmxText(runtime::LiveFunction function) const {
    const std::optional<input::DmxAddress>& address = runtime::hasDmxBinding(function) ? bindings_.dmxOf(function) : master_;
    return address ? describeDmx(*address) : tr("aus");
}

void LiveControlsPage::showRow(runtime::LiveFunction function) {
    if (midiLabels_[at(function)] == nullptr) {
        return;
    }
    midiLabels_[at(function)]->setText(midiText(function));
    dmxLabels_[at(function)]->setText(dmxText(function));
}

}  // namespace whitehole::studio
