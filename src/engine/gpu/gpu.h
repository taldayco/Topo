#pragma once
#include <SDL3/SDL.h>
#include <cstdint>

struct TextureHandle {
  SDL_GPUTexture *texture = nullptr;
  SDL_GPUSampler *sampler = nullptr;
  int width = 0;
  int height = 0;
};

struct GpuContext {
  SDL_Window   *window      = nullptr;
  SDL_Window   *game_window = nullptr;
  SDL_GPUDevice *device     = nullptr;
};

struct FrameContext {
  SDL_GPUCommandBuffer *cmd          = nullptr;
  SDL_GPUTexture       *swapchain    = nullptr;
  SDL_GPURenderPass    *render_pass  = nullptr;
  uint32_t              swapchain_w  = 0;
  uint32_t              swapchain_h  = 0;
};

bool gpu_init(GpuContext &ctx);
bool gpu_create_game_window(GpuContext &ctx);
void gpu_destroy_game_window(GpuContext &ctx);
bool gpu_acquire_frame(GpuContext &ctx, FrameContext &frame);
bool gpu_acquire_game_frame(GpuContext &ctx, FrameContext &frame);
bool gpu_begin_render_pass(GpuContext &ctx, FrameContext &frame);
void gpu_end_frame(FrameContext &frame);
void gpu_cleanup(GpuContext &ctx);

void release_texture(SDL_GPUDevice *device, const TextureHandle &handle);
TextureHandle upload_pixels_to_texture(SDL_GPUDevice *device,
                                       const uint32_t *pixels, int width,
                                       int height);
