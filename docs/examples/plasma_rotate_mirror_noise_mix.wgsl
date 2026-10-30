// Von WhiteHoleFX erzeugt (Feldcompiler). Nicht von Hand ändern.
struct WhFrame {
    time: f32,
    delta: f32,
    frame: u32,
    _pad: u32,
}
@group(0) @binding(0) var<uniform> wh_frame: WhFrame;

struct WhParams {
    n1_scale: f32,
    n1_speed: f32,
    n2_angle: f32,
    n3_mirrorX: f32,
    n3_mirrorY: f32,
    n4_factor: f32,
    n5_scale: f32,
    n5_speed: f32,
    n8_angle: f32,
    n8_colorA: vec4<f32>,
    n8_colorB: vec4<f32>,
    n7_amount: f32,
    _pad: vec4<f32>,
}
@group(0) @binding(1) var<uniform> wh_params: WhParams;

fn wh_hash31(p: vec3<f32>) -> f32 {
    var q = fract(p * 0.1031);
    q = q + dot(q, q.zyx + 31.32);
    return fract((q.x + q.y) * q.z);
}

fn wh_value_noise(p: vec3<f32>) -> f32 {
    let i = floor(p);
    let f = fract(p);
    let u = f * f * (3.0 - 2.0 * f);
    let n000 = wh_hash31(i + vec3<f32>(0.0, 0.0, 0.0));
    let n100 = wh_hash31(i + vec3<f32>(1.0, 0.0, 0.0));
    let n010 = wh_hash31(i + vec3<f32>(0.0, 1.0, 0.0));
    let n110 = wh_hash31(i + vec3<f32>(1.0, 1.0, 0.0));
    let n001 = wh_hash31(i + vec3<f32>(0.0, 0.0, 1.0));
    let n101 = wh_hash31(i + vec3<f32>(1.0, 0.0, 1.0));
    let n011 = wh_hash31(i + vec3<f32>(0.0, 1.0, 1.0));
    let n111 = wh_hash31(i + vec3<f32>(1.0, 1.0, 1.0));
    let x00 = mix(n000, n100, u.x);
    let x10 = mix(n010, n110, u.x);
    let x01 = mix(n001, n101, u.x);
    let x11 = mix(n011, n111, u.x);
    return mix(mix(x00, x10, u.y), mix(x01, x11, u.y), u.z);
}

fn wh_f1(p: vec3<f32>) -> vec4<f32> {
    let s = wh_params.n1_scale;
    let t = wh_frame.time * wh_params.n1_speed;
    let q = p * s;
    let v = sin(q.x + t) + sin(q.y * 1.3 + t * 1.1) + sin((q.x + q.y + t) * 0.7) + sin(length(q.xy - vec2<f32>(0.5 * s)) + t);
    let w = v * 1.5707963;
    return vec4<f32>(0.5 + 0.5 * sin(w), 0.5 + 0.5 * sin(w + 2.0943951), 0.5 + 0.5 * sin(w + 4.1887902), 1.0);
}

fn wh_f2(p: vec3<f32>) -> vec4<f32> {
    let a = radians(wh_params.n2_angle);
    let c = cos(a);
    let s = sin(a);
    let d = p.xy - vec2<f32>(0.5, 0.5);
    let r = vec2<f32>(c * d.x + s * d.y, -s * d.x + c * d.y) + vec2<f32>(0.5, 0.5);
    return wh_f1(vec3<f32>(r.x, r.y, p.z));
}

fn wh_f3(p: vec3<f32>) -> vec4<f32> {
    var q = p;
    if (wh_params.n3_mirrorX > 0.5) {
        q.x = 0.5 - abs(q.x - 0.5);
    }
    if (wh_params.n3_mirrorY > 0.5) {
        q.y = 0.5 - abs(q.y - 0.5);
    }
    return wh_f2(q);
}

fn wh_f4(p: vec3<f32>) -> vec4<f32> {
    let c = vec3<f32>(0.5, 0.5, p.z);
    return wh_f3((p - c) / wh_params.n4_factor + c);
}

fn wh_f5(p: vec3<f32>) -> vec4<f32> {
    let v = wh_value_noise(p * wh_params.n5_scale + vec3<f32>(0.0, 0.0, wh_frame.time * wh_params.n5_speed));
    return vec4<f32>(v, v, v, 1.0);
}

fn wh_f6(p: vec3<f32>) -> vec4<f32> {
    return clamp(wh_f4(p) + wh_f5(p), vec4<f32>(0.0, 0.0, 0.0, 0.0), vec4<f32>(1.0, 1.0, 1.0, 1.0));
}

fn wh_f8(p: vec3<f32>) -> vec4<f32> {
    let a = radians(wh_params.n8_angle);
    let d = vec2<f32>(cos(a), sin(a));
    let t = clamp(dot(p.xy - vec2<f32>(0.5, 0.5), d) + 0.5, 0.0, 1.0);
    return mix(wh_params.n8_colorA, wh_params.n8_colorB, t);
}

fn wh_f7(p: vec3<f32>) -> vec4<f32> {
    return mix(wh_f6(p), wh_f8(p), clamp(wh_params.n7_amount, 0.0, 1.0));
}

fn wh_field(p: vec3<f32>) -> vec4<f32> {
    return wh_f7(p);
}

@group(1) @binding(0) var<storage, read> positions: array<vec4<f32>>;
@group(1) @binding(1) var<storage, read_write> colors: array<vec4<f32>>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    let i = id.x;
    if (i >= arrayLength(&positions)) {
        return;
    }
    colors[i] = wh_field(positions[i].xyz);
}
