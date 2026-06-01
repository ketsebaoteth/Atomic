#pragma once

#include "math/vec.hpp"
#include "renderer/font/interface.hpp"
#include <cstdint>
// #include <variant>

namespace ui {

enum class ShapeType : uint32_t {
  RoundedRect = 0,
  Circle = 1,
  Text = 2,
  Image = 3
};

enum class GradientType : uint32_t { None = 0, Linear, Radial };

enum class GradientDirectionUnit : uint8_t { Rad = 0, Deg };

struct GradientStop {
  alignas(16) math::vec4<float> color;
  alignas(4) float position; // 0.0 - 1.0
  alignas(4) float _pad0;
  alignas(4) float _pad1;
  alignas(4) float _pad2;
};

enum class FlexDirection : uint32_t { Column = 0, Row = 1 };

inline constexpr float fit = -1.0f;
inline constexpr float fill = -2.0f;

struct EdgeInsets {
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
  float left = 0.0f;

  constexpr EdgeInsets() = default;
  constexpr EdgeInsets(float t, float r, float b, float l)
      : top(t), right(r), bottom(b), left(l) {}

  static constexpr EdgeInsets all(float val) {
    return EdgeInsets(val, val, val, val);
  }
  static constexpr EdgeInsets horizontal(float val) {
    return EdgeInsets(0.0f, val, 0.0f, val);
  }
  static constexpr EdgeInsets vertical(float val) {
    return EdgeInsets(val, 0.0f, val, 0.0f);
  }

  constexpr operator math::vec4<float>() const {
    return {top, right, bottom, left};
  }
};

enum class Overflow : uint32_t { Hidden = 0, Visible = 1 };

struct CornerRadius {
  float topLeft = 0.0f;
  float topRight = 0.0f;
  float bottomRight = 0.0f;
  float bottomLeft = 0.0f;

  constexpr CornerRadius() = default;
  constexpr CornerRadius(float tl, float tr, float br, float bl)
      : topLeft(tl), topRight(tr), bottomRight(br), bottomLeft(bl) {}

  static constexpr CornerRadius all(float val) {
    return CornerRadius(val, val, val, val);
  }
  static constexpr CornerRadius top(float val) {
    return CornerRadius(val, val, 0.0f, 0.0f);
  }
  static constexpr CornerRadius bottom(float val) {
    return CornerRadius(0.0f, 0.0f, val, val);
  }
  static constexpr CornerRadius left(float val) {
    return CornerRadius(val, 0.0f, 0.0f, val);
  }
  static constexpr CornerRadius right(float val) {
    return CornerRadius(0.0f, val, val, 0.0f);
  }

  constexpr operator math::vec4<float>() const {
    return {topLeft, topRight, bottomRight, bottomLeft};
  }
};

struct styleConfig {
  // INFO: GPU only data starts here
  struct StyleConfigGPU {
    alignas(8) math::vec2<float> pos{0.0f, 0.0f};
    alignas(8) math::vec2<float> size{fit, fit};

    alignas(16) math::vec4<float> backgroundColor = math::vec4<float>::all(1);

    alignas(16) math::vec4<float> radius;

    alignas(4) float opacity = 1.0f;
    alignas(4) uint32_t shapeType = 0;
    alignas(4) float strokeWidth = 0.0f;
    alignas(4) uint32_t strokePosition = 2;

    alignas(4) float dotGap = 0.0f;
    alignas(4) float dotSize = 0.0f;
    alignas(4) uint32_t textureIndex = 0;
    alignas(4) uint32_t isRadialUniform = true;

    alignas(8) math::vec2<float> uvMin{0.0f, 0.0f};
    alignas(8) math::vec2<float> uvMax{1.0f, 1.0f};

    alignas(16) math::vec4<float> strokeColor{0.3f, 0.3f, 0.3f, 1.0f};

    alignas(4) uint32_t gradientType = 0;
    alignas(4) float gradientDirection = 0.0f;
    alignas(8) math::vec2<float> gradientCenter{0.5f, 0.5f};

    alignas(4) float gradientRadius = 0.5f;
    alignas(4) uint32_t gradientStopCount = 0;
    alignas(8) uint32_t _padding[2]{};

    alignas(16) GradientStop gradientStops[8];

  } styleConfigGPU;

  // INFO: GPU only data ends here

  // ========================================================================
  // CPU-ONLY ZONE BEGINS HERE
  // ========================================================================

  EdgeInsets margin;
  EdgeInsets padding;
  math::vec2<float> gap{0.0f, 0.0f};
  FlexDirection flexDirection = FlexDirection::Column;

  math::vec4<float> textColor = math::vec4<float>{0, 0, 0, 1};

  Overflow overflow = Overflow::Hidden;

  // std::vector<GradientStop> gradientStops;

  // INFO: Linear Gradient Direction in Radians

