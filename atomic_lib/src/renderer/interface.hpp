#pragma once
#include "renderer/style.hpp"
#include <memory>

namespace ui {
class Renderer {
public:
  virtual ~Renderer() = default;
  virtual void begin_frame() = 0;
  virtual void end_frame() = 0;

  virtual void on_resize(int width, int height) = 0;

  virtual void add_rect(const math::vec2<float> &globalPosition,
                        const math::vec2<float> &computedSize,
                        ui::styleConfig *style) = 0;
  virtual void add_circle(const math::vec2<float> &globalPosition, float radius,
                          ui::styleConfig *style) = 0;
  virtual void add_text(const math::vec2<float> &globalPosition,
                        const std::string &text, ui::styleConfig *style,
                        float dpiScale) = 0;
  virtual void add_image(const math::vec2<float> &globalPosition,
                         const math::vec2<float> &computedSize,
                         const std::string &path, ui::styleConfig *style) = 0;
  virtual ui::font::Font *get_default_font() = 0;
  virtual void render_batch() = 0;

  static std::unique_ptr<Renderer> Create(class Window *window);
};
} // namespace ui
