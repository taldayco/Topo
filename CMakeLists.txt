cmake_minimum_required(VERSION 3.20)
project(TopographyGen)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Dependencies
find_package(SDL3 REQUIRED)

include(FetchContent)

FetchContent_Declare(EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt
    GIT_TAG v3.14.0
)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(EnTT json)

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

add_executable(topogen
    main.cpp
    gpu.cpp
    imgui_ui.cpp
    map_gen.cpp
    noise.cpp
    contour.cpp
    delve_render.cpp
    isometric.cpp
    detail.cpp
    hex.cpp
    basalt.cpp
    unbound_space.cpp
    crystal.cpp
    lava.cpp
    terrain_generator.cpp
    game/game.cpp
    game/input.cpp
    game/camera.cpp
    game/sprite.cpp
    game/render_system.cpp
)

target_link_libraries(topogen PRIVATE SDL3::SDL3 imgui EnTT::EnTT nlohmann_json::nlohmann_json)
target_include_directories(topogen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
