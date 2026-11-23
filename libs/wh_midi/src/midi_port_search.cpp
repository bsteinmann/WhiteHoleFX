// midi_port_search.cpp: Port über seinen gemerkten Namen finden (siehe findMidiPort in midi_backend.hpp).
#include <cctype>

#include "whitehole/midi/midi_backend.hpp"

namespace whitehole::midi {

namespace {

// Schneidet eine angehängte Portnummer wie " 20:0" ab (Linux/ALSA nummeriert Client und Port).
std::string_view withoutPortSuffix(std::string_view name) {
    const std::size_t colon = name.rfind(':');
    if (colon == std::string_view::npos || colon + 1 >= name.size()) {
        return name;
    }
    for (std::size_t i = colon + 1; i < name.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(name[i])) == 0) {
            return name;
        }
    }
    std::size_t start = colon;
    while (start > 0 && std::isdigit(static_cast<unsigned char>(name[start - 1])) != 0) {
        --start;
    }
    if (start == colon || start == 0 || name[start - 1] != ' ') {
        return name;
    }
    return name.substr(0, start - 1);
}

}  // namespace

int findMidiPort(const std::vector<std::string>& ports, std::string_view wanted) {
    if (wanted.empty()) {
        return -1;
    }
    for (std::size_t i = 0; i < ports.size(); ++i) {
        if (ports[i] == wanted) {
            return static_cast<int>(i);
        }
    }
    const std::string_view bare = withoutPortSuffix(wanted);
    for (std::size_t i = 0; i < ports.size(); ++i) {
        if (withoutPortSuffix(ports[i]) == bare) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace whitehole::midi
