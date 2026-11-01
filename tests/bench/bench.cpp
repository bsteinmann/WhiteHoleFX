// bench.cpp: Zeitmessung der CPU-Teile, die bei jedem Bild laufen (Vorschau, Layout, Projekt lesen).
//
// Warum es das gibt: Ob etwas "schnell genug" ist, soll gemessen werden statt geraten. Das Programm wird nur mit
// -DWHITEHOLEFX_BUILD_BENCH=ON gebaut (am besten mit -DCMAKE_BUILD_TYPE=Release) und gibt Millisekunden je Aufruf aus.
// Es prüft nichts; Grenzwerte stehen in den Tests der jeweiligen Bibliothek.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "whitehole/mapping/layout.hpp"
#include "whitehole/preview/preview.hpp"
#include "whitehole/preview/view3d.hpp"

using namespace whitehole;

namespace {

double millis(const std::function<void()>& work, int repeat) {
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < repeat; ++i) {
        work();
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / repeat;
}

mapping::LedMap cube(std::uint32_t n) {
    mapping::Layout layout;
    mapping::GridShape grid;
    grid.nx = grid.ny = grid.nz = n;
    (void)layout.add("Cube", Vec3{0, 0, 0}, grid);
    return layout.bake().value();
}

mapping::LedMap matrix(std::uint32_t w, std::uint32_t h) {
    mapping::Layout layout;
    mapping::MatrixShape shape;
    shape.columns = w;
    shape.rows = h;
    (void)layout.add("Wand", Vec3{0, 0, 0}, shape);
    return layout.bake().value();
}

std::vector<ColorRgba> colors(std::size_t count) {
    std::mt19937 rng{1};
    std::vector<ColorRgba> out(count);
    for (ColorRgba& c : out) {
        const bool on = (rng() % 3U) != 0U;
        c = ColorRgba{on ? static_cast<float>(rng() % 100U) / 100.0F : 0.0F, on ? 0.5F : 0.0F, on ? 0.8F : 0.0F, 1.0F};
    }
    return out;
}

void report(const char* name, double ms) { std::printf("%-52s %9.3f ms\n", name, ms); }

}  // namespace

int main() {
    for (const std::uint32_t n : {8U, 16U, 24U}) {
        const std::string name = std::to_string(n) + "^3 Cube (" + std::to_string(n * n * n) + " LEDs)";
        report(("bake         " + name).c_str(), millis([&] { (void)cube(n); }, 5));
        const mapping::LedMap map = cube(n);
        const std::vector<ColorRgba> c = colors(map.size());
        for (const bool glow : {true, false}) {
            preview::Camera3d camera;
            camera.glow = glow;
            report(("preview3d    " + name + (glow ? ", glow" : ", ohne glow")).c_str(),
                   millis([&] { (void)preview::renderPreview3d(map, c, camera, preview::PreviewOptions{}); }, 5));
        }
    }
    for (const std::uint32_t side : {32U, 64U, 128U}) {
        const mapping::LedMap map = matrix(side, side);
        const std::vector<ColorRgba> c = colors(map.size());
        report(("preview2d    " + std::to_string(side) + "x" + std::to_string(side) + " Matrix").c_str(),
               millis([&] { (void)preview::renderPreview(map, c, preview::PreviewOptions{}); }, 5));
    }
    return 0;
}
