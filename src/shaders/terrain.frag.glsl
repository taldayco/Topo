#version 450

layout(location = 0) in vec3 frag_color;
layout(location = 1) in vec2 frag_screen_pos;

layout(location = 0) out vec4 out_color;

void main() {
    // Hex-cell dither (matches CPU apply_hex_dither)
    float hex_size = 8.0;
    float sqrt3 = 1.732;
    float q = frag_screen_pos.x / (hex_size * sqrt3);
    float r = frag_screen_pos.y / hex_size - q * 0.5;

    int iq = int(round(q));
    int ir = int(round(r));
    int is = int(round(-q - r));

    float dq = abs(float(iq) - q);
    float dr = abs(float(ir) - r);
    float ds = abs(float(is) - (-q - r));

    if (dq > dr && dq > ds) iq = -ir - is;
    else if (dr > ds) ir = -iq - is;

    uint hash = uint(iq) * 374761393u ^ uint(ir) * 668265263u;
    float threshold = (float(hash & 0xFFu) / 255.0 - 0.5) * 0.25;

    vec3 dithered = frag_color * (1.0 + threshold);
    out_color = vec4(clamp(dithered, 0.0, 1.0), 1.0);
}
