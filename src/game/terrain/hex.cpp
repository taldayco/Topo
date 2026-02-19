#include "terrain/hex.h"
#include "terrain/color.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

void hex_to_pixel(int q, int r, float hex_size, float &out_x, float &out_y) {
  const float sqrt3 = 1.732f;
  out_x = hex_size * 1.5f * q;
  out_y = hex_size * sqrt3 * (r + q * 0.5f);
}

HexCoord pixel_to_hex(float x, float y, float hex_size) {
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

void get_hex_corners(int q, int r, float hex_size, Vec2 corners[6]) {
  const float PI = 3.14159265359f;

  float cx, cy;
  hex_to_pixel(q, r, hex_size, cx, cy);

  for (int i = 0; i < 6; ++i) {
    float angle = i * PI / 3.0f;
    corners[i].x = cx + hex_size * std::cos(angle);
    corners[i].y = cy + hex_size * std::sin(angle);
  }
}

bool pixel_in_hex(float px, float py, int q, int r, float hex_size) {
  Vec2 corners[6];
  get_hex_corners(q, r, hex_size, corners);

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

void project_hex_to_iso(const Vec2 corners[6], float z,
                        const IsometricParams &params,
                        IsoVec2 iso_corners[6]) {
  for (int i = 0; i < 6; ++i) {
    world_to_iso(corners[i].x, corners[i].y, z, iso_corners[i].x,
                 iso_corners[i].y, params);
  }
}

bool point_in_hex_iso(float px, float py, const IsoVec2 corners[6]) {
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

void compute_visible_edges(std::vector<HexColumn> &columns) {
  std::unordered_map<HexCoord, HexColumn *, HexHash> col_map;
  for (auto &col : columns) {
    HexCoord key = {col.q, col.r};
    col_map[key] = &col;
  }
  const int neighbors[6][2] = {{1, 0},  {0, 1},  {-1, 1},
                               {-1, 0}, {0, -1}, {1, -1}};
  for (auto &col : columns) {
    for (int i = 0; i < 6; ++i) {
      col.visible_edges[i] = false;
      col.edge_drops[i] = 0.0f;
      HexCoord neighbor_hc = {col.q + neighbors[i][0],
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

void draw_filled_hex_top(std::vector<uint32_t> &pixels, int width, int height,
                         const IsoVec2 iso_corners[6], float offset_x,
                         float offset_y, uint32_t color) {
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

      if (point_in_hex_iso(test_x, test_y, iso_corners)) {
        pixels[py * width + px] = color;
      }
    }
  }
}

void draw_side_face_filled(std::vector<uint32_t> &pixels, int width,
                           int height, const Vec2 &corner0,
                           const Vec2 &corner1, float top_height,
                           float bottom_height, const IsometricParams &params,
                           float offset_x, float offset_y,
                           uint32_t base_color) {
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

  uint32_t side_color = darken_color(base_color, 0.4f);

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
