// parameter_panel_midi.cpp: Umsetzung zu parameter_panel_midi.hpp.
#include "parameter_panel_midi.hpp"

#include "parameter_editors.hpp"
#include "parameter_panel.hpp"

#include <QGridLayout>
#include <QPushButton>
#include <QTimer>

namespace whitehole::studio {

QWidget* makeMidiLearnButton(std::shared_ptr<midi::MidiLearn> learn, bool isNote, MidiLearnTexts texts,
                             std::function<void(const midi::MidiMessage&)> apply, QWidget* parent) {
    auto* button = new QPushButton{texts.idle, parent};
    button->setCheckable(true);
    button->setToolTip(texts.tooltip);
    auto* timer = new QTimer{button};
    timer->setInterval(100);
    const auto stop = [button, timer, learn, idle = texts.idle]() {
        timer->stop();
        learn->cancel();
        button->setChecked(false);
        button->setText(idle);
    };
    QObject::connect(timer, &QTimer::timeout, button, [learn, apply, stop]() {
        if (const auto message = learn->poll()) {
            apply(*message);
            stop();
        }
    });
    QObject::connect(button, &QPushButton::toggled, button, [learn, isNote, button, timer, stop, waiting = texts.waiting](bool on) {
        if (!on) {
            stop();
            return;
        }
        learn->arm(isNote ? midi::MidiLearn::Want::Note : midi::MidiLearn::Want::Controller);
        button->setText(waiting);
        timer->start();
    });
    return button;
}

// MIDI-Nodes: "Lernen" trägt Kanal und Reglernummer bzw. Note der nächsten Controller-Nachricht ein.
void ParameterPanel::addMidiLearnRow(const runtime::NodeInfo& node, QGridLayout* grid, int& row,
                                     const std::unordered_map<std::string, std::shared_ptr<Editor>>& holders) {
    const bool midiCc = node.typeId == "wh.value.midicc";
    if (midiLearn_ && (midiCc || node.typeId == "wh.value.midinote")) {
        const std::string numberKey = midiCc ? "cc" : "note";
        const NodeId id = node.id;
        auto* learnButton = makeMidiLearnButton(
            midiLearn_, !midiCc,
            MidiLearnTexts{tr("Lernen"), tr("Warte auf MIDI …"),
                           midiCc ? tr("Wartet auf die nächste Reglerbewegung am MIDI-Controller und trägt Kanal und Reglernummer ein.")
                                  : tr("Wartet auf die nächste Taste am MIDI-Controller und trägt Kanal und Note ein.")},
            [this, holders, id, numberKey](const midi::MidiMessage& message) {
                const auto set = [&](const std::string& key, std::int64_t value) {
                    if (const auto found = holders.find(key); found != holders.end() && handler_) {
                        if (const auto stored = handler_(id, key, graph::ParameterValue{value})) {
                            found->second->show(*stored);
                        }
                    }
                };
                set("channel", std::int64_t{message.channel} + 1);
                set(numberKey, std::int64_t{message.data1});
            },
            grid->parentWidget());
        grid->addWidget(learnButton, row++, 1);
    }
}

}  // namespace whitehole::studio
