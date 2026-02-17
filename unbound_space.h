#pragma once
#include "hex.h"
#include "terrain_generator.h"
#include <cstdint>
#include <vector>

std::vector<HexColumn>
generate_unbound_space_columns(const std::vector<UnusedRegion> &regions,
                        std::span<const float> heightmap, int width, int height,
                        float hex_size, std::vector<int16_t> &terrain_map);

void render_unbound_space_columns(std::vector<uint32_t> &pixels, int view_width,
                           int view_height,
                           const std::vector<HexColumn> &columns,
                           float hex_size, float offset_x, float offset_y,
                           const IsometricParams &params,
                           const Palette &palette);
