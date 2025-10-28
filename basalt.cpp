#include "basalt.h"
#include "isometric.h"
#include "palettes.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static uint32_t hash2d(int x, int y) {
  uint32_t h = (x * 374761393) ^ (y * 668265263);
  return (h ^ (h >> 13)) * 1274126177;
}

struct HexCoord {
  int q, r;
  bool operator==(const HexCoord &o) const { return q == o.q && r == o.r; }
};

struct HexHash {
  size_t operator()(const HexCoord &h) const { return hash2d(h.q, h.r); }
};

static void hex_to_pixel(int q, int r, float hex_size, float &out_x,
                         float &out_y) {
  const float sqrt3 = 1.732f;
  // Flat-topped hex formulas
  out_x = hex_size * 1.5f * q;
  out_y = hex_size * sqrt3 * (r + q * 0.5f);
}

static HexCoord pixel_to_hex(float x, float y, float hex_size) {
  const float sqrt3 = 1.732f;
  // Flat-topped hex formulas
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
  // ... existing code
  const float PI = 3.14159f;
  float cx, cy;
  hex_to_pixel(q, r, hex_size, cx, cy);

  for (int i = 0; i < 6; ++i) {
    float angle = i * PI / 3.0f;
    corners[i].x = cx + hex_size * std::cos(angle);
    corners[i].y = cy + hex_size * std::sin(angle);
  }
}

struct IsoVec2 {
  float x, y;
};

static void project_hex_to_iso(const Vec2 corners[6], float z,
                               const IsometricParams &params,
                               IsoVec2 iso_corners[6]) {
  for (int i = 0; i < 6; ++i) {
    world_to_iso(corners[i].x, corners[i].y, z, iso_corners[i].x,
                 iso_corners[i].y, params);
  }
}

// Detect plateau regions using flood fill
struct Plateau {
  float height;
  std::vector<int> pixels; // Indices into heightmap
  float center_x, center_y;
};

static std::vector<Plateau> detect_plateaus(std::span<const float> heightmap,
                                            int width, int height) {

  std::vector<bool> visited(width * height, false);
  std::vector<Plateau> plateaus;

  const float HEIGHT_THRESHOLD = 0.02f; // Same height within ±2%

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int idx = y * width + x;
      if (visited[idx])
        continue;

      float plateau_height = heightmap[idx];
      Plateau plateau;
      plateau.height = plateau_height;

      // Flood fill
      std::queue<int> queue;
      queue.push(idx);
      visited[idx] = true;

      float sum_x = 0, sum_y = 0;
      int count = 0;

      while (!queue.empty()) {
        int current = queue.front();
        queue.pop();

        plateau.pixels.push_back(current);

        int cx = current % width;
        int cy = current / width;
        sum_x += cx;
        sum_y += cy;
        count++;

        // Check 4 neighbors
        int neighbors[4][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
        for (auto [dx, dy] : neighbors) {
          int nx = cx + dx;
          int ny = cy + dy;

          if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
            int nidx = ny * width + nx;
            if (!visited[nidx] &&
                std::abs(heightmap[nidx] - plateau_height) < HEIGHT_THRESHOLD) {
              visited[nidx] = true;
              queue.push(nidx);
            }
          }
        }
      }

      plateau.center_x = sum_x / count;
      plateau.center_y = sum_y / count;

      // Only keep plateaus with reasonable size
      if (count > 50) {
        plateaus.push_back(plateau);
      }
    }
  }

  SDL_Log("Detected %zu plateaus", plateaus.size());
  return plateaus;
}

// Check if hex center is within plateau
static bool hex_fits_in_plateau(int q, int r, float hex_size,
                                const std::unordered_set<int> &plateau_set,
                                int width, int height) {
  float cx, cy;
  hex_to_pixel(q, r, hex_size, cx, cy);

  int px = (int)cx;
  int py = (int)cy;

  if (px < 0 || px >= width || py < 0 || py >= height) {
    return false;
  }

  int idx = py * width + px;
  return plateau_set.find(idx) != plateau_set.end();
}

