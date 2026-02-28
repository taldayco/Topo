#pragma once
#include "config.h"
#include "terrain/contour.h"
#include <vector>

struct PointLightComponent {
    float pos_x, pos_y, pos_z;
    float radius;
    float color_r, color_g, color_b;
    float intensity;
};

constexpr bool DEFAULT_ISOMETRIC = true;

struct GamePhase {
  enum Phase { Menu, Playing, Paused };
  Phase current = Playing;
};

struct TerrainState {
  bool use_isometric = DEFAULT_ISOMETRIC;
  int current_palette = 0;
  float map_scale = Config::DEFAULT_MAP_SCALE;
  float contour_opacity = Config::DEFAULT_CONTOUR_OPACITY;
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
