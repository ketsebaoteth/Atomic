#pragma once
#include "windowing/interface.hpp"
#include <SDL3/SDL.h>
#include <memory>

namespace ui {

class SDLWindow : public Window {
public:
  SDLWindow(const WindowConfig &config);
  ~SDLWindow() override;

  void poll_events() override;
  void swap_buffers() override;
  bool should_close() override { return m_should_close; };
  void render() override;
  void *get_native_handle() const override { return (void *)m_window; };
  static std::unique_ptr<Window> Create(const WindowConfig &config) {
    return std::make_unique<SDLWindow>(config);
  }

private:
  SDL_Window *m_window = nullptr;
  SDL_GLContext m_gl_context = nullptr;
  bool m_should_close = false;
};

} // namespace ui
