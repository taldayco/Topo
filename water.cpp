#include "water.h"
#include "basalt.h"
#include "config.h"
#include "plateau.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <unordered_set>
#include <vector>

static uint32_t hash_plateau(int idx) {
  return (idx * 374761393u) ^ 668265263u;
}

struct P2 {
  float x, y;
};

static float poly_area(const std::vector<P2> &P) {
  double A = 0.0;
  size_t n = P.size();
  for (size_t i = 0, j = n - 1; i < n; j = i++)
    A += (double)(P[j].x * P[i].y - P[i].x * P[j].y);
  return (float)(A * 0.5);
}

static bool point_in_tri(const P2 &p, const P2 &a, const P2 &b, const P2 &c) {
  float v0x = c.x - a.x, v0y = c.y - a.y;
  float v1x = b.x - a.x, v1y = b.y - a.y;
  float v2x = p.x - a.x, v2y = p.y - a.y;
  float d00 = v0x * v0x + v0y * v0y;
  float d01 = v0x * v1x + v0y * v1y;
  float d11 = v1x * v1x + v1y * v1y;
  float d20 = v2x * v0x + v2y * v0y;
  float d21 = v2x * v1x + v2y * v1y;
  float denom = d00 * d11 - d01 * d01;
  if (std::fabs(denom) < 1e-12f)
    return false;
  float v = (d11 * d20 - d01 * d21) / denom;
  float w = (d00 * d21 - d01 * d20) / denom;
  float u = 1.0f - v - w;
  return u >= 0.0f && v >= 0.0f && w >= 0.0f;
}

static bool is_ear(int i0, int i1, int i2, const std::vector<int> &idx,
                   const std::vector<P2> &P) {
  const P2 &a = P[idx[i0]];
  const P2 &b = P[idx[i1]];
  const P2 &c = P[idx[i2]];
  float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  if (cross <= 0.0f)
    return false;
  for (size_t k = 0; k < idx.size(); ++k) {
    if ((int)k == i0 || (int)k == i1 || (int)k == i2)
      continue;
    if (point_in_tri(P[idx[k]], a, b, c))
      return false;
  }
  return true;
}

static void triangulate_ear_clipping(const std::vector<P2> &P,
                                     std::vector<int> &tri_indices) {
  tri_indices.clear();
  if (P.size() < 3)
    return;
  std::vector<int> idx(P.size());
  for (size_t i = 0; i < P.size(); ++i)
    idx[i] = (int)i;
  if (poly_area(P) < 0.0f)
    std::reverse(idx.begin(), idx.end());
  int guard = 0;
  while (idx.size() > 3 && guard < 100000) {
    bool clipped = false;
    for (size_t i = 0; i < idx.size(); ++i) {
      int i0 = (int)((i + idx.size() - 1) % idx.size());
      int i1 = (int)i;
      int i2 = (int)((i + 1) % idx.size());
      if (is_ear(i0, i1, i2, idx, P)) {
        tri_indices.push_back(idx[i0]);
        tri_indices.push_back(idx[i1]);
        tri_indices.push_back(idx[i2]);
        idx.erase(idx.begin() + i1);
        clipped = true;
        break;
      }
    }
    if (!clipped)
      break;
    ++guard;
  }
  if (idx.size() == 3) {
    tri_indices.push_back(idx[0]);
    tri_indices.push_back(idx[1]);
    tri_indices.push_back(idx[2]);
  }
}

