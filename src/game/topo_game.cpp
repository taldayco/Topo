#include "topo_game.h"
#include "terrain/basalt.h"
#include "terrain/lava.h"
#include "terrain/noise_composer.h"
#include "terrain/contour.h"
#include "terrain/palettes.h"
#include "render/sprite.h"
#include "ui/imgui_ui.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

static json params_to_json(const ElevationParams &elev, const RiverParams &river,
                           const WorleyParams &worley, const CompositionParams &comp,
                           const TerrainState &ts) {
  return {
    {"elevation", {
      {"frequency", elev.frequency}, {"octaves", elev.octaves},
      {"lacunarity", elev.lacunarity}, {"gain", elev.gain},
      {"seed", elev.seed}, {"scurve_bias", elev.scurve_bias}
    }},
    {"river", {
      {"frequency", river.frequency}, {"octaves", river.octaves},
      {"lacunarity", river.lacunarity}, {"gain", river.gain},
      {"seed", river.seed}, {"threshold", river.threshold}
    }},
    {"worley", {
      {"frequency", worley.frequency}, {"seed", worley.seed},
      {"jitter", worley.jitter}
    }},
    {"composition", {
      {"river_elevation_max", comp.river_elevation_max},
      {"lava_threshold", comp.lava_threshold},
      {"terrace_levels", comp.terrace_levels},
      {"min_region_size", comp.min_region_size}
    }},
    {"terrain", {
      {"use_isometric", ts.use_isometric},
      {"current_palette", ts.current_palette},
      {"map_scale", ts.map_scale},
      {"iso_padding", ts.iso_padding},
      {"iso_offset_x_adjust", ts.iso_offset_x_adjust},
      {"iso_offset_y_adjust", ts.iso_offset_y_adjust}
    }}
  };
}

static void json_to_params(const json &j, ElevationParams &elev, RiverParams &river,
                            WorleyParams &worley, CompositionParams &comp,
                            TerrainState &ts) {
  if (j.contains("elevation")) {
    auto &e = j["elevation"];
    if (e.contains("frequency"))  elev.frequency  = e["frequency"];
    if (e.contains("octaves"))    elev.octaves    = e["octaves"];
    if (e.contains("lacunarity")) elev.lacunarity = e["lacunarity"];
    if (e.contains("gain"))       elev.gain       = e["gain"];
    if (e.contains("seed"))       elev.seed       = e["seed"];
    if (e.contains("scurve_bias")) elev.scurve_bias = e["scurve_bias"];
  }
  if (j.contains("river")) {
    auto &r = j["river"];
    if (r.contains("frequency"))  river.frequency  = r["frequency"];
    if (r.contains("octaves"))    river.octaves    = r["octaves"];
    if (r.contains("lacunarity")) river.lacunarity = r["lacunarity"];
    if (r.contains("gain"))       river.gain       = r["gain"];
    if (r.contains("seed"))       river.seed       = r["seed"];
    if (r.contains("threshold"))  river.threshold  = r["threshold"];
  }
  if (j.contains("worley")) {
    auto &w = j["worley"];
    if (w.contains("frequency")) worley.frequency = w["frequency"];
    if (w.contains("seed"))      worley.seed      = w["seed"];
    if (w.contains("jitter"))    worley.jitter    = w["jitter"];
  }
  if (j.contains("composition")) {
    auto &c = j["composition"];
    if (c.contains("river_elevation_max")) comp.river_elevation_max = c["river_elevation_max"];
    if (c.contains("lava_threshold"))      comp.lava_threshold      = c["lava_threshold"];
    if (c.contains("terrace_levels"))      comp.terrace_levels      = c["terrace_levels"];
    if (c.contains("min_region_size"))     comp.min_region_size     = c["min_region_size"];
  }
  if (j.contains("terrain")) {
    auto &t = j["terrain"];
    if (t.contains("use_isometric"))       ts.use_isometric       = t["use_isometric"];
    if (t.contains("current_palette"))     ts.current_palette     = t["current_palette"];
    if (t.contains("map_scale"))           ts.map_scale           = t["map_scale"];
    if (t.contains("iso_padding"))         ts.iso_padding         = t["iso_padding"];
    if (t.contains("iso_offset_x_adjust")) ts.iso_offset_x_adjust = t["iso_offset_x_adjust"];
    if (t.contains("iso_offset_y_adjust")) ts.iso_offset_y_adjust = t["iso_offset_y_adjust"];
  }
}

