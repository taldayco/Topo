#pragma once
#include "terrain/isometric.h"
#include "terrain/terrain_generator.h"
#include <cstdint>
#include <vector>

struct AppState;

struct BasaltVertex {
  float pos_x, pos_y;
  float color_r, color_g, color_b;
};

struct GpuLavaVertex {
  float world_x, world_y;
  float base_z;
  float time_offset;
};

struct ContourVertex {
  float pos_x, pos_y;
};

struct SceneUniforms {
  float proj_scale_x, proj_scale_y, proj_offset_x, proj_offset_y;
  float time, tile_width, tile_height, height_scale;
  float iso_offset_x, iso_offset_y, contour_opacity, _pad0;
  float lava_color_r, lava_color_g, lava_color_b, _pad1;
};

struct TerrainMesh {
  std::vector<BasaltVertex> basalt_vertices;
  std::vector<uint32_t> basalt_indices;
  uint32_t side_index_count = 0;

  std::vector<GpuLavaVertex> lava_vertices;

  std::vector<ContourVertex> contour_vertices;

  float scene_width = 0, scene_height = 0;
  float iso_offset_x = 0, iso_offset_y = 0;
  IsometricParams iso_params;
};

TerrainMesh build_terrain_mesh(const AppState &state);

SceneUniforms compute_uniforms(const TerrainMesh &mesh, const struct ViewState &view,
                               uint32_t viewport_w, uint32_t viewport_h,
                               float time, float contour_opacity);
