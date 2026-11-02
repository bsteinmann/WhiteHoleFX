// project_cue.cpp: Cue-Zeilen der Projektdatei (siehe project_cue.hpp).
#include "project_cue.hpp"

#include <algorithm>
#include <string>

namespace whitehole::project::detail {

namespace {

double seconds(Reader& r, std::string_view what) {
    const double value = r.numberD(what);
    if (r.ok() && !(value >= 0.0 && value <= kMaxCueSeconds)) {
        r.fail(std::string{what} + " muss zwischen 0 und " + std::to_string(static_cast<int>(kMaxCueSeconds)) +
               " Sekunden liegen");
    }
    return value;
}

void parseTrigger(Reader& r, Cue& cue) {
    const std::string kind = r.word("Auslöser");
    if (!r.ok()) {
        return;
    }
    if (kind == "go") {
        cue.trigger = CueTrigger::Go;
    } else if (kind == "time") {
        cue.trigger = CueTrigger::Time;
        cue.time = seconds(r, "Wartezeit");
    } else if (kind == "key") {
        cue.trigger = CueTrigger::Key;
        cue.key = r.word("Taste");
        if (r.ok() && cue.key.empty()) {
            r.fail("Die Taste darf nicht leer sein");
        }
    } else if (kind == "dmx") {
        cue.trigger = CueTrigger::Dmx;
        const std::string protocol = r.word("Protokoll (artnet oder sacn)");
        if (r.ok() && protocol != "artnet" && protocol != "sacn") {
            r.fail("Protokoll muss artnet oder sacn sein, nicht \"" + protocol + "\"");
        }
        cue.dmxProtocol = protocol == "sacn" ? CueDmxProtocol::Sacn : CueDmxProtocol::ArtNet;
        cue.dmxUniverse = r.whole("Universe", 32767);
        cue.dmxChannel = r.whole("Kanal", 512);
        if (r.ok() && cue.dmxChannel < 1) {
            r.fail("Der Kanal liegt zwischen 1 und 512");
        }
        cue.dmxThreshold = 128;  // "threshold <n>" weiter hinten in der Zeile ändert es
    } else if (kind == "audio") {
        cue.trigger = CueTrigger::Audio;
    } else {
        r.fail("Unbekannter Auslöser \"" + kind + "\" (go, time, key, dmx oder audio)");
    }
}

}  // namespace

Cue parseCue(Reader& r) {
    Cue cue;
    while (r.ok() && !r.atEnd()) {
        const std::string key = r.word("Einstellung");
        if (!r.ok()) {
            break;
        }
        if (key == "name") {
            cue.name = r.word("Name");
        } else if (key == "look") {
            cue.look = r.word("Lookname");
        } else if (key == "trigger") {
            parseTrigger(r, cue);
        } else if (key == "threshold") {
            const std::uint32_t value = r.whole("Schwelle", 255);
            if (r.ok() && value < 1) {
                r.fail("Die Schwelle liegt zwischen 1 und 255");
            }
            cue.dmxThreshold = value;
        } else if (key == "delay") {
            cue.delay = seconds(r, "delay");
        } else if (key == "fade") {
            cue.fade = seconds(r, "fade");
        } else {
            r.fail("Unbekannte Einstellung \"" + key + "\" in der cue-Zeile");
        }
    }
    return cue;
}

void writeCue(std::ostream& out, const Cue& cue) {
    out << "cue";
    if (!cue.name.empty()) {
        out << " name " << quoteWord(cue.name);
    }
    if (!cue.look.empty()) {
        out << " look " << quoteWord(cue.look);
    }
    switch (cue.trigger) {
        case CueTrigger::Go: out << " trigger go"; break;
        case CueTrigger::Time: out << " trigger time " << formatDouble(cue.time); break;
        case CueTrigger::Key: out << " trigger key " << quoteWord(cue.key); break;
        case CueTrigger::Dmx:
            out << " trigger dmx " << (cue.dmxProtocol == CueDmxProtocol::Sacn ? "sacn" : "artnet") << ' '
                << cue.dmxUniverse << ' ' << cue.dmxChannel << " threshold " << cue.dmxThreshold;
            break;
        case CueTrigger::Audio: out << " trigger audio"; break;
    }
    out << " delay " << formatDouble(cue.delay) << " fade " << formatDouble(cue.fade) << "\n";
}

Result<void> checkCues(const std::vector<Cue>& cues, const std::vector<Look>& looks) {
    for (std::size_t i = 0; i < cues.size(); ++i) {
        const Cue& cue = cues[i];
        if (cue.look.empty()) {
            continue;
        }
        const bool found = std::any_of(looks.begin(), looks.end(), [&](const Look& l) { return l.name == cue.look; });
        if (!found) {
            return Error{ErrorCode::NotFound,
                         "Cue " + std::to_string(i + 1) + ": Der Look \"" + cue.look + "\" ist nicht in der Datei"};
        }
    }
    return {};
}

}  // namespace whitehole::project::detail
