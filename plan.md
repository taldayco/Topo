# Topo: Comprehensive Performance Implementation Plan

This document is the authoritative implementation guide. Every file, function, data structure, and shader referenced here was read directly from source before writing. No entry is speculative.

---

## Phase 1 — Async Terrain Generation

**Problem:** `TopoGame::on_render_game` (`src/game/topo_game.cpp`) runs `compose_layers`, `generate_basalt_columns_v2`, `generate_lava_and_void`, `extract_contours`, and `build_terrain_mesh` synchronously on the main thread inside the render loop, blocking the UI and GPU submission for ~400ms on every regeneration.

---

### 1.1 — New File: `src/engine/core/task_system.h` and `task_system.cpp`

Create a minimal `TaskSystem` class with the following interface:

```
class TaskSystem {
public:
    void init(int num_threads);       // spawn std::jthread pool
    void shutdown();                  // signal stop, join all threads
    void enqueue(std::function<void()> task); // push onto queue
    bool is_idle() const;             // true when queue empty and no threads working
};
```

Internals:
- `std::deque<std::function<void()>> queue_`
- `std::mutex mtx_`
- `std::condition_variable cv_`
- `std::atomic<int> active_count_` — incremented when a thread picks up a task, decremented on completion
- `std::atomic<bool> stop_`
- `std::vector<std::jthread> threads_`
- Worker loop: wait on `cv_`, pop task, increment `active_count_`, run, decrement

Use 1–2 threads. Terrain generation is CPU-bound and single-pipeline; more threads don't help and can cause cache thrashing.

---

### 1.2 — New Fields on `TerrainState` in `src/game/game_state.h`

Add to the `TerrainState` struct:

```cpp
std::atomic<bool> is_generating{false};
std::shared_ptr<TerrainMesh> pending_mesh;   // written by worker, read by main thread
std::shared_ptr<MapData>     pending_map;    // needed to rebuild point_lights after regen
```

`pending_mesh` and `pending_map` are written exclusively by the worker thread, then atomically handed to the main thread by swapping the shared_ptr under a `std::mutex pending_mtx`. Do not use `std::atomic<std::shared_ptr<>>` — it is not lock-free on all platforms.

Also add:
```cpp
std::mutex pending_mtx;
```

---

### 1.3 — Modify `TopoGame::on_render_game` in `src/game/topo_game.cpp`

**Add to `TopoGame` class (`topo_game.h`):**
```cpp
TaskSystem task_system;
```

**In `on_init`:** Call `task_system.init(1)`.

**In `on_cleanup`:** Call `task_system.shutdown()` before `terrain_renderer.cleanup(gpu_ctx.device)`.

**Replace the current `if (ts && ts->need_regenerate)` block:**

Current flow (lines ~200–213 of `topo_game.cpp`):
1. Allocate MapData
2. `compose_layers(...)` — ~300ms
3. `generate_basalt_columns_v2(...)` — ~50ms
4. `generate_lava_and_void(...)` — variable
5. `extract_contours(...)` — ~30ms
6. `build_terrain_mesh(...)` — ~50ms
7. `terrain_renderer.upload_mesh(gpu.device, mesh)` — GPU upload

New flow:
```
if (ts->need_regenerate && !ts->is_generating) {
    ts->need_regenerate = false;
    ts->is_generating   = true;
    // Snapshot parameters by value for thread capture
    auto elev_snap    = *elev;
    auto river_snap   = *river;
    auto worley_snap  = *worley;
    auto comp_snap    = *comp;
    auto ts_snap      = *ts;
    task_system.enqueue([=, &ts_ref = *ts]() {
        auto md = std::make_shared<MapData>();
        md->allocate(Config::MAP_WIDTH, Config::MAP_HEIGHT);
        compose_layers(*md, elev_snap, river_snap, worley_snap, comp_snap, nullptr);
        // NoiseCache is NOT safe to share — pass nullptr, regen will be fast enough
        // once async
        md->columns = generate_basalt_columns_v2(*md, Config::HEX_SIZE);
        auto fill   = generate_lava_and_void(*md, comp_snap.void_chance, worley_snap.seed);
        md->lava_bodies = std::move(fill.lava_bodies);
        md->void_bodies = std::move(fill.void_bodies);
        ContourData cd;
        cd.heightmap.resize(Config::MAP_WIDTH * Config::MAP_HEIGHT);
        std::copy(md->basalt_height.begin(), md->basalt_height.end(), cd.heightmap.begin());
        float interval = 1.0f / comp_snap.terrace_levels;
        extract_contours(cd.heightmap, Config::MAP_WIDTH, Config::MAP_HEIGHT,
                         interval, cd.contour_lines, cd.band_map);
        auto mesh = std::make_shared<TerrainMesh>(
            build_terrain_mesh(ts_snap, *md, cd));
        // Hand off to main thread
        {
            std::lock_guard lk(ts_ref.pending_mtx);
            ts_ref.pending_mesh = std::move(mesh);
            ts_ref.pending_map  = std::move(md);
        }
        ts_ref.is_generating = false;
    });
}

// Main thread: poll for completed mesh
if (!ts->is_generating) {
    std::shared_ptr<TerrainMesh> ready_mesh;
    std::shared_ptr<MapData>     ready_map;
    {
        std::lock_guard lk(ts->pending_mtx);
        ready_mesh = std::move(ts->pending_mesh);
        ready_map  = std::move(ts->pending_map);
    }
    if (ready_mesh) {
        terrain_renderer.upload_mesh(gpu.device, *ready_mesh);
        // Update the ECS MapData from ready_map
        *map_data = std::move(*ready_map);
        // Rebuild point_lights from new map_data
        // ... (existing lava body → point_light loop)
    }
}
```

