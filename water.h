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
  int grid_width;
  int grid_height;
};

struct WaterBody {
  int plateau_index;
  float height;
  float min_x, max_x;
  float min_y, max_y;
  float aspect_ratio;
  std::vector<int> pixels;
  float time_offset;
  WaterMesh mesh;
};
struct WaveParams {
  float frequency = 0.3f;
  float amplitude = 0.02f;
  float speed = 1.0f;
};
std::vector<WaterBody>
identify_water_bodies(std::span<const float> heightmap, int width, int height,
                      const std::vector<Plateau> &plateaus,
                      const std::vector<int> &plateaus_with_columns);

void generate_water_mesh(WaterBody &water, float grid_spacing);
float get_water_height(float x, float y, float base_z, float time,
                       float time_offset);
void render_water(std::vector<uint32_t> &pixels, int view_width,
                  int view_height, const std::vector<WaterBody> &water_bodies,
                  float offset_x, float offset_y,
                  const struct IsometricParams &params, float time);
