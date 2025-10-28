// detail.h
#pragma once
#include "palettes.h"
#include <cstdint>
#include <span>
#include <vector>

struct DetailParams {
  // Reserved for future basalt customization
  float column_size = 8.0f;
};

void add_procedural_details(std::vector<uint32_t> &pixels,
                            std::span<const float> heightmap, int width,
                            int height, const Palette &palette,
                            const DetailParams &params);
