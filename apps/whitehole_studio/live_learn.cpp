// live_learn.cpp: Umsetzung zu live_learn.hpp.
#include "live_learn.hpp"

#include <QObject>
#include <QTimer>

#include <array>
#include <cstddef>
#include <utility>

namespace whitehole::studio {

namespace {

constexpr std::array<const char*, runtime::kLiveFunctionCount> kNames{
    QT_TR_NOOP("GO"), QT_TR_NOOP("Live beenden"), QT_TR_NOOP("Master-Helligkeit"), QT_TR_NOOP("Überblendzeit (0–10 s)"),
    QT_TR_NOOP("Manuelle Überblendung"), QT_TR_NOOP("Tempo-Tap")};

int gWaiting = 0;  // Zahl der wartenden Lernen-Knöpfe (nur Hauptthread)

}  // namespace

bool liveLearnActive() noexcept { return gWaiting > 0; }

QString liveFunctionName(runtime::LiveFunction function) {
    return QObject::tr(kNames[static_cast<std::size_t>(function)]);
}

QString describeMidi(const runtime::MidiBinding& binding, runtime::LiveFunction function) {
    if (binding.number < 0) {
        return QObject::tr("aus");
    }
    return (runtime::isButton(function) ? QObject::tr("Note %1, Kanal %2") : QObject::tr("Regler %1, Kanal %2"))
        .arg(binding.number)
        .arg(binding.channel + 1);
}

QString describeDmx(const input::DmxAddress& address) {
    return QObject::tr("%1 Universe %2, Kanal %3")
        .arg(address.protocol == input::DmxProtocol::Sacn ? QStringLiteral("sACN") : QStringLiteral("Art-Net"))
        .arg(address.universe)
        .arg(address.channel);
}

QPushButton* makeLiveLearnButton(runtime::LiveFunction function, std::shared_ptr<midi::MidiStore> midiStore,
                                 std::shared_ptr<const input::DmxStore> dmxStore, std::function<void(const LearnedSource&)> done,
                                 QWidget* parent) {
    const auto midiLearn = midiStore ? std::make_shared<midi::MidiLearn>(std::move(midiStore)) : nullptr;
    const auto dmxLearn = dmxStore ? std::make_shared<input::DmxLearn>(std::move(dmxStore)) : nullptr;
    const QString idle = QObject::tr("Lernen");
    auto* button = new QPushButton{idle, parent};
    button->setCheckable(true);
    button->setEnabled(midiLearn != nullptr || dmxLearn != nullptr);
    button->setToolTip(runtime::isButton(function)
                           ? QObject::tr("Wartet auf die nächste Taste am MIDI-Controller oder einen DMX-Kanal, der sich bewegt.")
                           : QObject::tr("Wartet auf den nächsten Regler am MIDI-Controller oder einen DMX-Kanal, der sich bewegt."));
    auto* timer = new QTimer{button};
    timer->setInterval(100);
    const auto waiting = std::make_shared<bool>(false);
    QObject::connect(button, &QObject::destroyed, [waiting]() {
        if (*waiting) {
            --gWaiting;  // Knopf verschwindet mitten im Lernen (Fenster geschlossen)
        }
    });
    const auto stop = [button, timer, midiLearn, dmxLearn, idle, waiting]() {
        if (*waiting) {
            *waiting = false;
            --gWaiting;
        }
        timer->stop();
        if (midiLearn) {
            midiLearn->cancel();
        }
        if (dmxLearn) {
            dmxLearn->cancel();
        }
        button->setChecked(false);
        button->setText(idle);
    };
    QObject::connect(timer, &QTimer::timeout, button, [midiLearn, dmxLearn, done, stop]() {
        LearnedSource found;
        if (midiLearn) {
            if (const auto message = midiLearn->poll()) {
                found.midi = runtime::MidiBinding{message->channel, message->data1};
            }
        }
        if (!found.midi && dmxLearn) {
            found.dmx = dmxLearn->poll();
        }
        if (found.midi || found.dmx) {
            done(found);
            stop();
        }
    });
    QObject::connect(button, &QPushButton::toggled, button, [function, midiLearn, dmxLearn, button, timer, stop, waiting](bool on) {
        if (!on) {
            stop();
            return;
        }
        if (midiLearn) {
            midiLearn->arm(runtime::isButton(function) ? midi::MidiLearn::Want::Note : midi::MidiLearn::Want::Controller);
        }
        if (dmxLearn) {
            dmxLearn->arm();
        }
        button->setText(QObject::tr("Warte …"));
        if (!*waiting) {
            *waiting = true;
            ++gWaiting;
        }
        timer->start();
    });
    return button;
}

}  // namespace whitehole::studio
