#pragma once
#include <SDL3/SDL.h>

struct GpuContext;
struct FrameContext;

class Scene {
public:
  virtual ~Scene() = default;

  virtual void init(GpuContext &gpu) = 0;
  virtual void handle_event(const SDL_Event &event) = 0;
  virtual void update(float dt) = 0;
  virtual void render(GpuContext &gpu, FrameContext &frame) = 0;
  virtual void cleanup() = 0;
};