static void trace_outline_4connected(const std::vector<uint8_t> &mask, int W,
                                     int H, std::vector<P2> &out_poly) {
  out_poly.clear();
  int sx = -1, sy = -1;
  for (int y = 0; y < H && sy < 0; ++y)
    for (int x = 0; x < W; ++x)
      if (mask[(size_t)y * W + x]) {
        sx = x;
        sy = y;
        break;
      }
  if (sx < 0)
    return;
  int dirs[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
  int cx = sx, cy = sy, cd = 0;
  auto inside = [&](int x, int y) {
    return x >= 0 && y >= 0 && x < W && y < H;
  };
  auto filled = [&](int x, int y) {
    return inside(x, y) && mask[(size_t)y * W + x];
  };
  int loop_guard = 0;
  do {
    int left = (cd + 3) & 3;
    int lx = cx + dirs[left][0];
    int ly = cy + dirs[left][1];
    if (filled(lx, ly)) {
      cd = left;
      cx = lx;
      cy = ly;
    } else {
      int fx = cx + dirs[cd][0];
      int fy = cy + dirs[cd][1];
      if (filled(fx, fy)) {
        cx = fx;
        cy = fy;
      } else {
        cd = (cd + 1) & 3;
      }
    }
    float vx = cx + 0.5f;
    float vy = cy + 0.5f;
    if (out_poly.empty() || std::fabs(out_poly.back().x - vx) > 1e-4f ||
        std::fabs(out_poly.back().y - vy) > 1e-4f) {
      out_poly.push_back({vx, vy});
    }
    if (++loop_guard > W * H * 8)
      break;
  } while (!(cx == sx && cy == sy && out_poly.size() > 2));
  if (out_poly.size() >= 3 && poly_area(out_poly) < 0.0f)
    std::reverse(out_poly.begin(), out_poly.end());
}

static void build_triangle_mesh_from_polygon(const std::vector<P2> &poly,
                                             float z, WaterMesh &mesh_out) {
  mesh_out.vertices.clear();
  mesh_out.grid_width = 0;
  mesh_out.grid_height = 0;
  mesh_out.active.clear();
  if (poly.size() < 3)
    return;
  std::vector<int> tri_idx;
  triangulate_ear_clipping(poly, tri_idx);
  if (tri_idx.empty())
    return;
  mesh_out.vertices.reserve(tri_idx.size());
  for (size_t i = 0; i < tri_idx.size(); i += 3) {
    const P2 &a = poly[tri_idx[i + 0]];
    const P2 &b = poly[tri_idx[i + 1]];
    const P2 &c = poly[tri_idx[i + 2]];
    WaterVertex va{a.x, a.y, z};
    WaterVertex vb{b.x, b.y, z};
    WaterVertex vc{c.x, c.y, z};
    mesh_out.vertices.push_back(va);
    mesh_out.vertices.push_back(vb);
    mesh_out.vertices.push_back(vc);
  }
}

std::vector<WaterBody>
identify_water_bodies(std::span<const float> heightmap, int width, int height,
                      const std::vector<Plateau> &plateaus,
                      const std::vector<int> &plateaus_with_columns) {
  std::unordered_set<int> used(plateaus_with_columns.begin(),
                               plateaus_with_columns.end());
  float min_plateau_h = 1e9f;
  for (const auto &p : plateaus)
    min_plateau_h = std::min(min_plateau_h, p.height);

  std::vector<int> candidates;
  for (size_t i = 0; i < plateaus.size(); ++i)
    if (!used.count((int)i) && !plateaus[i].pixels.empty())
      candidates.push_back((int)i);

  if (candidates.empty()) {
    SDL_Log("Water: no unused plateaus available");
    return {};
  }

  uint32_t seed = 1469598103u;
  seed ^= (uint32_t)width + 0x9e3779b9u + (seed << 6) + (seed >> 2);
  seed ^= (uint32_t)height + 0x9e3779b9u + (seed << 6) + (seed >> 2);
  seed ^= (uint32_t)plateaus.size() + 0x9e3779b9u + (seed << 6) + (seed >> 2);
  std::mt19937 rng(seed);
  std::shuffle(candidates.begin(), candidates.end(), rng);
  if ((int)candidates.size() > 3)
    candidates.resize(3);

  std::vector<WaterBody> out;

  for (int pi : candidates) {
    const auto &plat = plateaus[pi];

    std::vector<uint8_t> mask((size_t)width * height, 0);
    for (int idx : plat.pixels)
      if ((uint32_t)idx < mask.size())
        mask[idx] = 1;

    float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f;
    for (int idx : plat.pixels) {
      int x = idx % width, y = idx / width;
      min_x = std::min(min_x, (float)x);
      max_x = std::max(max_x, (float)x);
      min_y = std::min(min_y, (float)y);
      max_y = std::max(max_y, (float)y);
    }

    std::vector<P2> poly_px;
    trace_outline_4connected(mask, width, height, poly_px);
    if (poly_px.size() < 3) {
      SDL_Log("Water: plateau %d produced no polygon outline", pi);
      continue;
    }

    float water_h = (std::fabs(plat.height - min_plateau_h) <= 1e-4f)
                        ? plat.height
                        : (plat.height - 0.015f);

    std::vector<P2> poly_world;
    poly_world.reserve(poly_px.size());
    for (const auto &p : poly_px)
      poly_world.push_back({p.x, p.y});

    WaterBody water;
    water.plateau_index = pi;
    water.height = water_h;
    water.min_x = min_x;
    water.max_x = max_x;
    water.min_y = min_y;
    water.max_y = max_y;
    float w = max_x - min_x + 1.f, h = max_y - min_y + 1.f;
    water.aspect_ratio = std::max(w, h) / std::max(1.0f, std::min(w, h));
    water.pixels = plat.pixels;
    water.time_offset = (hash_plateau(pi) % 1000) / 1000.0f * 6.283185f;

    build_triangle_mesh_from_polygon(poly_world, water.height, water.mesh);

    if (!water.mesh.vertices.empty())
      out.push_back(std::move(water));
  }

  SDL_Log("Water: produced %zu triangle bodies from %zu unused candidates",
          out.size(), candidates.size());
  return out;
}

float get_water_height(float x, float y, float base_z, float time,
                       float time_offset) {
  float t = time + time_offset;
  float wave1 = std::sin(x * 0.3f + t) * 0.02f;
  float wave2 = std::sin(y * 0.21f + t * 1.3f) * 0.015f;
  float wave3 = std::sin((x + y) * 0.15f + t * 0.8f) * 0.01f;
  return base_z + wave1 + wave2 + wave3;
}

static void draw_water_triangle(std::vector<uint32_t> &pixels, int width,
                                int height, float ax, float ay, float bx,
                                float by, float cx, float cy, uint32_t color) {
  float min_x = std::min({ax, bx, cx}), max_x = std::max({ax, bx, cx});
  float min_y = std::min({ay, by, cy}), max_y = std::max({ay, by, cy});
  for (int py = (int)min_y; py <= (int)max_y; ++py) {
    if (py < 0 || py >= height)
      continue;
    for (int px = (int)min_x; px <= (int)max_x; ++px) {
      if (px < 0 || px >= width)
        continue;
      float v0x = cx - ax, v0y = cy - ay, v1x = bx - ax, v1y = by - ay,
            v2x = px - ax, v2y = py - ay;
      float d00 = v0x * v0x + v0y * v0y, d01 = v0x * v1x + v0y * v1y,
            d11 = v1x * v1x + v1y * v1y;
      float d20 = v2x * v0x + v2y * v0y, d21 = v2x * v1x + v2y * v1y;
      float denom = d00 * d11 - d01 * d01;
      if (std::abs(denom) < 1e-6f)
        continue;
      float v = (d11 * d20 - d01 * d21) / denom,
            w = (d00 * d21 - d01 * d20) / denom, u = 1.f - v - w;
      if (u >= 0 && v >= 0 && w >= 0)
        pixels[py * width + px] = color;
    }
  }
}

void render_water(std::vector<uint32_t> &pixels, int view_w, int view_h,
                  const std::vector<WaterBody> &water_bodies, float off_x,
                  float off_y, const IsometricParams &params, float time) {
  uint32_t water_base = Config::WATER_COLOR;

  for (const auto &water : water_bodies) {
    const auto &verts = water.mesh.vertices;
    if (verts.size() < 3)
      continue;

    for (size_t i = 0; i + 2 < verts.size(); i += 3) {
      const auto &v0 = verts[i + 0];
      const auto &v1 = verts[i + 1];
      const auto &v2 = verts[i + 2];

      float z0 =
          get_water_height(v0.x, v0.y, v0.base_z, time, water.time_offset);
      float z1 =
          get_water_height(v1.x, v1.y, v1.base_z, time, water.time_offset);
      float z2 =
          get_water_height(v2.x, v2.y, v2.base_z, time, water.time_offset);

      float x0, y0, x1, y1, x2, y2;
      world_to_iso(v0.x, v0.y, z0, x0, y0, params);
      world_to_iso(v1.x, v1.y, z1, x1, y1, params);
      world_to_iso(v2.x, v2.y, z2, x2, y2, params);

      x0 += off_x;
      y0 += off_y;
      x1 += off_x;
      y1 += off_y;
      x2 += off_x;
      y2 += off_y;

      float avg_z = (z0 + z1 + z2) / 3.0f;
      float depth_factor =
          std::clamp(0.9f + (avg_z - v0.base_z) * 2.0f, 0.8f, 1.1f);
      uint8_t r = (uint8_t)(((water_base >> 16) & 0xFF) * depth_factor);
      uint8_t g = (uint8_t)(((water_base >> 8) & 0xFF) * depth_factor);
      uint8_t b = (uint8_t)((water_base & 0xFF) * depth_factor);
      uint32_t color = 0xFF000000 | (r << 16) | (g << 8) | b;

      draw_water_triangle(pixels, view_w, view_h, x0, y0, x1, y1, x2, y2,
                          color);
    }
  }

  SDL_Log("Rendered %zu water bodies", water_bodies.size());
}