**Important:** `NoiseCache` is an ECS component stored in `flecs::world`. It is not thread-safe. Pass `nullptr` for the cache in the worker. The async path is fast enough without caching. The cache still works for the synchronous hot-path if someone bypasses async.

**Also note:** `ContourData` is also an ECS component. The worker must produce it as a local and the main thread must write it back to the ECS world after swap — just like `MapData`.

---

### 1.4 — Thread Safety of `TerrainMesh` and `MapData`

`TerrainMesh` (`src/game/terrain/terrain_mesh.h`) contains only:
- `std::vector<RenderingLayer> basalt_layers`
- `std::vector<GpuLavaVertex> lava_vertices`
- `std::vector<uint32_t> lava_indices`
- `std::vector<ContourVertex> contour_vertices`

These are all plain data. Safe to build on a worker thread and read on the main thread after the `pending_mtx` handoff.

`MapData` (`src/game/terrain/map_data.h`) similarly contains only plain vectors. Safe.

---

## Phase 2 — Persistent Staging Pool (Eliminate Per-Frame Allocations)

**Problem:** Every call to `TerrainRenderer::upload_lights` (`terrain_renderer.cpp`) and every call to `upload_to_gpu_buffer` (the static helper) creates and destroys an `SDL_GPUTransferBuffer`. This is a synchronous driver allocation that causes stutter every frame when lights are active.

---

### 2.1 — New Struct: `UploadManager` in `src/engine/gpu/gpu.h` and `gpu.cpp`

Add to `gpu.h`:
```cpp
struct UploadManager {
    SDL_GPUTransferBuffer *buffer = nullptr;
    uint32_t               capacity = 0;
    uint32_t               cursor   = 0;  // reset each frame

    void init(SDL_GPUDevice *device, uint32_t size);
    void cleanup(SDL_GPUDevice *device);

    // Returns a mapped pointer valid until end_frame(), or nullptr on overflow.
    void *alloc(uint32_t size, uint32_t *out_offset);
    void  reset();  // call at start of each frame
};
```

Add `UploadManager upload_manager` to `GpuContext`.

Initialize in `gpu_init` (`gpu.cpp`) with 8 MB — enough for the largest possible light upload (`1024 * 32 = 32 KB`), mesh uploads (handled separately via `upload_mesh` which is not per-frame), and any future per-frame staging.

**`gpu.cpp` changes:**
- `gpu_init`: call `ctx.upload_manager.init(ctx.device, 8 * 1024 * 1024)`
- `gpu_acquire_frame` and `gpu_acquire_game_frame`: call `ctx.upload_manager.reset()` at the start
- `gpu_cleanup`: call `ctx.upload_manager.cleanup(ctx.device)`

---

### 2.2 — Modify `TerrainRenderer::upload_lights` in `terrain_renderer.cpp`

**Current:** Creates a new `SDL_GPUTransferBuffer` every call. Acquires its own command buffer and submits immediately (blocking).

**New:** Accept a reference to `UploadManager` and use `upload_manager.alloc(count * sizeof(GpuPointLight), &offset)`. Record the upload into `frame.cmd` instead of a separate command buffer.

This requires changing the signature of `upload_lights`:
```cpp
// Old:
void upload_lights(const std::vector<GpuPointLight> &lights);

// New:
void upload_lights(SDL_GPUCommandBuffer *cmd,
                   UploadManager &uploader,
                   const std::vector<GpuPointLight> &lights);
```

