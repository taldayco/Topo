#include "terrain/terrain_mesh.h"
#include "game_state.h"
#include "config.h"
#include "terrain/basalt.h"
#include "terrain/hex.h"
#include "terrain/lava.h"
#include "terrain/map_data.h"
#include "terrain/palettes.h"
#include "terrain/color.h"
#include "core/types.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

static void color_to_float(uint32_t c, float &r, float &g, float &b) {
  r = ((c >> 16) & 0xFF) / 255.0f;
  g = ((c >> 8) & 0xFF) / 255.0f;
  b = (c & 0xFF) / 255.0f;
}

// Add a hex top face: 6 vertices, 4 triangles (fan from vertex 0)
static void add_hex_top(TerrainMesh &mesh, const IsoVec2 iso_corners[6],
                        float offset_x, float offset_y,
                        float cr, float cg, float cb) {
  uint32_t base = (uint32_t)mesh.basalt_vertices.size();
  for (int i = 0; i < 6; ++i) {
    mesh.basalt_vertices.push_back({
        iso_corners[i].x + offset_x,
        iso_corners[i].y + offset_y,
        cr, cg, cb});
  }
  // Fan triangles: (0,1,2), (0,2,3), (0,3,4), (0,4,5)
  for (int i = 1; i <= 4; ++i) {
    mesh.basalt_indices.push_back(base);
    mesh.basalt_indices.push_back(base + i);
    mesh.basalt_indices.push_back(base + i + 1);
  }
}

// Add a side face quad: 4 vertices, 2 triangles
static void add_side_face(TerrainMesh &mesh, const Vec2 &corner0,
                          const Vec2 &corner1, float top_height,
                          float bottom_height, const IsometricParams &params,
                          float offset_x, float offset_y,
                          float cr, float cg, float cb) {
  if (top_height - bottom_height < 0.01f)
    return;

  IsoVec2 top0, top1, bot0, bot1;
  world_to_iso(corner0.x, corner0.y, top_height, top0.x, top0.y, params);
  world_to_iso(corner1.x, corner1.y, top_height, top1.x, top1.y, params);
  world_to_iso(corner0.x, corner0.y, bottom_height, bot0.x, bot0.y, params);
  world_to_iso(corner1.x, corner1.y, bottom_height, bot1.x, bot1.y, params);

  // Darken side color (same as CPU: darken_color(base, 0.4) = base * 0.6)
  float sr = cr * 0.6f, sg = cg * 0.6f, sb = cb * 0.6f;

  uint32_t base = (uint32_t)mesh.basalt_vertices.size();
  mesh.basalt_vertices.push_back({top0.x + offset_x, top0.y + offset_y, sr, sg, sb});
  mesh.basalt_vertices.push_back({top1.x + offset_x, top1.y + offset_y, sr, sg, sb});
  mesh.basalt_vertices.push_back({bot1.x + offset_x, bot1.y + offset_y, sr, sg, sb});
  mesh.basalt_vertices.push_back({bot0.x + offset_x, bot0.y + offset_y, sr, sg, sb});

  // Two triangles: (0,1,2) and (0,2,3)
  mesh.basalt_indices.push_back(base);
  mesh.basalt_indices.push_back(base + 1);
  mesh.basalt_indices.push_back(base + 2);
  mesh.basalt_indices.push_back(base);
  mesh.basalt_indices.push_back(base + 2);
  mesh.basalt_indices.push_back(base + 3);
}