static void compute_visible_edges(std::vector<HexColumn> &columns) {
  // Build spatial lookup
  std::unordered_map<HexCoord, HexColumn *, HexHash> col_map;
  for (auto &col : columns) {
    HexCoord key = {col.q, col.r};
    col_map[key] = &col;
  }

  const int neighbors[6][2] = {{1, 0},  {0, 1},  {-1, 1},
                               {-1, 0}, {0, -1}, {1, -1}};

  // For each column, check 6 neighbors
  for (auto &col : columns) {
    for (int i = 0; i < 6; ++i) {
      col.visible_edges[i] = false;
      col.edge_drops[i] = 0.0f;

      HexCoord neighbor_hc = {col.q + neighbors[i][0], col.r + neighbors[i][1]};
      auto it = col_map.find(neighbor_hc);

      if (it == col_map.end()) {
        // No neighbor: edge is visible with full height
        col.visible_edges[i] = true;
        col.edge_drops[i] = col.height;
      } else {
        // Neighbor exists: check height difference
        float height_diff = col.height - it->second->height;
        if (height_diff > 0.01f) {
          col.visible_edges[i] = true;
          col.edge_drops[i] = height_diff;
        }
      }
    }
  }

  SDL_Log("Computed edge visibility for %zu columns", columns.size());
}
std::vector<HexColumn> generate_basalt_columns(std::span<const float> heightmap,
                                               int width, int height,
                                               float hex_size) {

  std::vector<Plateau> plateaus = detect_plateaus(heightmap, width, height);
  std::vector<HexColumn> columns;

  SDL_Log("Starting column generation with hex_size=%.2f", hex_size);

  for (size_t p = 0; p < plateaus.size(); ++p) {
    const auto &plateau = plateaus[p];
    SDL_Log("Plateau %zu: center=(%.1f,%.1f) height=%.3f pixels=%zu", p,
            plateau.center_x, plateau.center_y, plateau.height,
            plateau.pixels.size());

    std::unordered_set<int> plateau_set(plateau.pixels.begin(),
                                        plateau.pixels.end());

    HexCoord center =
        pixel_to_hex(plateau.center_x, plateau.center_y, hex_size);
    SDL_Log("  Center hex: q=%d r=%d", center.q, center.r);

    float cx, cy;
    hex_to_pixel(center.q, center.r, hex_size, cx, cy);
    SDL_Log("  Center hex back to pixel: (%.1f,%.1f)", cx, cy);

    bool fits = hex_fits_in_plateau(center.q, center.r, hex_size, plateau_set,
                                    width, height);
    SDL_Log("  Center hex fits: %s", fits ? "YES" : "NO");

    if (!fits)
      continue;

    std::unordered_map<HexCoord, bool, HexHash> placed;
    std::queue<HexCoord> to_check;

    to_check.push(center);
    placed[center] = false;

    const int neighbors[6][2] = {{1, 0},  {0, 1},  {-1, 1},
                                 {-1, 0}, {0, -1}, {1, -1}};

    int added = 0;
    while (!to_check.empty()) {
      HexCoord hc = to_check.front();
      to_check.pop();

      if (placed[hc])
        continue;

      if (hex_fits_in_plateau(hc.q, hc.r, hex_size, plateau_set, width,
                              height)) {
        placed[hc] = true;

        uint32_t h = hash2d(hc.q, hc.r);
        float variation = ((h & 0xFF) / 255.0f - 0.5f) * 0.05f;

        columns.push_back(
            {hc.q, hc.r, plateau.height + variation, plateau.height});
        added++;

        for (auto [dq, dr] : neighbors) {
          HexCoord neighbor = {hc.q + dq, hc.r + dr};
          if (placed.find(neighbor) == placed.end()) {
            to_check.push(neighbor);
            placed[neighbor] = false;
          }
        }
      } else {
        placed[hc] = true;
      }
    }

    SDL_Log("  Added %d columns for this plateau", added);
  }

  for (auto &col : columns) {
    for (int i = 0; i < 6; ++i) {
      col.visible_edges[i] = false;
      col.edge_drops[i] = 0.0f;
    }
  }

  compute_visible_edges(columns);

  return columns;
}

static bool point_in_hex(float px, float py, const IsoVec2 corners[6]) {
  bool inside = true;

  for (int i = 0; i < 6; ++i) {
    int next = (i + 1) % 6;

    float edge_x = corners[next].x - corners[i].x;
    float edge_y = corners[next].y - corners[i].y;

    float to_point_x = px - corners[i].x;
    float to_point_y = py - corners[i].y;

    float cross = edge_x * to_point_y - edge_y * to_point_x;

    if (cross < 0) {
      inside = false;
      break;
    }
  }

  return inside;
}

// Fill hexagon top face
static void draw_filled_hex_top(std::vector<uint32_t> &pixels, int width,
                                int height, const IsoVec2 iso_corners[6],
                                float offset_x, float offset_y,
                                uint32_t color) {
  // Find bounding box
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

  // Scanline fill
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

  // Project corners
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

  // Darken for side face
  float darkness = 0.4f;
  uint8_t r = ((base_color >> 16) & 0xFF) * (1.0f - darkness);
  uint8_t g = ((base_color >> 8) & 0xFF) * (1.0f - darkness);
  uint8_t b = (base_color & 0xFF) * (1.0f - darkness);
  uint32_t side_color = 0xFF000000 | (r << 16) | (g << 8) | b;

  // Scanline fill - interpolate between left and right edges
  float min_y = std::min({top0.y, top1.y, bot0.y, bot1.y});
  float max_y = std::max({top0.y, top1.y, bot0.y, bot1.y});

  for (int py = (int)min_y; py <= (int)max_y; ++py) {
    if (py < 0 || py >= height)
      continue;

    float intersections[4];
    int count = 0;

    // Check all 4 edges for intersection with scanline
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
      // Sort intersections
      if (count > 2) {
        std::sort(intersections, intersections + count);
      }
      if (intersections[0] > intersections[1]) {
        std::swap(intersections[0], intersections[1]);
      }

      // Fill between leftmost and rightmost
      int x_start = std::max(0, (int)intersections[0]);
      int x_end = std::min(width - 1, (int)intersections[count - 1]);

      for (int px = x_start; px <= x_end; ++px) {
        pixels[py * width + px] = side_color;
      }
    }
  }
}
void render_basalt_columns(std::vector<uint32_t> &pixels, int view_width,
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
    uint32_t color = organic_color(col->base_height, col->q, col->r, palette);

    Vec2 corners[6];
    get_hex_corners(col->q, col->r, hex_size, corners);

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
    uint32_t color = organic_color(col->base_height, col->q, col->r, palette);

    Vec2 corners[6];
    get_hex_corners(col->q, col->r, hex_size, corners);

    IsoVec2 iso_corners[6];
    project_hex_to_iso(corners, col->height, params, iso_corners);

    draw_filled_hex_top(pixels, view_width, view_height, iso_corners, offset_x,
                        offset_y, color);
  }
}
