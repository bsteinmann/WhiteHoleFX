// readBindings: Bindungen aus dem WGSL-Text, den die Generatoren erzeugen.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "whitehole/executor/canvas_shader.hpp"
#include "whitehole/executor/sampling_shader.hpp"
#include "whitehole/gpu/wgsl_bindings.hpp"

using namespace whitehole;
using namespace whitehole::gpu;

namespace {

/// Das Gerüst, das die Feldbibliothek in Gruppe 0 liefert.
const char* const kLibrary =
    "struct WhFrame { time: f32, delta: f32, frame: u32, pad: u32 }\n"
    "@group(0) @binding(0) var<uniform> wh_frame: WhFrame;\n"
    "struct WhParams { values: array<vec4<f32>, 4> }\n"
    "@group(0) @binding(1) var<uniform> wh_params: WhParams;\n"
    "fn wh_field(p: vec3<f32>) -> vec4<f32> { return vec4<f32>(p, 1.0); }\n";

DeclaredBinding decl(std::uint32_t g, std::uint32_t b, BindingKind k, const std::string& name,
                     TextureFormat f = TextureFormat::Rgba8Unorm) {
    DeclaredBinding d;
    d.group = g;
    d.binding = b;
    d.kind = k;
    d.name = name;
    d.format = f;
    return d;
}

ErrorCode failureOf(const std::string& text) {
    const Result<std::vector<DeclaredBinding>> r = readBindings(text);
    REQUIRE_FALSE(r.ok());
    return r.error().code;
}

}  // namespace

TEST_CASE("Die Feldbibliothek deklariert Frame- und Parameterpuffer", "[gpu][bindings]") {
    const Result<std::vector<DeclaredBinding>> r = readBindings(kLibrary);
    REQUIRE(r.ok());
    const std::vector<DeclaredBinding> expected{decl(0, 0, BindingKind::UniformBuffer, "wh_frame"),
                                                decl(0, 1, BindingKind::UniformBuffer, "wh_params")};
    CHECK(r.value() == expected);
}

TEST_CASE("Der Canvas-Shader hängt eine Storage-Textur an Gruppe 1", "[gpu][bindings]") {
    for (const TextureFormat format :
         {TextureFormat::Rgba8Unorm, TextureFormat::Rgba16Float, TextureFormat::Rgba32Float}) {
        const Result<std::string> shader = executor::canvasShader(kLibrary, format);
        REQUIRE(shader.ok());
        const Result<std::vector<DeclaredBinding>> r = readBindings(shader.value());
        REQUIRE(r.ok());
        REQUIRE(r.value().size() == 3);
        CHECK(r.value()[2] == decl(1, 0, BindingKind::StorageTextureWrite, "wh_canvas", format));
    }
}

TEST_CASE("Der Abtast-Shader liefert zwei Storage-Puffer und einen Uniform", "[gpu][bindings]") {
    const Result<std::string> shader = executor::samplingShader(kLibrary);
    REQUIRE(shader.ok());
    const Result<std::vector<DeclaredBinding>> r = readBindings(shader.value());
    REQUIRE(r.ok());
    const std::vector<DeclaredBinding> expected{
        decl(0, 0, BindingKind::UniformBuffer, "wh_frame"),
        decl(0, 1, BindingKind::UniformBuffer, "wh_params"),
        decl(1, 0, BindingKind::StorageBufferRead, "wh_positions"),
        decl(1, 1, BindingKind::StorageBufferReadWrite, "wh_colors"),
        decl(1, 2, BindingKind::UniformBuffer, "wh_leds")};
    CHECK(r.value() == expected);
}

