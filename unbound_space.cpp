#include "unbound_space.h"
#include "palettes.h"
#include "types.h"
#include "util.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct UnboundSpaceHexCoord {
  int q, r;
  bool operator==(const UnboundSpaceHexCoord &o) const { return q == o.q && r == o.r; }
};

struct UnboundSpaceHexHash {
  size_t operator()(const UnboundSpaceHexCoord &h) const { return hash2d(h.q, h.r); }
};

static void hex_to_pixel(int q, int r, float hex_size, float &out_x,
                         float &out_y) {
  const float sqrt3 = 1.732f;
  out_x = hex_size * 1.5f * q;
  out_y = hex_size * sqrt3 * (r + q * 0.5f);
}

static UnboundSpaceHexCoord pixel_to_hex(float x, float y, float hex_size) {
  const float sqrt3 = 1.732f;
  float q = (2.0f / 3.0f * x) / hex_size;
  float r = (-1.0f / 3.0f * x + sqrt3 / 3.0f * y) / hex_size;

  int iq = (int)std::round(q);
  int ir = (int)std::round(r);
  int is = (int)std::round(-q - r);

  float dq = std::abs(iq - q);
  float dr = std::abs(ir - r);
  float ds = std::abs(is - (-q - r));

  if (dq > dr && dq > ds) {
    iq = -ir - is;
  } else if (dr > ds) {
    ir = -iq - is;
  }

  return {iq, ir};
}

static void get_hex_corners_local(int q, int r, float hex_size,
                                  Vec2 corners[6]) {
  const float PI = 3.14159265359f;
  float cx, cy;
  hex_to_pixel(q, r, hex_size, cx, cy);

  for (int i = 0; i < 6; ++i) {
    float angle = i * PI / 3.0f;
    corners[i].x = cx + hex_size * std::cos(angle);
    corners[i].y = cy + hex_size * std::sin(angle);
  }
}

static bool pixel_in_hex_local(float px, float py, int q, int r,
                               float hex_size) {
  Vec2 corners[6];
  get_hex_corners_local(q, r, hex_size, corners);

  for (int i = 0; i < 6; ++i) {
    int next = (i + 1) % 6;
    float edge_x = corners[next].x - corners[i].x;
    float edge_y = corners[next].y - corners[i].y;
    float to_point_x = px - corners[i].x;
    float to_point_y = py - corners[i].y;
    float cross = edge_x * to_point_y - edge_y * to_point_x;
    if (cross < 0)
      return false;
  }
  return true;
}

static void compute_visible_edges(std::vector<HexColumn> &columns) {
  std::unordered_map<UnboundSpaceHexCoord, HexColumn *, UnboundSpaceHexHash> col_map;
  for (auto &col : columns) {
    UnboundSpaceHexCoord key = {col.q, col.r};
    col_map[key] = &col;
  }
  const int neighbors[6][2] = {{1, 0},  {0, 1},  {-1, 1},
                               {-1, 0}, {0, -1}, {1, -1}};
  for (auto &col : columns) {
    for (int i = 0; i < 6; ++i) {
      col.visible_edges[i] = false;
      col.edge_drops[i] = 0.0f;
      UnboundSpaceHexCoord neighbor_hc = {col.q + neighbors[i][0],
                                    col.r + neighbors[i][1]};
      auto it = col_map.find(neighbor_hc);
      if (it == col_map.end()) {
        col.visible_edges[i] = true;
        col.edge_drops[i] = col.height;
      } else {
        float height_diff = col.height - it->second->height;
        if (height_diff > 0.01f) {
          col.visible_edges[i] = true;
          col.edge_drops[i] = height_diff;
        }
      }
    }
  }
}

