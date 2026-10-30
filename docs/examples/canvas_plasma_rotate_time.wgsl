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
    _pad: vec4<f32>,
}
@group(0) @binding(1) var<uniform> wh_params: WhParams;

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

fn wh_field(p: vec3<f32>) -> vec4<f32> {
    return wh_f2(p);
}

@group(1) @binding(0) var wh_canvas: texture_storage_2d<rgba16float, write>;

@compute @workgroup_size(8, 8)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    let size = textureDimensions(wh_canvas);
    if (id.x >= size.x || id.y >= size.y) {
        return;
    }
    let p = vec3<f32>((f32(id.x) + 0.5) / f32(size.x), (f32(id.y) + 0.5) / f32(size.y), 0.0);
    textureStore(wh_canvas, vec2<i32>(id.xy), wh_field(p));
}
