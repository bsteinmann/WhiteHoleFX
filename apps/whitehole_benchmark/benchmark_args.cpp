// benchmark_args.cpp: siehe benchmark_args.hpp.
#include "benchmark_args.hpp"

namespace whitehole::benchapp {

namespace {

Error usageError(const std::string& message) {
    return Error{ErrorCode::InvalidArgument, message + " (whitehole_benchmark --help zeigt die Optionen)"};
}

}  // namespace

Result<BenchmarkArgs> parseBenchmarkArgs(const std::vector<std::string>& args) {
    BenchmarkArgs out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--help" || arg == "-h") {
            out.help = true;
        } else if (arg == "--quick") {
            out.quick = true;
        } else if (arg == "--software") {
            out.software = true;
        } else if (arg == "--no-wait") {
            out.noWait = true;
        } else if (arg == "--series" || arg == "--out-dir") {
            if (i + 1 >= args.size()) {
                return usageError(arg + " braucht einen Wert");
            }
            const std::string& value = args[++i];
            if (arg == "--out-dir") {
                out.outDir = value;
            } else if (value == "projection") {
                out.series = runtime::BenchmarkSeries::Projection;
            } else if (value == "leds") {
                out.series = runtime::BenchmarkSeries::Leds;
            } else if (value == "both") {
                out.series = runtime::BenchmarkSeries::Both;
            } else {
                return usageError("--series kennt projection, leds oder both, nicht '" + value + "'");
            }
        } else {
            return usageError("Unbekannte Option '" + arg + "'");
        }
    }
    return out;
}

const char* benchmarkUsageText() {
    return "Aufruf: whitehole_benchmark [Optionen]\n"
           "\n"
           "Misst, wie viele LEDs und welche Projektionsgröße dieser Rechner mit 30 Bildern pro Sekunde schafft.\n"
           "Ohne Optionen läuft der volle Test (beide Reihen, etwa 6 Minuten). Das Ergebnis steht danach als\n"
           "whitehole-benchmark-<Datum>-<Uhrzeit>.md und .csv im aktuellen Ordner.\n"
           "\n"
           "  --quick          Schnelltest: wenige Stufen, etwa eine halbe Minute\n"
           "  --series <reihe> nur projection (Projektionsgröße), nur leds (LED-Zahl) oder both (Standard)\n"
           "  --software       Software-Adapter statt der Grafikkarte messen (z. B. lavapipe)\n"
           "  --out-dir <ord>  Ordner für die Ergebnisdateien (sonst der aktuelle)\n"
           "  --no-wait        am Ende nicht auf Enter warten (für Skripte)\n"
           "  -h, --help       diese Hilfe\n"
           "\n"
           "Strg+C bricht ab; was bis dahin gemessen wurde, steht trotzdem im Ergebnis.\n"
           "Schließe vorher andere Programme, vor allem WhiteHole Studio, Spiele und Browser mit Videos:\n"
           "Sie nehmen der Grafikkarte Zeit weg und verfälschen die Messung.\n";
}

}  // namespace whitehole::benchapp
