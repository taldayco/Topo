#include "gpu/gpu.h"
#include <algorithm>

bool gpu_init(GpuContext &ctx) {
  SDL_Log("Init starting...");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s",
                 SDL_GetError());
    return false;
  }
  SDL_Log("SDL_Init success");

  int tool_w = 450;
  int tool_h = 800;

  SDL_DisplayID display_id = SDL_GetPrimaryDisplay();
  SDL_Rect display_bounds;
  if (SDL_GetDisplayUsableBounds(display_id, &display_bounds)) {
    tool_h = std::min(tool_h, (int)(display_bounds.h * 0.85f));
  }

  ctx.window =
      SDL_CreateWindow("Topo — Controls", tool_w, tool_h, SDL_WINDOW_RESIZABLE);
  if (!ctx.window) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s",
                 SDL_GetError());
    return false;
  }
  SDL_Log("Tool window created (%dx%d)", tool_w, tool_h);

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

  SDL_SetGPUSwapchainParameters(ctx.device, ctx.window,
                                SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                SDL_GPU_PRESENTMODE_VSYNC);

  SDL_Log("Init complete");
  return true;
}

bool gpu_create_game_window(GpuContext &ctx) {
  if (ctx.game_window)
    return true;

  SDL_DisplayID display_id = SDL_GetPrimaryDisplay();
  SDL_Rect display_bounds;
  int win_w = 1024, win_h = 1024;

  if (SDL_GetDisplayUsableBounds(display_id, &display_bounds)) {
    win_h = (int)(display_bounds.h * 0.85f);
    win_w = win_h;
  }

  ctx.game_window = SDL_CreateWindow("Topo — Map", win_w, win_h, SDL_WINDOW_RESIZABLE);
  if (!ctx.game_window) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create game window: %s", SDL_GetError());
    return false;
  }

  if (!SDL_ClaimWindowForGPUDevice(ctx.device, ctx.game_window)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to claim game window: %s", SDL_GetError());
    SDL_DestroyWindow(ctx.game_window);
    ctx.game_window = nullptr;
    return false;
  }

  SDL_SetGPUSwapchainParameters(ctx.device, ctx.game_window,
                                SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                SDL_GPU_PRESENTMODE_VSYNC);

  SDL_Log("Game window created (%dx%d)", win_w, win_h);
  return true;
}

void gpu_destroy_game_window(GpuContext &ctx) {
  if (!ctx.game_window)
    return;
  SDL_WaitForGPUIdle(ctx.device);
  SDL_ReleaseWindowFromGPUDevice(ctx.device, ctx.game_window);
  SDL_DestroyWindow(ctx.game_window);
  ctx.game_window = nullptr;
  SDL_Log("Game window destroyed");
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

bool gpu_acquire_game_frame(GpuContext &ctx, FrameContext &frame) {
  if (!ctx.game_window)
    return false;

  frame.cmd = SDL_AcquireGPUCommandBuffer(ctx.device);
  if (!frame.cmd)
    return false;

  if (!SDL_AcquireGPUSwapchainTexture(frame.cmd, ctx.game_window,
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

void gpu_blit_texture(FrameContext &frame, const TextureHandle &tex, const ViewState &view) {
  // Compute visible region of the source texture based on zoom and pan
  float vis_w = (float)tex.width / view.zoom;
  float vis_h = (float)tex.height / view.zoom;

  // Center the source rect on the pan position (pan is 0-1 normalized)
  float center_x = view.pan_x * tex.width;
  float center_y = view.pan_y * tex.height;

  float src_x = center_x - vis_w * 0.5f;
  float src_y = center_y - vis_h * 0.5f;

  // Clamp to texture bounds
  src_x = std::clamp(src_x, 0.0f, (float)tex.width - vis_w);
  src_y = std::clamp(src_y, 0.0f, (float)tex.height - vis_h);

  // Compute destination rect preserving source aspect ratio
  float src_aspect = vis_w / vis_h;
  float dst_aspect = (float)frame.swapchain_w / (float)frame.swapchain_h;

  uint32_t dst_w, dst_h, dst_x, dst_y;
  if (src_aspect > dst_aspect) {
    dst_w = frame.swapchain_w;
    dst_h = (uint32_t)(frame.swapchain_w / src_aspect);
    dst_x = 0;
    dst_y = (frame.swapchain_h - dst_h) / 2;
  } else {
    dst_h = frame.swapchain_h;
    dst_w = (uint32_t)(frame.swapchain_h * src_aspect);
    dst_x = (frame.swapchain_w - dst_w) / 2;
    dst_y = 0;
  }

  SDL_GPUBlitInfo blit = {};
  blit.source.texture = tex.texture;
  blit.source.x = (uint32_t)src_x;
  blit.source.y = (uint32_t)src_y;
  blit.source.w = (uint32_t)vis_w;
  blit.source.h = (uint32_t)vis_h;
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
  if (ctx.game_window) {
    SDL_ReleaseWindowFromGPUDevice(ctx.device, ctx.game_window);
    SDL_DestroyWindow(ctx.game_window);
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
