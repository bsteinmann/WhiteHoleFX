// node_render_support.hpp: gemeinsame Hilfen der Node-Tests auf einem echten Gerät.
//
// Warum es diese Datei gibt: Die Katalog-Tests sind auf mehrere Dateien verteilt (Grenze von etwa
// 400 Zeilen je Datei); alle rendern ein Feld auf eine kleine Leinwand und prüfen einzelne Pixel.
#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "builtin_lab.hpp"
#include "gpu_stage_support.hpp"

namespace whitehole::gputest::nodes {

using namespace whitehole;
using namespace whitehole::gpu;
using whitehole::test::Lab;


/// Das gerenderte Bild: `at(x, y)` liefert RGBA, y zählt von oben (Zeile 0 = oben).
struct Pixels {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<float> data;

    [[nodiscard]] std::array<float, 4> at(std::uint32_t x, std::uint32_t y) const {
        const std::size_t i = (std::size_t{y} * width + x) * 4;
        return {data[i], data[i + 1], data[i + 2], data[i + 3]};
    }
};

inline Pixels render(WebGpuDevice& device, Lab& lab, NodeId target, std::uint32_t width, std::uint32_t height,
             double seconds = 0.0) {
    executor::CanvasStageConfig config;
    config.extent = Extent3{width, height, 1};
    config.format = TextureFormat::Rgba32Float;
    config.target = graph::PortRef{target, "out"};
    auto created = executor::CanvasRenderStage::create(device, lab.snippets, lab.kernels, config);
    REQUIRE(created.ok());
    REQUIRE(created.value()->setGraph(lab.graph).ok());
    const std::vector<std::byte> bytes = renderCanvas(device, *created.value(), config.extent, config.format, seconds);
    REQUIRE(bytes.size() == std::size_t{width} * height * 16U);
    Pixels image;
    image.width = width;
    image.height = height;
    image.data.resize(std::size_t{width} * height * 4);
    std::memcpy(image.data.data(), bytes.data(), bytes.size());
    CHECK(device.takeErrors().empty());
    return image;
}

inline bool closeTo(float a, float b, float tolerance = 2e-3F) { return std::abs(a - b) <= tolerance; }

inline bool isColor(const std::array<float, 4>& c, float r, float g, float b, float a = 1.0F) {
    return closeTo(c[0], r) && closeTo(c[1], g) && closeTo(c[2], b) && closeTo(c[3], a);
}

inline NodeId solid(Lab& lab, float r, float g, float b) {
    const NodeId node = lab.add("wh.generator.solid");
    lab.set(node, "color", ColorRgba{r, g, b, 1.0F});
    return node;
}

/// Verlauf in x von Schwarz nach Weiß: Rot = x der Pixelmitte.
inline NodeId rampX(Lab& lab) {
    const NodeId node = lab.add("wh.generator.gradient");
    lab.set(node, "angle", 0.0);
    return node;
}

inline float centerX(std::uint32_t x, std::uint32_t width) { return (static_cast<float>(x) + 0.5F) / static_cast<float>(width); }

}  // namespace whitehole::gputest::nodes

/// Gerät anlegen oder den Test überspringen, wenn es keinen Adapter gibt.
#define REQUIRE_DEVICE()                                      \
    const std::unique_ptr<::whitehole::gpu::WebGpuDevice> device = ::whitehole::gputest::makeDevice(); \
    if (!device) {                                            \
        SKIP("Kein WebGPU-Adapter verfügbar");                \
    }
