#pragma once
#include "isometric.h"
#include "terrain_generator.h"
#include <cstdint>
#include <span>
#include <vector>

void render_crystal_debug_overlay(std::vector<uint32_t> &pixels, int view_width,
    int view_height, const std::vector<UnusedRegion> &regions,
    std::span<const float> heightmap, int map_width,
    float offset_x, float offset_y, const IsometricParams &params);
