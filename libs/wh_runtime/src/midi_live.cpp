// midi_live.cpp: Umsetzung zu midi_live.hpp.
#include "whitehole/runtime/midi_live.hpp"

#include <algorithm>

#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

constexpr std::uint8_t kChannel = 0;  // der APC mini sendet und empfängt auf Kanal 1
constexpr int kButtonOn = 1;
constexpr int kButtonBlink = 2;

/// Noten, deren LED der Controller verwaltet: Pads und die Tasten am Rand.
bool managed(int note) { return note < midi::LiveProfile::kPads || (note >= 64 && note <= 71) || (note >= 82 && note <= 89) || note == 98; }

}  // namespace

MidiLiveController::MidiLiveController(Player& player, std::shared_ptr<midi::MidiStore> store)
    : player_{player}, store_{std::move(store)}, profile_{midi::builtinLiveProfiles().front()} {
    sent_.fill(-1);
    dmxLast_.fill(-1);
}

void MidiLiveController::setBindings(LiveBindings bindings) {
    bindings_ = std::move(bindings);
    dmxLast_.fill(-1);
}

void MidiLiveController::setOutput(midi::IMidiOutput* output) {
    output_ = output;
    sent_.fill(-1);
}

void MidiLiveController::setProfile(midi::LiveProfile profile) { profile_ = std::move(profile); }

void MidiLiveController::report(const Result<void>& result, MidiLiveResult& out) {
    if (!result.ok()) {
        out.error = result.error().message;
    }
}

void MidiLiveController::syncArmed() {
    const LiveStatus status = player_.liveStatus();
    const int look = status.armed >= 0 ? status.armed : (status.manual ? status.incoming : -1);
    if (look != armedLook_) {
        armedLook_ = look;
        // Mitnahme: Ein neu vorgeladener Look folgt dem Fader erst, wenn dieser unten steht.
        crossfadeReady_ = crossfade_ >= 0 && crossfade_ <= kPickupThreshold;
    }
}

void MidiLiveController::handleCrossfade(int value, MidiLiveResult& result) {
    syncArmed();
    if (value <= kPickupThreshold) {
        crossfadeReady_ = armedLook_ >= 0;
    }
    if (!crossfadeReady_ || armedLook_ < 0) {
        return;
    }
    const double progress = value <= kPickupThreshold ? 0.0 : value / 127.0;
    report(player_.liveManualFade(progress), result);
    if (value >= 127) {
        crossfadeReady_ = false;  // durch: für den nächsten Look erst wieder nach unten
    }
    syncArmed();
}

void MidiLiveController::handle(const midi::MidiMessage& message, const MidiLiveOptions& options, bool live,
                                MidiLiveResult& result) {
    // Steuerfunktionen: Belegung (eigene oder Profil), auch auf anderen Kanälen
    for (const LiveFunction function : kLiveFunctions) {
        const MidiBinding binding = effectiveMidi(bindings_, profile_, function);
        if (binding.number < 0 || binding.channel != message.channel || binding.number != message.data1) {
            continue;
        }
        if (isButton(function) && message.kind == midi::MessageKind::NoteOn) {
            act(function, 127, options, live, result);
            return;
        }
        if (!isButton(function) && message.kind == midi::MessageKind::ControlChange) {
            act(function, message.data2, options, live, result);
            return;
        }
    }
    if (message.channel != kChannel || !live) {
        return;
    }
    if (player_.cuesRunning()) {
        // Cue-Sprung: Bei laufender Cuelist rufen die Pads Cues ab (Raster wie bei den Looks), nicht Looks - die stehen
        // dann im Cue-Ablauf. Loslassen tut nichts (kein Flash).
        if (message.kind == midi::MessageKind::NoteOn) {
            if (const auto cue = profile_.lookForNote(message.data1); cue && *cue < static_cast<int>(player_.cues().size())) {
                report(player_.cueFire(*cue), result);
            }
        }
        return;
    }
    const bool grid = player_.liveGrid().defined();  // mit Raster lösen die Pads Felder aus, sonst (alte Wege) Looks
    if (message.kind == midi::MessageKind::NoteOff) {
        if (grid) {
            if (const int cell = cellForPad(message.data1); cell >= 0) {
                report(player_.liveCellUp(cell), result);  // nur flashende Felder reagieren
            }
        } else if (const auto look = profile_.lookForNote(message.data1); look && *look < static_cast<int>(player_.looks().size())) {
            report(player_.liveUp(*look), result);  // nur Flash-Looks reagieren
        }
        return;
    }
    if (message.kind != midi::MessageKind::NoteOn) {
        return;
    }
    const std::optional<double> fade = fadeFader_ ? fadeFader_ : options.fadeOverride;
    if (grid) {
        if (const int cell = cellForPad(message.data1); cell >= 0) {
            report(player_.liveCellDown(cell, fade, options.useGo), result);  // flashende Felder: Vorrang vor "Mit GO"
        }
        return;
    }
    if (const auto look = profile_.lookForNote(message.data1);
               look && *look < static_cast<int>(player_.looks().size())) {
        report(player_.liveDown(*look, fade, options.useGo), result);  // Flash-Looks: Vorrang vor "Mit GO"

    }
}

