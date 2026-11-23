// midi_live.cpp: Umsetzung zu midi_live.hpp.
#include "whitehole/runtime/midi_live.hpp"

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
    if (message.channel != kChannel) {
        return;
    }
    const midi::LiveControls& k = profile_.controls;
    if (message.kind == midi::MessageKind::ControlChange) {
        const int cc = message.data1;
        const int value = message.data2;
        if (cc == k.fadeCc) {
            fadeFader_ = value / 127.0 * kMaxFadeSeconds;
        } else if (cc == k.masterCc && live) {
            player_.setMasterBrightness(static_cast<float>(value) / 127.0F);
            result.masterChanged = true;
        } else if (cc == k.crossfadeCc) {
            if (live) {
                handleCrossfade(value, result);
            }
            crossfade_ = value;
        }
        return;
    }
    if (message.kind != midi::MessageKind::NoteOn || !live) {
        return;
    }
    const int note = message.data1;
    const std::optional<double> fade = fadeFader_ ? fadeFader_ : options.fadeOverride;
    if (note == k.goNote) {
        report(player_.liveGo(fade), result);
    } else if (note == k.stopNote) {
        result.leaveRequested = true;
    } else if (const auto look = profile_.lookForNote(message.data1);
               look && *look < static_cast<int>(player_.looks().size())) {
        report(options.useGo ? player_.liveArm(*look) : player_.liveTrigger(*look, fade), result);
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

void MidiLiveController::updateLeds(bool live) {
    std::array<int, 128> wanted{};
    if (live) {
        const LiveStatus status = player_.liveStatus();
        const int looks = static_cast<int>(player_.looks().size());
        for (int i = 0; i < looks; ++i) {
            const auto note = profile_.noteForLook(i);
            if (!note) {
                break;
            }
            midi::PadLed led = midi::PadLed::Off;
            if (status.fading && i == status.incoming) {
                led = midi::PadLed::Yellow;
            } else if (i == status.armed) {
                led = midi::PadLed::YellowBlink;
            } else if (i == status.current) {
                led = midi::PadLed::Green;
            }
            wanted[*note] = static_cast<int>(led);
        }
        const midi::LiveControls& k = profile_.controls;
        if (k.goNote >= 0 && k.goNote < 128) {
            wanted[static_cast<std::size_t>(k.goNote)] = status.armed >= 0 ? kButtonBlink : 0;
        }
        if (k.stopNote >= 0 && k.stopNote < 128) {
            wanted[static_cast<std::size_t>(k.stopNote)] = kButtonOn;
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
