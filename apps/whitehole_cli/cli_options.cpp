#include "cli_options.hpp"

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
        } else if (!a.empty() && a[0] == '-') {
            return bad("Unbekannte Option " + a);
        } else if (options.projectPath.empty()) {
            options.projectPath = a;
        } else {
            return bad("Zweite Projektdatei \"" + a + "\" angegeben");
        }
    }
    if (!options.help && options.projectPath.empty()) {
        return bad("Es fehlt die Projektdatei");
    }
    return options;
}

}  // namespace whitehole::cli