TerrainMesh build_terrain_mesh(const TerrainState &terrain, const MapData &map_data,
                               const ContourData &contours) {
  TerrainMesh mesh;

  mesh.iso_params.tile_width = Config::ISO_TILE_WIDTH;
  mesh.iso_params.tile_height = Config::ISO_TILE_HEIGHT;
  mesh.iso_params.height_scale = Config::ISO_HEIGHT_SCALE;

  const auto &columns = map_data.columns;
  const auto &lava_bodies = map_data.lava_bodies;

  if (columns.empty()) {
    SDL_Log("TerrainMesh: No columns, empty mesh");
    return mesh;
  }

  // Compute iso bounds for the scene
  float min_x = 1e9f, max_x = -1e9f;
  float min_y = 1e9f, max_y = -1e9f;

  float test_heights[] = {0.0f, 0.5f, 1.0f};
  int test_coords[][2] = {{0, 0},
                          {Config::MAP_WIDTH, 0},
                          {Config::MAP_WIDTH, Config::MAP_HEIGHT},
                          {0, Config::MAP_HEIGHT}};

  for (auto h : test_heights) {
    for (auto [x, y] : test_coords) {
      float iso_x, iso_y;
      world_to_iso(x, y, h, iso_x, iso_y, mesh.iso_params);
      min_x = std::min(min_x, iso_x);
      max_x = std::max(max_x, iso_x);
      min_y = std::min(min_y, iso_y);
      max_y = std::max(max_y, iso_y);
    }
  }

  float padding = terrain.iso_padding;
  mesh.iso_offset_x = -min_x + padding + terrain.iso_offset_x_adjust;
  mesh.iso_offset_y = -min_y + padding + terrain.iso_offset_y_adjust;
  mesh.scene_width = (max_x - min_x) + padding * 2;
  mesh.scene_height = (max_y - min_y) + padding * 2;

  const Palette &palette = PALETTES[terrain.current_palette];

  // Sort columns back-to-front by (q+r) descending
  std::vector<const HexColumn *> sorted;
  sorted.reserve(columns.size());
  for (const auto &col : columns)
    sorted.push_back(&col);
  std::sort(sorted.begin(), sorted.end(),
            [](const HexColumn *a, const HexColumn *b) {
              return (a->q + a->r) > (b->q + b->r);
            });

  // --- Pass 1: Side faces ---
  for (const auto *col : sorted) {
    uint32_t color = organic_color(col->base_height, col->q, col->r, palette);
    float cr, cg, cb;
    color_to_float(color, cr, cg, cb);

    Vec2 corners[6];
    get_hex_corners(col->q, col->r, Config::HEX_SIZE, corners);

    for (int i = 0; i < 6; ++i) {
      if (col->visible_edges[i]) {
        int next = (i + 1) % 6;
        float neighbor_height = col->height - col->edge_drops[i];
        add_side_face(mesh, corners[i], corners[next], col->height,
                      neighbor_height, mesh.iso_params, mesh.iso_offset_x,
                      mesh.iso_offset_y, cr, cg, cb);
      }
    }
  }

  mesh.side_index_count = (uint32_t)mesh.basalt_indices.size();

  // --- Pass 2: Top faces ---
  for (const auto *col : sorted) {
    uint32_t color = organic_color(col->base_height, col->q, col->r, palette);
    float cr, cg, cb;
    color_to_float(color, cr, cg, cb);

    Vec2 corners[6];
    get_hex_corners(col->q, col->r, Config::HEX_SIZE, corners);

    IsoVec2 iso_corners[6];
    project_hex_to_iso(corners, col->height, mesh.iso_params, iso_corners);

    add_hex_top(mesh, iso_corners, mesh.iso_offset_x, mesh.iso_offset_y,
                cr, cg, cb);
  }

  SDL_Log("TerrainMesh: %zu basalt verts, %zu indices (sides: %u, tops: %zu)",
          mesh.basalt_vertices.size(), mesh.basalt_indices.size(),
          mesh.side_index_count,
          mesh.basalt_indices.size() - mesh.side_index_count);

  // --- Lava vertices ---
  for (const auto &lava : lava_bodies) {
    for (int idx : lava.pixels) {
      int px = idx % Config::MAP_WIDTH;
      int py = idx / Config::MAP_WIDTH;
      mesh.lava_vertices.push_back(
          {(float)px, (float)py, lava.height, lava.time_offset});
    }
  }

  // Also add triangle mesh vertices for lava bodies
  // We use the pixel-based approach (matching CPU render_lava) for now,
  // since the triangle mesh approach produces sparse coverage.
  // The pixel approach renders each lava pixel as a point in iso space.

  SDL_Log("TerrainMesh: %zu lava vertices", mesh.lava_vertices.size());

  // --- Contour lines ---
  for (const auto &line : contours.contour_lines) {
    float iso_x0, iso_y0, iso_x1, iso_y1;
    world_to_iso(line.x1, line.y1, line.elevation, iso_x0, iso_y0,
                 mesh.iso_params);
    world_to_iso(line.x2, line.y2, line.elevation, iso_x1, iso_y1,
                 mesh.iso_params);
    mesh.contour_vertices.push_back(
        {iso_x0 + mesh.iso_offset_x, iso_y0 + mesh.iso_offset_y});
    mesh.contour_vertices.push_back(
        {iso_x1 + mesh.iso_offset_x, iso_y1 + mesh.iso_offset_y});
  }

  SDL_Log("TerrainMesh: %zu contour vertices (%zu lines)",
          mesh.contour_vertices.size(), contours.contour_lines.size());

  return mesh;
}

SceneUniforms compute_uniforms(const TerrainMesh &mesh, const ViewState &view,
                               uint32_t viewport_w, uint32_t viewport_h,
                               float time, float contour_opacity) {
  SceneUniforms u = {};
  u.time = time;
  u.tile_width = mesh.iso_params.tile_width;
  u.tile_height = mesh.iso_params.tile_height;
  u.height_scale = mesh.iso_params.height_scale;
  u.iso_offset_x = mesh.iso_offset_x;
  u.iso_offset_y = mesh.iso_offset_y;
  u.contour_opacity = contour_opacity;

  // LAVA_COLOR is 0xFF4488CC (ARGB) -> R=0x44, G=0x88, B=0xCC
  u.lava_color_r = 0x44 / 255.0f;
  u.lava_color_g = 0x88 / 255.0f;
  u.lava_color_b = 0xCC / 255.0f;

  // Compute orthographic projection from iso-screen space to NDC
  float vis_w = mesh.scene_width / view.zoom;
  float vis_h = mesh.scene_height / view.zoom;
  float center_x = view.pan_x * mesh.scene_width;
  float center_y = view.pan_y * mesh.scene_height;

  float left = center_x - vis_w * 0.5f;
  float right = center_x + vis_w * 0.5f;
  float top = center_y - vis_h * 0.5f;
  float bottom = center_y + vis_h * 0.5f;

  // Aspect ratio correction
  float vis_aspect = vis_w / vis_h;
  float vp_aspect = (float)viewport_w / (float)viewport_h;

  if (vp_aspect > vis_aspect) {
    float extra = vis_h * vp_aspect - vis_w;
    left -= extra * 0.5f;
    right += extra * 0.5f;
  } else {
    float extra = vis_w / vp_aspect - vis_h;
    top -= extra * 0.5f;
    bottom += extra * 0.5f;
  }

  // ortho: ndc_x = x * 2/(right-left) - (right+left)/(right-left)
  //         ndc_y = -y * 2/(bottom-top) + (bottom+top)/(bottom-top)
  u.proj_scale_x = 2.0f / (right - left);
  u.proj_scale_y = -2.0f / (bottom - top);
  u.proj_offset_x = -(right + left) / (right - left);
  u.proj_offset_y = (bottom + top) / (bottom - top);

  return u;
}
