
#pragma once
#include "terrain/basalt.h"
#include "terrain/contour.h"
#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

struct MapData;

struct ChannelRegion {
  std::vector<int> pixels;
  float min_x, max_x, min_y, max_y;
  float aspect_ratio;
  float avg_elevation;
};
struct LavaVertex {
  float x, y;
  float base_z;
};

struct LavaMesh {
  std::vector<LavaVertex> vertices;
  std::vector<uint32_t> indices;
  int grid_width = 0;
  int grid_height = 0;
  std::vector<uint8_t> active;
};

struct LavaBody {
  int plateau_index = -1;
  float height = 0.f;
  float min_x = 0, max_x = 0;
  float min_y = 0, max_y = 0;
  float aspect_ratio = 0.f;
  std::vector<int> pixels;
  float time_offset = 0.f;
  LavaMesh mesh;
  std::unordered_set<int> pixel_set;
};

struct WaveParams {
  float frequency = 0.3f;
  float amplitude = 0.02f;
  float speed = 1.0f;
};

std::vector<ChannelRegion>
extract_channel_spaces(std::span<const int16_t> terrain_map, int width,
                       int height, std::span<const float> heightmap);

std::vector<ChannelRegion>
filter_lava_channels(const std::vector<ChannelRegion> &regions,
                      std::span<const float> heightmap, int width, int height);
std::vector<LavaBody>
channels_to_lava_bodies(const std::vector<ChannelRegion> &channels,
                         std::span<const float> heightmap, int width,
                         int height);
std::vector<LavaBody>
identify_lava_bodies(std::span<const float> heightmap, int width, int height,
                      const std::vector<Plateau> &plateaus,
                      const std::vector<int> &plateaus_with_columns);

void generate_lava_mesh_masked(LavaBody &lava,
                                const std::vector<uint8_t> &mask, int mask_w,
                                int mask_h, float grid_spacing);

float get_lava_height(float x, float y, float base_z, float time,
                       float time_offset);

struct FloodFillResult {
  std::vector<LavaBody> lava_bodies;
  std::vector<LavaBody> void_bodies;
};

FloodFillResult generate_lava_and_void(MapData &data, float void_chance, int seed = 0);


