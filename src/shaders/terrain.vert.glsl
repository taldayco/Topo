#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 frag_color;
layout(location = 1) out vec2 frag_screen_pos;

layout(set = 1, binding = 0) uniform Uniforms {
    vec4 proj_scale_offset;  // scale_x, scale_y, offset_x, offset_y
    vec4 params1;            // time, tile_width, tile_height, height_scale
    vec4 params2;            // iso_offset_x, iso_offset_y, contour_opacity, _pad
    vec4 lava_color;         // r, g, b, _pad
};

void main() {
    vec2 ndc = in_pos * proj_scale_offset.xy + proj_scale_offset.zw;
    gl_Position = vec4(ndc, 0.0, 1.0);
    frag_color = in_color;
    frag_screen_pos = in_pos;
}
