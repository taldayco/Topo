#pragma once
#include "palettes.h"
#include <cstdint>
#include <span>
#include <vector>

struct DetailParams {
  float column_size = 8.0f;
  float dither_strength = 0.15f;
};

void add_procedural_details(std::vector<uint32_t> &pixels,
                            std::span<const float> heightmap, int width,
                            int height, const Palette &palette,
                            const DetailParams &params);
