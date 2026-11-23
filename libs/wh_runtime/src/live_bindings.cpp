// live_bindings.cpp: Umsetzung zu live_bindings.hpp.
#include "whitehole/runtime/live_bindings.hpp"

#include <sstream>

namespace whitehole::runtime {

namespace {

constexpr std::array<const char*, kLiveFunctionCount> kNames{"go", "stop", "master", "fade", "crossfade"};

std::optional<LiveFunction> functionNamed(const std::string& name) {
    for (std::size_t i = 0; i < kNames.size(); ++i) {
        if (name == kNames[i]) {
            return kLiveFunctions[i];
        }
    }
    return std::nullopt;
}

}  // namespace

MidiBinding profileBinding(const midi::LiveProfile& profile, LiveFunction function) noexcept {
    const midi::LiveControls& k = profile.controls;
    switch (function) {
        case LiveFunction::Go: return MidiBinding{0, k.goNote};
        case LiveFunction::Stop: return MidiBinding{0, k.stopNote};
        case LiveFunction::Master: return MidiBinding{0, k.masterCc};
        case LiveFunction::FadeTime: return MidiBinding{0, k.fadeCc};
        case LiveFunction::Crossfade: return MidiBinding{0, k.crossfadeCc};
    }
    return MidiBinding{};
}

MidiBinding effectiveMidi(const LiveBindings& bindings, const midi::LiveProfile& profile, LiveFunction function) noexcept {
    const auto& own = bindings.midiOf(function);
    return own ? *own : profileBinding(profile, function);
}

std::vector<std::string> serializeBindings(const LiveBindings& bindings) {
    std::vector<std::string> lines;
    for (std::size_t i = 0; i < kLiveFunctionCount; ++i) {
        if (const auto& m = bindings.midi[i]) {
            lines.push_back(std::string{kNames[i]} + " midi " + std::to_string(m->channel) + ' ' + std::to_string(m->number));
        }
        if (const auto& d = bindings.dmx[i]; d && hasDmxBinding(kLiveFunctions[i])) {
            lines.push_back(std::string{kNames[i]} + " dmx " + (d->protocol == input::DmxProtocol::Sacn ? "sacn " : "artnet ") +
                            std::to_string(d->universe) + ' ' + std::to_string(d->channel));
        }
    }
    return lines;
}

LiveBindings parseBindings(const std::vector<std::string>& lines) {
    LiveBindings bindings;
    for (const std::string& line : lines) {
        std::istringstream in{line};
        std::string name;
        std::string kind;
        in >> name >> kind;
        const auto function = functionNamed(name);
        if (!function) {
            continue;
        }
        if (kind == "midi") {
            int channel = -1;
            int number = -2;
            if ((in >> channel >> number) && in.eof() && channel >= 0 && channel <= 15 && number >= -1 && number <= 127) {
                bindings.midiOf(*function) = MidiBinding{channel, number};
            }
        } else if (kind == "dmx" && hasDmxBinding(*function)) {
            std::string protocol;
            long long universe = -1;
            long long channel = -1;
            if ((in >> protocol >> universe >> channel) && in.eof() && (protocol == "artnet" || protocol == "sacn") &&
                universe >= 0 && universe <= 63999 && channel >= 1 && channel <= 512) {
                bindings.dmxOf(*function) = input::DmxAddress{
                    protocol == "sacn" ? input::DmxProtocol::Sacn : input::DmxProtocol::ArtNet,
                    static_cast<std::uint32_t>(universe), static_cast<std::uint32_t>(channel)};
            }
        }
    }
    return bindings;
}

}  // namespace whitehole::runtime
