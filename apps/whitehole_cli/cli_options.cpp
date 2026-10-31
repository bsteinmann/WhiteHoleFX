// cli_options.cpp: Umsetzung zu cli_options.hpp. Kommandozeile des Runners.
#include "cli_options.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace whitehole::cli {

const char* usageText() {
    return "Aufruf: whitehole_cli <projekt.whfx> [Optionen]\n"
           "\n"
           "  --check          Projekt lesen und die Ausgabe prüfen, nichts senden (braucht keine GPU)\n"
           "  --frames <n>     nach n Frames beenden (sonst läuft das Programm bis Strg+C)\n"
           "  --fps <zahl>     Bildrate überschreiben (sonst die aus dem Projekt)\n"
           "  --software       Software-Adapter für die GPU bevorzugen (z. B. lavapipe)\n"
           "  --snapshot <png> die LEDs des letzten Frames als Bild speichern (ohne --frames: 1 Frame);\n"
           "                   mit --check das Layout, eingefärbt nach Fixture und Kabelrichtung\n"
           "  --dmx-in <quelle> DMX empfangen für die DMX-Nodes: artnet, sacn oder artnet,sacn (sonst aus)\n"
           "  --audio-in <q>   Audio für die Audio-Nodes: default (Standardgerät), Geräte-Nummer oder -Name,\n"
           "                   oder eine .wav-Datei, die in Schleife läuft (sonst aus)\n"
           "  --audio-devices  die Aufnahmegeräte auflisten und beenden\n"
           "  --size <BxH>     Bildgröße der Vorschau, z. B. 1280x720 (Standard 960x540)\n"
           "  --help           diese Hilfe\n";
}

Result<Options> parseOptions(const std::vector<std::string>& args) {
    Options options;
    const auto bad = [](const std::string& message) {
        return Error{ErrorCode::InvalidArgument, message + " (whitehole_cli --help zeigt die Optionen)"};
    };
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        const auto next = [&]() -> const std::string* { return i + 1 < args.size() ? &args[++i] : nullptr; };
        if (a == "--check") {
            options.check = true;
        } else if (a == "--software") {
            options.software = true;
        } else if (a == "--help" || a == "-h") {
            options.help = true;
        } else if (a == "--frames") {
            const std::string* value = next();
            std::uint64_t n = 0;
            if (value == nullptr) {
                return bad("--frames braucht eine Zahl");
            }
            const auto r = std::from_chars(value->data(), value->data() + value->size(), n);
            if (r.ec != std::errc{} || r.ptr != value->data() + value->size() || n == 0) {
                return bad("--frames braucht eine ganze Zahl ab 1, nicht \"" + *value + "\"");
            }
            options.frames = n;
        } else if (a == "--fps") {
            const std::string* value = next();
            double f = 0.0;
            if (value == nullptr) {
                return bad("--fps braucht eine Zahl");
            }
            const auto r = std::from_chars(value->data(), value->data() + value->size(), f);
            if (r.ec != std::errc{} || r.ptr != value->data() + value->size() || !std::isfinite(f) || f <= 0.0 ||
                f > 1000.0) {
                return bad("--fps braucht eine Zahl größer 0 und höchstens 1000, nicht \"" + *value + "\"");
            }
            options.fps = f;
        } else if (a == "--snapshot") {
            const std::string* value = next();
            if (value == nullptr || value->empty()) {
                return bad("--snapshot braucht einen Dateinamen");
            }
            options.snapshotPath = *value;
        } else if (a == "--dmx-in") {
            const std::string* value = next();
            if (value == nullptr) {
                return bad("--dmx-in braucht artnet, sacn oder artnet,sacn");
            }
            std::size_t from = 0;
            while (from <= value->size()) {
                const std::size_t comma = std::min(value->find(',', from), value->size());
                const std::string name = value->substr(from, comma - from);
                if (name == "artnet") {
                    options.dmxArtnet = true;
                } else if (name == "sacn") {
                    options.dmxSacn = true;
                } else {
                    return bad("--dmx-in kennt nur artnet und sacn, nicht \"" + name + "\"");
                }
                from = comma + 1;
            }
        } else if (a == "--audio-in") {
            const std::string* value = next();
            if (value == nullptr || value->empty()) {
                return bad("--audio-in braucht default, ein Gerät oder eine .wav-Datei");
            }
            options.audioIn = *value;
        } else if (a == "--audio-devices") {
            options.audioDevices = true;
        } else if (a == "--size") {
            const std::string* value = next();
            if (value == nullptr) {
                return bad("--size braucht Breite und Höhe, z. B. 1280x720");
            }
            const std::size_t x = value->find('x');
            std::uint32_t w = 0;
            std::uint32_t h = 0;
            bool ok = x != std::string::npos && x > 0 && x + 1 < value->size();
            if (ok) {
                const auto rw = std::from_chars(value->data(), value->data() + x, w);
                const auto rh = std::from_chars(value->data() + x + 1, value->data() + value->size(), h);
                ok = rw.ec == std::errc{} && rw.ptr == value->data() + x && rh.ec == std::errc{} &&
                     rh.ptr == value->data() + value->size() && w >= 16 && h >= 16 && w <= 8192 && h <= 8192;
            }
            if (!ok) {
                return bad("--size braucht Breite x Höhe von 16 bis 8192, nicht \"" + *value + "\"");
            }
            options.snapshotWidth = w;
            options.snapshotHeight = h;
        } else if (!a.empty() && a[0] == '-') {
            return bad("Unbekannte Option " + a);
        } else if (options.projectPath.empty()) {
            options.projectPath = a;
        } else {
            return bad("Zweite Projektdatei \"" + a + "\" angegeben");
        }
    }
    if (!options.help && !options.audioDevices && options.projectPath.empty()) {
        return bad("Es fehlt die Projektdatei");
    }
    if (options.snapshotPath.empty() && (options.snapshotWidth != 960 || options.snapshotHeight != 540)) {
        return bad("--size gehört zu --snapshot");
    }
    return options;
}

}  // namespace whitehole::cli
