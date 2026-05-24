#pragma once

#include "math/vec.hpp"
#include "renderer/font/interface.hpp"
#include <cstdint>
#include <variant>

namespace ui {

enum class ShapeType : uint32_t {
  RoundedRect = 0,
  Circle = 1,
  Text = 2,
  Image = 3
};

enum class FlexDirection : uint32_t { Column = 0, Row = 1 };

struct SizeFit {};
struct SizeFill {};

using Size = std::variant<float, SizeFit, SizeFill>;

inline constexpr SizeFit fit{};
inline constexpr SizeFill fill{};

struct Size2D {
  Size x = fit;
  Size y = fit;
};

struct styleConfig {
  math::vec2<float> pos{0.0f, 0.0f};
  Size2D size{ui::SizeFit{}, ui::SizeFit{}};
  math::vec4<float> margin{0.0f, 0.0f, 0.0f,
                           0.0f}; // [Top, Right, Bottom, Left]
  math::vec4<float> padding{0.0f, 0.0f, 0.0f,
                            0.0f}; // [Top, Right, Bottom, Left]
  math::vec2<float> gap{0.0f, 0.0f};
  FlexDirection flexDirection = FlexDirection::Column;
  math::vec4<float> color = math::vec4<float>::all(1);
  math::vec4<float> radius{0.0f, 0.0f, 0.0f, 0.0f};
  ShapeType shape = ShapeType::RoundedRect;
  float strokeWidth = 1.0f;
  math::vec4<float> strokeColor{0.3f, 0.3f, 0.3f, 1.0f};
  float dotGap = 0.0f;
  float dotSize = 0.0f;
  uint32_t strokePosition = 2; // 0-inner, 1-center, 2-out
  void *font = nullptr;
  int fontSize = 16;
  ui::font::TextStyleBit styleFlag = ui::font::TextStyleBit::Regular;
  int tracking = 0;
  int maxWidth = 0;

  // builder pattern for styling
  constexpr styleConfig &SetPos(const math::vec2<float> &val) {
    pos = val;
    return *this;
  }
  constexpr styleConfig &SetSize(const Size2D &val) {
    size = val;
    return *this;
  }
  constexpr styleConfig &SetMargin(const math::vec4<float> &val) {
    margin = val;
    return *this;
  }
  constexpr styleConfig &SetPadding(const math::vec4<float> &val) {
    padding = val;
    return *this;
  }
  constexpr styleConfig &SetGap(const math::vec2<float> &val) {
    gap = val;
    return *this;
  }
  constexpr styleConfig &SetFlexDirection(FlexDirection val) {
    flexDirection = val;
    return *this;
  }

  constexpr styleConfig &SetColor(const math::vec4<float> &val) {
    color = val;
    return *this;
  }
  constexpr styleConfig &SetRadius(const math::vec4<float> &val) {
    radius = val;
    return *this;
  }
  constexpr styleConfig &SetShape(ShapeType val) {
    shape = val;
    return *this;
  }

  constexpr styleConfig &SetStrokeWidth(float val) {
    strokeWidth = val;
    return *this;
  }
  constexpr styleConfig &SetStrokeColor(const math::vec4<float> &val) {
    strokeColor = val;
    return *this;
  }
  constexpr styleConfig &SetDotGap(float val) {
    dotGap = val;
    return *this;
  }
  constexpr styleConfig &SetDotSize(float val) {
    dotSize = val;
    return *this;
  }
  constexpr styleConfig &SetStrokePosition(uint32_t val) {
    strokePosition = val;
    return *this;
  }

  constexpr styleConfig &SetFont(void *val) {
    font = val;
    return *this;
  }
  constexpr styleConfig &SetFontSize(int val) {
    fontSize = val;
    return *this;
  }
  constexpr styleConfig &SetStyleFlag(ui::font::TextStyleBit val) {
    styleFlag = val;
    return *this;
  }
  constexpr styleConfig &SetTracking(int val) {
    tracking = val;
    return *this;
  }
  constexpr styleConfig &SetMaxWidth(int val) {
    maxWidth = val;
    return *this;
  }
};

} // namespace ui
