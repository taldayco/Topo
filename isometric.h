#pragma once
#include "contour.h"
#include <cstdint>
#include <span>
#include <vector>

struct IsometricParams {
  float tile_width = 2.0f;   // Width of isometric tile in pixels
  float tile_height = 1.0f;  // Height of isometric tile in pixels
  float height_scale = 0.3f; // How much height affects Y position
};

struct IsometricView {
  int width;
  int height;
  std::vector<uint32_t> pixels;
};

// Convert world coordinates to isometric screen coordinates
void world_to_iso(float x, float y, float z, float &out_x, float &out_y,
                  const IsometricParams &params);

// Create isometric view of heightmap with contour lines
IsometricView create_isometric_heightmap(std::span<const float> heightmap,
                                         std::span<const Line> contour_lines,
                                         int map_width, int map_height,
                                         const IsometricParams &params);
