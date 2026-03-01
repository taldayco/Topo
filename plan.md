# GPU Device Loss Fix Plan

## Confirmed Root Causes

Two distinct use-after-free bugs. Both involve SDL_ReleaseGPUTexture or
SDL_ReleaseGPUBuffer being called on a resource the GPU is still using, without
a preceding SDL_WaitForGPUIdle.

---

## Bug 1 — depth_texture freed mid-frame (CONFIRMED CRASH CAUSE)

### Where
- `terrain_renderer.cpp`, `TerrainRenderer::begin_render_pass()` — lines starting
  with `if (!depth_texture || depth_w != w || depth_h != h)`
- `terrain_renderer.cpp`, `TerrainRenderer::begin_render_pass_load()` — identical
  block, same problem

### What happens
Both functions are called inside `on_render_game` while a frame command buffer is
open. Both contain the identical block:

  if (!depth_texture || depth_w != w || depth_h != h) {
    SDL_ReleaseGPUTexture(gpu_device, depth_texture);   // ← UNSAFE: GPU may still read this
    depth_texture = nullptr;
    ... SDL_CreateGPUTexture ...
  }

This fires whenever swapchain dimensions change. On the second regen the window
settles to its true size (680×680) for the first time, the block fires, and the
depth texture used by the previous frame's GPU commands is freed while the GPU is
still executing them. This is the GPUVM null fault.

### What calls these functions
- `begin_render_pass` is called from `on_render_game` (topo_game.cpp) at:
    SDL_GPURenderPass *bg_pass = terrain_renderer.begin_render_pass(
        frame.cmd, frame.swapchain, frame.swapchain_w, frame.swapchain_h);
- `begin_render_pass_load` is called from `TerrainRenderer::draw()` (terrain_renderer.cpp)
  via `begin_render_pass_load(cmd, swapchain, w, h)`.
  `draw()` is called from `on_render_game` at:
    terrain_renderer.draw(frame.cmd, frame.swapchain,
                          frame.swapchain_w, frame.swapchain_h, ...);

### Fix
Move depth texture recreation out of both begin_render_pass functions and into
`on_pre_frame_game`, where SDL_WaitForGPUIdle is safe to call.

---

## Bug 2 — SDL_GetWindowSize is unreliable at startup, causing double cluster creation

### Where
- `topo_game.cpp`, `TopoGame::on_pre_frame_game()` — the cluster rebuild block:
    int w = 0, h = 0;
    SDL_GetWindowSize(gpu.game_window, &w, &h);

### What happens
SDL_GetWindowSize returns different values on consecutive frames at startup because
the WM has not committed the window to its final position/size. The log confirms:
  Cluster buffers created (40×24×24 clusters)   ← frame 1: SDL_GetWindowSize=640×384
  Cluster buffers created (20×23×24 clusters)   ← frame 2: SDL_GetWindowSize=680×680
This is wasteful and indicates the wrong size source is being used.

`frame.swapchain_w` and `frame.swapchain_h` (from the acquired FrameContext in
`on_render_game`) are the GPU-confirmed true dimensions and must be used instead.
On the very first frame before any game frame has been acquired, these will be zero,
so the cluster check must be skipped when they are zero.

### Fix
Store the last-seen swapchain dimensions in TopoGame. Update them in on_render_game
after the frame is acquired. Use them in on_pre_frame_game instead of SDL_GetWindowSize.

---

## Complete List of Changes

### terrain_renderer.h

1. Add two new private fields after `depth_w` and `depth_h`:
     uint32_t desired_depth_w = 0;
     uint32_t desired_depth_h = 0;
   These hold the dimensions requested by the most recent begin_render_pass call.

2. Add one new public method declaration:
     void prepare_frame_resources(SDL_GPUDevice *device);
   This is called from on_pre_frame_game. It handles both depth texture recreation
   and is the single place where SDL_WaitForGPUIdle is needed for texture work.
   (Replaces the previously planned `recreate_depth_texture_if_needed` name —
   calling it prepare_frame_resources makes its role clearer as it may grow.)

### terrain_renderer.cpp — begin_render_pass (around line 880)

