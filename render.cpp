#include "render.h"
#include "detail.h"
#include <algorithm>
#include <cmath>
#include <vector>

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

TextureHandle create_texture_from_heightmap(
    SDL_GPUDevice *device, std::span<const float> heightmap,
    std::span<const Line> contour_lines, int width, int height,
    bool use_isometric, const Palette &palette,
    const DetailParams &detail_params, float contour_opacity) {

  auto start = SDL_GetTicks();
  std::vector<uint32_t> pixels;
  int tex_width, tex_height;

  if (use_isometric) {
    IsometricParams params;
    params.tile_width = 2.0f;
    params.tile_height = 1.0f;
    params.height_scale = 100.0f;

    IsometricView iso_view =
        create_isometric_heightmap(heightmap, contour_lines, width, height,
                                   params, palette, contour_opacity);
    pixels = std::move(iso_view.pixels);
    tex_width = iso_view.width;
    tex_height = iso_view.height;

    SDL_Log("Isometric view: %dx%d", tex_width, tex_height);
  } else {
    // 2D orthographic view
    tex_width = width;
    tex_height = height;
    pixels.resize(tex_width * tex_height);

    // STEP 1: Base color pass
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int i = y * width + x;
        pixels[i] = organic_color(heightmap[i], x, y, palette);
      }
    }

    // STEP 2: Add procedural details (rocks, moss, grass)
    add_procedural_details(pixels, heightmap, width, height, palette,
                           detail_params);

    // STEP 3: Draw contour lines on top
    uint32_t base_line = palette.colors[5]; // Use palette line color
    uint32_t line_color =
        ((uint8_t)(contour_opacity * 255) << 24) | (base_line & 0x00FFFFFF);
    for (const auto &line : contour_lines) {
      draw_line_soft(pixels, tex_width, tex_height, line.x1, line.y1, line.x2,
                     line.y2, line_color);
    }
  }

  auto after_render = SDL_GetTicks();
  SDL_Log("Rendering: %llu ms for %zu lines", after_render - start,
          contour_lines.size());

  // Upload to GPU
  SDL_GPUTextureCreateInfo tex_info = {};
  tex_info.type = SDL_GPU_TEXTURETYPE_2D;
  tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  tex_info.width = tex_width;
  tex_info.height = tex_height;
  tex_info.layer_count_or_depth = 1;
  tex_info.num_levels = 1;
  tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);

  SDL_GPUTransferBufferCreateInfo transfer_info = {};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = tex_width * tex_height * 4;

  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device, &transfer_info);
  void *data = SDL_MapGPUTransferBuffer(device, transfer, false);
  SDL_memcpy(data, pixels.data(), transfer_info.size);
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *upload_cmd = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmd);

  SDL_GPUTextureTransferInfo src_info = {};
  src_info.transfer_buffer = transfer;
  src_info.offset = 0;

  SDL_GPUTextureRegion dst_region = {};
  dst_region.texture = texture;
  dst_region.w = tex_width;
  dst_region.h = tex_height;
  dst_region.d = 1;

  SDL_UploadToGPUTexture(copy_pass, &src_info, &dst_region, false);
  SDL_EndGPUCopyPass(copy_pass);
  SDL_SubmitGPUCommandBuffer(upload_cmd);

  SDL_ReleaseGPUTransferBuffer(device, transfer);

  SDL_GPUSamplerCreateInfo sampler_info = {};
  sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
  sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
  sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

  SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &sampler_info);

  TextureHandle handle;
  handle.texture = texture;
  handle.sampler = sampler;
  handle.width = tex_width;
  handle.height = tex_height;

  return handle;
}

void release_texture(SDL_GPUDevice *device, const TextureHandle &handle) {
  if (handle.sampler) {
    SDL_ReleaseGPUSampler(device, handle.sampler);
  }
  if (handle.texture) {
    SDL_ReleaseGPUTexture(device, handle.texture);
  }
}
