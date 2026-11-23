// live_profile.cpp: Umsetzung zu live_profile.hpp.
#include "whitehole/midi/live_profile.hpp"

#include <charconv>

namespace whitehole::midi {

namespace {

constexpr int kSceneTop = 82;  // oberste Seitentaste
constexpr int kFader1 = 48;
constexpr int kFader9 = 56;

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t found = text.find(separator, start);
        parts.push_back(text.substr(start, found == std::string_view::npos ? std::string_view::npos : found - start));
        if (found == std::string_view::npos) {
            return parts;
        }
        start = found + 1;
    }
}

std::optional<int> number(std::string_view text, int low, int high) {
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < low || value > high) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

std::optional<std::uint8_t> LiveProfile::noteForLook(int index) const noexcept {
    if (columns < 1 || columns > 8 || index < 0 || index >= capacity()) {
        return std::nullopt;
    }
    const int rowFromTop = index / columns;
    const int column = index % columns;
    return static_cast<std::uint8_t>((kRows - 1 - rowFromTop) * 8 + column);
}

std::optional<int> LiveProfile::lookForNote(std::uint8_t note) const noexcept {
    if (columns < 1 || columns > 8 || note >= kPads) {
        return std::nullopt;
    }
    const int column = note % 8;
    if (column >= columns) {
        return std::nullopt;
    }
    const int rowFromTop = kRows - 1 - note / 8;
    return rowFromTop * columns + column;
}

std::vector<LiveProfile> builtinLiveProfiles() {
    LiveProfile withGo;
    withGo.name = "Looks 4 x 8 mit GO und Fadern";
    withGo.columns = 4;
    withGo.controls = LiveControls{kSceneTop, kSceneTop + 1, kFader9, kFader1, kFader1 + 1};
    withGo.builtin = true;

    LiveProfile full;
    full.name = "Looks 8 x 8 ohne Steuertasten";
    full.columns = 8;
    full.controls = LiveControls{-1, -1, kFader9, kFader1, kFader1 + 1};
    full.builtin = true;
    return {withGo, full};
}

std::string serializeProfile(const LiveProfile& profile) {
    std::string name = profile.name;
    for (char& c : name) {
        if (c == '|' || c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    const LiveControls& k = profile.controls;
    return name + "|apcmini1|" + std::to_string(profile.columns) + '|' + std::to_string(k.goNote) + '|' +
           std::to_string(k.stopNote) + '|' + std::to_string(k.masterCc) + '|' + std::to_string(k.fadeCc) + '|' +
           std::to_string(k.crossfadeCc);
}

std::optional<LiveProfile> parseProfile(std::string_view line) {
    const std::vector<std::string_view> parts = split(line, '|');
    if (parts.size() != 8 || parts[0].empty() || parts[1] != "apcmini1") {
        return std::nullopt;
    }
    const auto columns = number(parts[2], 1, 8);
    std::array<std::optional<int>, 5> controls{};
    for (std::size_t i = 0; i < controls.size(); ++i) {
        controls[i] = number(parts[3 + i], -1, 127);
    }
    if (!columns) {
        return std::nullopt;
    }
    for (const auto& value : controls) {
        if (!value) {
            return std::nullopt;
        }
    }
    LiveProfile profile;
    profile.name = std::string{parts[0]};
    profile.columns = *columns;
    profile.controls = LiveControls{*controls[0], *controls[1], *controls[2], *controls[3], *controls[4]};
    return profile;
}

}  // namespace whitehole::midi
