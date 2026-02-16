#pragma once
#include "isometric.h"
#include "contour.h"
#include "types.h"
#include <cstdint>
#include <vector>

struct HexColumn {
  int q, r;
  float height;
  float base_height;
  bool visible_edges[6];
  float edge_drops[6];
};

std::vector<HexColumn>
generate_basalt_columns(std::span<const float> heightmap, int width, int height,
                        float hex_size,
                        const std::vector<Plateau> &plateaus,
                        std::vector<int> &plateaus_with_columns_out,
                        std::vector<int16_t> &terrain_map);
void get_hex_corners(int q, int r, float hex_size, Vec2 corners[6]);
bool pixel_in_hex(float px, float py, int q, int r, float hex_size);
void render_basalt_columns(std::vector<uint32_t> &pixels, int view_width,
                           int view_height,
                           const std::vector<HexColumn> &columns,
                           float hex_size, float offset_x, float offset_y,
                           const struct IsometricParams &params,
                           const struct Palette &palette);
