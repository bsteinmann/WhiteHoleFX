// Der Compute-Einstieg, der ein Feld in eine Textur rastert (nur Text, ohne GPU).
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "whitehole/executor/canvas_shader.hpp"

using namespace whitehole;
using executor::canvasShader;

namespace {
bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}
}  // namespace

TEST_CASE("canvasShader hängt den Einstieg hinter die Feldbibliothek") {
    const std::string library = "fn wh_field(p: vec3<f32>) -> vec4<f32> { return vec4<f32>(p, 1.0); }\n";
    const auto shader = canvasShader(library, gpu::TextureFormat::Rgba16Float);
    REQUIRE(shader.ok());
    const std::string& text = shader.value();

    CHECK(text.rfind(library, 0) == 0U);  // die Bibliothek steht unverändert am Anfang
    CHECK(contains(text, "@group(1) @binding(0) var wh_canvas: texture_storage_2d<rgba16float, write>;"));
    CHECK(contains(text, "@compute @workgroup_size(8, 8)"));
    CHECK(contains(text, "fn main("));
    CHECK(contains(text, "wh_field(p)"));
    CHECK(contains(text, "textureStore(wh_canvas"));
    CHECK(text.find("wh_canvas") > library.size());
}

TEST_CASE("canvasShader prüft die Pixelmitte und den Rand") {
    const auto shader = canvasShader("", gpu::TextureFormat::Rgba8Unorm);
    REQUIRE(shader.ok());
    CHECK(contains(shader.value(), "(f32(id.x) + 0.5) / f32(size.x)"));
    CHECK(contains(shader.value(), "1.0 - (f32(id.y) + 0.5) / f32(size.y)"));  // y zeigt nach oben
    CHECK(contains(shader.value(), "id.x >= size.x || id.y >= size.y"));
}

TEST_CASE("canvasShader kennt die Storage-Formate") {
    CHECK(contains(canvasShader("", gpu::TextureFormat::Rgba8Unorm).value(), "texture_storage_2d<rgba8unorm, write>"));
    CHECK(contains(canvasShader("", gpu::TextureFormat::Rgba16Float).value(), "texture_storage_2d<rgba16float, write>"));
    CHECK(contains(canvasShader("", gpu::TextureFormat::Rgba32Float).value(), "texture_storage_2d<rgba32float, write>"));
}

TEST_CASE("canvasShader lehnt R32Float ab") {
    const auto shader = canvasShader("", gpu::TextureFormat::R32Float);
    REQUIRE_FALSE(shader.ok());
    CHECK(shader.error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("Die Konstanten passen zum Text") {
    CHECK(executor::kCanvasBindGroup == 1U);  // Gruppe 0 gehört der Feldbibliothek
    CHECK(executor::kCanvasTextureBinding == 0U);
    CHECK(executor::kCanvasWorkgroupSize == 8U);
}