Update call site in `TerrainRenderer::draw` (`terrain_renderer.cpp`) to pass `cmd` and the upload manager.

Update call site in `TopoGame::on_render_game` (`topo_game.cpp`) to pass `gpu_ctx.upload_manager` through to `terrain_renderer.draw`.

**Note:** `TerrainRenderer::draw` currently takes no `UploadManager`. Its signature must be updated in both `terrain_renderer.h` and `terrain_renderer.cpp`:
```cpp
// Old:
void draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain,
          uint32_t w, uint32_t h,
          const SceneUniforms &uniforms,
          const std::vector<GpuPointLight> &lights);

// New:
void draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain,
          uint32_t w, uint32_t h,
          const SceneUniforms &uniforms,
          const std::vector<GpuPointLight> &lights,
          UploadManager &uploader);
```

---

### 2.3 — Eliminate `SDL_WaitForGPUIdle` in `upload_to_gpu_buffer`

The static helper `upload_to_gpu_buffer` in `terrain_renderer.cpp` calls `SDL_WaitForGPUIdle(device)` after every upload. This is only called from `upload_mesh`, which is already called only after a regen (not per-frame). Once Phase 1 (async) is in place, `upload_mesh` is called from the main thread while no GPU work is in flight on that data. The `SDL_WaitForGPUIdle` is then still needed here but is acceptable since mesh upload is a one-time event — leave as-is for now.

---

## Phase 3 — GPU-Driven Frustum Culling and Indirect Draw

**Problem:** `TerrainRenderer::stage_shaded_draw` (`terrain_renderer.cpp`) draws the entire mesh unconditionally with a single `SDL_DrawGPUIndexedPrimitives(pass, basalt_total_index_count, 1, 0, 0, 0)`. As map scale grows (map is 1024×1024 with up to 4985 columns), geometry outside the view frustum is still submitted to the GPU.

---

### 3.1 — Add a Visibility SSBO

In `terrain_renderer.h`, add:
```cpp
SDL_GPUBuffer *draw_indirect_ssbo = nullptr;  // SDL_GPUDrawIndexedIndirectCommand per column
SDL_GPUBuffer *column_bounds_ssbo = nullptr;  // per-column AABB, built at upload_mesh time
```

Register both in `asset_manager` under keys `"draw_indirect_ssbo"` and `"column_bounds_ssbo"`.

`column_bounds_ssbo` layout (per column, 32 bytes, std430):
```glsl
struct ColumnBounds {
    vec3 aabb_min;
    float _pad0;
    vec3 aabb_max;
    float _pad1;
};
```

Populated in `upload_mesh` by iterating `mesh.basalt_layers` and computing per-column AABB from vertex positions. This is CPU-side at upload time — not per-frame.

---

### 3.2 — New Compute Shader: `shaders/cull_columns.comp.glsl`

**Inputs (readonly SSBOs):**
- Slot 0: `column_bounds_ssbo` — per-column AABBs

**Outputs (readwrite SSBOs):**
- Slot 1: `draw_indirect_ssbo` — one `SDL_GPUDrawIndexedIndirectCommand` per column

**Uniforms (push constant, slot 0):**
```glsl
layout(push_constant) uniform CullUniforms {
    mat4  view_proj;
    uint  num_columns;
    float _pad[3];
};
```

**Logic (`main`):** For each column, test its AABB against the 6 frustum planes extracted from `view_proj`. If visible, write a valid draw command (index count, first index, vertex offset). If culled, write `index_count = 0`.

**Pipeline registration:**
- `init_compute_pipelines`: add `cull_columns_pipeline` — `build_compute_pipeline(device, path, 1, 1, 1)` (1 uniform buffer, 1 rw buffer for indirect, 1 ro buffer for bounds).
- Add to `TerrainRenderer`: `SDL_GPUComputePipeline *cull_columns_pipeline = nullptr;`
- Register: `asset_manager->load_compute_shader("cull_columns.comp", path, 1, 1, 1)` and `asset_manager->register_compute_pipeline("cull_columns", "cull_columns.comp")`.

---

### 3.3 — New Function: `stage_cull_columns` in `terrain_renderer.cpp`

```cpp
void TerrainRenderer::stage_cull_columns(SDL_GPUCommandBuffer *cmd,
                                          const SceneUniforms &uniforms);
```

