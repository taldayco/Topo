#include "render.h"
#include <algorithm>
#include <vector>

// Fast line drawing - direct pixel writes, no Bresenham overhead
static void draw_line_fast(std::vector<uint32_t> &pixels, int width, int height,
                           float x0, float y0, float x1, float y1,
                           uint32_t color) {
  float dx = x1 - x0;
  float dy = y1 - y0;
  float len = std::max(std::abs(dx), std::abs(dy));

  if (len < 1.0f)
    return;

  float step_x = dx / len;
  float step_y = dy / len;

  for (int i = 0; i <= (int)len; ++i) {
    int x = (int)(x0 + step_x * i);
    int y = (int)(y0 + step_y * i);

    if (x >= 0 && x < width && y >= 0 && y < height) {
      pixels[y * width + x] = color;
    }
  }
}

TextureHandle create_texture_from_heightmap(SDL_GPUDevice *device,
                                            std::span<const float> heightmap,
                                            std::span<const Line> contour_lines,
                                            int width, int height) {
  auto start = SDL_GetTicks();

  SDL_GPUTextureCreateInfo tex_info = {};
  tex_info.type = SDL_GPU_TEXTURETYPE_2D;
  tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  tex_info.width = width;
  tex_info.height = height;
  tex_info.layer_count_or_depth = 1;
  tex_info.num_levels = 1;
  tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);

  // Create heightmap pixels
  std::vector<uint32_t> pixels(width * height);
  for (int i = 0; i < width * height; ++i) {
    uint8_t gray = (uint8_t)(heightmap[i] * 255.0f);
    pixels[i] = 0xFF000000 | (gray << 16) | (gray << 8) | gray;
  }

  auto after_heightmap = SDL_GetTicks();

  // Draw contour lines (black)
  uint32_t line_color = 0xFF000000;
  for (const auto &line : contour_lines) {
    draw_line_fast(pixels, width, height, line.x1, line.y1, line.x2, line.y2,
                   line_color);
  }

  auto after_lines = SDL_GetTicks();
  SDL_Log("Line drawing: %llu ms for %zu lines", after_lines - after_heightmap,
          contour_lines.size());

  // Upload to GPU
  SDL_GPUTransferBufferCreateInfo transfer_info = {};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = width * height * 4;

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
  dst_region.w = width;
  dst_region.h = height;
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

  auto end = SDL_GetTicks();
  SDL_Log("Total texture creation: %llu ms", end - start);

  TextureHandle handle;
  handle.texture = texture;
  handle.sampler = sampler;

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
