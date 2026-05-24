#include "layout/elements/div.hpp"
#include "layout/elements/text.hpp"
#include "math/vec.hpp"
#include "renderer/style.hpp"
#include "renderer/utils/color.hpp"
#include "windowing/interface.hpp"
#include "windowing/linux/window_sdl.hpp"
#include <cstdlib>
#include <iostream>

using namespace ui;

int main() {
  try {
    ui::WindowConfig config{"Test 1: Complex Nested Flows", 1200, 720, 100,
                            100};
    auto window = ui::SDLWindow::Create(config);

    auto rootdiv = Div(
        ui::styleConfig()
            .SetColor(ui::Color::slate[900])
            .SetPadding({24.0f, 24.0f, 24.0f, 24.0f})
            .SetRadius(math::vec4<float>::all(12))
            .SetFlexDirection(ui::FlexDirection::Row)
            .SetGap({16.0f, 0.0f})
            .SetSize({ui::SizeFit{}, ui::SizeFit{}}),
        Text(
            ui::styleConfig().SetColor(ui::Color::emerald[400]).SetFontSize(20),
            "Success"));

    window->SetRootNode(std::move(rootdiv));

    while (!window->should_close()) {
      window->poll_events();
      window->render();
    }
  } catch (const std::exception &e) {
    std::cerr << "Engine Crash: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
