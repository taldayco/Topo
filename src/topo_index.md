# Topo Source Index

A comprehensive guide to navigating the Topo project codebase. This index is organized by module and designed for efficient AI-assisted development.

## Table of Contents
- [Project Structure](#project-structure)
- [Module Overview](#module-overview)
- [Engine (src/engine)](#engine-srcengine)
- [Game (src/game)](#game-srcgame)
- [Terrain System (src/game/terrain)](#terrain-system-srcgameterrain)
- [Shaders (src/shaders)](#shaders-srcshaders)
- [Key Workflows](#key-workflows)
- [Data Flow](#data-flow)

---

## Project Structure

```
src/
├── engine/                 # Core engine systems
│   ├── app.h/cpp          # Application framework
│   ├── camera/            # Camera system
│   ├── core/              # Core types and definitions
│   ├── gpu/               # GPU/graphics context
│   ├── input/             # Input handling
│   ├── render/            # Rendering systems
│   └── ui/                # UI (ImGui integration)
├── game/                  # Game-specific code
│   ├── terrain/           # Terrain generation & rendering
│   ├── main.cpp           # Entry point
│   ├── config.h           # Game configuration
│   ├── game_state.h       # Game state structures
│   └── topo_game.h/cpp    # Game class implementation
└── shaders/               # GLSL shaders
```

---

## Module Overview

| Module | Purpose | Key Files | Dependencies |
|--------|---------|-----------|--------------|
| **Engine** | Core application framework | app.h/cpp, gpu.h/cpp | SDL3, FlECS |
| **Camera** | View management & transforms | camera.h/cpp | Engine core |
| **GPU** | Graphics device & rendering | gpu.h/cpp | SDL3-GPU |
| **Input** | Keyboard/mouse handling | input.h/cpp | SDL3 |
| **Render** | Sprite/texture rendering | render_system.h/cpp, sprite.h/cpp | GPU, Engine |
| **UI** | User interface | imgui_ui.h/cpp | ImGui, SDL3 |
| **Background** | Star field background | background.h/cpp | Engine core |
| **Terrain** | Heightmap & geometry generation | terrain_*.h/cpp | FastNoiseLite, Flecs |
| **Shaders** | GPU programs | *.glsl | GLSL |

---

## Engine (src/engine)

### Application Framework
**Files:** `app.h`, `app.cpp`

The base `Application` class defines the main game loop and override points.

#### Key Classes & Methods

**class Application**
- `int run()` - Main game loop (defined in app.cpp)
- `virtual void on_init(GpuContext &gpu, flecs::world &ecs)` - Initialize game
- `virtual void on_event(const SDL_Event &event, flecs::world &ecs)` - Handle events
- `virtual void on_render_tool(GpuContext &gpu, FrameContext &frame, flecs::world &ecs)` - Render UI/tool layer
- `virtual void on_render_game(GpuContext &gpu, FrameContext &frame, flecs::world &ecs)` - Render game layer
- `virtual void on_cleanup(flecs::world &ecs)` - Cleanup resources
- `virtual bool wants_game_window_open(flecs::world &ecs)` - Query game window state
- `virtual bool wants_game_window_close(flecs::world &ecs)` - Query window close request
- `void request_quit()` - Signal application shutdown
- `GpuContext &gpu()` - Get GPU context reference

#### Related Structures
- `GpuContext` - GPU device state (see gpu.h)
- `FrameContext` - Per-frame rendering state (see gpu.h)
- `flecs::world` - ECS world from FlECS

---

### Camera System
**Files:** `camera/camera.h`, `camera/camera.cpp`

Manages camera positioning, zooming, shaking, and view transformations.

#### Key Classes & Methods

**struct CameraState**
- Holds camera position, zoom, shake parameters
- Used in Camera namespace functions

**namespace CameraSystem**
- `void update(CameraState &cam, float dt)` - Update camera state per frame
- `void follow(CameraState &cam, float target_x, float target_y)` - Smooth follow target
- `void stop_follow(CameraState &cam)` - Stop following
- `void shake(CameraState &cam, float intensity, float duration)` - Screen shake effect
- `void set_zoom(CameraState &cam, float zoom)` - Set zoom level
- `void apply_to_view(const CameraState &cam, ViewState &view)` - Apply camera to view

#### Utility Functions
- `static float lerp(float a, float b, float t)` - Linear interpolation

#### Related Structures
- `ViewState` - View transformation state (see gpu.h)

---

### Core Types
**Files:** `core/types.h`

Contains fundamental type definitions and constants used throughout the engine.

---

### GPU & Graphics
**Files:** `gpu/gpu.h`, `gpu/gpu.cpp`

Manages SDL3 GPU device, framebuffers, textures, and rendering operations.

#### Key Structures

**struct GpuContext**
- `SDL_GPUDevice *device` - GPU device handle
- `SDL_Window *window` - Main window (tool/editor window)
- `SDL_Window *game_window` - Game viewport window (optional)
- Other frame/texture resources

**struct FrameContext**
- `SDL_GPUCommandBuffer *cmd` - Command buffer for frame
- `SDL_GPURenderPass *render_pass` - Active render pass
- Other rendering state

**struct TextureHandle**
- Opaque texture handle for GPU resources

**struct ViewState**
- View matrix and projection parameters

#### Key Functions

**Initialization & Cleanup**
- `bool gpu_init(GpuContext &ctx)` - Initialize GPU device and main window
- `void gpu_cleanup(GpuContext &ctx)` - Cleanup all GPU resources

**Window Management**
- `bool gpu_create_game_window(GpuContext &ctx)` - Create secondary game window
- `void gpu_destroy_game_window(GpuContext &ctx)` - Destroy game window

**Frame Management**
- `bool gpu_acquire_frame(GpuContext &ctx, FrameContext &frame)` - Acquire frame for tool window
- `bool gpu_acquire_game_frame(GpuContext &ctx, FrameContext &frame)` - Acquire frame for game window
- `bool gpu_begin_render_pass(GpuContext &ctx, FrameContext &frame)` - Begin render pass
- `void gpu_end_frame(FrameContext &frame)` - Submit and present frame

**Rendering**
- `void gpu_blit_texture(FrameContext &frame, const TextureHandle &tex, const ViewState &view)` - Draw texture to framebuffer

**Resource Management**
- `void release_texture(SDL_GPUDevice *device, const TextureHandle &handle)` - Release GPU texture

---

### Input System
**Files:** `input/input.h`, `input/input.cpp`

Handles keyboard and mouse input events.

#### Key Types

**enum Action**
- Game input actions (defined in input.h)

**namespace InputSystem**
- `void init()` - Initialize input system
- `void begin_frame()` - Reset input state each frame
- `void handle_event(const SDL_Event &event)` - Process SDL event
- `void bind(SDL_Scancode key, Action action)` - Bind key to action

---

### Render System
**Files:** `render/render_system.h`, `render/render_system.cpp`

Core rendering utilities and color blending.

#### Key Functions

**Color Processing**
- `static uint32_t alpha_blend_rs(uint32_t src, uint32_t dst, float alpha)` - Blend two colors with alpha

---

### Sprite Rendering
**Files:** `render/sprite.h`, `render/sprite.cpp`

Sprite management and texture handling.

#### Key Classes

**class SpriteManager**
- `void cleanup(SDL_GPUDevice *device)` - Cleanup sprite resources

### Background Rendering
**Files:** `render/background.h`, `render/background.cpp`

Animated star field background rendering.

#### Key Classes

**class BackgroundRenderer**
- `bool init(SDL_GPUDevice* device, SDL_GPUTextureFormat swapchain_format)` - Initialize background shaders and pipeline
- `void draw(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* render_pass, float time)` - Draw background to render pass
- `void cleanup()` - Release GPU resources

---

### UI System
**Files:** `ui/imgui_ui.h`, `ui/imgui_ui.cpp`

ImGui integration for tool UI.

#### Key Functions

**UI Lifecycle**
- `void ui_init(SDL_Window *window, SDL_GPUDevice *device)` - Initialize ImGui
- `void ui_shutdown()` - Cleanup ImGui
- `void ui_process_event(SDL_Event &event)` - Forward SDL events to ImGui
- `void ui_begin_frame()` - Start UI frame
- `void ui_end_frame()` - End UI frame

**Rendering**
- `void ui_prepare_draw(SDL_GPUCommandBuffer *cmd)` - Prepare GPU for UI drawing
- `void ui_draw(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *render_pass)` - Draw UI to render pass

---

## Game (src/game)

### Game Entry Point
**Files:** `main.cpp`

- `int main()` - Application entry point, creates TopoGame instance and runs main loop

---

### Configuration
**Files:** `config.h`

Game configuration constants and parameters.

---

### Game State
**Files:** `game_state.h`

Game state structures and ECS component definitions.

---

### Game Implementation
**Files:** `topo_game.h`, `topo_game.cpp`

Main game class implementing `Application` interface.

#### Key Classes & Methods

**class TopoGame : public Application**

**Lifecycle Overrides**
- `void on_init(GpuContext &gpu, flecs::world &ecs) override` - Initialize game, load terrain
- `void on_event(const SDL_Event &event, flecs::world &ecs) override` - Handle input events
- `void on_render_tool(GpuContext &gpu, FrameContext &frame, flecs::world &ecs) override` - Render editor UI
- `void on_render_game(GpuContext &gpu, FrameContext &frame, flecs::world &ecs) override` - Render game scene
- `void on_cleanup(flecs::world &ecs) override` - Cleanup game resources

**Window Management Overrides**
- `bool wants_game_window_open(flecs::world &ecs) override` - Game window state query
- `bool wants_game_window_close(flecs::world &ecs) override` - Game window close query

**Game Logic**
- `void render_ui(flecs::world &ecs, bool game_window_open)` - Render editor/tool UI panels
- `params_to_json(...)` - Serialize parameters to JSON (in on_event)
- `json_to_params(...)` - Deserialize parameters from JSON (in on_event)

#### Data Members
- Terrain generator parameters (elevation, worley, composition, terrain style)
- Game state (camera, input, rendering state)
- ECS entities and systems

---

## Terrain System (src/game/terrain)

The terrain system is the heart of Topo. It generates procedural heightmaps and converts them into renderable geometry.

### System Architecture

```
Noise Generation (noise_layers.cpp)
    ↓
Elevation Map → Compositing (noise_composer.cpp)
    ↓
Band/Contour Detection (contour.cpp)
    ↓
Terrain Generator (terrain_generator.cpp)
    ├→ Plateau Detection & Extraction
    ├→ Hexagon Column Generation
    ├→ Lava Body Generation
    └→ Mesh Generation (terrain_mesh.cpp)
    ↓
Mesh Upload (terrain_renderer.cpp)
    ↓
GPU Rendering (shaders/)
```

### Noise Generation
**Files:** `noise.h`, `noise.cpp`, `noise_layers.h`, `noise_layers.cpp`

Procedural noise generation using FastNoiseLite.

#### Key Structures

**struct NoiseParams**
- `int seed` - Random seed
- Fractal, frequency, and other noise configuration

**struct ElevationNoiseParams**
- Elevation-specific noise parameters

**struct WorleyNoiseParams**
- Worley/cellular noise parameters

#### Key Functions

**Noise Generation** (in noise.cpp)
- Creates layered noise with multiple octaves
- Applies gradient-based smoothing
- Caches results for performance

**Noise Layering** (in noise_layers.cpp)
- `static void seed_offset(int seed, float &ox, float &oy)` - Seed-based coordinate offset
- `static float biased_smoothstep(float t, float bias)` - Biased smoothing curve
- Combines multiple noise layers for terrain features

---

### Noise Composition
**Files:** `noise_composer.h`, `noise_composer.cpp`

Combines multiple noise sources (elevation, worley) into final elevation map.

#### Key Functions

- Blends elevation and worley noise
- Applies masks and filters
- Generates band classification map

---

### Elevation & Composition
**Files:** `map_data.h`

Data structures for elevation and band maps.

#### Key Structures

**struct MapData**
- `std::vector<float> elevation` - Height values [0..1]
- `std::vector<int> bands` - Elevation band/contour indices
- `int width, height` - Map dimensions
- `void allocate(int w, int h)` - Initialize map

---

### Contour & Band Detection
**Files:** `contour.h`, `contour.cpp`

Detects elevation bands and contour lines.

#### Key Structures

**struct Plateau**
- Represents a contiguous elevation band
- Contains pixel indices and computed properties

#### Key Functions

- Flood fill to identify connected regions
- Classifies pixels into elevation bands

---

### Terrain Generation
**Files:** `terrain_generator.h`, `terrain_generator.cpp`

Converts heightmap and band data into geometric meshes.

#### Key Classes

**class TerrainGenerator**

**struct TerrainData**
- `std::vector<Plateau> plateaus` - Elevation regions
- `std::vector<HexColumn> columns` - Hexagonal geometry
- `std::vector<LavaBody> lava_bodies` - Lava regions
- `std::vector<int> plateaus_with_columns` - Plateau-to-column mapping
- `std::vector<int16_t> terrain_map` - Spatial indexing (1..N = plateau+1, 0 = void)

**Static Methods**
- `TerrainData generate(std::span<const float> heightmap, std::span<const int> band_map, int width, int height)` - Main generation function

---

### Hexagon System
**Files:** `hex.h`, `hex.cpp`, `isometric.h`, `isometric.cpp`

Hexagonal grid and isometric coordinate systems.

#### Key Structures

**struct HexCoord**
- `int q, r` - Axial hex coordinates
- `HexCoordHash` - Hash function for use in unordered_map

**struct IsoVec2**
- Isometric 2D vector for screen-space rendering

**struct HexColumn**
- 3D hexagonal column with height/color data

#### Key Functions

**Coordinate Conversion**
- `void hex_to_pixel(int q, int r, float hex_size, float &out_x, float &out_y)` - Hex to screen coords
- `HexCoord pixel_to_hex(float x, float y, float hex_size)` - Screen to hex coords
- `void get_hex_corners(int q, int r, float hex_size, Vec2 corners[6])` - Get corner vertices

**Point-in-Hex Testing**
- `bool pixel_in_hex(float px, float py, int q, int r, float hex_size)` - 2D containment test
- `bool point_in_hex_iso(float px, float py, const IsoVec2 corners[6])` - Isometric containment test

**Rendering**
- `void compute_visible_edges(std::vector<HexColumn> &columns)` - Cull hidden edges

---

### Lava Generation
**Files:** `lava.h`, `lava.cpp`

Procedural lava flow and void generation.

#### Key Structures

**struct LavaBody**
- Represents a lava region with pixels and properties

**struct FloodFillResult**
- Results of lava/void generation

#### Key Functions

- `FloodFillResult generate_lava_and_void(MapData &data, float void_chance, int seed = 0)` - Generate lava and void regions
- `static float poly_area(const std::vector<P2> &P)` - Polygon area calculation
- `static bool point_in_tri(const P2 &p, const P2 &a, const P2 &b, const P2 &c)` - Triangle containment
- `static void densify_region(std::vector<int> &pixels, int width, int height)` - Fill region gaps

---

### Terrain Mesh
**Files:** `terrain_mesh.h`, `terrain_mesh.cpp`

Converts geometric data into renderable mesh vertex/index buffers.

#### Key Structures

**struct BasaltVertex**
- `float pos_x, pos_y` — iso scene-space position
- `float color_r, color_g, color_b` — base color
- `float depth` — normalized depth for sorting
- `float sheen` — 0–1 reflectivity for lava/star sheen
- `float nx, ny, nz` — world-space facet normal (tops: 0,0,1; sides: outward horizontal)

**struct SceneUniforms** (8 × vec4, std140)
- `proj_scale_offset` — orthographic scale + offset
- `params1` — time, tile_width, tile_height, height_scale
- `params2` — iso_offset_x, iso_offset_y, contour_opacity
- `lava_color` — r,g,b + depth_range
- `lava_light` — iso scene-space point light x,y + intensity
- `star_light` — sparkle color r,g,b + intensity
- `light_dir` — world-space directional light direction (normalized) + ambient in .w
- `light_col` — directional light color

**struct TerrainMesh**
- `basalt_layers[0]` — side face vertices/indices
- `basalt_layers[1]` — top face vertices/indices

#### Key Functions

- `static void color_to_float(uint32_t c, float &r, float &g, float &b)` - Color format conversion
- `static void add_hex_top(...)` — emits 6 verts with normal (0,0,1)
- `static void add_side_face(...)` — emits 4 verts with outward horizontal normal derived from edge cross product
- `TerrainMesh build_terrain_mesh(terrain, map_data, contours)` — full mesh construction
- `SceneUniforms compute_uniforms(mesh, map_data, view, w, h, time, contour_opacity)` — computes projection, lava centroid light position, and directional light

---

### Terrain Rendering
**Files:** `terrain_renderer.h`, `terrain_renderer.cpp`

GPU rendering pipeline for terrain meshes.

#### Key Classes

**class TerrainRenderer**

**Initialization & Cleanup**
- `void init(SDL_GPUDevice *device, SDL_Window *window)` - Initialize shaders and buffers
- `void release_buffers(SDL_GPUDevice *device)` - Release GPU buffers
- `void cleanup(SDL_GPUDevice *device)` - Full cleanup

**Rendering**
- `void upload_mesh(SDL_GPUDevice *device, const TerrainMesh &mesh)` - Upload mesh to GPU
- Returns render pass for drawing (allocated with `SDL_BeginGPURenderPass`)

**State Queries**
- `bool is_initialized() const` - Check if renderer is ready
- `bool has_mesh() const` - Check if mesh is loaded

#### Utility Functions
- `static std::vector<uint8_t> load_shader_file(const char *path)` - Load compiled shader

---

### Color & Palettes
**Files:** `color.h`, `palettes.h`

Color utilities and elevation-based color palettes.

#### Key Functions (color.h)

- `inline uint32_t lerp_color(uint32_t c1, uint32_t c2, float t)` - Interpolate between colors
- `inline uint32_t darken_color(uint32_t color, float darkness)` - Darken color
- `inline uint32_t alpha_blend(uint32_t src, uint32_t dst, float alpha)` - Blend colors with alpha
- `inline uint32_t modulate_color(uint32_t color, float factor)` - Scale color brightness

#### Key Functions (palettes.h)

**Palette Structures**
- `struct Palette` - Contains 5 elevation-based colors

**Color Mapping**
- `inline uint32_t get_elevation_color_smooth(float h, const Palette &p)` - Smooth interpolation across palette
- `inline uint32_t get_elevation_color(float h, const Palette &p)` - Discrete elevation colors
- `inline uint32_t organic_color(float h, int x, int y, const Palette &p)` - Add noise variation

---

### Basalt & Detail Layers
**Files:** `basalt.h`, `basalt.cpp`, `detail.h`, `detail.cpp`, `delve_render.h`, `delve_render.cpp`

Surface detail and visual effects on terrain.

---

### Utility Functions
**Files:** `util.h`, `flood_fill.h`

General-purpose utilities.

#### Key Functions (util.h)

- `inline uint32_t hash2d(int x, int y)` - 2D integer hash
- `inline uint32_t hash1d(int idx)` - 1D integer hash

#### Flood Fill (flood_fill.h)

- Connected-component analysis
- Region detection and labeling

---

### Noise Caching
**Files:** `noise_cache.h`

Optional noise caching system for performance optimization.

#### Key Methods

**template <typename T> struct NoiseCache**
- `bool get(Slot slot, uint64_t param_hash, std::vector<float> &out)` - Retrieve cached noise
- `void put(Slot slot, uint64_t param_hash, const std::vector<float> &data)` - Cache noise
- `void invalidate_all()` - Clear cache

---

### FastNoiseLite
**Files:** `FastNoiseLite.h`

Third-party noise library (header-only). Provides:
- Perlin, Simplex, Value, Cellular noise types
- Fractal noise (FBm, Ridged, PingPong)
- Domain warping effects
- Full documentation in header

#### Key Methods

**Configuration**
- `void SetSeed(int seed)`
- `void SetFrequency(float frequency)`
- `void SetNoiseType(NoiseType noiseType)`
- `void SetFractalType(FractalType fractalType)`
- `void SetFractalOctaves(int octaves)`
- `void SetFractalLacunarity(float lacunarity)`
- `void SetFractalGain(float gain)`

**Evaluation**
- `float GetNoise(FNfloat x, FNfloat y)` - 2D noise
- `float GetNoise(FNfloat x, FNfloat y, FNfloat z)` - 3D noise
- `void DomainWarp(FNfloat &x, FNfloat &y)` - Coordinate warping

---

## Shaders (src/shaders)

GLSL shader programs for GPU rendering. Organized by terrain feature.

### Terrain Shaders
**Files:** `terrain.vert.glsl`, `terrain.frag.glsl`

Main terrain mesh rendering.

#### Vertex Shader (locations)
- `0` `vec2 in_pos` — iso scene-space position
- `1` `vec3 in_color` — base color
- `2` `float in_depth` — depth value
- `3` `float in_sheen` — sheen strength
- `4` `vec3 in_normal` — world-space facet normal
- Passes `frag_color`, `frag_screen_pos`, `frag_sheen`, `frag_normal` to fragment stage

#### Fragment Shader
- `apply_lighting(color, normal)` — Lambertian diffuse in world space using `light_dir`/`light_col`/`ambient` uniforms; camera-independent
- `apply_sheen(pos, color, strength)` — additive lava point-light glow + per-hex star sparkle
- Pipeline: hex-cell dither → Lambertian lighting → sheen → clamp output

---

### Contour Shaders
**Files:** `contour.vert.glsl`, `contour.frag.glsl`

Elevation contour line rendering (optional overlay).

---

### Lava Shaders
**Files:** `lava.vert.glsl`, `lava.frag.glsl`

Lava region rendering with animation effects.

#### Vertex Shader
- Transforms lava geometry

#### Fragment Shader
- Animated lava color/glow
- Lava-specific visual effects

---

## Key Workflows

### Workflow: Terrain Generation

1. **Noise Generation** (`noise_layers.cpp`)
   - Create elevation noise (Perlin/Simplex)
   - Create worley noise (cellular)
   - Layer and composite them

2. **Elevation Composition** (`noise_composer.cpp`)
   - Blend elevation and worley maps
   - Apply post-processing
   - Output: `MapData` with elevation and bands

3. **Band Contour Detection** (`contour.cpp`)
   - Flood-fill to identify plateaus
   - Build `Plateau` structures
   - Generate plateau-to-column mapping

4. **Terrain Generation** (`terrain_generator.cpp::TerrainGenerator::generate()`)
   - Create hexagonal columns for each plateau
   - Generate lava bodies
   - Assign colors from palettes

5. **Mesh Generation** (`terrain_mesh.cpp`)
   - Convert columns to vertex/index buffers
   - Apply color mapping

6. **GPU Upload** (`terrain_renderer.cpp::TerrainRenderer::upload_mesh()`)
   - Upload mesh to GPU
   - Create render pass for drawing

7. **Rendering** (shaders)
   - Vertex shader transforms vertices
   - Fragment shader applies final coloring

---

### Workflow: User Interaction (Editor)

1. **Input Handling** (`input.cpp`)
   - Process SDL events
   - Map keys to `Action` enum

2. **Event Processing** (`topo_game.cpp::on_event()`)
   - Update parameters based on actions
   - Trigger terrain regeneration

3. **UI Rendering** (`topo_game.cpp::render_ui()`)
   - Draw ImGui panels with parameter sliders
   - Show real-time terrain preview

4. **Parameters Serialization**
   - `params_to_json()` - Save to file
   - `json_to_params()` - Load from file

---

### Workflow: Frame Rendering

1. **Main Loop** (`app.cpp::Application::run()`)
   - Acquire frame from GPU
   - Begin render pass

2. **Tool Rendering** (`topo_game.cpp::on_render_tool()`)
   - Render UI with `ui_draw()`
   - Terrain preview rendering

3. **Game Rendering** (`topo_game.cpp::on_render_game()`)
   - Render full terrain mesh
   - Apply camera transforms

4. **Frame Submission** (`gpu.cpp::gpu_end_frame()`)
   - End render pass
   - Present frame to window

---

## Data Flow

### Core Data Pipeline

```
┌─────────────────────────────────────────────────────────┐
│                    User Parameters                       │
│  (elevation, worley, composition, terrain style, seed)  │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
         ┌─────────────────────────────┐
         │   Noise Generation          │
         │ (noise_layers.cpp)          │
         └──────────────┬──────────────┘
                        │
                        ▼
              ┌──────────────────────┐
              │    Elevation Map     │
              │ (MapData::elevation) │
              └──────────┬───────────┘
                         │
                         ▼
             ┌───────────────────────────┐
             │ Noise Composition         │
             │ (noise_composer.cpp)      │
             └──────────┬────────────────┘
                        │
         ┌──────────────┴──────────────┐
         │                             │
         ▼                             ▼
  ┌─────────────┐            ┌──────────────┐
  │ Elevation   │            │ Band Map     │
  │ (float[])   │            │ (int[])      │
  └──────┬──────┘            └────────┬─────┘
         │                           │
         └───────────────┬───────────┘
                         │
                         ▼
         ┌───────────────────────────────┐
         │  Contour Detection            │
         │  (contour.cpp)                │
         │  → Identifies plateaus        │
         │  → Flood-fill regions         │
         └──────────────┬────────────────┘
                        │
                        ▼
            ┌────────────────────────────┐
            │   Terrain Generation       │
            │ (terrain_generator.cpp)    │
            │ → Hex columns              │
            │ → Lava bodies              │
            │ → Color assignment         │
            └──────────────┬─────────────┘
                           │
                           ▼
              ┌──────────────────────────┐
              │  Terrain Mesh            │
              │ (terrain_mesh.cpp)       │
              │ Vertex/Index buffers     │
              └──────────────┬───────────┘
                             │
                             ▼
                  ┌───────────────────────┐
                  │  GPU Upload           │
                  │ (terrain_renderer.cpp)│
                  └──────────────┬────────┘
                                 │
                                 ▼
                      ┌────────────────────┐
                      │ Frame Rendering    │
                      │ (shaders)          │
                      └────────────────────┘
```

---

## Finding Components

### By Responsibility

| What I need | Look in | Key files |
|------------|---------|-----------|
| Change camera behavior | `engine/camera/` | camera.h, camera.cpp |
| Modify input handling | `engine/input/` | input.h, input.cpp |
| Adjust terrain colors | `game/terrain/` | color.h, palettes.h |
| Change noise parameters | `game/terrain/` | noise_layers.h, terrain_generator.h |
| Modify hex geometry | `game/terrain/` | hex.h, hex.cpp |
| Adjust lava generation | `game/terrain/` | lava.h, lava.cpp |
| Change shader effects | `shaders/` | *.glsl files |
| Modify UI panels | `game/` | topo_game.cpp |
| Add ECS systems | `game/` | game_state.h, topo_game.cpp |
| GPU rendering pipeline | `engine/gpu/` | gpu.h, gpu.cpp |

### By Data Structure

| Data Structure | Definition | Usage |
|---|---|---|
| `MapData` | `terrain/map_data.h` | Stores elevation & band maps |
| `TerrainData` | `terrain/terrain_generator.h` | Output of terrain generation |
| `TerrainMesh` | `terrain/terrain_mesh.h` | Renderable mesh data |
| `HexColumn` | `terrain/hex.h` | Individual hex geometry |
| `Plateau` | `terrain/contour.h` | Elevation region |
| `LavaBody` | `terrain/lava.h` | Lava region |
| `CameraState` | `engine/camera/camera.h` | Camera position/zoom |
| `GpuContext` | `engine/gpu/gpu.h` | GPU device state |
| `GameState` | `game/game_state.h` | Game-level state |

---

---

## Known Bugs Fixed (session history)

### Fix 1 — `lava.cpp`: `trace_outline_4connected` loop guard (Session 1)
- **File:** `src/game/terrain/lava.cpp` ~line 147
- **Bug:** Guard was `W * H * 8` (8 388 608 on 1024×1024), stalling the main thread
- **Fix:** Changed to `2 * (W + H) * 4` (16 384 max), proportional to map perimeter

### Fix 2 — `noise_layers.cpp`: Missing `out_cell_value.resize(n)` (Session 2)
- **File:** `src/game/terrain/noise_layers.cpp`, `generate_worley_layer`
- **Bug:** Wrote to all `n` indices of `out_cell_value` without ever allocating it → 1M+ heap-corrupting writes
- **Fix:** Added `out_cell_value.resize(n)` after the two existing resizes

### Fix 3 — `noise_composer.cpp`: Worley cache used `get2`/`put2` instead of `get3`/`put3` (Session 2)
- **File:** `src/game/terrain/noise_composer.cpp` ~lines 122–128
- **Bug:** `worley_cell_value` was silently dropped on every cache hit, so `generate_basalt_columns_v2` read stale/empty data
- **Fix:** Switched to `get3`/`put3` with `data.worley_cell_value` as the third argument

### Fix 4 — `lava.cpp`: `triangulate_ear_clipping` O(N³) blow-up (Session 3)
- **File:** `src/game/terrain/lava.cpp`, `build_triangle_mesh_from_polygon`
- **Bug:** `trace_outline_4connected` (even after Fix 1) can produce up to 16 384-vertex polygons.  
  `triangulate_ear_clipping` is O(N³) worst-case — 16 384³ ≈ 4 trillion ops → system freeze.
- **Fix:** Added a polygon decimation step in `build_triangle_mesh_from_polygon` before passing to  
  the ear clipper: if `poly.size() > MAX_POLY_VERTS (256)`, stride-sample down to 256 vertices.  
  Also updated the post-clipping vertex-lookup loop to use `*input` instead of `poly`.

---

### Fix 5 — `lava.cpp`: `subdivide_large_regions` O(pixels × columns) freeze (Session 4)
- **File:** `src/game/terrain/lava.cpp`, `subdivide_large_regions`
- **Bug:** For every region with >50 000 pixels, the inner loop called `get_hex_corners` for every
  column for every pixel. On a 1024×1024 map this is O(50 000+ × thousands of columns) ≈ hundreds
  of millions of redundant corner computations → 100 % CPU, system freeze.
- **Fix:** Pre-compute all hex centres with `hex_to_pixel` once before the pixel loop (a simple
  `(cx, cy)` pair per column). Added an early-exit inside the distance scan so the inner loop
  breaks as soon as a column within the threshold is found.

### Fix 6 — `lava.cpp`: `render_lava` broken `std::clamp` arguments (Session 4)
- **File:** `src/game/terrain/lava.cpp`, `render_lava`
- **Bug:** `std::clamp(1.0f, 0.8f, 1.1f)` passes the *value* as the first argument and the
  *min/max* as second/third — but all three are literals, so the result is always `1.0f`
  (the constant is already within [0.8, 1.1]).  This means `depth_factor` is always exactly
  1.0 and `modulate_color` is called on every lava pixel with `1.0`, which still works
  functionally but was the remnant of a broken "fix" that left misleading dead code and
  incorrect visual depth shading.
- **Fix:** Replaced with a real depth computation derived from `iso_y / view_h` clamped to
  [0, 1], producing a smooth `depth_factor` in [0.8, 1.1] that actually varies per pixel.

### Fix 10 — `noise_composer.cpp`: Missing `liquid_mask` logic (Session 5)
- **File:** `src/game/terrain/noise_composer.cpp`, `compose_layers`
- **Bug:** The logic to populate `data.liquid_mask` based on elevation and river paths was
  missing/commented out. This meant `liquid_mask` was always false, allowing basalt columns to
  be generated in low-elevation areas intended for lava/void.
- **Fix:** Restored the loop that marks `liquid_mask[i]` based on `river_elevation_max` and
  `river_mask`.

### Fix 11 — `hex.cpp` / `basalt.cpp`: Redundant `get_hex_corners` in `pixel_in_hex` (Session 5)
- **Files:** `src/game/terrain/hex.cpp`, `hex.h`, `basalt.cpp`
- **Bug:** `pixel_in_hex` called `get_hex_corners` internally, which was called for every pixel
  in a column's bounding box during terrain marking. This resulted in millions of redundant
  `sin`/`cos` calls per terrain regeneration, creating a significant performance bottleneck.
- **Fix:** Overloaded `pixel_in_hex` to accept pre-computed corners. Updated `basalt.cpp` to pass
  the corners already calculated in the outer loop.

### Fix 12 — `terrain_mesh.h` / `terrain_renderer.*`: Redundant `void_vbo` and `lava_indices` (Session 5)
- **Files:** `src/game/terrain/terrain_mesh.h`, `terrain_renderer.h`, `terrain_renderer.cpp`
- **Cleanup:** Removed the `void_vbo` buffer and `lava_indices` vector which were defined but
  neither populated nor used in the current point-based lava rendering pipeline.

### Fix 13 — `gpu.cpp` / `terrain_renderer.cpp` / `config.h`: Background color changed to dark grey (Session 6)
- **Files:** `src/engine/gpu/gpu.cpp`, `src/game/terrain/terrain_renderer.cpp`, `src/game/config.h`
- **Change:** Changed background clear color from a light bluish-grey (`0.176f, 0.176f, 0.188f`) to
  a dark grey (`0.1f, 0.1f, 0.1f` / `0xFF1A1A1A`). This provides better contrast for the terrain.

### Fix 15 — `background.h/cpp` / `background.frag.glsl`: Star field background (Session 7)
- **Files:** `src/engine/render/background.h`, `src/engine/render/background.cpp`, `src/shaders/background.frag.glsl`, `src/game/topo_game.cpp`
- **Change:** Implemented a dedicated `BackgroundRenderer` class to render an animated star field
  using a custom GLSL shader. The background is now pitch black (`0.0, 0.0, 0.0`) with glowing,
  twinkling stars. Integrated into the main render loop before terrain rendering.

**Generated:** 2025-02-25  
**Updated:** 2025-02-26 (session 8 — faceted Lambertian lighting added)  
**For:** AI-assisted development, code navigation, and project understanding
