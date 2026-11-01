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

// Spiegelt an einer Linie (x, y oder z, jeweils an einer einstellbaren Stelle): Die Koordinate wird
// auf die eine Seite gefaltet, die andere zeigt das Spiegelbild. Standard: die untere Hälfte (x < 0.5)
// ist die Vorlage; "Other half" nimmt stattdessen die obere.
constexpr const char* kMirrorBody = R"wgsl(var q = p;
let high = @param(otherHalf) > 0.5;
if (@param(mirrorX) > 0.5) {
    let c = @param(centerX);
    let d = abs(q.x - c);
    q.x = select(c - d, c + d, high);
}
if (@param(mirrorY) > 0.5) {
    let c = @param(centerY);
    let d = abs(q.y - c);
    q.y = select(c - d, c + d, high);
}
if (@param(mirrorZ) > 0.5) {
    let c = @param(centerZ);
    let d = abs(q.z - c);
    q.z = select(c - d, c + d, high);
}
return @in(source, q);)wgsl";

// "Region" (Bild-in-Bild): Der Eingang wird auf ein Rechteck der Leinwand abgebildet, als wäre dieses
// Rechteck die ganze Leinwand; außerhalb ist das Ergebnis durchsichtig (Alpha 0, die LED bleibt aus).
// left und top zählen von der linken oberen Ecke, alle vier Werte sind Anteile der Leinwand (0..1).
// Breite und Höhe haben eine Untergrenze, damit nie durch null geteilt wird. Im Raum wird aus dem Rechteck
// ein Quader: `zStart` und `zSize` sind der Ausschnitt in z (Anteile der Tiefe); der Standard 0 und 1 nimmt
// die ganze Tiefe, dann bleibt z unverändert.
constexpr const char* kRegionBody = R"wgsl(let w = max(@param(width), 0.0001);
let h = max(@param(height), 0.0001);
let dz = max(@param(zSize), 0.0001);
let u = (p.x - @param(left)) / w;
let v = (p.y - (1.0 - @param(top) - h)) / h;
let k = (p.z - @param(zStart)) / dz;
if (u < -0.0001 || u > 1.0001 || v < -0.0001 || v > 1.0001 || k < -0.0001 || k > 1.0001) {
    return vec4<f32>(0.0, 0.0, 0.0, 0.0);
}
return @in(source, vec3<f32>(u, v, k));)wgsl";

// Zoom um die Mitte (0.5, 0.5, 0.5): factor 2 zeigt das Bild doppelt so groß. `factorZ` zoomt die Tiefe
// getrennt; bei 1 (Standard) bleibt z wie es ist, das Bild verhält sich also wie auf einer 2D-Leinwand.
constexpr const char* kScaleBody = R"wgsl(let c = vec3<f32>(0.5, 0.5, 0.5);
let f = vec3<f32>(@param(factor), @param(factor), @param(factorZ));
return @in(source, (p - c) / f + c);)wgsl";

// Dreht das Bild im Raum um die Mitte (0.5, 0.5, 0.5). Positive Winkel drehen rechtshändig
// um die jeweilige Achse (Daumen zeigt in Achsenrichtung): +x wandert unter angleZ nach +y,
// Symmetrie: Eine Hälfte des Eingangs (je nach Wahl die untere oder die obere Seite der Linie) wird
// an der Linie gespiegelt, so entsteht ein symmetrisches Muster. Achse und Seite sind Auswahlfelder.
constexpr const char* kSymmetryBody = R"wgsl(var q = p;
let c = @param(center);
let axis = @param(axis);
let upper = @param(half) > 0.5;
if (axis < 0.5) {
    let d = abs(q.x - c);
    q.x = select(c - d, c + d, upper);
} else if (axis < 1.5) {
    let d = abs(q.y - c);
    q.y = select(c - d, c + d, upper);
} else {
    let d = abs(q.z - c);
    q.z = select(c - d, c + d, upper);
}
return @in(source, q);)wgsl";

// Verschiebt das Bild um (x, y) Leinwandbreiten. Mit Wrap kommt es auf der anderen Seite wieder herein,
// sonst bleibt der Rest durchsichtig. Mit einem LFO oder Time an x entsteht ein Übergang oder Scroll.
// `z` verschiebt in der Tiefe; solange z = 0 ist, bleibt die Tiefe unangetastet (kein Umbruch, kein Abschneiden).
constexpr const char* kSlideBody = R"wgsl(var q = vec3<f32>(p.x - @param(x), p.y - @param(y), p.z - @param(z));
let moveZ = abs(@param(z)) > 0.00001;
if (@param(wrap) > 0.5) {
    q = vec3<f32>(fract(q.x), fract(q.y), select(q.z, fract(q.z), moveZ));
} else if (q.x < 0.0 || q.x > 1.0 || q.y < 0.0 || q.y > 1.0 || (moveZ && (q.z < -0.0001 || q.z > 1.0001))) {
    return vec4<f32>(0.0, 0.0, 0.0, 0.0);
}
return @in(source, q);)wgsl";

