#include "terrain/delve_render.h"
#include "terrain/color.h"
#include "config.h"
#include "terrain/isometric.h"
#include <SDL3/SDL_log.h>
#include <algorithm>
#include <cmath>

static void draw_line_soft(std::vector<uint32_t> &pixels, int width, int height,
                           float x0, float y0, float x1, float y1,
                           uint32_t color) {
  float dx = x1 - x0;
  float dy = y1 - y0;
  float len = std::hypot(dx, dy);

  if (len < 1.0f)
    return;

  float step_x = dx / len;
  float step_y = dy / len;

  uint8_t alpha = (color >> 24) & 0xFF;
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;

  for (int i = 0; i <= (int)len; ++i) {
    float fx = x0 + step_x * i;
    float fy = y0 + step_y * i;

    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        int x = (int)(fx + dx);
        int y = (int)(fy + dy);

        if (x >= 0 && x < width && y >= 0 && y < height) {
          float dist = std::hypot(dx, dy);
          float falloff = std::max(0.0f, 1.0f - dist * 0.5f);
          uint8_t aa_alpha = (uint8_t)(alpha * falloff);

          uint32_t src_color = 0xFF000000 | (r << 16) | (g << 8) | b;
          pixels[y * width + x] =
              alpha_blend(src_color, pixels[y * width + x],
                          aa_alpha / 255.0f);
        }
      }
    }
  }
}

PixelBuffer generate_map_pixels(std::span<const float> heightmap,
                                std::span<const int> band_map,
                                std::span<const Line> contour_lines, int width,
                                int height, bool use_isometric,
                                const Palette &palette,
                                const DetailParams &detail_params,
                                float contour_opacity, float iso_padding,
                                float iso_offset_x_adjust,
                                float iso_offset_y_adjust) {

  PixelBuffer buf;

  if (use_isometric) {
    IsometricParams params;
    params.tile_width = Config::ISO_TILE_WIDTH;
    params.tile_height = Config::ISO_TILE_HEIGHT;
    params.height_scale = Config::ISO_HEIGHT_SCALE;

    PixelBuffer iso_view = create_isometric_heightmap(
        heightmap, band_map, contour_lines, width, height, params, palette,
        contour_opacity, iso_padding, iso_offset_x_adjust, iso_offset_y_adjust);
    buf.pixels = std::move(iso_view.pixels);
    buf.width = iso_view.width;
    buf.height = iso_view.height;

    SDL_Log("Isometric view: %dx%d", buf.width, buf.height);

    SDL_Log("Isometric view: %dx%d", buf.width, buf.height);

    apply_hex_dither(buf.pixels, buf.width, buf.height, 0.25f,
                     Config::BACKGROUND_COLOR);

  } else {
    buf.width = width;
    buf.height = height;
    buf.pixels.resize(buf.width * buf.height);

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int i = y * width + x;
        buf.pixels[i] = organic_color(heightmap[i], x, y, palette);
      }
    }

    add_procedural_details(buf.pixels, heightmap, width, height, palette,
                           detail_params);

    uint32_t base_line = palette.colors[5];
    uint32_t line_color =
        ((uint8_t)(contour_opacity * 255) << 24) | (base_line & 0x00FFFFFF);
    for (const auto &line : contour_lines) {
      draw_line_soft(buf.pixels, buf.width, buf.height, line.x1, line.y1,
                     line.x2, line.y2, line_color);
    }
  }

  return buf;
}
