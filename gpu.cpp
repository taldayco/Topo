#include "gpu.h"
#include "config.h"

bool gpu_init(GpuContext &ctx) {
  SDL_Log("Init starting...");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s",
                 SDL_GetError());
    return false;
  }
  SDL_Log("SDL_Init success");

  SDL_DisplayID display_id = SDL_GetPrimaryDisplay();
  SDL_Rect display_bounds;

  if (SDL_GetDisplayUsableBounds(display_id, &display_bounds)) {
    int display_width = display_bounds.w;
    int display_height = display_bounds.h;

    Config::WINDOW_WIDTH = (int)(display_width * Config::WINDOW_WIDTH_PERCENT);
    Config::WINDOW_HEIGHT =
        (int)(display_height * Config::WINDOW_HEIGHT_PERCENT);

    int map_area_width = Config::WINDOW_HEIGHT;

    if constexpr (Config::use_IMGUI) {
      Config::UI_PANEL_WIDTH =
          (int)(Config::WINDOW_WIDTH * Config::UI_PANEL_WIDTH_PERCENT);

      if (Config::UI_PANEL_WIDTH < Config::UI_PANEL_MIN_WIDTH) {
        Config::UI_PANEL_WIDTH = Config::UI_PANEL_MIN_WIDTH;
      }

      Config::WINDOW_WIDTH = map_area_width + Config::UI_PANEL_WIDTH + 50;
    } else {
      Config::WINDOW_WIDTH = Config::WINDOW_HEIGHT;
    }

    SDL_Log("Display: %dx%d", display_width, display_height);
    SDL_Log("Window: %dx%d, Panel: %d", Config::WINDOW_WIDTH,
            Config::WINDOW_HEIGHT, Config::UI_PANEL_WIDTH);
    SDL_Log("Map Resolution: %dx%d", Config::MAP_WIDTH, Config::MAP_HEIGHT);
  } else {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Could not get display bounds: %s", SDL_GetError());
  }

  ctx.window =
      SDL_CreateWindow("Topographical Map Generator", Config::WINDOW_WIDTH,
                       Config::WINDOW_HEIGHT, SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!ctx.window) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s",
                 SDL_GetError());
    return false;
  }
  SDL_Log("SDL_CreateWindow success");

  ctx.device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
  if (!ctx.device) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "SDL_CreateGPUDevice failed: %s", SDL_GetError());
    return false;
  }
  SDL_Log("SDL_CreateGPUDevice success");

  if (!SDL_ClaimWindowForGPUDevice(ctx.device, ctx.window)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
    return false;
  }
  SDL_Log("SDL_ClaimWindowForGPUDevice success");

  SDL_SetGPUSwapchainParameters(ctx.device, ctx.window,
                                SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                SDL_GPU_PRESENTMODE_VSYNC);

  SDL_Log("Init complete");
  return true;
}

bool gpu_acquire_frame(GpuContext &ctx, FrameContext &frame) {
  frame.cmd = SDL_AcquireGPUCommandBuffer(ctx.device);
  if (!frame.cmd)
    return false;

  if (!SDL_AcquireGPUSwapchainTexture(frame.cmd, ctx.window,
                                      &frame.swapchain, &frame.swapchain_w,
                                      &frame.swapchain_h) ||
      !frame.swapchain) {
    SDL_SubmitGPUCommandBuffer(frame.cmd);
    return false;
  }

  return true;
}

bool gpu_begin_render_pass(GpuContext &ctx, FrameContext &frame) {
  SDL_GPUColorTargetInfo color_target = {};
  color_target.texture = frame.swapchain;
  color_target.clear_color = {0.176f, 0.176f, 0.188f, 1.0f};
  color_target.load_op = SDL_GPU_LOADOP_CLEAR;
  color_target.store_op = SDL_GPU_STOREOP_STORE;

  frame.render_pass =
      SDL_BeginGPURenderPass(frame.cmd, &color_target, 1, nullptr);

  return frame.render_pass != nullptr;
}

