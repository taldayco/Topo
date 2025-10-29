// water.h  (additions are minimal and self-contained)
#pragma once
#include "basalt.h"
#include "plateau.h"
#include "types.h"
#include <cstdint>
#include <span>
#include <vector>

struct ChannelRegion {
  std::vector<int> pixels;
  float min_x, max_x, min_y, max_y;
  float aspect_ratio;
};
struct WaterVertex {
  float x, y;
  float base_z;
};

struct WaterMesh {
  std::vector<WaterVertex> vertices;
  int grid_width = 0;
  int grid_height = 0;
  std::vector<uint8_t> active;
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

std::vector<ChannelRegion>
extract_channel_spaces(const std::vector<HexColumn> &columns, int width,
                       int height);
std::vector<ChannelRegion>
filter_water_channels(const std::vector<ChannelRegion> &regions,
                      std::span<const float> heightmap, int width, int height);
std::vector<WaterBody>
channels_to_water_bodies(const std::vector<ChannelRegion> &channels,
                         std::span<const float> heightmap, int width,
                         int height);
std::vector<WaterBody>
identify_water_bodies(std::span<const float> heightmap, int width, int height,
                      const std::vector<Plateau> &plateaus,
                      const std::vector<int> &plateaus_with_columns);

void generate_water_mesh_masked(WaterBody &water,
                                const std::vector<uint8_t> &mask, int mask_w,
                                int mask_h, float grid_spacing);

float get_water_height(float x, float y, float base_z, float time,
                       float time_offset);

void render_water(std::vector<uint32_t> &pixels, int view_width,
                  int view_height, const std::vector<WaterBody> &water_bodies,
                  float offset_x, float offset_y,
                  const struct IsometricParams &params, float time);
