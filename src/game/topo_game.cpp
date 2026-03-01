#include "topo_game.h"
#include "terrain/basalt.h"
#include "terrain/lava.h"
#include "terrain/noise_composer.h"
#include "terrain/contour.h"
#include "terrain/palettes.h"
#include "ui/imgui_ui.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <glm/glm.hpp>

using json = nlohmann::json;

static json params_to_json(const ElevationParams &elev,
                           const WorleyParams &worley, const CompositionParams &comp,
                           const TerrainState &ts) {
  return {
    {"elevation", {
      {"frequency",  elev.frequency}, {"octaves",    elev.octaves},
      {"lacunarity", elev.lacunarity},{"gain",        elev.gain},
      {"seed",       elev.seed},      {"scurve_bias", elev.scurve_bias}
    }},
    {"worley", {
      {"frequency",      worley.frequency},    {"seed",           worley.seed},
      {"jitter",         worley.jitter},       {"warp_amp",       worley.warp_amp},
      {"warp_frequency", worley.warp_frequency},{"warp_octaves",  worley.warp_octaves}
    }},
    {"composition", {
      {"void_chance",    comp.void_chance},
      {"terrace_levels", comp.terrace_levels},
      {"min_region_size",comp.min_region_size}
    }},
    {"terrain", {
      {"use_isometric",  ts.use_isometric},
      {"current_palette",ts.current_palette},
      {"map_scale",      ts.map_scale}
    }}
  };
}

static void json_to_params(const json &j, ElevationParams &elev,
                            WorleyParams &worley, CompositionParams &comp,
                            TerrainState &ts) {
  if (j.contains("elevation")) {
    auto &e = j["elevation"];
    if (e.contains("frequency"))   elev.frequency   = e["frequency"];
    if (e.contains("octaves"))     elev.octaves     = e["octaves"];
    if (e.contains("lacunarity"))  elev.lacunarity  = e["lacunarity"];
    if (e.contains("gain"))        elev.gain        = e["gain"];
    if (e.contains("seed"))        elev.seed        = e["seed"];
    if (e.contains("scurve_bias")) elev.scurve_bias = e["scurve_bias"];
  }
  if (j.contains("worley")) {
    auto &w = j["worley"];
    if (w.contains("frequency"))       worley.frequency       = w["frequency"];
    if (w.contains("seed"))            worley.seed            = w["seed"];
    if (w.contains("jitter"))          worley.jitter          = w["jitter"];
    if (w.contains("warp_amp"))        worley.warp_amp        = w["warp_amp"];
    if (w.contains("warp_frequency"))  worley.warp_frequency  = w["warp_frequency"];
    if (w.contains("warp_octaves"))    worley.warp_octaves    = w["warp_octaves"];
  }
  if (j.contains("composition")) {
    auto &c = j["composition"];
    if (c.contains("void_chance"))     comp.void_chance     = c["void_chance"];
    if (c.contains("terrace_levels"))  comp.terrace_levels  = c["terrace_levels"];
    if (c.contains("min_region_size")) comp.min_region_size = c["min_region_size"];
  }
  if (j.contains("terrain")) {
    auto &t = j["terrain"];
    if (t.contains("use_isometric"))   ts.use_isometric   = t["use_isometric"];
    if (t.contains("current_palette")) ts.current_palette = t["current_palette"];
    if (t.contains("map_scale"))       ts.map_scale       = t["map_scale"];
  }
}

