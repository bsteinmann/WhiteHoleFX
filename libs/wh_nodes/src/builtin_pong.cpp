// Pong als Node-Paar: "Pong" (Rechen-Node mit dem Spiel) und "Pong Display" (Bild-Node, der es zeichnet).
//
// Warum zwei Nodes: Bild-Nodes sind zustandslos (das Bild hängt nur von Koordinate und Zeit ab), ein Spiel
// braucht aber Gedächtnis. Der Rechen-Node hält die Simulation (pong_sim.hpp) und gibt Ball, Schläger,
// Punkte und Ereignisse als Werte aus; der Bild-Node nimmt diese Werte als Eingänge und zeichnet sie. So lassen
// sich die Werte auch anderswo nutzen (ein Treffer lässt das Bild blitzen, der Ball steuert eine Farbe),
// und beide Nodes sind einzeln testbar.
//
// Zeichnung: x und y laufen von 0 bis 1 wie bei allen Feldern. `aspect` (Breite / Höhe der Leinwand, 32×16 = 2)
// macht Ball und Punkte rund und gibt Abstände in Höheneinheiten. Die Ränder sind weich (`soft`); auf kleinen
// Matrizen setzt man `soft` auf etwa die Größe einer LED, damit sich der Ball zwischen den Pixeln bewegt.
#include <cstdint>

#include "builtin_internal.hpp"
#include "pong_sim.hpp"

namespace whitehole::nodes::detail {

namespace {

using values::KernelContext;
using values::KernelOutputs;

class PongKernel final : public values::ICpuKernel {
public:
    void evaluate(const KernelContext& context, KernelOutputs& outputs) override {
        const auto& in = context.inputs;
        PongSettings s;
        s.leftControl = static_cast<int>(in.integer("leftControl", 0));
        s.rightControl = static_cast<int>(in.integer("rightControl", 0));
        s.leftInput = in.number("leftInput", 0.5);
        s.rightInput = in.number("rightInput", 0.5);
        s.beatMode = in.integer("timing", 0) == 1;
        s.speed = in.number("speed", 0.6);
        s.bpm = in.number("bpm", 120.0);
        s.beatsPerCrossing = in.number("beats", 2.0);
        s.speedUp = in.number("speedUp", 0.05);
        s.paddleSize = in.number("paddleSize", 0.25);
        s.ballRadius = in.number("ballRadius", 0.03);
        s.margin = in.number("margin", 0.05);
        s.skill = in.number("skill", 0.85);
        s.paddleSpeed = in.number("paddleSpeed", 1.2);
        s.maxScore = static_cast<int>(in.integer("maxScore", 7));
        s.serveDelay = in.number("serveDelay", 0.8);
        s.autoServe = in.boolean("autoServe", true);
        s.winHold = in.number("winHold", 2.0);
        s.restart = in.boolean("restart", true);
        s.seed = static_cast<std::uint64_t>(std::max<std::int64_t>(in.integer("seed", 1), 0));

        serve_.update(in.boolean("serve", false));
        beat_.update(in.boolean("beat", false));
        PongEvents events;
        const PongView v = sim_.step(context.time.delta.count(), s, serve_.rose(), beat_.rose(), events);

        outputs.set("ballX", v.ballX);
        outputs.set("ballY", v.ballY);
        outputs.set("left", v.left);
        outputs.set("right", v.right);
        outputs.set("paddleLength", s.paddleSize);
        outputs.set("ballSize", s.ballRadius);
        outputs.set("paddleX", s.margin);
        outputs.set("scoreLeft", static_cast<std::int64_t>(v.scoreLeft));
        outputs.set("scoreRight", static_cast<std::int64_t>(v.scoreRight));
        outputs.set("target", static_cast<std::int64_t>(s.maxScore));
        outputs.set("winner", static_cast<std::int64_t>(v.winner));
        outputs.set("flash", v.flash);
        outputs.set("hitPaddle", events.paddleHit);
        outputs.set("hitWall", events.wallHit);
        outputs.set("point", events.point);
    }

private:
    PongSim sim_;
    EdgeDetector serve_;
    EdgeDetector beat_;
};

constexpr const char* kPongPrelude = R"wgsl(fn wh_pong_cov(sd: f32, soft: f32) -> f32 {
    return 1.0 - smoothstep(-soft * 0.5, soft * 0.5, sd);
}

fn wh_pong_box(q: vec2<f32>, c: vec2<f32>, h: vec2<f32>) -> f32 {
    let d = abs(q - c) - h;
    return length(max(d, vec2<f32>(0.0, 0.0))) + min(max(d.x, d.y), 0.0);
}
)wgsl";

