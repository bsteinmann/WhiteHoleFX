// canvas_shader.cpp: Umsetzung zu canvas_shader.hpp. Der Compute-Einstieg, der ein Feld in eine Textur rastert.
#include "whitehole/executor/canvas_shader.hpp"

#include <string>

namespace whitehole::executor {

namespace {

const char* storageFormatName(gpu::TextureFormat format) {
    switch (format) {
        case gpu::TextureFormat::Rgba8Unorm:
            return "rgba8unorm";
        case gpu::TextureFormat::Rgba16Float:
            return "rgba16float";
        case gpu::TextureFormat::Rgba32Float:
            return "rgba32float";
        case gpu::TextureFormat::R32Float:
            break;
    }
    return nullptr;
}

}  // namespace

Result<std::string> canvasShader(std::string_view fieldLibrary, gpu::TextureFormat format) {
    const char* formatName = storageFormatName(format);
    if (formatName == nullptr) {
        return Error{ErrorCode::InvalidArgument,
                     "Das Textur-Format eignet sich nicht für eine RGBA-Zieltextur"};
    }

    const std::string group = std::to_string(kCanvasBindGroup);
    const std::string binding = std::to_string(kCanvasTextureBinding);
    const std::string size = std::to_string(kCanvasWorkgroupSize);

    std::string text{fieldLibrary};
    text += "\n@group(" + group + ") @binding(" + binding + ") var wh_canvas: texture_storage_2d<" +
            formatName + ", write>;\n\n";
    text += "@compute @workgroup_size(" + size + ", " + size + ")\n";
    text += "fn main(@builtin(global_invocation_id) id: vec3<u32>) {\n";
    text += "    let size = textureDimensions(wh_canvas);\n";
    text += "    if (id.x >= size.x || id.y >= size.y) {\n";
    text += "        return;\n";
    text += "    }\n";
    text += "    let p = vec3<f32>((f32(id.x) + 0.5) / f32(size.x), 1.0 - (f32(id.y) + 0.5) / f32(size.y), 0.0);\n";
    text += "    textureStore(wh_canvas, vec2<i32>(id.xy), wh_field(p));\n";
    text += "}\n";
    return text;
}

}  // namespace whitehole::executor
