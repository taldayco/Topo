#version 450

layout(location = 0) in vec2 in_world_pos;
layout(location = 1) in float in_base_z;
layout(location = 2) in float in_time_offset;

layout(location = 0) out vec3 frag_color;

layout(set = 1, binding = 0) uniform Uniforms {
    vec4 proj_scale_offset;  // scale_x, scale_y, offset_x, offset_y
    vec4 params1;            // time, tile_width, tile_height, height_scale
    vec4 params2;            // iso_offset_x, iso_offset_y, contour_opacity, _pad
    vec4 lava_color;         // r, g, b, _pad
};

void main() {
    float time = params1.x;
    float tile_width = params1.y;
    float tile_height = params1.z;
    float height_scale = params1.w;

    // Wave animation
    float t = time + in_time_offset;
    float wave1 = sin(in_world_pos.x * 0.3 + t) * 0.02;
    float wave2 = sin(in_world_pos.y * 0.21 + t * 1.3) * 0.015;
    float wave3 = sin((in_world_pos.x + in_world_pos.y) * 0.15 + t * 0.8) * 0.01;
    float z = in_base_z + wave1 + wave2 + wave3;

    // World to isometric projection
    float iso_x = (in_world_pos.x - in_world_pos.y) * tile_width;
    float iso_y = (in_world_pos.x + in_world_pos.y) * tile_height - z * height_scale;

    // Add rendering offset
    iso_x += params2.x;
    iso_y += params2.y;

    // Project to NDC
    vec2 ndc = vec2(iso_x, iso_y) * proj_scale_offset.xy + proj_scale_offset.zw;
    gl_Position = vec4(ndc, 0.0, 1.0);

    // Modulate lava color by depth
    float depth_factor = clamp(0.9 + (z - in_base_z) * 2.0, 0.8, 1.1);
    frag_color = lava_color.rgb * depth_factor;
}