void MidiLiveController::act(LiveFunction function, int value, const MidiLiveOptions& options, bool live,
                             MidiLiveResult& result) {
    switch (function) {
        case LiveFunction::FadeTime:
            fadeFader_ = value / 127.0 * kMaxFadeSeconds;  // auch außerhalb des Live-Betriebs merken
            return;
        case LiveFunction::Crossfade:
            if (live) {
                handleCrossfade(value, result);
            }
            crossfade_ = value;
            return;
        case LiveFunction::Master:
            if (live) {
                player_.setMasterBrightness(static_cast<float>(value) / 127.0F);
                result.masterChanged = true;
            }
            return;
        case LiveFunction::Go:
            if (live) {
                if (player_.cuesRunning()) {
                    player_.cueGo();  // die Cuelist läuft: GO löst ihren nächsten Cue aus (wie der GO-Knopf im Fenster)
                } else {
                    report(player_.liveGo(fadeFader_ ? fadeFader_ : options.fadeOverride), result);
                }
            }
            return;
        case LiveFunction::Stop:
            if (live) {
                result.leaveRequested = true;
            }
            return;
    }
}

void MidiLiveController::pollDmx(const MidiLiveOptions& options, bool live, MidiLiveResult& result) {
    if (!dmx_) {
        return;
    }
    for (std::size_t i = 0; i < kLiveFunctionCount; ++i) {
        const LiveFunction function = kLiveFunctions[i];
        const auto& address = bindings_.dmx[i];
        if (!address || !hasDmxBinding(function)) {
            continue;
        }
        const auto sample = dmx_->read(address->protocol, address->universe, address->channel, 1);
        if (!sample || sample->ageSeconds > kDmxTimeoutSeconds) {
            continue;
        }
        const int value = sample->values[0];
        const int before = dmxLast_[i];
        dmxLast_[i] = value;
        if (before < 0 || before == value) {
            continue;  // erster Wert oder keine Änderung
        }
        if (isButton(function)) {
            if (before < 128 && value >= 128) {
                act(function, 127, options, live, result);
            }
        } else {
            act(function, (value * 127 + 127) / 255, options, live, result);
        }
    }
}

MidiLiveResult MidiLiveController::step(const MidiLiveOptions& options) {
    MidiLiveResult result;
    const bool live = options.enabled && player_.liveActive();
    if (live) {
        syncArmed();
    }
    if (store_) {
        for (const midi::MidiMessage& message : store_->takeEvents()) {
            handle(message, options, live && player_.liveActive(), result);
        }
    }
    pollDmx(options, live && player_.liveActive(), result);
    const bool liveNow = options.enabled && player_.liveActive();
    if (liveNow || wasLive_) {
        updateLeds(liveNow);
    }
    wasLive_ = liveNow;
    if (!liveNow) {
        armedLook_ = -1;
    }
    return result;
}

int MidiLiveController::cellForPad(std::uint8_t note) const {
    const auto position = profile_.lookForNote(note);
    if (!position) {
        return -1;
    }
    const project::LiveGrid& grid = player_.liveGrid();
    const int row = *position / profile_.columns;
    const int column = *position % profile_.columns;
    return row < grid.rows && column < grid.columns ? row * grid.columns + column : -1;
}