  // Radial gradient
  ui::font::Font *font;
  int fontSize = 16;
  ui::font::TextStyleBit styleFlag = ui::font::TextStyleBit::Regular;
  int tracking = 0;
  int maxWidth = 0;

  constexpr styleConfig &SetPos(const math::vec2<float> &val) {
    styleConfigGPU.pos = val;
    return *this;
  }
  constexpr styleConfig &SetSize(const math::vec2<float> &val) {
    styleConfigGPU.size = val;
    return *this;
  }
  constexpr styleConfig &SetMargin(const EdgeInsets &val) {
    margin = val;
    return *this;
  }
  constexpr styleConfig &SetPadding(const EdgeInsets &val) {
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

  constexpr styleConfig &SetGradientType(const GradientType val) {
    styleConfigGPU.gradientType = static_cast<uint32_t>(val);
    return *this;
  }

  styleConfig &SetGradientStops(std::initializer_list<GradientStop> stops) {

    styleConfigGPU.gradientStopCount =
        static_cast<uint32_t>(std::min<size_t>(stops.size(), 8));

    uint32_t i = 0;
    for (const auto &stop : stops) {
      if (i >= 8)
        break;

      styleConfigGPU.gradientStops[i++] = stop;
    }

    return *this;
  }

  constexpr styleConfig &SetGradientRadius(const float radius) {
    styleConfigGPU.gradientRadius = radius;
    return *this;
  }

  constexpr styleConfig &SetLinearGradDirection(
      const float angle,
      const GradientDirectionUnit unit = GradientDirectionUnit::Rad) {

    if (unit == GradientDirectionUnit::Deg) {
      styleConfigGPU.gradientDirection =
          angle * (3.14159265358979323846f / 180.0f);
    } else {
      styleConfigGPU.gradientDirection = angle;
    }

    return *this;
  }

  constexpr styleConfig &SetRadialGradCenter(const math::vec2<float> &center) {
    styleConfigGPU.gradientCenter = center;
    return *this;
  }

  constexpr styleConfig &SetRadialUniform(const bool isUniform) {
    styleConfigGPU.isRadialUniform = isUniform;
    return *this;
  }

  constexpr styleConfig &SetTextColor(const math::vec4<float> &val) {
    textColor = val;
    return *this;
  }
  constexpr styleConfig &SetBGColor(const math::vec4<float> &val) {
    styleConfigGPU.backgroundColor = val;
    return *this;
  }
  constexpr styleConfig &SetRadius(const CornerRadius &val) {
    styleConfigGPU.radius = val;
    return *this;
  }
  constexpr styleConfig &SetShape(ShapeType val) {
    styleConfigGPU.shapeType = static_cast<uint32_t>(val);
    return *this;
  }
  constexpr styleConfig &SetOpacity(float opacity) {
    styleConfigGPU.opacity = opacity;
    return *this;
  }
  constexpr styleConfig &SetOverflow(Overflow val) {
    overflow = val;
    return *this;
  }

  constexpr styleConfig &SetStrokeWidth(float val) {
    styleConfigGPU.strokeWidth = val;
    return *this;
  }
  constexpr styleConfig &SetStrokeColor(const math::vec4<float> &val) {
    styleConfigGPU.strokeColor = val;
    return *this;
  }
  constexpr styleConfig &SetDotGap(float val) {
    styleConfigGPU.dotGap = val;
    return *this;
  }
  constexpr styleConfig &SetDotSize(float val) {
    styleConfigGPU.dotSize = val;
    return *this;
  }
  constexpr styleConfig &SetStrokePosition(uint32_t val) {
    styleConfigGPU.strokePosition = val;
    return *this;
  }

  styleConfig &SetFont(ui::font::Font *fontName) {
    font = fontName;
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

namespace Typography {

inline styleConfig H1() {
  return styleConfig().SetFontSize(32).SetStyleFlag(
      ui::font::TextStyleBit::Bold);
}

inline styleConfig H2() {
  return styleConfig().SetFontSize(24).SetStyleFlag(
      ui::font::TextStyleBit::Bold);
}

inline styleConfig H3() {
  return styleConfig().SetFontSize(20).SetStyleFlag(
      ui::font::TextStyleBit::Bold);
}

inline styleConfig Body() {
  return styleConfig().SetFontSize(16).SetStyleFlag(
      ui::font::TextStyleBit::Regular);
}

inline styleConfig Small() {
  return styleConfig().SetFontSize(14).SetStyleFlag(
      ui::font::TextStyleBit::Regular);
}

inline styleConfig Muted() {
  return styleConfig()
      .SetFontSize(14)
      .SetStyleFlag(ui::font::TextStyleBit::Regular)
      .SetTextColor({0.6f, 0.6f, 0.6f, 1.0f});
}

} // namespace Typography

} // namespace ui
