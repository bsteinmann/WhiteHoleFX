// parser_fuzz_test.cpp: Mutationstest für die Dateileser (Projektdatei, CSV/JSON-Import).
//
// Warum es den Test gibt: Projektdateien und Punktdateien kommen von Nutzern und aus dem Netz. Ein Leser darf bei beliebig
// kaputtem Text einen Fehler melden, aber nie abstürzen, ewig rechnen oder Speicher verschwenden. Der Test nimmt die
// Beispielprojekte, verändert sie tausendfach (Bytes, Zeilen, Zahlen, abgeschnitten, eingestreute Schlüsselwörter) und
// prüft: kein Absturz (unter ASan/UBSan in der CI), und was sich lesen lässt, lässt sich schreiben und wieder lesen, mit
// demselben Ergebnis. Die Zufallszahlen sind fest gesät; ein Fehler ist also reproduzierbar.
#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include "builtin_lab.hpp"
#include "whitehole/mapping/point_import.hpp"
#include "whitehole/project/project.hpp"

using namespace whitehole;
using whitehole::test::Lab;

namespace {

std::vector<std::string> exampleTexts() {
    std::vector<std::string> texts;
    const auto dir = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "examples";
    for (const auto& entry : std::filesystem::recursive_directory_iterator{dir}) {
        if (entry.path().extension() == ".whfx") {
            const auto text = project::readTextFile(entry.path().string());
            if (text.ok()) {
                texts.push_back(text.value());
            }
        }
    }
    return texts;
}

const char* const kTokens[] = {"1e30", "-1", "nan", "inf", "-inf", "99999999", "4294967296", "0", "0.0000001", "end", "endlook",
                               "node", "fixture", "output", "connect", "look", "grid", "matrix", "strip", "point", "patch",
                               "destination", "universe", "->", "\"", "\\", "#", "set", "at", "from", "to", "pitch", "serpentine",
                               "true", "false", "\t", " ", "\r\n", "é", "\xC3"};

std::string mutate(const std::string& original, std::mt19937& rng) {
    std::string text = original;
    const int edits = 1 + static_cast<int>(rng() % 4);
    for (int e = 0; e < edits && !text.empty(); ++e) {
        const std::size_t pos = rng() % text.size();
        switch (rng() % 7) {
            case 0: text[pos] = static_cast<char>(32 + rng() % 95); break;  // Byte ersetzen
            case 1: text.erase(pos, 1 + rng() % 8); break;                   // ein Stück löschen
            case 2: {                                                        // Zeile löschen
                const std::size_t a = text.rfind('\n', pos);
                const std::size_t b = text.find('\n', pos);
                text.erase(a == std::string::npos ? 0 : a, (b == std::string::npos ? text.size() : b) - (a == std::string::npos ? 0 : a));
                break;
            }
            case 3: {  // Zeile verdoppeln
                const std::size_t a = text.rfind('\n', pos);
                const std::size_t b = text.find('\n', pos);
                const std::size_t from = a == std::string::npos ? 0 : a;
                const std::size_t to = b == std::string::npos ? text.size() : b;
                text.insert(to, text.substr(from, to - from));
                break;
            }
            case 4: text.insert(pos, kTokens[rng() % std::size(kTokens)]); break;  // Schlüsselwort oder Extremwert einstreuen
            case 5: text.resize(pos); break;                                         // abschneiden
            default: {                                                               // eine Zahl durch einen Extremwert ersetzen
                std::size_t digit = text.find_first_of("0123456789", pos);
                if (digit != std::string::npos) {
                    std::size_t end = digit;
                    while (end < text.size() && (std::isdigit(static_cast<unsigned char>(text[end])) != 0 || text[end] == '.')) {
                        ++end;
                    }
                    text.replace(digit, end - digit, kTokens[rng() % 11]);
                }
            }
        }
    }
    return text;
}

/// Umgebungsvariable als Zahl (für lange Läufe von Hand: WH_FUZZ_ROUNDS=500000 WH_FUZZ_SEED=3 wh_tests "[fuzz]").
std::uint64_t envNumber(const char* name, std::uint64_t fallback) {
    const char* text = std::getenv(name);
    return text != nullptr ? std::strtoull(text, nullptr, 10) : fallback;
}

}  // namespace

