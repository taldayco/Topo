// detail.h
#pragma once
#include "palettes.h"
#include <cstdint>
#include <span>
#include <vector>

struct DetailParams {
  bool enable_rocks = true;
  bool enable_moss = true;
  bool enable_grass = true;
  bool enable_hatching = false;

  float rock_density = 1.0f;
  float moss_density = 1.0f;
  float grass_density = 1.0f;
};

void add_procedural_details(std::vector<uint32_t> &pixels,
                            std::span<const float> heightmap, int width,
                            int height, const Palette &palette,
                            const DetailParams &params);
