// Von WhiteHoleFX erzeugt (Feldcompiler). Nicht von Hand ändern.
struct WhFrame {
    time: f32,
    delta: f32,
    frame: u32,
    _pad: u32,
}
@group(0) @binding(0) var<uniform> wh_frame: WhFrame;

struct WhParams {
    n1_dirX: f32,
    n1_dirY: f32,
    n1_dirZ: f32,
    n1_frequency: f32,
    n1_speed: f32,
    n1_colorA: vec4<f32>,
    n1_colorB: vec4<f32>,
    n2_angleX: f32,
    n2_angleY: f32,
    n2_angleZ: f32,
    n3_cx: f32,
    n3_cy: f32,
    n3_cz: f32,
    n3_radius: f32,
    n3_softness: f32,
    n3_colorInside: vec4<f32>,
    n3_colorOutside: vec4<f32>,
    _pad: vec4<f32>,
}
@group(0) @binding(1) var<uniform> wh_params: WhParams;

fn wh_f1(p: vec3<f32>) -> vec4<f32> {
    let dv = vec3<f32>(wh_params.n1_dirX, wh_params.n1_dirY, wh_params.n1_dirZ);
    let len = length(dv);
    let n = select(vec3<f32>(1.0, 0.0, 0.0), dv / max(len, 0.000001), len > 0.000001);
    let phase = wh_params.n1_frequency * dot(p, n) - wh_params.n1_speed * wh_frame.time;
    let w = 0.5 + 0.5 * sin(6.2831853 * phase);
    return mix(wh_params.n1_colorA, wh_params.n1_colorB, w);
}

fn wh_f2(p: vec3<f32>) -> vec4<f32> {
    let c = vec3<f32>(0.5, 0.5, 0.5);
    var d = p - c;
    let az = radians(wh_params.n2_angleZ);
    d = vec3<f32>(cos(az) * d.x + sin(az) * d.y, -sin(az) * d.x + cos(az) * d.y, d.z);
    let ay = radians(wh_params.n2_angleY);
    d = vec3<f32>(cos(ay) * d.x - sin(ay) * d.z, d.y, sin(ay) * d.x + cos(ay) * d.z);
    let ax = radians(wh_params.n2_angleX);
    d = vec3<f32>(d.x, cos(ax) * d.y + sin(ax) * d.z, -sin(ax) * d.y + cos(ax) * d.z);
    return wh_f1(d + c);
}

fn wh_f3(p: vec3<f32>) -> vec4<f32> {
    let center = vec3<f32>(wh_params.n3_cx, wh_params.n3_cy, wh_params.n3_cz);
    let r = wh_params.n3_radius;
    let t = smoothstep(r, r + max(wh_params.n3_softness, 0.0001), length(p - center));
    return mix(wh_params.n3_colorInside, wh_params.n3_colorOutside, t);
}

fn wh_f4(p: vec3<f32>) -> vec4<f32> {
    return clamp(wh_f2(p) + wh_f3(p), vec4<f32>(0.0, 0.0, 0.0, 0.0), vec4<f32>(1.0, 1.0, 1.0, 1.0));
}

fn wh_field(p: vec3<f32>) -> vec4<f32> {
    return wh_f4(p);
}

struct WhLeds {
    count: u32,
    pad0: u32,
    pad1: u32,
    pad2: u32,
}

@group(1) @binding(0) var<storage, read> wh_positions: array<vec4<f32>>;
@group(1) @binding(1) var<storage, read_write> wh_colors: array<vec4<f32>>;
@group(1) @binding(2) var<uniform> wh_leds: WhLeds;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3<u32>, @builtin(num_workgroups) groups: vec3<u32>) {
    let i = id.y * groups.x * 64u + id.x;
    if (i >= wh_leds.count) {
        return;
    }
    wh_colors[i] = wh_field(wh_positions[i].xyz);
}