void TopoGame::on_init(GpuContext &gpu, flecs::world &ecs) {
  ecs.set<GamePhase>({});
  ecs.set<TerrainState>({});
  ecs.set<WindowState>({true, false});
  ecs.set<ElevationParams>({});
  ecs.set<RiverParams>({});
  ecs.set<WorleyParams>({});
  ecs.set<CompositionParams>({});
  ecs.set<MapData>({});
  ecs.set<NoiseCache>({});
  ecs.set<ContourData>({});

  input.init();

  const float half = Config::MAP_WIDTH_UNITS * 0.5f;
  camera.world_x = half;
  camera.world_y = half;
  camera.follow_x = half;
  camera.follow_y = half;
  camera.following = true;
  camera.min_x = 0.0f;
  camera.max_x = Config::MAP_WIDTH_UNITS;
  camera.min_y = 0.0f;
  camera.max_y = Config::MAP_HEIGHT_UNITS;
  camera.base_frustum_half_w = half;
  camera.base_frustum_half_h = half;

  ecs.system("InputBeginFrame")
      .kind(flecs::PreUpdate)
      .run([this](flecs::iter &) { input.begin_frame(); });

  ecs.system("CameraUpdate")
      .kind(flecs::PostUpdate)
      .run([this, &ecs](flecs::iter &) {
        auto *phase = ecs.get<GamePhase>();
        if (phase && phase->current != GamePhase::Playing) return;

        auto &in  = input.state();
        float dt  = ecs.delta_time();
        float spd = (camera.base_frustum_half_w / camera.zoom) * 1.5f * dt;

        if (in.held[(int)Action::CameraUp])    camera.world_y -= spd;
        if (in.held[(int)Action::CameraDown])  camera.world_y += spd;
        if (in.held[(int)Action::CameraLeft])  camera.world_x -= spd;
        if (in.held[(int)Action::CameraRight]) camera.world_x += spd;

        bool cam_moved = in.held[(int)Action::CameraUp]    || in.held[(int)Action::CameraDown] ||
                         in.held[(int)Action::CameraLeft]  || in.held[(int)Action::CameraRight];
        if (cam_moved) camera.following = false;

        if (in.held[(int)Action::ZoomIn])
          camera_system.set_zoom(camera, camera.target_zoom + dt * 2.0f);
        if (in.held[(int)Action::ZoomOut])
          camera_system.set_zoom(camera, camera.target_zoom - dt * 2.0f);

        camera_system.update(camera, dt);
      });
}

void TopoGame::on_event(const SDL_Event &event, flecs::world &ecs) {
  input.handle_event(event);

  if (event.type == SDL_EVENT_KEY_DOWN) {
    auto *phase = ecs.get_mut<GamePhase>();
    if (!phase) return;

    if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
      if (phase->current == GamePhase::Playing)  phase->current = GamePhase::Paused;
      else if (phase->current == GamePhase::Paused) phase->current = GamePhase::Playing;
    }
    if (event.key.scancode == SDL_SCANCODE_RETURN) {
      if (phase->current == GamePhase::Menu) phase->current = GamePhase::Playing;
    }
  }
}

void TopoGame::on_render_tool(GpuContext &gpu, FrameContext &frame, flecs::world &ecs) {
  render_ui(ecs, gpu.game_window != nullptr);
  ui_prepare_draw(frame.cmd);
  gpu_begin_render_pass(gpu, frame);
  ui_draw(frame.cmd, frame.render_pass);
}