std::vector<HexColumn>
generate_unbound_space_columns(const std::vector<UnusedRegion> &regions,
                        std::span<const float> heightmap, int width, int height,
                        float hex_size, std::vector<int16_t> &terrain_map) {
  std::vector<HexColumn> columns;

  for (const auto &region : regions) {
    if (region.type != RegionType::UnboundSpace)
      continue;

    // Convert pixel set to unique hex coordinates
    std::unordered_set<UnboundSpaceHexCoord, UnboundSpaceHexHash> hex_set;
    for (int idx : region.pixels) {
      float px = (float)(idx % width);
      float py = (float)(idx / width);
      UnboundSpaceHexCoord hc = pixel_to_hex(px, py, hex_size);
      hex_set.insert(hc);
    }

    float uniform_height = region.avg_elevation;

    for (const auto &hc : hex_set) {
      columns.push_back({hc.q, hc.r, uniform_height, uniform_height});

      // Mark column footprint in terrain_map
      Vec2 corners[6];
      get_hex_corners_local(hc.q, hc.r, hex_size, corners);
      float fmin_x = 1e9f, fmax_x = -1e9f, fmin_y = 1e9f, fmax_y = -1e9f;
      for (int i = 0; i < 6; ++i) {
        fmin_x = std::min(fmin_x, corners[i].x);
        fmax_x = std::max(fmax_x, corners[i].x);
        fmin_y = std::min(fmin_y, corners[i].y);
        fmax_y = std::max(fmax_y, corners[i].y);
      }
      int x0 = std::max(0, (int)fmin_x - 1);
      int x1 = std::min(width - 1, (int)fmax_x + 1);
      int y0 = std::max(0, (int)fmin_y - 1);
      int y1 = std::min(height - 1, (int)fmax_y + 1);
      for (int ry = y0; ry <= y1; ++ry) {
        for (int rx = x0; rx <= x1; ++rx) {
          if (pixel_in_hex_local((float)rx, (float)ry, hc.q, hc.r, hex_size))
            terrain_map[ry * width + rx] = TERRAIN_UNBOUND_SPACE;
        }
      }
    }

    SDL_Log("  UnboundSpace region: %zu hexes from %zu pixels (height=%.3f)",
            hex_set.size(), region.pixels.size(), uniform_height);
  }

  for (auto &col : columns) {
    for (int i = 0; i < 6; ++i) {
      col.visible_edges[i] = false;
      col.edge_drops[i] = 0.0f;
    }
  }

  compute_visible_edges(columns);
  SDL_Log("Generated %zu unbound_space columns", columns.size());
  return columns;
}

static void project_hex_to_iso(const Vec2 corners[6], float z,
                               const IsometricParams &params,
                               IsoVec2 iso_corners[6]) {
  for (int i = 0; i < 6; ++i) {
    world_to_iso(corners[i].x, corners[i].y, z, iso_corners[i].x,
                 iso_corners[i].y, params);
  }
}

static bool point_in_hex(float px, float py, const IsoVec2 corners[6]) {
  for (int i = 0; i < 6; ++i) {
    int next = (i + 1) % 6;
    float edge_x = corners[next].x - corners[i].x;
    float edge_y = corners[next].y - corners[i].y;
    float to_point_x = px - corners[i].x;
    float to_point_y = py - corners[i].y;
    float cross = edge_x * to_point_y - edge_y * to_point_x;
    if (cross < 0)
      return false;
  }
  return true;
}

static void draw_filled_hex_top(std::vector<uint32_t> &pixels, int width,
                                int height, const IsoVec2 iso_corners[6],
                                float offset_x, float offset_y,
                                uint32_t color) {
  float min_x = 1e9f, max_x = -1e9f;
  float min_y = 1e9f, max_y = -1e9f;

  for (int i = 0; i < 6; ++i) {
    float x = iso_corners[i].x + offset_x;
    float y = iso_corners[i].y + offset_y;
    min_x = std::min(min_x, x);
    max_x = std::max(max_x, x);
    min_y = std::min(min_y, y);
    max_y = std::max(max_y, y);
  }

  int start_x = std::max(0, (int)min_x);
  int end_x = std::min(width - 1, (int)max_x + 1);
  int start_y = std::max(0, (int)min_y);
  int end_y = std::min(height - 1, (int)max_y + 1);

  for (int py = start_y; py <= end_y; ++py) {
    for (int px = start_x; px <= end_x; ++px) {
      float test_x = px - offset_x;
      float test_y = py - offset_y;

      if (point_in_hex(test_x, test_y, iso_corners)) {
        pixels[py * width + px] = color;
      }
    }
  }
}

