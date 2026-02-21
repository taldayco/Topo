#pragma once
#include "config.h"
#include "terrain/contour.h"
#include "terrain/detail.h"
#include "core/types.h"
#include <vector>

constexpr bool DEFAULT_ISOMETRIC = true;

struct GamePhase {
  enum Phase { Menu, Playing, Paused };
  Phase current = Playing;
};

struct TerrainState {
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
};

struct WindowState {
  bool launch_game_requested = false;
  bool close_game_requested = false;
};

struct ContourData {
  std::vector<float> heightmap;
  std::vector<int> band_map;
  std::vector<Line> contour_lines;
};
