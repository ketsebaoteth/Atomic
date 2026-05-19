#pragma once
#include <memory>

namespace ui {
class Renderer {
public:
  virtual ~Renderer() = default;
  virtual void begin_frame() = 0;
  virtual void end_frame() = 0;

  virtual void on_resize(int width, int height) = 0;

  static std::unique_ptr<Renderer> Create(class Window *window);
};
} // namespace ui
