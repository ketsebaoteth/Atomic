#include "windowing/interface.hpp"
#include "windowing/linux/window_sdl.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>

int main() {
  try {
    ui::WindowConfig config{};
    config.title = "hellow wayland";
    config.width = 1200;
    config.height = 720;
    config.x = 100;
    config.y = 100;

    auto window = ui::SDLWindow::Create(config);

    while (!window->should_close()) {
      window->poll_events();
      window->render();
    }
  } catch (const std::exception &e) {
    std::cerr << "Engine Crashed" << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