void TopoGame::on_render_game(GpuContext &gpu, FrameContext &frame, flecs::world &ecs) {
  if (!terrain_renderer.is_initialized()) {
    terrain_renderer.init(gpu.device, gpu.game_window, asset_manager);
    background_renderer.init(gpu.device,
                             SDL_GetGPUSwapchainTextureFormat(gpu.device, gpu.game_window),
                             terrain_renderer.get_depth_format(),
                             asset_manager);
  }

  terrain_renderer.rebuild_dirty_pipelines(gpu.game_window);
  background_renderer.rebuild_if_dirty(
      SDL_GetGPUSwapchainTextureFormat(gpu.device, gpu.game_window),
      terrain_renderer.get_depth_format());

  auto *ts       = ecs.get_mut<TerrainState>();
  auto *elev     = ecs.get_mut<ElevationParams>();
  auto *river    = ecs.get_mut<RiverParams>();
  auto *worley   = ecs.get_mut<WorleyParams>();
  auto *comp     = ecs.get_mut<CompositionParams>();
  auto *map_data = ecs.get_mut<MapData>();
  auto *cache    = ecs.get_mut<NoiseCache>();
  auto *contours = ecs.get_mut<ContourData>();

  if (ts && ts->need_regenerate) {
    SDL_Log("Starting GPU regeneration...");
    auto start = SDL_GetTicks();

    int w = Config::MAP_WIDTH;
    int h = Config::MAP_HEIGHT;
    elev->map_scale = ts->map_scale;

    map_data->allocate(w, h);
    compose_layers(*map_data, *elev, *river, *worley, *comp, cache);
    map_data->columns = generate_basalt_columns_v2(*map_data, Config::HEX_SIZE);

    auto fill_result = generate_lava_and_void(*map_data, comp->void_chance, worley->seed);
    map_data->lava_bodies = std::move(fill_result.lava_bodies);
    map_data->void_bodies = std::move(fill_result.void_bodies);

    int n = w * h;
    contours->heightmap.resize(n);
    std::copy(map_data->basalt_height.begin(), map_data->basalt_height.end(),
              contours->heightmap.begin());
    contours->contour_lines.clear();
    float contour_interval = 1.0f / comp->terrace_levels;
    extract_contours(contours->heightmap, w, h, contour_interval,
                     contours->contour_lines, contours->band_map);

    TerrainMesh mesh = build_terrain_mesh(*ts, *map_data, *contours);
    terrain_renderer.upload_mesh(gpu.device, mesh);

    ts->need_regenerate = false;
    SDL_Log("GPU regeneration: %lu ms", (unsigned long)(SDL_GetTicks() - start));
  }

  float time = SDL_GetTicks() / 1000.0f;

  float aspect = (frame.swapchain_w > 0 && frame.swapchain_h > 0)
                 ? (float)frame.swapchain_w / (float)frame.swapchain_h
                 : 1.0f;

  CameraMatrices cam_mats = camera_system.build_matrices(camera, aspect);

  point_lights.clear();
  if (map_data) {
    const float inv = 1.0f / Config::HEX_SIZE;
    for (const auto &lava : map_data->lava_bodies) {
      if (lava.pixels.empty()) continue;
      float cx = (lava.min_x + lava.max_x) * 0.5f * inv;
      float cy = (lava.min_y + lava.max_y) * 0.5f * inv;
      GpuPointLight pl;
      pl.pos_x     = cx;
      pl.pos_y     = cy;
      pl.pos_z     = lava.height + 1.0f;
      pl.radius    = 40.0f;
      pl.color_r   = 1.0f;
      pl.color_g   = 0.35f;
      pl.color_b   = 0.05f;
      pl.intensity = 3.0f;
      point_lights.push_back(pl);
    }
  }

  SDL_GPURenderPass *bg_pass = terrain_renderer.begin_render_pass(
      frame.cmd, frame.swapchain, frame.swapchain_w, frame.swapchain_h);
  if (!bg_pass) return;

  background_renderer.draw(frame.cmd, bg_pass, time, camera.world_x, camera.world_y);
  SDL_EndGPURenderPass(bg_pass);

  if (terrain_renderer.has_mesh() && ts) {
    const auto *md = ecs.get<MapData>();
    static const MapData empty_map_data;

    terrain_renderer.rebuild_clusters_if_needed(
        frame.cmd, frame.swapchain_w, frame.swapchain_h,
        16.0f, 24, 1.0f, 1000.0f);

    SceneUniforms uniforms = compute_uniforms(
        md ? *md : empty_map_data,
        cam_mats.view, cam_mats.projection,
        terrain_renderer.cluster_tiles_x(), terrain_renderer.cluster_tiles_y(),
        time, ts->contour_opacity,
        (uint32_t)point_lights.size());

    terrain_renderer.draw(frame.cmd, frame.swapchain,
                          frame.swapchain_w, frame.swapchain_h,
                          uniforms, point_lights);
  }

  frame.render_pass = nullptr;
}

void TopoGame::on_cleanup(flecs::world &ecs) {
  terrain_renderer.cleanup(gpu_ctx.device);
  background_renderer.cleanup();
}

bool TopoGame::wants_game_window_open(flecs::world &ecs) {
  auto *ws = ecs.get_mut<WindowState>();
  if (ws && ws->launch_game_requested) {
    ws->launch_game_requested = false;
    return true;
  }
  return false;
}

bool TopoGame::wants_game_window_close(flecs::world &ecs) {
  auto *ws = ecs.get_mut<WindowState>();
  if (ws && ws->close_game_requested) {
    ws->close_game_requested = false;
    return true;
  }
  return false;
}

