#pragma once
#include "terrain/isometric.h"
#include "core/types.h"
#include "terrain/util.h"
#include <cstdint>
#include <vector>

struct HexColumn {
  int q, r;
  float height;
  float base_height;
  bool visible_edges[6];
  float edge_drops[6];
};

struct HexCoord {
  int q, r;
  bool operator==(const HexCoord &o) const { return q == o.q && r == o.r; }
};

struct HexHash {
  size_t operator()(const HexCoord &h) const { return hash2d(h.q, h.r); }
};

void hex_to_pixel(int q, int r, float hex_size, float &out_x, float &out_y);
HexCoord pixel_to_hex(float x, float y, float hex_size);
void get_hex_corners(int q, int r, float hex_size, Vec2 corners[6]);
bool pixel_in_hex(float px, float py, int q, int r, float hex_size);
void project_hex_to_iso(const Vec2 corners[6], float z,
                        const IsometricParams &params,
                        IsoVec2 iso_corners[6]);
bool point_in_hex_iso(float px, float py, const IsoVec2 corners[6]);
void compute_visible_edges(std::vector<HexColumn> &columns);
void draw_filled_hex_top(std::vector<uint32_t> &pixels, int width, int height,
                         const IsoVec2 iso_corners[6], float offset_x,
                         float offset_y, uint32_t color);
void draw_side_face_filled(std::vector<uint32_t> &pixels, int width, int height,
                           const Vec2 &corner0, const Vec2 &corner1,
                           float top_height, float bottom_height,
                           const IsometricParams &params, float offset_x,
                           float offset_y, uint32_t base_color);