void TopoGame::on_init(GpuContext &gpu, flecs::world &ecs) {
  // Register singletons
  ecs.set<GamePhase>({});
  ecs.set<TerrainState>({});
  ecs.set<WindowState>({true, false}); // launch_game_requested = true
  ecs.set<ElevationParams>({});
  ecs.set<RiverParams>({});
  ecs.set<WorleyParams>({});
  ecs.set<CompositionParams>({});
  ecs.set<MapData>({});
  ecs.set<NoiseCache>({});
  ecs.set<ContourData>({});

  // Create player entity
  auto player = ecs.entity("Player");
  player.set<PositionComponent>({50.0f, 50.0f, 0.0f});
  player.set<SpriteComponent>({});

  // Init input
  input.init();

  // Init camera
  camera.max_x = Config::MAP_WIDTH;
  camera.max_y = Config::MAP_HEIGHT;
  camera.x = Config::MAP_WIDTH / 2.0f;
  camera.y = Config::MAP_HEIGHT / 2.0f;
  camera.follow_x = Config::MAP_WIDTH / 2.0f;
  camera.follow_y = Config::MAP_HEIGHT / 2.0f;
  camera.following = true;

  // Register Flecs systems

  // InputBeginFrame — clear per-frame pressed/released flags
  ecs.system("InputBeginFrame")
      .kind(flecs::PreUpdate)
      .run([this](flecs::iter &) { input.begin_frame(); });

  // PlayerMovement — move entities with WASD
  ecs.system<PositionComponent>("PlayerMovement")
      .kind(flecs::OnUpdate)
      .each([this, &ecs](PositionComponent &pos) {
        auto *phase = ecs.get<GamePhase>();
        if (phase && phase->current != GamePhase::Playing)
          return;

        auto &in = input.state();
        float speed = 100.0f * ecs.delta_time();
        if (in.held[(int)Action::MoveUp])    pos.y -= speed;
        if (in.held[(int)Action::MoveDown])  pos.y += speed;
        if (in.held[(int)Action::MoveLeft])  pos.x -= speed;
        if (in.held[(int)Action::MoveRight]) pos.x += speed;

        if (camera.following) {
          camera.follow_x = pos.x;
          camera.follow_y = pos.y;
        }
      });

  // CameraUpdate — handle camera movement and zoom
  ecs.system("CameraUpdate")
      .kind(flecs::PostUpdate)
      .run([this, &ecs](flecs::iter &) {
        auto *phase = ecs.get<GamePhase>();
        if (phase && phase->current != GamePhase::Playing)
          return;

        auto &in = input.state();
        float dt = ecs.delta_time();
        float cam_speed = 200.0f * dt;

        bool cam_moved = in.held[(int)Action::CameraUp] ||
                         in.held[(int)Action::CameraDown] ||
                         in.held[(int)Action::CameraLeft] ||
                         in.held[(int)Action::CameraRight];
        if (cam_moved)
          camera.following = false;
        if (in.held[(int)Action::CameraUp])    camera.y -= cam_speed;
        if (in.held[(int)Action::CameraDown])  camera.y += cam_speed;
        if (in.held[(int)Action::CameraLeft])  camera.x -= cam_speed;
        if (in.held[(int)Action::CameraRight]) camera.x += cam_speed;

        if (in.held[(int)Action::ZoomIn])
          camera_system.set_zoom(camera, camera.target_zoom + dt * 2.0f);
        if (in.held[(int)Action::ZoomOut])
          camera_system.set_zoom(camera, camera.target_zoom - dt * 2.0f);

        camera_system.update(camera, dt);

        auto *ts = ecs.get_mut<TerrainState>();
        if (ts)
          camera_system.apply_to_view(camera, ts->view);
      });
}

