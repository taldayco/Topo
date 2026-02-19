#pragma once
#include "scene/scene.h"
#include <memory>
#include <vector>

class SceneManager {
public:
  void push(std::unique_ptr<Scene> scene, GpuContext &gpu) {
    scene->init(gpu);
    stack.push_back(std::move(scene));
  }

  void pop() {
    if (!stack.empty()) {
      stack.back()->cleanup();
      stack.pop_back();
    }
  }

  void switch_to(std::unique_ptr<Scene> scene, GpuContext &gpu) {
    pop();
    push(std::move(scene), gpu);
  }

  void handle_event(const SDL_Event &event) {
    if (!stack.empty())
      stack.back()->handle_event(event);
  }

  void update(float dt) {
    if (!stack.empty())
      stack.back()->update(dt);
  }

  void render(GpuContext &gpu, FrameContext &frame) {
    if (!stack.empty())
      stack.back()->render(gpu, frame);
  }

  void cleanup() {
    while (!stack.empty()) {
      stack.back()->cleanup();
      stack.pop_back();
    }
  }

  Scene *current() {
    return stack.empty() ? nullptr : stack.back().get();
  }

  bool empty() const { return stack.empty(); }

private:
  std::vector<std::unique_ptr<Scene>> stack;
};
