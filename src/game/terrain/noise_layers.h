#pragma once
#include <vector>

struct ElevationParams {
  float frequency = 0.003f;
  int octaves = 6;
  float lacunarity = 2.0f;
  float gain = 0.5f;
  int seed = 1337;
  float scurve_bias = 0.65f;
  float map_scale = 1.0f;
};

struct RiverParams {
  float frequency = 0.008f;
  int octaves = 4;
  float lacunarity = 2.0f;
  float gain = 0.5f;
  int seed = 7331;
  float threshold = 0.7f;
  float map_scale = 1.0f;
};

struct WorleyParams {
  float frequency = 0.015f;
  int seed = 4242;
  float jitter = 1.0f;
  float map_scale = 1.0f;
};

void generate_elevation_layer(std::vector<float> &out, int width, int height,
                              const ElevationParams &params);

void generate_river_mask(std::vector<float> &out, int width, int height,
                         const RiverParams &params);

void generate_worley_layer(std::vector<float> &out_value,
                           std::vector<float> &out_edge, int width, int height,
                           const WorleyParams &params);