void TopoGame::render_ui(flecs::world &ecs, bool game_window_open) {
  ui_begin_frame();

  auto *ts     = ecs.get_mut<TerrainState>();
  auto *elev   = ecs.get_mut<ElevationParams>();
  auto *river  = ecs.get_mut<RiverParams>();
  auto *worley = ecs.get_mut<WorleyParams>();
  auto *comp   = ecs.get_mut<CompositionParams>();
  auto *ws     = ecs.get_mut<WindowState>();
  auto *cache  = ecs.get_mut<NoiseCache>();
  auto *contours = ecs.get<ContourData>();

  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y}, ImGuiCond_Always);
  ImGui::Begin("Controls", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

  if (!game_window_open) {
    if (ImGui::Button("Launch Game", {-1, 40})) ws->launch_game_requested = true;
  } else {
    if (ImGui::Button("Close Game",  {-1, 40})) ws->close_game_requested  = true;
  }

  ImGui::Separator();
  ImGui::Text("Elevation");
  ts->need_regenerate |= ImGui::SliderFloat("Frequency",   &elev->frequency,   0.001f, 0.05f);
  ts->need_regenerate |= ImGui::SliderInt(  "Octaves",     &elev->octaves,     1, 8);
  ts->need_regenerate |= ImGui::SliderFloat("Lacunarity",  &elev->lacunarity,  1.0f, 4.0f);
  ts->need_regenerate |= ImGui::SliderFloat("Gain",        &elev->gain,        0.1f, 1.0f);
  ts->need_regenerate |= ImGui::SliderInt(  "Seed",        &elev->seed,        0, 10000);
  ts->need_regenerate |= ImGui::SliderInt(  "Terrace Levels", &comp->terrace_levels, 3, 20);
  ts->need_regenerate |= ImGui::SliderInt(  "Min Region Size",&comp->min_region_size,50, 2000);
  ts->need_regenerate |= ImGui::SliderFloat("S-Curve Bias",&elev->scurve_bias, 0.0f, 1.0f);

  ImGui::Separator();
  ImGui::Text("Worley Noise");
  ts->need_regenerate |= ImGui::SliderFloat("Worley Freq",  &worley->frequency,      0.001f, 0.1f);
  ts->need_regenerate |= ImGui::SliderInt(  "Worley Seed",  &worley->seed,           0, 10000);
  ts->need_regenerate |= ImGui::SliderFloat("Worley Jitter",&worley->jitter,         0.0f, 2.0f);
  ts->need_regenerate |= ImGui::SliderFloat("Warp Amp",     &worley->warp_amp,       0.0f, 100.0f);
  ts->need_regenerate |= ImGui::SliderFloat("Warp Freq",    &worley->warp_frequency, 0.001f, 0.02f);
  ts->need_regenerate |= ImGui::SliderInt(  "Warp Octaves", &worley->warp_octaves,   1, 6);

  ImGui::Separator();
  ImGui::Text("Composition");
  ts->need_regenerate |= ImGui::SliderFloat("Void Chance", &comp->void_chance, 0.0f, 1.0f);

  ImGui::Separator();
  ImGui::Text("Contour Lines");
  ImGui::Text("Interval: %.4f (from %d terrace levels)",
              1.0f / comp->terrace_levels, comp->terrace_levels);

  ImGui::Separator();
  ImGui::Text("Color Palette");
  if (ImGui::BeginCombo("##palette", PALETTES[ts->current_palette].name)) {
    for (int i = 0; i < PALETTE_COUNT; ++i) {
      bool sel = (ts->current_palette == i);
      if (ImGui::Selectable(PALETTES[i].name, sel)) {
        ts->current_palette = i;
        ts->need_regenerate = true;
      }
      if (sel) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::Separator();
  ImGui::Text("Map Scale");
  ts->need_regenerate |= ImGui::SliderFloat("Map Scale", &ts->map_scale, 0.25f, 4.0f);

  ImGui::Separator();
  if (ImGui::Button("Regenerate", {-1, 40})) ts->need_regenerate = true;
  if (ImGui::Button("Reset", {-1, 40})) {
    *elev   = ElevationParams{};
    *worley = WorleyParams{};
    *comp   = CompositionParams{};
    ts->use_isometric   = DEFAULT_ISOMETRIC;
    ts->current_palette = 0;
    ts->map_scale       = Config::DEFAULT_MAP_SCALE;
    cache->invalidate_all();
    ts->need_regenerate = true;
  }

  float half_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
  if (ImGui::Button("Save Config", {half_w, 30})) {
    std::ofstream f("config.json");
    if (f.is_open()) {
      f << params_to_json(*elev, *worley, *comp, *ts).dump(2);
      save_status_timer = 60;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Config", {half_w, 30})) {
    std::ifstream f("config.json");
    if (f.is_open()) {
      try {
        json j = json::parse(f);
        json_to_params(j, *elev, *worley, *comp, *ts);
        cache->invalidate_all();
        ts->need_regenerate = true;
        save_status_timer = -60;
      } catch (...) { save_status_timer = -1; }
    }
  }
  if (save_status_timer > 0)    { ImGui::SameLine(); ImGui::Text("Saved!");  save_status_timer--; }
  else if (save_status_timer < -1) { ImGui::SameLine(); ImGui::Text("Loaded!"); save_status_timer++; }

  ImGui::Separator();
  ImGui::Text("Stats");
  ImGui::Text("Contour Lines: %zu", contours ? contours->contour_lines.size() : 0u);
  ImGui::Text("Resolution: %dx%d", Config::MAP_WIDTH, Config::MAP_HEIGHT);
  ImGui::Text("Camera: (%.1f, %.1f) zoom %.2fx", camera.world_x, camera.world_y, camera.zoom);

  ImGui::Separator();
  if (ImGui::CollapsingHeader("Resources")) {
    asset_manager.render_debug_ui();
  }

  ImGui::End();
  ui_end_frame();
}