TEST_CASE("Fuzz: Projektdateien mit Mutationen stürzen nie ab und lassen sich rund schreiben", "[fuzz][project]") {
    const std::vector<std::string> seeds = exampleTexts();
    REQUIRE(seeds.size() >= 15);
    Lab lab;
    const std::uint64_t rounds = envNumber("WH_FUZZ_ROUNDS", 2500);
    std::mt19937 rng{static_cast<std::uint32_t>(envNumber("WH_FUZZ_SEED", 20261101))};
    std::size_t parsedOk = 0;
    std::size_t rejected = 0;
    const auto started = std::chrono::steady_clock::now();
    for (std::uint64_t round = 0; round < rounds; ++round) {
        const std::string& seed = seeds[rng() % seeds.size()];
        const std::string text = mutate(seed, rng);
        INFO("Runde " << round << ", Text:\n" << text);
        auto parsed = project::parseProject(text, lab.registry);
        if (!parsed.ok()) {
            ++rejected;
            continue;
        }
        ++parsedOk;
        if (parsed.value().target.has_value()) {
            (void)compiler::compileField(parsed.value().graph, lab.snippets, *parsed.value().target);  // ok oder Fehler, kein Absturz
        }
        const auto written = project::writeProject(parsed.value());
        if (!written.ok()) {
            continue;  // das Format kann etwas nicht ausdrücken (z. B. leerer Nodename): ein Fehler, kein Absturz
        }
        auto again = project::parseProject(written.value(), lab.registry);
        REQUIRE(again.ok());
        const auto rewritten = project::writeProject(again.value());
        REQUIRE(rewritten.ok());
        CHECK(rewritten.value() == written.value());  // Schreiben ist nach einmal Lesen stabil
    }
    // Weder alles abgelehnt noch alles gelesen: die Mutationen treffen wirklich beides.
    if (rounds >= 2500) {
        CHECK(parsedOk > 100);
        CHECK(rejected > 100);
    }
    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    CHECK(seconds < 120.0);  // keine Mutation darf eine Berechnung ewig laufen lassen
}

TEST_CASE("Fuzz: Punktdateien (CSV und JSON) mit Mutationen und Extremfällen", "[fuzz][import]") {
    const std::vector<std::string> seeds = {
        "x,y,z\n1,2,3\n4,5,6\n7,8,9\n",
        "1;2\n3;4\n5;6\n",
        "# Kommentar\n0 0 0\n1 1 1\n",
        "[[0,0,0],[1,2,3],[4,5,6]]",
        "[{\"x\":1,\"y\":2,\"z\":3},{\"x\":4,\"y\":5}]",
        "{\"points\":[[1,2],[3,4]],\"unit\":\"mm\"}",
    };
    std::mt19937 rng{static_cast<std::uint32_t>(envNumber("WH_FUZZ_SEED", 7))};
    for (std::uint64_t round = 0, rounds = envNumber("WH_FUZZ_ROUNDS", 4000); round < rounds; ++round) {
        const std::string text = mutate(seeds[rng() % seeds.size()], rng);
        INFO("Runde " << round << ": " << text);
        (void)mapping::parsePointsCsv(text);
        (void)mapping::parsePointsJson(text);
        (void)mapping::parsePoints(text, ".json");
        (void)mapping::parsePoints(text, ".csv");
    }

    // Tief verschachtelt darf kein Stapelüberlauf werden.
    for (const std::size_t depth : {100U, 10000U, 1000000U}) {
        const std::string deepArray(depth, '[');
        CHECK_FALSE(mapping::parsePointsJson(deepArray).ok());
        std::string deepObject;
        for (std::size_t i = 0; i < depth; ++i) {
            deepObject += "{\"a\":";
        }
        CHECK_FALSE(mapping::parsePointsJson(deepObject).ok());
    }
    // Riesige Zahlen und lange Zeilen
    CHECK_FALSE(mapping::parsePointsCsv(std::string(2000000, '9')).ok());
    (void)mapping::parsePointsCsv("1e999,1e999,1e999\nnan,nan,nan\n");
}