// Schichten werden addiert (wie bei Lightning): Licht = Summe Farbe * Deckung, Deckung = Summe, begrenzt auf 1.
constexpr const char* kPongBody = R"wgsl(let asp = max(@param(aspect), 0.1);
let q = vec2<f32>(p.x * asp, p.y);
let soft = max(@param(soft), 0.001);
let thick = max(@param(thickness), 0.002);
let half = max(@param(paddleSize), 0.01) * 0.5;
let margin = @param(margin);
var light = vec3<f32>(0.0, 0.0, 0.0);
var asum = 0.0;

let net = @param(net);
let dashes = max(@param(dashes), 1.0);
let netCov = wh_pong_cov(abs(q.x - 0.5 * asp) - thick * 0.5, soft) * step(fract(p.y * dashes), 0.5);
let na = net.a * netCov;
light = light + net.rgb * na;
asum = asum + na;

let lc = @param(paddleLeft);
let rc = @param(paddleRight);
let la = lc.a * wh_pong_cov(wh_pong_box(q, vec2<f32>(margin * asp, @param(left)), vec2<f32>(thick, half)), soft);
let ra = rc.a * wh_pong_cov(wh_pong_box(q, vec2<f32>((1.0 - margin) * asp, @param(right)), vec2<f32>(thick, half)), soft);
light = light + lc.rgb * la + rc.rgb * ra;
asum = asum + la + ra;

let bc = @param(ball);
let ba = bc.a * wh_pong_cov(length(q - vec2<f32>(@param(ballX) * asp, @param(ballY))) - max(@param(ballRadius), 0.002), soft);
light = light + bc.rgb * ba;
asum = asum + ba;

if (@param(dots) > 0.5) {
    let n = i32(clamp(floor(@param(maxScore) + 0.5), 0.0, 15.0));
    let dotR = max(@param(dotSize), 0.002);
    let gap = @param(dotSpacing);
    let empty = clamp(@param(emptyLevel), 0.0, 1.0);
    let win = i32(floor(@param(winner) + 0.5));
    for (var s = 0; s < 2; s = s + 1) {
        let right = s == 1;
        let o = select(vec2<f32>(@param(leftX), @param(leftY)), vec2<f32>(@param(rightX), @param(rightY)), right);
        let ang = radians(select(@param(leftAngle), @param(rightAngle), right));
        let dir = vec2<f32>(cos(ang), sin(ang));
        let score = select(@param(scoreLeft), @param(scoreRight), right);
        let col = select(lc, rc, right);
        let blink = select(1.0, clamp(@param(flash), 0.0, 1.0), win == s + 1);
        for (var i = 0; i < 15; i = i + 1) {
            if (i < n) {
                let c = vec2<f32>(o.x * asp, o.y) + dir * gap * f32(i);
                let on = select(empty, blink, f32(i) + 0.5 < score);
                let a = col.a * on * wh_pong_cov(length(q - c) - dotR, soft);
                light = light + col.rgb * a;
                asum = asum + a;
            }
        }
    }
}

let alpha = clamp(asum, 0.0, 1.0);
return vec4<f32>(min(light / max(alpha, 0.000001), vec3<f32>(1.0, 1.0, 1.0)), alpha);)wgsl";

}  // namespace

