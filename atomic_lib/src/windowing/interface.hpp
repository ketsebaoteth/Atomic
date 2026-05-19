#pragma once
#include "renderer/interface.hpp"
#include <memory>
#include <string>

namespace ui {
struct WindowConfig {
  std::string title = "atomic window default title";
  int width = 1200;
  int height = 720;
  int x = 0;
  int y = 0;
  bool vsync = true;
};

class Window {
public:
  virtual ~Window() = default;

  virtual void poll_events() = 0;
  virtual void swap_buffers() = 0;
  virtual bool should_close() = 0;

  virtual void render() = 0;

  virtual void *get_native_handle() const = 0;
  virtual float get_dpi_scale() { return 1.0f; };

  std::unique_ptr<Window> Create(const WindowConfig &config);

protected:
  std::unique_ptr<Renderer> m_renderer;
};
} // namespace ui