void gpu_end_frame(FrameContext &frame) {
  if (frame.render_pass)
    SDL_EndGPURenderPass(frame.render_pass);
  SDL_SubmitGPUCommandBuffer(frame.cmd);
}

void gpu_blit_texture(FrameContext &frame, const TextureHandle &tex) {
  // Compute aspect-ratio-preserving destination region (letterboxed)
  float src_aspect = (float)tex.width / (float)tex.height;
  float dst_aspect = (float)frame.swapchain_w / (float)frame.swapchain_h;

  uint32_t dst_w, dst_h, dst_x, dst_y;
  if (src_aspect > dst_aspect) {
    // Source wider: fit to width, letterbox top/bottom
    dst_w = frame.swapchain_w;
    dst_h = (uint32_t)(frame.swapchain_w / src_aspect);
    dst_x = 0;
    dst_y = (frame.swapchain_h - dst_h) / 2;
  } else {
    // Source taller: fit to height, pillarbox left/right
    dst_h = frame.swapchain_h;
    dst_w = (uint32_t)(frame.swapchain_h * src_aspect);
    dst_x = (frame.swapchain_w - dst_w) / 2;
    dst_y = 0;
  }

  SDL_GPUBlitInfo blit = {};
  blit.source.texture = tex.texture;
  blit.source.w = tex.width;
  blit.source.h = tex.height;
  blit.destination.texture = frame.swapchain;
  blit.destination.x = dst_x;
  blit.destination.y = dst_y;
  blit.destination.w = dst_w;
  blit.destination.h = dst_h;
  blit.load_op = SDL_GPU_LOADOP_CLEAR;
  blit.clear_color = {0.176f, 0.176f, 0.188f, 1.0f};
  blit.filter = SDL_GPU_FILTER_LINEAR;

  SDL_BlitGPUTexture(frame.cmd, &blit);
}

void gpu_cleanup(GpuContext &ctx) {
  SDL_WaitForGPUIdle(ctx.device);
  if (ctx.map_texture.texture) {
    release_texture(ctx.device, ctx.map_texture);
  }
  if (ctx.device)
    SDL_DestroyGPUDevice(ctx.device);
  if (ctx.window)
    SDL_DestroyWindow(ctx.window);
  SDL_Quit();
}

void release_texture(SDL_GPUDevice *device, const TextureHandle &handle) {
  if (handle.sampler) {
    SDL_ReleaseGPUSampler(device, handle.sampler);
  }
  if (handle.texture) {
    SDL_ReleaseGPUTexture(device, handle.texture);
  }
}

TextureHandle upload_pixels_to_texture(SDL_GPUDevice *device,
                                       const uint32_t *pixels, int width,
                                       int height) {
  SDL_GPUTextureCreateInfo tex_info = {};
  tex_info.type = SDL_GPU_TEXTURETYPE_2D;
  tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  tex_info.width = width;
  tex_info.height = height;
  tex_info.layer_count_or_depth = 1;
  tex_info.num_levels = 1;
  tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &tex_info);
  if (!texture) {
    SDL_Log("Failed to create GPU texture");
    return {};
  }

  SDL_GPUTransferBufferCreateInfo transfer_info = {};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = width * height * 4;

  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (!transfer) {
    SDL_Log("Failed to create transfer buffer");
    SDL_ReleaseGPUTexture(device, texture);
    return {};
  }

  void *data = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (!data) {
    SDL_Log("Failed to map transfer buffer");
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUTexture(device, texture);
    return {};
  }
  SDL_memcpy(data, pixels, transfer_info.size);
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
  if (!sampler) {
    SDL_Log("Failed to create GPU sampler");
    SDL_ReleaseGPUTexture(device, texture);
    return {};
  }

  TextureHandle handle;
  handle.texture = texture;
  handle.sampler = sampler;
  handle.width = width;
  handle.height = height;

  return handle;
}