std::vector<values::CpuNodeDefinition> pongNodes() {
    using graph::PortType;
    std::vector<values::CpuNodeDefinition> nodes;
    nodes.push_back(cpuNode(
        "wh.value.pong", "Pong", "Game",
        {valueOutput("ballX", "Ball X", PortType::Scalar), valueOutput("ballY", "Ball Y", PortType::Scalar),
         valueOutput("left", "Left paddle", PortType::Scalar), valueOutput("right", "Right paddle", PortType::Scalar),
         valueOutput("paddleLength", "Paddle size", PortType::Scalar), valueOutput("ballSize", "Ball radius", PortType::Scalar),
         valueOutput("paddleX", "Paddle margin", PortType::Scalar), valueOutput("scoreLeft", "Score left", PortType::Integer),
         valueOutput("scoreRight", "Score right", PortType::Integer), valueOutput("target", "Points to win", PortType::Integer),
         valueOutput("winner", "Winner", PortType::Integer), valueOutput("flash", "Winner flash", PortType::Scalar),
         valueOutput("hitPaddle", "Paddle hit", PortType::Trigger), valueOutput("hitWall", "Wall hit", PortType::Trigger),
         valueOutput("point", "Point", PortType::Trigger)},
        {choice("leftControl", "Left player", {"Computer", "Input"}), choice("rightControl", "Right player", {"Computer", "Input"}),
         number("leftInput", "Left paddle input", 0.5, 0.0, 1.0), number("rightInput", "Right paddle input", 0.5, 0.0, 1.0),
         choice("timing", "Timing", {"Speed", "Beat"}), number("speed", "Speed (widths/s)", 0.6, 0.05, 3.0),
         number("bpm", "BPM (beat timing)", 120.0, 20.0, 300.0), number("beats", "Beats per crossing", 2.0, 0.25, 16.0),
         toggle("beat", "Beat (serves on beat)", false), number("speedUp", "Speed-up per hit", 0.05, 0.0, 0.5),
         number("paddleSize", "Paddle size", 0.25, 0.05, 0.8), number("ballRadius", "Ball radius", 0.03, 0.005, 0.15),
         number("margin", "Paddle margin", 0.05, 0.0, 0.3), number("skill", "Computer skill", 0.85, 0.0, 1.0),
         number("paddleSpeed", "Computer paddle speed", 1.2, 0.1, 5.0), integer("maxScore", "Points to win", 7, 1.0, 15.0),
         number("serveDelay", "Serve delay (s)", 0.8, 0.0, 5.0), toggle("autoServe", "Serve automatically", true),
         toggle("serve", "Serve now", false), number("winHold", "Winner pause (s)", 2.0, 0.0, 10.0),
         toggle("restart", "Start a new game", true), integer("seed", "Seed", 1, 0.0, 9999.0)},
        kernelFactory<PongKernel>()));
    return nodes;
}

std::vector<compiler::NodeDefinition> pongFieldNodes() {
    using graph::PortType;
    std::vector<compiler::NodeDefinition> nodes;
    const ColorRgba white{1.0F, 1.0F, 1.0F, 1.0F};
    nodes.push_back(fieldNode(
        "wh.generator.pong", "Pong Display", "Generators", {fieldOutput()},
        {number("ballX", "Ball X", 0.5, 0.0, 1.0), number("ballY", "Ball Y", 0.5, 0.0, 1.0),
         number("left", "Left paddle", 0.5, 0.0, 1.0), number("right", "Right paddle", 0.5, 0.0, 1.0),
         number("paddleSize", "Paddle size", 0.25, 0.05, 0.8), number("ballRadius", "Ball radius", 0.03, 0.005, 0.15),
         number("margin", "Paddle margin", 0.05, 0.0, 0.3), number("scoreLeft", "Score left", 0.0, 0.0, 15.0),
         number("scoreRight", "Score right", 0.0, 0.0, 15.0), number("maxScore", "Points to win", 7.0, 0.0, 15.0),
         number("winner", "Winner (0 none, 1 left, 2 right)", 0.0, 0.0, 2.0), number("flash", "Winner flash", 1.0, 0.0, 1.0),
         number("aspect", "Aspect (width / height)", 1.0, 0.1, 10.0), number("soft", "Edge softness", 0.02, 0.001, 0.3),
         number("thickness", "Paddle thickness", 0.04, 0.002, 0.2), color("ball", "Ball", white),
         color("paddleLeft", "Left paddle color", white), color("paddleRight", "Right paddle color", white),
         color("net", "Net", ColorRgba{1.0F, 1.0F, 1.0F, 0.25F}), number("dashes", "Net dashes", 12.0, 1.0, 40.0),
         choice("dots", "Score display", {"Off", "Dots"}, 1), number("dotSize", "Dot size", 0.025, 0.002, 0.2),
         number("dotSpacing", "Dot spacing", 0.07, 0.01, 0.5), number("emptyLevel", "Empty dot brightness", 0.15, 0.0, 1.0),
         number("leftX", "Left dots X", 0.06, 0.0, 1.0), number("leftY", "Left dots Y", 0.93, 0.0, 1.0),
         number("leftAngle", "Left dots direction (deg)", 0.0, -360.0, 360.0), number("rightX", "Right dots X", 0.94, 0.0, 1.0),
         number("rightY", "Right dots Y", 0.93, 0.0, 1.0), number("rightAngle", "Right dots direction (deg)", 180.0, -360.0, 360.0)},
        kPongBody, kPongPrelude));
    return nodes;
}

}  // namespace whitehole::nodes::detail