void TopoGame::on_event(const SDL_Event &event, flecs::world &ecs) {
  input.handle_event(event);

  if (event.type == SDL_EVENT_KEY_DOWN) {
    auto *phase = ecs.get_mut<GamePhase>();
    if (!phase) return;

    if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
      if (phase->current == GamePhase::Playing)
        phase->current = GamePhase::Paused;
      else if (phase->current == GamePhase::Paused)
        phase->current = GamePhase::Playing;
    }

    if (event.key.scancode == SDL_SCANCODE_RETURN) {
      if (phase->current == GamePhase::Menu)
        phase->current = GamePhase::Playing;
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
  // Lazy-init the renderer on first game frame
  if (!terrain_renderer.is_initialized()) {
    terrain_renderer.init(gpu.device, gpu.game_window);
  }

  auto *ts = ecs.get_mut<TerrainState>();
  auto *elev = ecs.get_mut<ElevationParams>();
  auto *river = ecs.get_mut<RiverParams>();
  auto *worley = ecs.get_mut<WorleyParams>();
  auto *comp = ecs.get_mut<CompositionParams>();
  auto *map_data = ecs.get_mut<MapData>();
  auto *cache = ecs.get_mut<NoiseCache>();
  auto *contours = ecs.get_mut<ContourData>();

  // Regenerate terrain mesh when needed
  if (ts && ts->need_regenerate) {
    SDL_Log("Starting GPU regeneration...");
    auto start = SDL_GetTicks();

    int w = Config::MAP_WIDTH;
    int h = Config::MAP_HEIGHT;

    elev->map_scale = ts->map_scale;

    // Run the composition pipeline
    map_data->allocate(w, h);
    compose_layers(*map_data, *elev, *river, *worley, *comp, cache);

    // Generate Worley-driven basalt columns
    map_data->columns =
        generate_basalt_columns_v2(*map_data, Config::HEX_SIZE);

    // Generate lava from liquid mask
    map_data->lava_bodies =
        generate_lava_from_mask(*map_data);

    // Copy basalt_height to heightmap for contour extraction
    int n = w * h;
    contours->heightmap.resize(n);
    std::copy(map_data->basalt_height.begin(),
              map_data->basalt_height.end(),
              contours->heightmap.begin());

    contours->contour_lines.clear();
    float contour_interval = 1.0f / comp->terrace_levels;
    extract_contours(contours->heightmap, w, h, contour_interval,
                     contours->contour_lines, contours->band_map);

    terrain_mesh = build_terrain_mesh(*ts, *map_data, *contours);
    terrain_renderer.upload_mesh(gpu.device, terrain_mesh);

    ts->need_regenerate = false;
    SDL_Log("GPU regeneration: %lu ms", (unsigned long)(SDL_GetTicks() - start));
  }

  // Begin render pass and draw terrain
  gpu_begin_render_pass(gpu, frame);

  if (terrain_renderer.has_mesh()) {
    float time = SDL_GetTicks() / 1000.0f;
    SceneUniforms uniforms = compute_uniforms(
        terrain_mesh, ts->view, frame.swapchain_w, frame.swapchain_h,
        time, ts->contour_opacity);
    terrain_renderer.draw(frame.render_pass, frame.cmd, uniforms);
  }
}

void TopoGame::on_cleanup(flecs::world &ecs) {
  terrain_renderer.cleanup(gpu_ctx.device);
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

  auto *ts = ecs.get_mut<TerrainState>();
  auto *elev = ecs.get_mut<ElevationParams>();
  auto *river = ecs.get_mut<RiverParams>();
  auto *worley = ecs.get_mut<WorleyParams>();
  auto *comp = ecs.get_mut<CompositionParams>();
  auto *ws = ecs.get_mut<WindowState>();
  auto *cache = ecs.get_mut<NoiseCache>();
  auto *contours = ecs.get<ContourData>();

  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y},
                           ImGuiCond_Always);
  ImGui::Begin("Controls", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

  if (!game_window_open) {
    if (ImGui::Button("Launch Game", {-1, 40}))
      ws->launch_game_requested = true;
  } else {
    if (ImGui::Button("Close Game", {-1, 40}))
      ws->close_game_requested = true;
  }

  ImGui::Separator();
  ImGui::Text("Elevation");
  ts->need_regenerate |= ImGui::SliderFloat(
      "Frequency", &elev->frequency, 0.001f, 0.05f);
  ts->need_regenerate |=
      ImGui::SliderInt("Octaves", &elev->octaves, 1, 8);
  ts->need_regenerate |= ImGui::SliderFloat(
      "Lacunarity", &elev->lacunarity, 1.0f, 4.0f);
  ts->need_regenerate |=
      ImGui::SliderFloat("Gain", &elev->gain, 0.1f, 1.0f);
  ts->need_regenerate |=
      ImGui::SliderInt("Seed", &elev->seed, 0, 10000);
  ts->need_regenerate |= ImGui::SliderInt(
      "Terrace Levels", &comp->terrace_levels, 3, 20);
  ts->need_regenerate |= ImGui::SliderInt(
      "Min Region Size", &comp->min_region_size, 50, 2000);
  ts->need_regenerate |= ImGui::SliderFloat(
      "S-Curve Bias", &elev->scurve_bias, 0.0f, 1.0f);

  ImGui::Separator();
  ImGui::Text("River Mask");
  ts->need_regenerate |= ImGui::SliderFloat(
      "River Freq", &river->frequency, 0.001f, 0.05f);
  ts->need_regenerate |= ImGui::SliderInt(
      "River Octaves", &river->octaves, 1, 8);
  ts->need_regenerate |= ImGui::SliderInt(
      "River Seed", &river->seed, 0, 10000);
  ts->need_regenerate |= ImGui::SliderFloat(
      "River Threshold", &river->threshold, 0.0f, 1.0f);

  ImGui::Separator();
  ImGui::Text("Worley Noise");
  ts->need_regenerate |= ImGui::SliderFloat(
      "Worley Freq", &worley->frequency, 0.001f, 0.1f);
  ts->need_regenerate |= ImGui::SliderInt(
      "Worley Seed", &worley->seed, 0, 10000);
  ts->need_regenerate |= ImGui::SliderFloat(
      "Worley Jitter", &worley->jitter, 0.0f, 2.0f);

  ImGui::Separator();
  ImGui::Text("Composition");
  ts->need_regenerate |= ImGui::SliderFloat(
      "River Elev Max", &comp->river_elevation_max, 0.0f, 1.0f);
  ts->need_regenerate |= ImGui::SliderFloat(
      "Lava Threshold", &comp->lava_threshold, 0.0f, 1.0f);

  ImGui::Separator();
  ImGui::Text("Contour Lines");
  float computed_interval = 1.0f / comp->terrace_levels;
  ImGui::Text("Interval: %.4f (from %d terrace levels)", computed_interval,
              comp->terrace_levels);

  ImGui::Separator();
  ImGui::Text("Color Palette");
  if (ImGui::BeginCombo("##palette",
                         PALETTES[ts->current_palette].name)) {
    for (int i = 0; i < PALETTE_COUNT; ++i) {
      bool is_selected = (ts->current_palette == i);
      if (ImGui::Selectable(PALETTES[i].name, is_selected)) {
        ts->current_palette = i;
        ts->need_regenerate = true;
      }
      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::Separator();
  ts->need_regenerate |=
      ImGui::Checkbox("Isometric View", &ts->use_isometric);
  if (ts->use_isometric) {
    ts->need_regenerate |=
        ImGui::SliderFloat("Padding", &ts->iso_padding, 0.0f, 200.0f);
    ts->need_regenerate |= ImGui::SliderFloat(
        "Offset X", &ts->iso_offset_x_adjust, -200.0f, 200.0f);
    ts->need_regenerate |= ImGui::SliderFloat(
        "Offset Y", &ts->iso_offset_y_adjust, -200.0f, 200.0f);
  }

  ImGui::Separator();
  ImGui::Text("Map Scale");
  ts->need_regenerate |=
      ImGui::SliderFloat("Map Scale", &ts->map_scale, 0.25f, 4.0f);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Controls zoom level: higher = more terrain visible");
  }

  ImGui::Separator();
  if (ImGui::Button("Regenerate", {-1, 40}))
    ts->need_regenerate = true;
  if (ImGui::Button("Reset", {-1, 40})) {
    *elev = ElevationParams{};
    *river = RiverParams{};
    *worley = WorleyParams{};
    *comp = CompositionParams{};
    ts->use_isometric = DEFAULT_ISOMETRIC;
    ts->current_palette = 0;
    ts->map_scale = Config::DEFAULT_MAP_SCALE;
    ts->view = ViewState{};
    cache->invalidate_all();
    ts->need_regenerate = true;
  }

  float half_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
  if (ImGui::Button("Save Config", {half_w, 30})) {
    std::ofstream f("config.json");
    if (f.is_open()) {
      f << params_to_json(*elev, *river, *worley, *comp, *ts).dump(2);
      save_status_timer = 60;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Config", {half_w, 30})) {
    std::ifstream f("config.json");
    if (f.is_open()) {
      try {
        json j = json::parse(f);
        json_to_params(j, *elev, *river, *worley, *comp, *ts);
        cache->invalidate_all();
        ts->need_regenerate = true;
        save_status_timer = -60;
      } catch (...) {
        save_status_timer = -1; // signal error briefly
      }
    }
  }
  if (save_status_timer > 0) {
    ImGui::SameLine();
    ImGui::Text("Saved!");
    save_status_timer--;
  } else if (save_status_timer < -1) {
    ImGui::SameLine();
    ImGui::Text("Loaded!");
    save_status_timer++;
  }

  ImGui::Separator();
  ImGui::Text("Stats");
  ImGui::Text("Lines: %zu", contours->contour_lines.size());
  ImGui::Text("Resolution: %dx%d", Config::MAP_WIDTH, Config::MAP_HEIGHT);
  ImGui::Text("View Zoom: %.1fx", ts->view.zoom);

  ImGui::End();

  ui_end_frame();
}