Remove the entire block:
  if (!depth_texture || depth_w != w || depth_h != h) {
    if (depth_texture) {
      SDL_ReleaseGPUTexture(gpu_device, depth_texture);
      depth_texture = nullptr;
    }
    SDL_GPUTextureCreateInfo ti = { ... };
    depth_texture = SDL_CreateGPUTexture(gpu_device, &ti);
    depth_w = w;
    depth_h = h;
  }

Replace with:
  desired_depth_w = w;
  desired_depth_h = h;
  if (!depth_texture || depth_w != w || depth_h != h) return nullptr;

The function then proceeds to build color_target and depth_target and call
SDL_BeginGPURenderPass exactly as before. Returning nullptr when the depth texture
is not ready is already handled in on_render_game:
  if (!bg_pass) return;

### terrain_renderer.cpp — begin_render_pass_load (around line 925)

Apply the identical change as begin_render_pass. The draw() function (terrain_renderer.cpp)
already checks `if (!pass) return;` after begin_render_pass_load, so returning
nullptr is safe.

### terrain_renderer.cpp — new method prepare_frame_resources

Add after release_cluster_buffers or before cleanup. Logic:
- If desired_depth_w == 0 || desired_depth_h == 0: return (no frame seen yet)
- If depth_texture && depth_w == desired_depth_w && depth_h == desired_depth_h: return (no change)
- SDL_WaitForGPUIdle(gpu_device)  ← safe: called from on_pre_frame_game, no cmd buf open
- If depth_texture: SDL_ReleaseGPUTexture(gpu_device, depth_texture); depth_texture = nullptr
- SDL_CreateGPUTexture with desired_depth_w/h, same SDL_GPUTextureCreateInfo as before
  (type=2D, format=depth_stencil_format, usage=DEPTH_STENCIL_TARGET, layers=1, levels=1)
- Set depth_w = desired_depth_w, depth_h = desired_depth_h
- Log: "TerrainRenderer: Depth texture (re)created (%ux%u)"

Note: prepare_frame_resources must NOT call SDL_WaitForGPUIdle if the depth texture
size has not changed — it will be called every pre-frame and must be cheap when idle.

### terrain_renderer.cpp — cleanup (around line 1005)

No change needed. `cleanup` already calls SDL_WaitForGPUIdle at the top before
releasing depth_texture. desired_depth_w/h are plain uint32_t with no cleanup needed.

### topo_game.h

Add two new private fields after the ready_*_pending fields:
  uint32_t last_swapchain_w = 0;
  uint32_t last_swapchain_h = 0;

### topo_game.cpp — on_render_game

After the game frame is acquired (after the terrain_renderer.init block and
rebuild_dirty_pipelines calls, but before any rendering), add:
  last_swapchain_w = frame.swapchain_w;
  last_swapchain_h = frame.swapchain_h;

The exact insertion point is after `rebuild_dirty_pipelines` and before the
`ts->need_regenerate` block, approximately line 230 in the current file.

### topo_game.cpp — on_pre_frame_game

Current structure:
  1. if ready_mesh_pending → upload_mesh (has its own SDL_WaitForGPUIdle internally)
  2. if gpu.game_window → SDL_GetWindowSize → compare tiles → SDL_WaitForGPUIdle
     → rebuild_clusters_if_needed

