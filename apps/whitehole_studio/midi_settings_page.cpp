// midi_settings_page.cpp: Umsetzung zu midi_settings_page.hpp.
#include "midi_settings_page.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "whitehole/midi/midi_backend.hpp"

namespace whitehole::studio {

namespace {

/// Füllt eine Auswahl: "Aus", dann die vorhandenen Ports. Ein gemerkter Port, den es gerade nicht gibt, bleibt als
/// eigener Eintrag stehen, damit die Wahl beim Ab- und Anstecken nicht stillschweigend verschwindet.
void fillPorts(QComboBox* combo, const std::vector<std::string>& ports, const QString& selected) {
    combo->clear();
    combo->addItem(QObject::tr("Aus"), QString{});
    for (const std::string& port : ports) {
        const QString name = QString::fromUtf8(port.data(), static_cast<qsizetype>(port.size()));
        combo->addItem(name, name);
    }
    int index = combo->findData(selected);
    if (index < 0 && !selected.isEmpty()) {
        combo->addItem(QObject::tr("%1 (nicht gefunden)").arg(selected), selected);
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(index < 0 ? 0 : index);
}

}  // namespace

MidiSettingsPage::MidiSettingsPage(const QString& input, const QString& output, std::shared_ptr<midi::MidiStore> store,
                                   QWidget* parent)
    : QWidget{parent}, store_{std::move(store)} {
    auto* layout = new QVBoxLayout{this};
    auto* intro = new QLabel{tr("Ein- und Ausgang eines MIDI-Controllers, zum Beispiel eines Akai APC mini. "
                                "Die Wahl gilt für diesen Rechner und Auftritt, nicht für das Projekt."),
                             this};
    intro->setWordWrap(true);
    layout->addWidget(intro);
    if (!midi::midiDevicesAvailable()) {
        auto* warning = new QLabel{tr("Dieser Build hat keine MIDI-Geräteunterstützung (unter Linux fehlt die "
                                      "ALSA-Bibliothek beim Bauen). Es gibt keine Ports."),
                                   this};
        warning->setWordWrap(true);
        layout->addWidget(warning);
    }
    auto* form = new QFormLayout;
    input_ = new QComboBox{this};
    output_ = new QComboBox{this};
    form->addRow(tr("MIDI-Eingang"), input_);
    form->addRow(tr("MIDI-Ausgang"), output_);
    layout->addLayout(form);
    auto* rescan = new QPushButton{tr("Ports neu einlesen"), this};
    layout->addWidget(rescan, 0, Qt::AlignLeft);
    last_ = new QLabel{this};
    last_->setTextFormat(Qt::PlainText);
    layout->addWidget(last_);
    layout->addStretch(1);

    fillPorts(input_, midi::makeMidiInput()->ports(), input);
    fillPorts(output_, midi::makeMidiOutput()->ports(), output);
    connect(rescan, &QPushButton::clicked, this, [this]() { refreshPorts(); });
    timer_ = new QTimer{this};
    connect(timer_, &QTimer::timeout, this, [this]() { showLast(); });
    timer_->start(200);
    showLast();
}

QString MidiSettingsPage::input() const { return input_->currentData().toString(); }
QString MidiSettingsPage::output() const { return output_->currentData().toString(); }

void MidiSettingsPage::refreshPorts() {
    fillPorts(input_, midi::makeMidiInput()->ports(), input());
    fillPorts(output_, midi::makeMidiOutput()->ports(), output());
}

void MidiSettingsPage::showLast() {
    const auto last = store_ ? store_->last() : std::nullopt;
    if (!last) {
        last_->setText(tr("Letzte Nachricht: noch keine empfangen"));
        return;
    }
    const midi::MidiMessage& m = last->message;
    QString what;
    if (m.kind == midi::MessageKind::ControlChange) {
        what = tr("Regler %1 = %2").arg(int{m.data1}).arg(int{m.data2});
    } else if (m.kind == midi::MessageKind::NoteOn) {
        what = tr("Note %1 an (Velocity %2)").arg(int{m.data1}).arg(int{m.data2});
    } else {
        what = tr("Note %1 aus").arg(int{m.data1});
    }
    last_->setText(tr("Letzte Nachricht: Kanal %1, %2 (vor %3 s)")
                       .arg(m.channel + 1)
                       .arg(what)
                       .arg(last->ageSeconds, 0, 'f', 1));
}

}  // namespace whitehole::studio
