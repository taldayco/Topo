// isometric.h
#pragma once
#include "contour.h"
#include "palettes.h"
#include <cstdint>
#include <span>
#include <vector>

struct IsometricParams {
  float tile_width = 2.0f;
  float tile_height = 1.0f;
  float height_scale = 0.3f;
};

struct IsometricView {
  int width;
  int height;
  std::vector<uint32_t> pixels;
};

void world_to_iso(float x, float y, float z, float &out_x, float &out_y,
                  const IsometricParams &params);

IsometricView create_isometric_heightmap(std::span<const float> heightmap,
                                         std::span<const Line> contour_lines,
                                         int map_width, int map_height,
                                         const IsometricParams &params,
                                         const Palette &palette,
                                         float contour_opacity);
