// water.h  (additions are minimal and self-contained)
#pragma once
#include "basalt.h"
#include "plateau.h"
#include "types.h"
#include <cstdint>
#include <span>
#include <vector>

struct WaterVertex {
  float x, y;
  float base_z;
};

struct WaterMesh {
  std::vector<WaterVertex> vertices;
  int grid_width = 0;
  int grid_height = 0;
  std::vector<uint8_t> active; // size: (grid_width - 1) * (grid_height - 1)
};

struct WaterBody {
  int plateau_index = -1;
  float height = 0.f;
  float min_x = 0, max_x = 0;
  float min_y = 0, max_y = 0;
  float aspect_ratio = 0.f;
  std::vector<int> pixels;
  float time_offset = 0.f;
  WaterMesh mesh;
};

struct WaveParams {
  float frequency = 0.3f;
  float amplitude = 0.02f;
  float speed = 1.0f;
};

// Keep the same signature TerrainGenerator already calls:
std::vector<WaterBody>
identify_water_bodies(std::span<const float> heightmap, int width, int height,
                      const std::vector<Plateau> &plateaus,
                      const std::vector<int> &plateaus_with_columns);

// NEW: masked water grid built from a region mask (per-plateau)
void generate_water_mesh_masked(WaterBody &water,
                                const std::vector<uint8_t> &mask, int mask_w,
                                int mask_h, float grid_spacing);

float get_water_height(float x, float y, float base_z, float time,
                       float time_offset);

void render_water(std::vector<uint32_t> &pixels, int view_width,
                  int view_height, const std::vector<WaterBody> &water_bodies,
                  float offset_x, float offset_y,
                  const struct IsometricParams &params, float time);