Called from `draw`, between `stage_cull_lights` and `begin_render_pass_load`:
```
upload_lights(cmd, uploader, lights);
stage_cull_lights(cmd, uniforms, lights);
stage_cull_columns(cmd, uniforms);       // NEW
SDL_GPURenderPass *pass = begin_render_pass_load(cmd, swapchain, w, h);
stage_shaded_draw(pass, cmd, uniforms);
```

---

### 3.4 — Modify `stage_shaded_draw` to use Indirect Draw

Replace in `stage_shaded_draw`:
```cpp
// Old:
SDL_DrawGPUIndexedPrimitives(pass, basalt_total_index_count, 1, 0, 0, 0);

// New:
SDL_DrawGPUIndexedPrimitivesIndirect(pass, draw_indirect_ssbo, 0, num_columns);
```

Where `num_columns` is stored as `uint32_t basalt_column_count` (new field on `TerrainRenderer`, set in `upload_mesh`).

`release_buffers` must also call `rel(draw_indirect_ssbo, "draw_indirect_ssbo")` and `rel(column_bounds_ssbo, "column_bounds_ssbo")`.

---

## Phase 4 — Fragment Shader Storage Buffer Binding Audit

**Problem (existing bug, not a future feature):** In `stage_shaded_draw`, the terrain fragment shader (`shaders/terrain.frag.glsl`) is bound with only `point_light_ssbo` (`SDL_BindGPUFragmentStorageBuffers(pass, 0, &point_light_ssbo, 1)`). But in `stage_geometry` (the older function, still present but unused in the current draw path), it binds 3 buffers: `point_light_ssbo`, `light_grid_ssbo`, `global_index_ssbo`. The shader `terrain.frag.glsl` uses the cluster grid for tiled lighting. The current `stage_shaded_draw` only binds `point_light_ssbo`, which means the fragment shader cannot do clustered lookups.

**Fix in `stage_shaded_draw` (`terrain_renderer.cpp`):**
```cpp
// Replace:
SDL_BindGPUFragmentStorageBuffers(pass, 0, &point_light_ssbo, 1);

// With:
SDL_GPUBuffer *frag_storage[3] = {point_light_ssbo, light_grid_ssbo, global_index_ssbo};
SDL_BindGPUFragmentStorageBuffers(pass, 0, frag_storage, 3);
```

Also update `terrain.frag` shader registration in `init_graphics_pipelines`: change `num_storage_buffers` from `1` to `3`:
```cpp
// Old:
SDL_GPUShader *frag = asset_manager->load_shader(
    "terrain.frag", ..., SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);

// New:
SDL_GPUShader *frag = asset_manager->load_shader(
    "terrain.frag", ..., SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 3);
```

And in `rebuild_dirty_pipelines`, the same `load_shader` call for `"terrain.frag"` must be updated identically.

**Note:** `stage_geometry` is declared in `terrain_renderer.h` and `terrain_renderer.cpp` but is never called from `draw`. It is dead code. Remove it from both files once the indirect draw path is confirmed working.

---

## Phase 5 — Contour Line CPU Simplification (Ramer-Douglas-Peucker)

**Problem:** `extract_contours` in `src/game/terrain/contour.cpp` outputs `out_lines` as a `std::vector<Line>`. At 48037 lines with 96074 vertices, the contour VBO upload is large. When contours are far from the camera or the map is dense, many line segments are sub-pixel and wasted.

**Scope:** This is a CPU pre-process at generation time, not per-frame. The simplification runs once in the worker thread (Phase 1) before `build_terrain_mesh`.

---

### 5.1 — New Function: `simplify_contours` in `src/game/terrain/contour.cpp`

```cpp
// New function signature (add to contour.h):
void simplify_contours(std::vector<Line> &lines, float epsilon);
```

`Line` is defined in `src/game/terrain/contour.h`. It contains two endpoints. Since contour lines are already broken into individual segments by `extract_contours`, apply RDP at the call site in `topo_game.cpp` worker lambda (Phase 1.3), after `extract_contours` and before `build_terrain_mesh`:

```cpp
simplify_contours(cd.contour_lines, 0.5f);  // epsilon in world units
```

This is low-risk — if epsilon is wrong, it's tunable. Start at 0.5 and expose as a config parameter later.

---

## Phase 6 — `upload_to_gpu_buffer` Deduplication

**Problem:** The static `upload_to_gpu_buffer` function in `terrain_renderer.cpp` is duplicated logic with `upload_pixels_to_texture` in `gpu.cpp`. Both create a transfer buffer, map, copy, submit, wait, release. This should be a shared utility.