TEST_CASE("Die Reihenfolge im Text ist egal, das Ergebnis ist sortiert", "[gpu][bindings]") {
    const std::string text =
        "@group(1) @binding(2) var<uniform> c: C;\n"
        "@group(0) @binding(1) var<uniform> b: B;\n"
        "@group(1) @binding(0) var<uniform> a: A;\n";
    const Result<std::vector<DeclaredBinding>> r = readBindings(text);
    REQUIRE(r.ok());
    REQUIRE(r.value().size() == 3);
    CHECK(r.value()[0].name == "b");
    CHECK(r.value()[1].name == "a");
    CHECK(r.value()[2].name == "c");
}

TEST_CASE("Zugriffsarten und Schreibweisen", "[gpu][bindings]") {
    const std::string text =
        "@group(0) @binding(0) var<storage> a: array<f32>;\n"
        "@group(0) @binding(1) var< storage , read > b: array<f32>;\n"
        "@group(0) @binding(2) var<storage,read_write> c: array<f32>;\n"
        "@group(0)\n@binding(3)\nvar t: texture_storage_2d< r32float , read >;\n"
        "@group(0) @binding(4) var u: texture_storage_2d<rgba8unorm, read_write>;\n";
    const Result<std::vector<DeclaredBinding>> r = readBindings(text);
    REQUIRE(r.ok());
    REQUIRE(r.value().size() == 5);
    CHECK(r.value()[0].kind == BindingKind::StorageBufferRead);
    CHECK(r.value()[1].kind == BindingKind::StorageBufferRead);
    CHECK(r.value()[2].kind == BindingKind::StorageBufferReadWrite);
    CHECK(r.value()[3].kind == BindingKind::StorageTextureRead);
    CHECK(r.value()[3].format == TextureFormat::R32Float);
    CHECK(r.value()[4].kind == BindingKind::StorageTextureReadWrite);
}

TEST_CASE("Auskommentierte Deklarationen zählen nicht", "[gpu][bindings]") {
    const std::string text =
        "// @group(0) @binding(0) var<uniform> alt: A;\n"
        "/* @group(0) @binding(1)\n   var<uniform> alt2: A; */\n"
        "@group(0) @binding(2) var<uniform> echt: A; // Rest\n";
    const Result<std::vector<DeclaredBinding>> r = readBindings(text);
    REQUIRE(r.ok());
    REQUIRE(r.value().size() == 1);
    CHECK(r.value()[0].name == "echt");
    CHECK(r.value()[0].binding == 2);
}

TEST_CASE("Ein Shader ohne Bindungen ergibt eine leere Liste", "[gpu][bindings]") {
    const Result<std::vector<DeclaredBinding>> r = readBindings("fn main() {}\n");
    REQUIRE(r.ok());
    CHECK(r.value().empty());
    CHECK(readBindings("").ok());
}

TEST_CASE("Nicht unterstützte Deklarationen werden abgelehnt", "[gpu][bindings]") {
    CHECK(failureOf("@group(0) @binding(0) var s: sampler;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var t: texture_2d<f32>;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var t: texture_storage_3d<rgba8unorm, write>;") ==
          ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var t: texture_storage_2d<bgra8unorm, write>;") ==
          ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var t: texture_storage_2d<rgba8unorm, bogus>;") ==
          ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var t: texture_storage_2d<rgba8unorm>;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var<private> p: f32;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var<storage, write> p: array<f32>;") == ErrorCode::ShaderError);
}

TEST_CASE("Kaputte oder doppelte Deklarationen werden abgelehnt", "[gpu][bindings]") {
    CHECK(failureOf("@group(x) @binding(0) var<uniform> a: A;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) var<uniform> a: A;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) let a = 1;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding() var<uniform> a: A;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var<uniform a: A;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var<uniform> : A;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var<uniform> a: A") == ErrorCode::ShaderError);  // kein ';'
    CHECK(failureOf("@group(1234567) @binding(0) var<uniform> a: A;") == ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) var<uniform> a: A;\n@group(0) @binding(0) var<uniform> b: B;") ==
          ErrorCode::ShaderError);
    CHECK(failureOf("@group(0) @binding(0) variable<uniform> a: A;") == ErrorCode::ShaderError);
}