// Kachelt das Bild countX mal countY (und countZ mal in der Tiefe); mit Mirror wird jede zweite Kachel
// gespiegelt (nahtlos). countZ = 1 (Standard) lässt z unangetastet.
constexpr const char* kTileBody = R"wgsl(let n = vec3<f32>(max(@param(countX), 1.0), max(@param(countY), 1.0), max(@param(countZ), 1.0));
let tileZ = n.z > 1.0;
let u = vec3<f32>(p.xy * n.xy, select(0.0, p.z * n.z, tileZ));
var f = fract(u);
if (@param(mirror) > 0.5) {
    let cell = floor(u);
    let oddX = cell.x - 2.0 * floor(cell.x * 0.5);
    let oddY = cell.y - 2.0 * floor(cell.y * 0.5);
    let oddZ = cell.z - 2.0 * floor(cell.z * 0.5);
    f = vec3<f32>(select(f.x, 1.0 - f.x, oddX > 0.5), select(f.y, 1.0 - f.y, oddY > 0.5), select(f.z, 1.0 - f.z, oddZ > 0.5));
}
return @in(source, vec3<f32>(f.x, f.y, select(p.z, f.z, tileZ)));)wgsl";

// Mosaik: jede Zelle zeigt den Wert an ihrer Mitte. `cellsZ` rastert auch die Tiefe (Würfel statt Quadrate);
// 0 (Standard) lässt z glatt.
constexpr const char* kPixelateBody = R"wgsl(let n = vec2<f32>(max(@param(cellsX), 1.0), max(@param(cellsY), 1.0));
let cell = min(floor(p.xy * n), n - vec2<f32>(1.0, 1.0));
let c = (cell + vec2<f32>(0.5, 0.5)) / n;
let nz = floor(@param(cellsZ));
let cz = (min(floor(p.z * nz), nz - 1.0) + 0.5) / max(nz, 1.0);
return @in(source, vec3<f32>(c.x, c.y, select(p.z, cz, nz >= 1.0)));)wgsl";

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
                              {toggle("mirrorX", "Mirror X", true), toggle("mirrorY", "Mirror Y", false),
                               toggle("mirrorZ", "Mirror Z", false), number("centerX", "Center X", 0.5, 0.0, 1.0),
                               number("centerY", "Center Y", 0.5, 0.0, 1.0), number("centerZ", "Center Z", 0.5, 0.0, 1.0),
                               toggle("otherHalf", "Use other half", false)},
                              kMirrorBody));

    nodes.push_back(fieldNode("wh.transform.symmetry", "Symmetry", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {choice("axis", "Axis", {"X (left/right)", "Y (top/bottom)", "Z (front/back)"}),
                               choice("half", "Source half", {"Lower (left/bottom)", "Upper (right/top)"}),
                               number("center", "Center", 0.5, 0.0, 1.0)},
                              kSymmetryBody));

    nodes.push_back(fieldNode("wh.transform.slide", "Slide", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("x", "Offset X", 0.0, -1.0, 1.0), number("y", "Offset Y", 0.0, -1.0, 1.0),
                               toggle("wrap", "Wrap around", false), number("z", "Offset Z", 0.0, -1.0, 1.0)},
                              kSlideBody));

    nodes.push_back(fieldNode("wh.transform.tile", "Tile", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("countX", "Tiles X", 2.0, 1.0, 64.0), number("countY", "Tiles Y", 2.0, 1.0, 64.0),
                               toggle("mirror", "Mirror every other tile", false),
                               number("countZ", "Tiles Z", 1.0, 1.0, 64.0)},
                              kTileBody));

    nodes.push_back(fieldNode("wh.transform.pixelate", "Pixelate", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("cellsX", "Cells X", 8.0, 1.0, 256.0), number("cellsY", "Cells Y", 8.0, 1.0, 256.0),
                               number("cellsZ", "Cells Z (0 = smooth)", 0.0, 0.0, 256.0)},
                              kPixelateBody));

    nodes.push_back(fieldNode("wh.transform.region", "Region", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("left", "Left", 0.0, 0.0, 1.0), number("top", "Top", 0.0, 0.0, 1.0),
                               number("width", "Width", 0.5, 0.0, 1.0), number("height", "Height", 0.5, 0.0, 1.0),
                               number("zStart", "Depth start", 0.0, 0.0, 1.0), number("zSize", "Depth size", 1.0, 0.0, 1.0)},
                              kRegionBody));

    nodes.push_back(fieldNode("wh.transform.scale", "Scale", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("factor", "Factor", 1.0, 0.01, 100.0), number("factorZ", "Factor Z", 1.0, 0.01, 100.0)},
                              kScaleBody));

    nodes.push_back(fieldNode("wh.transform.rotate3d", "Rotate 3D", "Transform",
                              {fieldInput("source", "Source"), fieldOutput()},
                              {number("angleX", "Angle X (deg)", 0.0), number("angleY", "Angle Y (deg)", 0.0),
                               number("angleZ", "Angle Z (deg)", 0.0)},
                              kRotate3dBody));

    return nodes;
}

}  // namespace whitehole::nodes::detail
