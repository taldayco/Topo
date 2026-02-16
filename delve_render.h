// delve_render.h
#pragma once
#include "contour.h"
#include "detail.h"
#include "palettes.h"
#include <cstdint>
#include <span>
#include <vector>

struct PixelBuffer {
  std::vector<uint32_t> pixels;
  int width;
  int height;
};

PixelBuffer generate_map_pixels(std::span<const float> heightmap,
                                std::span<const int> band_map,
                                std::span<const Line> contour_lines, int width,
                                int height, bool use_isometric,
                                const Palette &palette,
                                const DetailParams &detail_params,
                                float contour_opacity, float iso_padding,
                                float iso_offset_x_adjust,
                                float iso_offset_y_adjust);
