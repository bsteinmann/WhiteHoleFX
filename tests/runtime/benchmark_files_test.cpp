// benchmark_files_test.cpp: Dateinamen und Berichtsdateien des Benchmarks (apps/benchmark_run/benchmark_files.*).
//
// Warum es die Datei gibt: Die Berichte gehen als Dateianhang in Issues anderer Nutzer. Dort müssen Name, Endung und
// Inhalt stimmen, und ein nicht beschreibbarer Ordner muss als Fehler mit dem Pfad ankommen, nicht als leere Datei.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "benchmark_files.hpp"

using namespace whitehole;
using namespace whitehole::benchapp;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

namespace {

BenchmarkResult sample() {
    BenchmarkResult result;
    runtime::BenchmarkStageResult r;
    r.stage = runtime::BenchmarkStage{runtime::BenchmarkKind::Leds, runtime::BenchmarkScene::Plasma, 0, 0, 1'000};
    r.outcome = runtime::BenchmarkOutcome::Ok;
    r.stats.frames = 100;
    r.stats.medianMs = 2.0;
    r.stats.p95Ms = 3.0;
    r.stats.maxMs = 4.0;
    r.stats.cpuMedianMs = 0.5;
    result.run.stages.push_back(r);
    result.info = runtime::BenchmarkSystemInfo{"Testkarte", "Vulkan", "1.2.3", "1.0.0", "commit-abc"};
    return result;
}

std::string readAll(const std::filesystem::path& path) {
    std::ifstream in{path, std::ios::binary};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::filesystem::path freshDir(const char* name) {
    const auto dir = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

}  // namespace

TEST_CASE("Benchmark-Dateien: Dateiname aus Datum und Uhrzeit", "[benchmark]") {
    std::tm t{};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 10;  // November
    t.tm_mday = 24;
    t.tm_hour = 18;
    t.tm_min = 41;
    CHECK(benchmarkFileStem(t) == "whitehole-benchmark-20261124-1841");
    t.tm_mon = 0;
    t.tm_mday = 5;
    t.tm_hour = 3;
    t.tm_min = 7;
    CHECK(benchmarkFileStem(t) == "whitehole-benchmark-20260105-0307");  // immer zweistellig, sortiert nach Name = nach Zeit
}

TEST_CASE("Benchmark-Dateien: CSV nur bei der Endung .csv", "[benchmark]") {
    CHECK(hasCsvExtension("bericht.csv"));
    CHECK(hasCsvExtension("ordner/bericht.csv"));
    CHECK_FALSE(hasCsvExtension("bericht.md"));
    CHECK_FALSE(hasCsvExtension("bericht.CSV.txt"));
    CHECK_FALSE(hasCsvExtension(".csv"));  // nur die Endung, kein Dateiname
    CHECK_FALSE(hasCsvExtension(""));
}

TEST_CASE("Benchmark-Dateien: Markdown und CSV landen in der Datei", "[benchmark]") {
    const auto dir = freshDir("wh_benchmark_files_test");
    const BenchmarkResult result = sample();

    const auto md = (dir / "bericht.md").string();
    REQUIRE(writeBenchmarkFile(result, md).ok());
    const std::string mdText = readAll(md);
    CHECK_THAT(mdText, ContainsSubstring("Testkarte"));
    CHECK_THAT(mdText, ContainsSubstring("|"));  // Tabelle

    const auto csv = (dir / "bericht.csv").string();
    REQUIRE(writeBenchmarkFile(result, csv).ok());
    const std::string csvText = readAll(csv);
    CHECK_THAT(csvText, StartsWith("#"));
    CHECK_THAT(csvText, ContainsSubstring("kind,stage,scene,points,outcome"));
    CHECK_THAT(csvText, ContainsSubstring("Testkarte"));
    std::filesystem::remove_all(dir);
}

TEST_CASE("Benchmark-Dateien: nicht beschreibbarer Pfad ist ein Fehler mit dem Pfad", "[benchmark]") {
    const auto dir = freshDir("wh_benchmark_files_test_fehler");
    const std::string path = (dir / "gibt-es-nicht" / "bericht.md").string();
    const auto saved = writeBenchmarkFile(sample(), path);
    REQUIRE_FALSE(saved.ok());
    CHECK(saved.error().code == ErrorCode::IoError);
    CHECK_THAT(saved.error().message, ContainsSubstring("bericht.md"));
    std::filesystem::remove_all(dir);
}
