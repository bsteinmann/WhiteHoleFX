// Transformationen: verändern die Koordinate und werten den Eingang dort aus.
// Sie wirken dadurch auf jeden Effekt dahinter, ohne Zwischenbild.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

// Dreht die Abtastkoordinate um den Mittelpunkt (0.5, 0.5) um -angle. Das Bild
// selbst dreht sich damit um +angle: gegen den Uhrzeigersinn, denn y zeigt im ganzen
// Projekt nach oben (siehe wh_mapping/fixture.hpp).
constexpr const char* kRotateBody = R"wgsl(let a = radians(@param(angle));
let c = cos(a);
let s = sin(a);
let d = p.xy - vec2<f32>(0.5, 0.5);
let r = vec2<f32>(c * d.x + s * d.y, -s * d.x + c * d.y) + vec2<f32>(0.5, 0.5);
return @in(source, vec3<f32>(r.x, r.y, p.z));)wgsl";

// Spiegelt an der Mittellinie: Die Koordinate wird auf die Hälfte <= 0.5 gefaltet.
constexpr const char* kMirrorBody = R"wgsl(var q = p;
if (@param(mirrorX) > 0.5) {
    q.x = 0.5 - abs(q.x - 0.5);
}
if (@param(mirrorY) > 0.5) {
    q.y = 0.5 - abs(q.y - 0.5);
}
return @in(source, q);)wgsl";

// Zoom um die Mitte in x und y: factor 2 zeigt das Bild doppelt so groß.
constexpr const char* kScaleBody = R"wgsl(let c = vec3<f32>(0.5, 0.5, p.z);
return @in(source, (p - c) / @param(factor) + c);)wgsl";

// Dreht das Bild im Raum um die Mitte (0.5, 0.5, 0.5). Positive Winkel drehen rechtshändig
// um die jeweilige Achse (Daumen zeigt in Achsenrichtung): +x wandert unter angleZ nach +y,
// +y unter angleX nach +z, +z unter angleY nach +x. Die Reihenfolge ist R = Rz * Ry * Rx
// (erst um x, dann um y, dann um z). Gesampelt wird mit der Umkehrung R^-1.
constexpr const char* kRotate3dBody =
    R"wgsl(let c = vec3<f32>(0.5, 0.5, 0.5);
var d = p - c;
let az = radians(@param(angleZ));
d = vec3<f32>(cos(az) * d.x + sin(az) * d.y, -sin(az) * d.x + cos(az) * d.y, d.z);
let ay = radians(@param(angleY));
d = vec3<f32>(cos(ay) * d.x - sin(ay) * d.z, d.y, sin(ay) * d.x + cos(ay) * d.z);
let ax = radians(@param(angleX));
d = vec3<f32>(d.x, cos(ax) * d.y + sin(ax) * d.z, -sin(ax) * d.y + cos(ax) * d.z);
return @in(source, d + c);)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> transformNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode("wh.transform.rotate", "Rotate", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("angle", "Angle (deg)", 0.0)}, kRotateBody));

    nodes.push_back(fieldNode("wh.transform.mirror", "Mirror", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {toggle("mirrorX", "Mirror X", true), toggle("mirrorY", "Mirror Y", false)},
                              kMirrorBody));

    nodes.push_back(fieldNode("wh.transform.scale", "Scale", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("factor", "Factor", 1.0, 0.01, 100.0)}, kScaleBody));

    nodes.push_back(fieldNode("wh.transform.rotate3d", "Rotate 3D", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("angleX", "Angle X (deg)", 0.0), number("angleY", "Angle Y (deg)", 0.0),
                               number("angleZ", "Angle Z (deg)", 0.0)},
                              kRotate3dBody));

    return nodes;
}

}  // namespace whitehole::nodes::detail
