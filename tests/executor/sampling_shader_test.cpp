// Abtast-Einstieg (nur Text) und Dispatch-Größe.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

#include "whitehole/executor/sampling_render_stage.hpp"
#include "whitehole/executor/sampling_shader.hpp"

using namespace whitehole;
using namespace whitehole::executor;

namespace {
bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}
}  // namespace

TEST_CASE("samplingShader hängt den Einstieg hinter die Feldbibliothek") {
    const std::string library = "fn wh_field(p: vec3<f32>) -> vec4<f32> { return vec4<f32>(p, 1.0); }\n";
    const std::string text = samplingShader(library);

    CHECK(text.rfind(library, 0) == 0U);
    CHECK(contains(text, "@group(1) @binding(0) var<storage, read> wh_positions: array<vec4<f32>>;"));
    CHECK(contains(text, "@group(1) @binding(1) var<storage, read_write> wh_colors: array<vec4<f32>>;"));
    CHECK(contains(text, "@group(1) @binding(2) var<uniform> wh_leds: WhLeds;"));
    CHECK(contains(text, "@compute @workgroup_size(64)"));
    CHECK(contains(text, "var color = wh_field(position.xyz);"));
    CHECK(contains(text, "color.a = color.a * (1.0 - position.w);"));
}

TEST_CASE("samplingShader prüft den Index gegen die Anzahl, nicht gegen die Puffergröße") {
    const std::string text = samplingShader("");
    CHECK(contains(text, "if (i >= wh_leds.count)"));
    CHECK_FALSE(contains(text, "arrayLength"));
    // Der Index setzt sich aus beiden Dispatch-Dimensionen zusammen (mehr als 65535 Gruppen).
    CHECK(contains(text, "id.y * groups.x * 64u + id.x"));
}

TEST_CASE("Die Abtast-Konstanten passen zum Text") {
    CHECK(kSamplingBindGroup == 1U);
    CHECK(kSamplingPositionsBinding == 0U);
    CHECK(kSamplingColorsBinding == 1U);
    CHECK(kSamplingInfoBinding == 2U);
    CHECK(kSamplingWorkgroupSize == 64U);
}

TEST_CASE("samplingDispatch: Grenzfälle") {
    const auto check = [](std::uint64_t leds, std::uint32_t x, std::uint32_t y) {
        CAPTURE(leds);
        const DispatchSize size = samplingDispatch(leds);
        CHECK(size.x == x);
        CHECK(size.y == y);
        CHECK(size.z == 1U);
    };
    check(0, 1, 1);
    check(1, 1, 1);
    check(64, 1, 1);
    check(65, 2, 1);
    check(128, 2, 1);
    check(std::uint64_t{65535} * 64U, 65535, 1);
    check(std::uint64_t{65535} * 64U + 1U, 65535, 2);
    check(std::uint64_t{1} << 23, 65535, 3);  // Obergrenze eines Layouts
}

TEST_CASE("samplingDispatch deckt jede LED ab und bleibt unter dem Limit") {
    const std::uint64_t counts[] = {1, 2, 63, 64, 65, 1000, 4095, 4096, 4097, 100000, 4194240, 4194241,
                                    5000000, 8388608};
    for (const std::uint64_t leds : counts) {
        CAPTURE(leds);
        const DispatchSize size = samplingDispatch(leds);
        CHECK(size.x <= kMaxGroupsPerDimension);
        CHECK(std::uint64_t{size.x} * size.y * kSamplingWorkgroupSize >= leds);
        // Nicht mehr als eine zusätzliche Zeile Gruppen.
        CHECK(std::uint64_t{size.x} * (size.y - 1U) * kSamplingWorkgroupSize < leds);
    }
}

TEST_CASE("packPositions: vec4 je Position mit w = 0") {
    const std::vector<Vec3> positions{Vec3{1.0F, 2.0F, 3.0F}, Vec3{-4.0F, 0.5F, 0.25F}};
    const std::vector<std::byte> bytes = packPositions(positions);
    REQUIRE(bytes.size() == 32U);
    float values[8];
    std::memcpy(values, bytes.data(), sizeof values);
    CHECK(values[0] == 1.0F);
    CHECK(values[1] == 2.0F);
    CHECK(values[2] == 3.0F);
    CHECK(values[3] == 0.0F);
    CHECK(values[4] == -4.0F);
    CHECK(values[5] == 0.5F);
    CHECK(values[6] == 0.25F);
    CHECK(values[7] == 0.0F);
    CHECK(packPositions({}).empty());
}

TEST_CASE("packPositions: LEDs außerhalb des Effektbereichs bekommen w = 1") {
    const std::vector<Vec3> positions{Vec3{0.1F, 0.2F, 0.0F}, Vec3{0.9F, 0.8F, 0.0F}, Vec3{0.5F, 0.5F, 0.0F}};
    const std::vector<std::uint8_t> outside{0U, 1U};  // kürzer als die Positionen: der Rest gilt als innen
    const std::vector<std::byte> bytes = packPositions(positions, outside);
    REQUIRE(bytes.size() == 48U);
    float values[12];
    std::memcpy(values, bytes.data(), sizeof values);
    CHECK(values[3] == 0.0F);
    CHECK(values[7] == 1.0F);
    CHECK(values[11] == 0.0F);
    CHECK(values[4] == 0.9F);  // die Position selbst bleibt unverändert
}
