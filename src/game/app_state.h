#pragma once
#include "config.h"
#include "terrain/contour.h"
#include "terrain/detail.h"
#include "terrain/noise.h"
#include "core/types.h"
#include <vector>

constexpr NoiseParams DEFAULT_NOISE = {
    Config::DEFAULT_NOISE_SCALE,      Config::DEFAULT_NOISE_OCTAVES,
    Config::DEFAULT_NOISE_LACUNARITY, Config::DEFAULT_NOISE_GAIN,
    Config::DEFAULT_NOISE_SEED,       Config::DEFAULT_NOISE_LEVELS};
constexpr bool DEFAULT_ISOMETRIC = true;

struct AppState {
  NoiseParams noise_params = DEFAULT_NOISE;
  float contour_interval = Config::DEFAULT_CONTOUR_INTERVAL;
  bool use_isometric = DEFAULT_ISOMETRIC;
  int current_palette = 0;
  float map_scale = Config::DEFAULT_MAP_SCALE;
  float iso_padding = Config::DEFAULT_ISO_PADDING;
  float iso_offset_x_adjust = 0.0f;
  float iso_offset_y_adjust = 0.0f;
  float contour_opacity = Config::DEFAULT_CONTOUR_OPACITY;
  DetailParams detail_params = {};
  ViewState view;
  bool need_regenerate = true;
  bool launch_game_requested = false;
  bool close_game_requested = false;
  std::vector<float> heightmap;
  std::vector<int> band_map;
  std::vector<Line> contour_lines;
};
