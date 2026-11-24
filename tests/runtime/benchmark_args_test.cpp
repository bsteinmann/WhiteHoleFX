// benchmark_args_test.cpp: Kommandozeile von whitehole_benchmark (apps/whitehole_benchmark/benchmark_args.*).
//
// Warum es die Datei gibt: Der Doppelklick ohne Argumente muss den vollen Lauf beider Reihen ergeben, und eine
// vertippte Option darf nicht still ignoriert werden, sonst misst jemand 6 Minuten das Falsche.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "benchmark_args.hpp"

using namespace whitehole;
using namespace whitehole::benchapp;
using Catch::Matchers::ContainsSubstring;
using runtime::BenchmarkSeries;

TEST_CASE("whitehole_benchmark: ohne Argumente voller Lauf beider Reihen", "[benchmark]") {
    const auto args = parseBenchmarkArgs({});
    REQUIRE(args.ok());
    CHECK(args.value().series == BenchmarkSeries::Both);
    CHECK_FALSE(args.value().quick);
    CHECK_FALSE(args.value().software);
    CHECK_FALSE(args.value().noWait);
    CHECK_FALSE(args.value().help);
    CHECK(args.value().outDir.empty());
}

TEST_CASE("whitehole_benchmark: alle Optionen", "[benchmark]") {
    const auto args = parseBenchmarkArgs(
        {"--quick", "--software", "--no-wait", "--series", "leds", "--out-dir", "C:\\Messungen"});
    REQUIRE(args.ok());
    CHECK(args.value().quick);
    CHECK(args.value().software);
    CHECK(args.value().noWait);
    CHECK(args.value().series == BenchmarkSeries::Leds);
    CHECK(args.value().outDir == "C:\\Messungen");

    CHECK(parseBenchmarkArgs({"--series", "projection"}).value().series == BenchmarkSeries::Projection);
    CHECK(parseBenchmarkArgs({"--series", "both"}).value().series == BenchmarkSeries::Both);
    CHECK(parseBenchmarkArgs({"--help"}).value().help);
    CHECK(parseBenchmarkArgs({"-h"}).value().help);
}

TEST_CASE("whitehole_benchmark: Fehler nennen die Option und verweisen auf die Hilfe", "[benchmark]") {
    const auto unknown = parseBenchmarkArgs({"--schnell"});
    REQUIRE_FALSE(unknown.ok());
    CHECK(unknown.error().code == ErrorCode::InvalidArgument);
    CHECK_THAT(unknown.error().message, ContainsSubstring("--schnell"));
    CHECK_THAT(unknown.error().message, ContainsSubstring("--help"));

    const auto badSeries = parseBenchmarkArgs({"--series", "alles"});
    REQUIRE_FALSE(badSeries.ok());
    CHECK_THAT(badSeries.error().message, ContainsSubstring("alles"));

    const auto missing = parseBenchmarkArgs({"--out-dir"});
    REQUIRE_FALSE(missing.ok());
    CHECK_THAT(missing.error().message, ContainsSubstring("--out-dir"));
    CHECK_FALSE(parseBenchmarkArgs({"--series"}).ok());
}

TEST_CASE("whitehole_benchmark: die Hilfe nennt jede Option", "[benchmark]") {
    const std::string usage = benchmarkUsageText();
    for (const char* option : {"--quick", "--series", "--software", "--out-dir", "--no-wait", "--help"}) {
        CHECK_THAT(usage, ContainsSubstring(option));
    }
}