void MidiLiveController::updateLeds(bool live) {
    std::array<int, 128> wanted{};
    if (live) {
        const bool cueMode = player_.cuesRunning();
        bool goBlinks = false;
        if (cueMode) {
            // Cuelist: ein Pad je Cue. Aktuell grün, im Delay grün blinkend, der nächste gelb blinkend, die übrigen gelb.
            const CueRunnerStatus cues = player_.cueStatus();
            const int count = static_cast<int>(player_.cues().size());
            for (int i = 0; i < count; ++i) {
                const auto note = profile_.noteForLook(i);
                if (!note) {
                    break;
                }
                midi::PadLed led = midi::PadLed::Yellow;
                if (i == cues.pending) {
                    led = midi::PadLed::GreenBlink;
                } else if (i == cues.current) {
                    led = midi::PadLed::Green;
                } else if (i == cues.next) {
                    led = midi::PadLed::YellowBlink;
                }
                wanted[*note] = static_cast<int>(led);
            }
            goBlinks = cues.next >= 0;  // GO löst den nächsten Cue aus: es gibt einen
        } else if (const project::LiveGrid& grid = player_.liveGrid(); grid.defined()) {
            // Raster: ein Pad je Feld (Raster des Profils links oben im Raster); leere Felder bleiben dunkel
            const LiveStatus status = player_.liveStatus();
            for (int row = 0; row < grid.rows; ++row) {
                for (int column = 0; column < std::min(grid.columns, profile_.columns); ++column) {
                    const int cell = row * grid.columns + column;
                    const int look = player_.cellLook(cell);
                    const auto note = profile_.noteForLook(row * profile_.columns + column);
                    if (look < 0 || !note) {
                        continue;
                    }
                    midi::PadLed led = grid.cells[static_cast<std::size_t>(cell)].trigger == project::LookTrigger::Flash
                                           ? midi::PadLed::Red
                                           : midi::PadLed::Yellow;
                    // Kam der Look über ein Feld, leuchtet nur dieses Pad; sonst jedes Pad mit dem Look
                    const auto matches = [cell, look](int statusCell, int statusLook) {
                        return statusCell >= 0 ? statusCell == cell : statusLook >= 0 && look == statusLook;
                    };
                    if ((status.fading && matches(status.incomingCell, status.incoming)) ||
                        matches(status.currentCell, status.current)) {
                        led = midi::PadLed::Green;
                    } else if (matches(status.armedCell, status.armed)) {
                        led = midi::PadLed::YellowBlink;
                    }
                    wanted[*note] = static_cast<int>(led);
                }
            }
            goBlinks = status.armed >= 0;
        } else {
            const LiveStatus status = player_.liveStatus();
            const int looks = static_cast<int>(player_.looks().size());
            for (int i = 0; i < looks; ++i) {
                const auto note = profile_.noteForLook(i);
                if (!note) {
                    break;
                }
                // belegt gelb, Flash-Look rot, vorgeladen gelb blinkend, aktiv (auch einblendend) grün
                midi::PadLed led = player_.looks()[static_cast<std::size_t>(i)].trigger == project::LookTrigger::Flash
                                       ? midi::PadLed::Red
                                       : midi::PadLed::Yellow;
                if ((status.fading && i == status.incoming) || i == status.current) {
                    led = midi::PadLed::Green;
                } else if (i == status.armed) {
                    led = midi::PadLed::YellowBlink;
                }
                wanted[*note] = static_cast<int>(led);
            }
            goBlinks = status.armed >= 0;
        }
        // Tasten-LEDs nur auf Kanal 1 (dort sitzt der Controller); eine Belegung auf anderem Kanal bleibt dunkel
        const MidiBinding go = effectiveMidi(bindings_, profile_, LiveFunction::Go);
        const MidiBinding stop = effectiveMidi(bindings_, profile_, LiveFunction::Stop);
        if (go.channel == kChannel && go.number >= 0) {
            wanted[static_cast<std::size_t>(go.number)] = goBlinks ? kButtonBlink : 0;
        }
        if (stop.channel == kChannel && stop.number >= 0) {
            wanted[static_cast<std::size_t>(stop.number)] = kButtonOn;
        }
    }
    if (output_ == nullptr || !output_->isOpen()) {
        return;
    }
    for (int note = 0; note < 128; ++note) {
        const auto i = static_cast<std::size_t>(note);
        if (!managed(note) || sent_[i] == wanted[i]) {
            continue;
        }
        if (output_->send(midi::noteOn(kChannel, static_cast<std::uint8_t>(note), static_cast<std::uint8_t>(wanted[i]))).ok()) {
            sent_[i] = wanted[i];
        }
    }
}

void MidiLiveController::clear() {
    updateLeds(false);
    wasLive_ = false;
}

}  // namespace whitehole::runtime