New structure:
  1. if ready_mesh_pending → upload_mesh (unchanged, has its own SDL_WaitForGPUIdle)
  2. Compute needs_cluster_rebuild and needs_depth_rebuild:
       bool needs_cluster_rebuild = false;
       if (last_swapchain_w > 0 && last_swapchain_h > 0) {
         uint32_t tilesX = (uint32_t)std::ceil(last_swapchain_w / 16.0f);
         uint32_t tilesY = (uint32_t)std::ceil(last_swapchain_h / 16.0f);
         needs_cluster_rebuild = (tilesX != terrain_renderer.cluster_tiles_x() ||
                                  tilesY != terrain_renderer.cluster_tiles_y());
       }
       bool needs_depth_rebuild = terrain_renderer.depth_needs_rebuild();
         (depth_needs_rebuild() is a new inline on TerrainRenderer: returns true if
          desired_depth_w/h differ from depth_w/h and desired values are nonzero)
  3. If needs_cluster_rebuild OR needs_depth_rebuild:
       SDL_WaitForGPUIdle(gpu.device)   ← single wait covering both
       if needs_cluster_rebuild:
         SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu.device)
         terrain_renderer.rebuild_clusters_if_needed(cmd, last_swapchain_w,
             last_swapchain_h, 16.0f, 24, 1.0f, 1000.0f)
         SDL_SubmitGPUCommandBuffer(cmd)
       terrain_renderer.prepare_frame_resources(gpu.device)
         (prepare_frame_resources skips the SDL_WaitForGPUIdle it would normally call
          because the caller already called it — see note below)

  NOTE on SDL_WaitForGPUIdle ownership: prepare_frame_resources should NOT call
  SDL_WaitForGPUIdle internally. The wait is done once here in on_pre_frame_game.
  prepare_frame_resources just does the release + create. This avoids a double wait
  when both cluster and depth need rebuilding. Rename the method accordingly or add
  a comment making this contract explicit.

  Also remove the `if (gpu.game_window)` check and SDL_GetWindowSize call entirely.
  Replace with the last_swapchain_w/h check described above.

### topo_game.h — add depth_needs_rebuild helper

Add to terrain_renderer.h public section:
  bool depth_needs_rebuild() const {
    return desired_depth_w > 0 && desired_depth_h > 0 &&
           (desired_depth_w != depth_w || desired_depth_h != depth_h);
  }

This keeps the logic for "does depth need work" inside TerrainRenderer where the
relevant fields live, avoiding leaking desired_depth_w/h as public fields.

### topo_index.md

Update the following entries:
- TerrainRenderer::begin_render_pass — note it now sets desired_depth_w/h and
  returns nullptr if depth_texture is not ready for current dimensions
- TerrainRenderer::begin_render_pass_load — same note
- TerrainRenderer (new method) prepare_frame_resources — describe its role
- TerrainRenderer (new inline) depth_needs_rebuild — describe it
- TerrainRenderer fields — add desired_depth_w, desired_depth_h
- TopoGame::on_pre_frame_game — update description to reflect new structure
- TopoGame::on_render_game — note last_swapchain_w/h update
- TopoGame fields — add last_swapchain_w, last_swapchain_h

---

## What Is NOT Changing

- upload_mesh: already safe. SDL_WaitForGPUIdle at top, then release_buffers, then
  upload, then second SDL_WaitForGPUIdle, then register_buffer. Correct order.
- release_buffers / release_cluster_buffers: already safe when called from
  on_pre_frame_game or cleanup (both preceded by SDL_WaitForGPUIdle).
- stage_shaded_draw dummy_ssbo fallback: already in place from previous fix.
- Double-buffering mesh buffers: not needed. The wait-then-swap pattern is correct
  and sufficient for this map size and regen frequency.
- AssetManager::register_buffer / release_buffer: these call SDL_ReleaseGPUBuffer
  immediately, which is correct as long as callers ensure GPU idle first. All
  current callers (upload_mesh, release_buffers, cleanup) do so correctly.

---

## Execution Order Summary

on_pre_frame_game (safe zone — no frame cmd buf open):
  [1] upload_mesh if pending          → includes its own SDL_WaitForGPUIdle
  [2] check needs_cluster / needs_depth
  [3] SDL_WaitForGPUIdle if either    → single wait
  [4] rebuild clusters if needed      → acquire cmd, dispatch, submit
  [5] prepare_frame_resources         → release + recreate depth texture if needed

on_render_game (frame cmd buf open — NO resource destruction allowed):
  [1] store last_swapchain_w/h
  [2] begin_render_pass               → sets desired_depth_w/h, uses existing texture
  [3] draw / begin_render_pass_load   → uses existing depth_texture
  [4] poll async results → ready_mesh_pending

GPU resource destruction is now confined entirely to:
  - on_pre_frame_game (after SDL_WaitForGPUIdle)
  - on_cleanup (after SDL_WaitForGPUIdle via cleanup())
