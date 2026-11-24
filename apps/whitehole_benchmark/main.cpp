// main.cpp: whitehole_benchmark, der Benchmark als eigenes Konsolenprogramm.
//
// Warum es diese Datei gibt: Wer wissen will, was sein Rechner schafft (oder einem Issue eine Messung beilegen soll),
// braucht weder ein Projekt noch das Studio: Doppelklick, warten, Dateien abgeben. Das Programm ist deshalb klein, ohne
// Qt, mit sinnvollen Voreinstellungen (voller Lauf, beide Reihen, Dateien neben dem Aufrufordner) und lässt das Fenster am
// Ende offen, bis Enter gedrückt wird. Gemessen wird in wh_benchmark_run, dieselbe Messung wie bei `whitehole_cli --benchmark`.
#include <atomic>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define WH_ISATTY(fd) _isatty(fd)
#define WH_STDIN_FD 0
#else
#include <unistd.h>
#define WH_ISATTY(fd) isatty(fd)
#define WH_STDIN_FD STDIN_FILENO
#endif

#include "benchmark_args.hpp"
#include "benchmark_files.hpp"
#include "benchmark_job.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/benchmark_report.hpp"

#ifndef WH_VERSION
#define WH_VERSION "unbekannt"
#endif
#ifndef WH_BUILD_ID
#define WH_BUILD_ID "unbekannt"
#endif

using namespace whitehole;

namespace {

std::atomic<bool> gStop{false};

extern "C" void onSignal(int) { gStop.store(true); }

/// Hält das Fenster offen, wenn das Programm per Doppelklick gestartet wurde. Nur an einem Terminal, nie in Skripten.
void waitForEnter(const benchapp::BenchmarkArgs& args) {
    if (args.noWait || !WH_ISATTY(WH_STDIN_FD)) {
        return;
    }
    std::cerr << "\nZum Beenden Enter drücken ...";
    std::string ignored;
    std::getline(std::cin, ignored);
}

int finish(const benchapp::BenchmarkArgs& args, int code) {
    waitForEnter(args);
    return code;
}

std::string timestampStem() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return benchapp::benchmarkFileStem(local);
}

/// Schreibt Markdown und CSV; meldet jeden Fehler, bricht aber nach dem ersten nicht ab. false, wenn etwas fehlschlug.
bool saveReports(const benchapp::BenchmarkResult& result, const benchapp::BenchmarkArgs& args) {
    const std::filesystem::path dir = args.outDir.empty() ? std::filesystem::path{"."} : std::filesystem::path{args.outDir};
    const std::string stem = timestampStem();
    std::error_code createError;
    std::filesystem::create_directories(dir, createError);  // fehlt der Ordner, meldet writeBenchmarkFile den Pfad
    bool allSaved = true;
    for (const char* extension : {".md", ".csv"}) {
        const std::filesystem::path path = dir / (stem + extension);
        if (const Result<void> saved = benchapp::writeBenchmarkFile(result, path.string()); saved.ok()) {
            std::error_code ignored;
            std::cerr << "Ergebnis gespeichert: " << std::filesystem::absolute(path, ignored).string() << "\n";
        } else {
            std::cerr << "Fehler: " << saved.error().message << "\n";
            allSaved = false;
        }
    }
    return allSaved;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // Umlaute in der Konsole
#endif
    const auto args = benchapp::parseBenchmarkArgs(std::vector<std::string>(argv + 1, argv + argc));
    if (!args.ok()) {
        std::cerr << args.error().message << "\n";
        return 2;
    }
    if (args.value().help) {
        std::cout << benchapp::benchmarkUsageText();
        return 0;
    }
    std::cerr << "WhiteHoleFX Benchmark " << WH_VERSION << "\n\n"
              << "Schließe jetzt andere Programme, vor allem WhiteHole Studio, Spiele und Browser mit Videos.\n"
              << "Strg+C bricht ab.\n\n";

    auto library = nodes::BuiltinLibrary::create();
    if (!library.ok()) {
        std::cerr << "Fehler: " << library.error().message << "\n";
        return finish(args.value(), 2);
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    benchapp::BenchmarkJob job;
    job.series = args.value().series;
    job.mode = args.value().quick ? runtime::BenchmarkMode::Quick : runtime::BenchmarkMode::Full;
    job.software = args.value().software;
    const auto result = benchapp::runBenchmarkJob(job, *library.value(), gStop, WH_VERSION, WH_BUILD_ID);
    if (!result.ok()) {
        std::cerr << "Fehler: " << result.error().message << "\n";
        return finish(args.value(), 2);
    }
    std::cout << "\n" << runtime::benchmarkMarkdown(result.value().run, result.value().info, result.value().limits);
    const bool saved = saveReports(result.value(), args.value());
    return finish(args.value(), !saved ? 2 : result.value().run.aborted ? 1 : 0);
}
