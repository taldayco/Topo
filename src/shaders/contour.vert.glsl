#version 450

layout(location = 0) in vec2 in_pos;

layout(location = 0) out float frag_opacity;

layout(set = 1, binding = 0) uniform Uniforms {
    vec4 proj_scale_offset;  // scale_x, scale_y, offset_x, offset_y
    vec4 params1;            // time, tile_width, tile_height, height_scale
    vec4 params2;            // iso_offset_x, iso_offset_y, contour_opacity, _pad
    vec4 lava_color;         // r, g, b, _pad
};

void main() {
    vec2 ndc = in_pos * proj_scale_offset.xy + proj_scale_offset.zw;
    gl_Position = vec4(ndc, 0.0, 1.0);
    frag_opacity = params2.z;
}
