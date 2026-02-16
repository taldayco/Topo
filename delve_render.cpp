#include "delve_render.h"
#include "config.h"
#include "isometric.h"
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

    // Draw 3x3 kernel for thickness and smoothness
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        int x = (int)(fx + dx);
        int y = (int)(fy + dy);

        if (x >= 0 && x < width && y >= 0 && y < height) {
          // Distance-based falloff for anti-aliasing
          float dist = std::hypot(dx, dy);
          float falloff = std::max(0.0f, 1.0f - dist * 0.5f);
          uint8_t aa_alpha = (uint8_t)(alpha * falloff);

          uint32_t dst = pixels[y * width + x];
          uint8_t dst_r = (dst >> 16) & 0xFF;
          uint8_t dst_g = (dst >> 8) & 0xFF;
          uint8_t dst_b = dst & 0xFF;

          float blend = aa_alpha / 255.0f;
          uint8_t out_r = r * blend + dst_r * (1.0f - blend);
          uint8_t out_g = g * blend + dst_g * (1.0f - blend);
          uint8_t out_b = b * blend + dst_b * (1.0f - blend);

          pixels[y * width + x] =
              0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
        }
      }
    }
  }
}

PixelBuffer generate_map_pixels(std::span<const float> heightmap,
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

    IsometricView iso_view = create_isometric_heightmap(
        heightmap, contour_lines, width, height, params, palette,
        contour_opacity, iso_padding, iso_offset_x_adjust, iso_offset_y_adjust);
    buf.pixels = std::move(iso_view.pixels);
    buf.width = iso_view.width;
    buf.height = iso_view.height;

    SDL_Log("Isometric view: %dx%d", buf.width, buf.height);

    SDL_Log("Isometric view: %dx%d", buf.width, buf.height);

    const float HEX_SIZE = 8.0f;
    const float sqrt3 = 1.732f;
    const uint32_t BACKGROUND = 0xFF2D2D30;

    for (int y = 0; y < buf.height; ++y) {
      for (int x = 0; x < buf.width; ++x) {
        int idx = y * buf.width + x;
        uint32_t pixel = buf.pixels[idx];

        if (pixel == BACKGROUND)
          continue;

        float q = x / (HEX_SIZE * sqrt3);
        float r = y / HEX_SIZE - q * 0.5f;

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

        uint32_t hash = ((uint32_t)iq * 374761393) ^ ((uint32_t)ir * 668265263);
        float threshold = ((hash & 0xFF) / 255.0f - 0.5f) * 0.25f;

        uint8_t r_out = std::clamp(
            (int)(((pixel >> 16) & 0xFF) * (1.0f + threshold)), 0, 255);
        uint8_t g_out = std::clamp(
            (int)(((pixel >> 8) & 0xFF) * (1.0f + threshold)), 0, 255);
        uint8_t b_out =
            std::clamp((int)((pixel & 0xFF) * (1.0f + threshold)), 0, 255);

        buf.pixels[idx] = 0xFF000000 | (r_out << 16) | (g_out << 8) | b_out;
      }
    }

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
