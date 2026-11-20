// Quelltext-Prüfung: Jeder Datei-Dialog des Studios (QFileDialog::get...) steht unter einer StallPause und ist der Qt-Dialog
// (DontUseNativeDialog). Warum es das gibt:
// Der Windows-Dialog hat eine eigene Nachrichtenschleife; ohne Pause meldete der Wächter einen falschen "Stillstand"
// (ABSTUERZE_TODO.md Punkt 2, zuletzt beim Fixture-Export/-Import). Das Studio lässt sich lokal nicht bauen, also prüft
// dieser Test den Quelltext. Er liest die Dateien über __FILE__, wie example_projects_test.cpp die Beispiele.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> linesOf(const std::filesystem::path& file) {
    std::ifstream in{file};
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        lines.push_back(line);
    }
    return lines;
}

}  // namespace

TEST_CASE("Studio: jeder QFileDialog::get... steht unter einer StallPause und nutzt den Qt-Dialog", "[studio][source]") {
    const std::filesystem::path dir =
        std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "apps" / "whitehole_studio";
    if (!std::filesystem::exists(dir)) {
        SKIP("Quelltext des Studios nicht vorhanden");
    }
    int found = 0;
    for (const auto& entry : std::filesystem::directory_iterator{dir}) {
        if (entry.path().extension() != ".cpp") {
            continue;
        }
        const std::vector<std::string> lines = linesOf(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].find("QFileDialog::get") == std::string::npos || lines[i].find("#include") != std::string::npos) {
                continue;
            }
            ++found;
            bool paused = false;
            for (std::size_t back = 0; back <= 12 && back <= i; ++back) {
                paused = paused || lines[i - back].find("StallPause") != std::string::npos;
            }
            INFO(entry.path().filename().string() << ":" << (i + 1));
            CHECK(paused);
            // Nie der Windows-Dialog: Seine Erweiterungen von Fremdprogrammen beendeten das Studio ohne Bericht
            // (Datei -> Öffnen, 20.11.). Der Aufruf steht über mehrere Zeilen, also die nächsten 6 mitlesen.
            bool qtDialog = false;
            for (std::size_t ahead = 0; ahead <= 6 && i + ahead < lines.size(); ++ahead) {
                qtDialog = qtDialog || lines[i + ahead].find("DontUseNativeDialog") != std::string::npos;
            }
            CHECK(qtDialog);
        }
    }
    CHECK(found >= 5);  // Projekt öffnen/speichern, WAV, Fixture-Export/-Import: sonst sucht der Test nichts mehr
}