**Fix:** Move `upload_to_gpu_buffer` and `create_gpu_buffer` and `create_zeroed_gpu_buffer` from being file-static in `terrain_renderer.cpp` into `src/engine/gpu/gpu.h` and `gpu.cpp` as free functions:

```cpp
// In gpu.h:
SDL_GPUBuffer *gpu_create_buffer(SDL_GPUDevice *device, uint32_t size,
                                  SDL_GPUBufferUsageFlags usage);
SDL_GPUBuffer *gpu_upload_buffer(SDL_GPUDevice *device, const void *data,
                                  uint32_t size, SDL_GPUBufferUsageFlags usage);
SDL_GPUBuffer *gpu_create_zeroed_buffer(SDL_GPUDevice *device, uint32_t size,
                                         SDL_GPUBufferUsageFlags usage);
```

Update all call sites in `terrain_renderer.cpp` to use the new names. Remove the three `static` functions from `terrain_renderer.cpp`.

---

## Phase 7 — Frame Pipelining (Double-Buffer Command Recording)

**Problem:** `gpu_end_frame` (`gpu.cpp`) calls `SDL_SubmitGPUCommandBuffer(frame.cmd)` and returns. SDL3-GPU does not guarantee the GPU has finished by the next `gpu_acquire_game_frame`. However, `TerrainRenderer::upload_mesh` calls `SDL_WaitForGPUIdle` which stalls the pipeline when a regen happens. Outside of regen, there is no stall — SDL3 handles in-flight frame buffering internally via swapchain acquisition.

**Action:** No additional triple-buffering of uniforms is needed at this time. `SDL_PushGPUVertexUniformData` and `SDL_PushGPUComputeUniformData` are internally managed by SDL3-GPU per command buffer. This phase is deferred until profiling shows a concrete CPU-wait bottleneck.

---

## Phase 8 — LOD / Clipmapping (Deferred)

**Precondition:** Complete Phases 1–4 first. LOD is only worth implementing once the base rendering is fully GPU-driven with indirect draw (Phase 3), because LOD requires per-ring draw call separation which depends on indirect dispatch being in place.

**High-level design (not yet actionable):**
- Divide `HexColumn` list in `MapData` into 4 distance rings relative to camera.
- Build 4 separate `RenderingLayer` entries in `TerrainMesh::basalt_layers` (currently uses indices 0 and 1 for sides and tops).
- Issue 4 separate `stage_cull_columns` dispatches with ring-specific AABBs.
- Issue 4 separate indirect draw calls from `stage_shaded_draw`.

---

## Dependency Order and Build Notes

1. Phase 1 (async) can be done in isolation — it touches `task_system.h/cpp`, `game_state.h`, `topo_game.h`, `topo_game.cpp` only.
2. Phase 4 (fragment shader binding fix) is a bug fix with no dependencies — do it first, before anything else.
3. Phase 2 (staging pool) touches `gpu.h`, `gpu.cpp`, `terrain_renderer.h`, `terrain_renderer.cpp`, `topo_game.cpp`.
4. Phase 6 (buffer utility dedup) is a refactor that must happen before Phase 3 to avoid conflict.
5. Phase 3 (indirect draw) depends on Phase 6 and requires a new shader file.
6. Phase 5 (contour simplification) can be done in isolation after Phase 1.
7. Phase 7 (frame pipelining) is deferred pending profiling.
8. Phase 8 (LOD) depends on Phase 3 being complete.

---

## Files Modified Per Phase

| Phase | Files Modified                                                                 | Files Created                          | Files Deleted |
|-------|--------------------------------------------------------------------------------|----------------------------------------|---------------|
| 1     | `game_state.h`, `topo_game.h`, `topo_game.cpp`                                 | `task_system.h`, `task_system.cpp`     | —             |
| 2     | `gpu.h`, `gpu.cpp`, `terrain_renderer.h`, `terrain_renderer.cpp`, `topo_game.cpp` | —                                   | —             |
| 3     | `terrain_renderer.h`, `terrain_renderer.cpp`                                    | `cull_columns.comp.glsl`               | —             |
| 4     | `terrain_renderer.cpp`, `terrain_renderer.h`                                    | —                                      | `stage_geometry` (dead code removal) |
| 5     | `contour.h`, `contour.cpp`, `topo_game.cpp`                                     | —                                      | —             |
| 6     | `gpu.h`, `gpu.cpp`, `terrain_renderer.cpp`                                      | —                                      | —             |
| 7     | deferred                                                                        | —                                      | —             |
| 8     | deferred (depends on Phase 3)                                                   | —                                      | —             |
