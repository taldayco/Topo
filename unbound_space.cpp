#include "unbound_space.h"
#include "palettes.h"
#include "types.h"
#include "util.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <unordered_set>
#include <vector>

std::vector<HexColumn>
generate_unbound_space_columns(const std::vector<UnusedRegion> &regions,
                        std::span<const float> heightmap, int width, int height,
                        float hex_size, std::vector<int16_t> &terrain_map) {
  std::vector<HexColumn> columns;

  for (const auto &region : regions) {
    if (region.type != RegionType::UnboundSpace)
      continue;

    // Convert pixel set to unique hex coordinates
    std::unordered_set<HexCoord, HexHash> hex_set;
    for (int idx : region.pixels) {
      float px = (float)(idx % width);
      float py = (float)(idx / width);
      HexCoord hc = pixel_to_hex(px, py, hex_size);
      hex_set.insert(hc);
    }

    float uniform_height = region.avg_elevation;

    for (const auto &hc : hex_set) {
      columns.push_back({hc.q, hc.r, uniform_height, uniform_height});

      // Mark column footprint in terrain_map
      Vec2 corners[6];
      get_hex_corners(hc.q, hc.r, hex_size, corners);
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
          if (pixel_in_hex((float)rx, (float)ry, hc.q, hc.r, hex_size))
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
    uint32_t color = get_elevation_color_smooth(col->base_height, palette);

    Vec2 corners[6];
    get_hex_corners(col->q, col->r, hex_size, corners);

    IsoVec2 iso_corners[6];
    project_hex_to_iso(corners, col->height, params, iso_corners);

    draw_filled_hex_top(pixels, view_width, view_height, iso_corners, offset_x,
                        offset_y, color);
  }
}
