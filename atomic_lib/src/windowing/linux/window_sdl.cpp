#include "window_sdl.hpp"
#include "renderer/vulkan/vulkan_renderer.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <memory>
#include <stdexcept>

namespace ui {

SDLWindow::SDLWindow(const WindowConfig &config) {
  // SDL3 return values are often booleans: true for success
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error("SDL3 could not be initialized! Error: " +
                             std::string(SDL_GetError()));
  }

  // Set hints before window creation
  SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "1");

  // SDL_CreateWindow in SDL3 has a new signature:
  // SDL_CreateWindow(title, width, height, flags)
  // Note: Position (x, y) is now set via flags or SDL_SetWindowPosition
  m_window = SDL_CreateWindow(config.title.c_str(), config.width, config.height,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                                  SDL_WINDOW_HIGH_PIXEL_DENSITY);

  if (!m_window) {
    throw std::runtime_error("Window could not be created! SDL_Error: " +
                             std::string(SDL_GetError()));
  }

  // If you need specific x/y placement:
  SDL_SetWindowPosition(m_window, config.x, config.y);

  // Initialize vulkan renderer
  m_renderer = VulkanRenderer::Create(this);
}

SDLWindow::~SDLWindow() {
  // Order matters: Kill the renderer/vulkan objects before the window
  m_renderer.reset();

  if (m_window) {
    SDL_DestroyWindow(m_window);
  }
  SDL_Quit();
}

void SDLWindow::render() {
  if (m_renderer) {
    m_renderer->begin_frame();
    // UI logic will plug in here
    m_renderer->end_frame();
  }
}

void SDLWindow::poll_events() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    switch (e.type) {
    case SDL_EVENT_QUIT:
      m_should_close = true;
      break;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      m_should_close = true;
      break;
    case SDL_EVENT_WINDOW_RESIZED:
      int w, h;
      SDL_GetWindowSize(m_window, &w, &h);
      m_renderer->on_resize(w, h);
    }
  }
}

void SDLWindow::swap_buffers() {
  if (m_renderer) {
    m_renderer->end_frame();
  }
}

} // namespace ui
