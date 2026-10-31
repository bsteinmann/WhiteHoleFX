// Tests des LayoutBuilders: Lage der Parameter im Uniform-Puffer nach den WGSL-Ausrichtungsregeln.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

#include "whitehole/compiler/parameter_layout.hpp"

using namespace whitehole;
using namespace whitehole::compiler;

TEST_CASE("LayoutBuilder: leeres Layout besteht nur aus dem Abschlusselement") {
    const ParameterLayout layout = LayoutBuilder{}.build();
    CHECK(layout.slots.empty());
    CHECK(layout.sizeBytes == 16U);
}

TEST_CASE("LayoutBuilder: Offsets folgen den WGSL-Ausrichtungsregeln") {
    LayoutBuilder builder;
    builder.add(NodeId{1}, "a", GpuType::Float);  // 0..4
    builder.add(NodeId{1}, "b", GpuType::Vec3);   // Ausrichtung 16 -> 16..28
    builder.add(NodeId{1}, "c", GpuType::Float);  // passt in den Rest des vec3 -> 28..32
    builder.add(NodeId{2}, "d", GpuType::Vec2);   // Ausrichtung 8 -> 32..40
    builder.add(NodeId{2}, "e", GpuType::Vec4);   // Ausrichtung 16 -> 48..64

    const ParameterLayout layout = builder.build();
    REQUIRE(layout.slots.size() == 5U);
    CHECK(layout.slots[0].offsetBytes == 0U);
    CHECK(layout.slots[1].offsetBytes == 16U);
    CHECK(layout.slots[2].offsetBytes == 28U);
    CHECK(layout.slots[3].offsetBytes == 32U);
    CHECK(layout.slots[4].offsetBytes == 48U);
    CHECK(layout.sizeBytes == 80U);  // Ende bei 64, dazu das vec4-Abschlusselement
}

TEST_CASE("LayoutBuilder: Float vor Vec2 lässt eine Lücke") {
    LayoutBuilder builder;
    builder.add(NodeId{1}, "a", GpuType::Float);
    builder.add(NodeId{1}, "b", GpuType::Vec2);

    const ParameterLayout layout = builder.build();
    CHECK(layout.slots[1].offsetBytes == 8U);
    CHECK(layout.sizeBytes == 32U);
}

TEST_CASE("memberName und wgslParameterStruct") {
    CHECK(memberName(NodeId{7}, "gain") == "n7_gain");

    LayoutBuilder builder;
    builder.add(NodeId{3}, "gain", GpuType::Float);
    builder.add(NodeId{3}, "tint", GpuType::Vec4);
    const std::string text = wgslParameterStruct(builder.build());

    CHECK(text == "struct WhParams {\n"
                  "    n3_gain: f32,\n"
                  "    n3_tint: vec4<f32>,\n"
                  "    _pad: vec4<f32>,\n"
                  "}\n");
}

TEST_CASE("gpuTypeOf ordnet jede Parameterart zu, Texte haben keine GPU-Entsprechung") {
    using graph::ParameterValue;
    CHECK(gpuTypeOf(ParameterValue{true}) == GpuType::Float);
    CHECK(gpuTypeOf(ParameterValue{std::int64_t{3}}) == GpuType::Float);
    CHECK(gpuTypeOf(ParameterValue{0.5}) == GpuType::Float);
    CHECK(gpuTypeOf(ParameterValue{Vec2{}}) == GpuType::Vec2);
    CHECK(gpuTypeOf(ParameterValue{Vec3{}}) == GpuType::Vec3);
    CHECK(gpuTypeOf(ParameterValue{ColorRgba{}}) == GpuType::Vec4);
    CHECK_FALSE(gpuTypeOf(ParameterValue{std::string{"text"}}).has_value());
}

TEST_CASE("wgslTypeName") {
    CHECK(wgslTypeName(GpuType::Float) == "f32");
    CHECK(wgslTypeName(GpuType::Vec2) == "vec2<f32>");
    CHECK(wgslTypeName(GpuType::Vec3) == "vec3<f32>");
    CHECK(wgslTypeName(GpuType::Vec4) == "vec4<f32>");
}