static void draw_side_face_filled(std::vector<uint32_t> &pixels, int width,
                                  int height, const Vec2 &corner0,
                                  const Vec2 &corner1, float top_height,
                                  float bottom_height,
                                  const IsometricParams &params, float offset_x,
                                  float offset_y, uint32_t base_color) {
  if (top_height - bottom_height < 0.01f)
    return;

  IsoVec2 top0, top1, bot0, bot1;
  world_to_iso(corner0.x, corner0.y, top_height, top0.x, top0.y, params);
  world_to_iso(corner1.x, corner1.y, top_height, top1.x, top1.y, params);
  world_to_iso(corner0.x, corner0.y, bottom_height, bot0.x, bot0.y, params);
  world_to_iso(corner1.x, corner1.y, bottom_height, bot1.x, bot1.y, params);

  top0.x += offset_x;
  top0.y += offset_y;
  top1.x += offset_x;
  top1.y += offset_y;
  bot0.x += offset_x;
  bot0.y += offset_y;
  bot1.x += offset_x;
  bot1.y += offset_y;

  float darkness = 0.4f;
  uint8_t r = ((base_color >> 16) & 0xFF) * (1.0f - darkness);
  uint8_t g = ((base_color >> 8) & 0xFF) * (1.0f - darkness);
  uint8_t b = (base_color & 0xFF) * (1.0f - darkness);
  uint32_t side_color = 0xFF000000 | (r << 16) | (g << 8) | b;

  float min_y = std::min({top0.y, top1.y, bot0.y, bot1.y});
  float max_y = std::max({top0.y, top1.y, bot0.y, bot1.y});

  for (int py = (int)min_y; py <= (int)max_y; ++py) {
    if (py < 0 || py >= height)
      continue;

    float intersections[4];
    int count = 0;

    auto check_edge = [&](IsoVec2 a, IsoVec2 b) {
      if ((a.y <= py && b.y > py) || (b.y <= py && a.y > py)) {
        float t = (py - a.y) / (b.y - a.y);
        intersections[count++] = a.x + t * (b.x - a.x);
      }
    };

    check_edge(top0, top1);
    check_edge(top1, bot1);
    check_edge(bot1, bot0);
    check_edge(bot0, top0);

    if (count >= 2) {
      if (count > 2) {
        std::sort(intersections, intersections + count);
      }
      if (intersections[0] > intersections[1]) {
        std::swap(intersections[0], intersections[1]);
      }

      int x_start = std::max(0, (int)intersections[0]);
      int x_end = std::min(width - 1, (int)intersections[count - 1]);

      for (int px = x_start; px <= x_end; ++px) {
        pixels[py * width + px] = side_color;
      }
    }
  }
}

void render_unbound_space_columns(std::vector<uint32_t> &pixels, int view_width,
                           int view_height,
                           const std::vector<HexColumn> &columns,
                           float hex_size, float offset_x, float offset_y,
                           const IsometricParams &params,
                           const Palette &palette) {
  std::vector<const HexColumn *> sorted;
  sorted.reserve(columns.size());

  for (const auto &col : columns) {
    sorted.push_back(&col);
  }

  std::sort(sorted.begin(), sorted.end(),
            [](const HexColumn *a, const HexColumn *b) {
              return (a->q + a->r) > (b->q + b->r);
            });

  // Draw side faces back to front
  for (const auto *col : sorted) {
    uint32_t color = get_elevation_color_smooth(col->base_height, palette);

    Vec2 corners[6];
    get_hex_corners_local(col->q, col->r, hex_size, corners);

    for (int i = 0; i < 6; ++i) {
      if (col->visible_edges[i]) {
        int next = (i + 1) % 6;
        float neighbor_height = col->height - col->edge_drops[i];

        draw_side_face_filled(pixels, view_width, view_height, corners[i],
                              corners[next], col->height, neighbor_height,
                              params, offset_x, offset_y, color);
      }
    }
  }

  // Draw top faces back to front
  for (const auto *col : sorted) {
    uint32_t color = get_elevation_color_smooth(col->base_height, palette);

    Vec2 corners[6];
    get_hex_corners_local(col->q, col->r, hex_size, corners);

    IsoVec2 iso_corners[6];
    project_hex_to_iso(corners, col->height, params, iso_corners);

    draw_filled_hex_top(pixels, view_width, view_height, iso_corners, offset_x,
                        offset_y, color);
  }
}

void render_unbound_space_debug_overlay(std::vector<uint32_t> &pixels, int view_width,
    int view_height, const std::vector<UnusedRegion> &regions,
    std::span<const float> heightmap, int map_width,
    float offset_x, float offset_y, const IsometricParams &params) {
  constexpr float ALPHA = 0.35f;
  constexpr uint8_t cr = 14, cg = 14, cb = 14;

  for (const auto &region : regions) {
    if (region.type != RegionType::UnboundSpace)
      continue;

    for (int idx : region.pixels) {
      int wx = idx % map_width;
      int wy = idx / map_width;
      float z = heightmap[idx];

      float iso_x, iso_y;
      world_to_iso((float)wx, (float)wy, z, iso_x, iso_y, params);
      int sx = (int)(iso_x + offset_x);
      int sy = (int)(iso_y + offset_y);

      constexpr int R = 2;
      for (int dy = -R; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
          if (dx * dx + dy * dy > R * R)
            continue;
          int px = sx + dx;
          int py = sy + dy;
          if (px >= 0 && px < view_width && py >= 0 && py < view_height) {
            uint32_t dst = pixels[py * view_width + px];
            uint8_t dr = (dst >> 16) & 0xFF;
            uint8_t dg = (dst >> 8) & 0xFF;
            uint8_t db = dst & 0xFF;
            uint8_t out_r = (uint8_t)(cr * ALPHA + dr * (1.0f - ALPHA));
            uint8_t out_g = (uint8_t)(cg * ALPHA + dg * (1.0f - ALPHA));
            uint8_t out_b = (uint8_t)(cb * ALPHA + db * (1.0f - ALPHA));
            pixels[py * view_width + px] =
                0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
          }
        }
      }
    }
  }
}
