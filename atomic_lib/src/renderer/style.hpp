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
  float position; // 0.0 - 1.0
  math::vec4<float> color;
};

enum class FlexDirection : uint32_t { Column = 0, Row = 1 };

// struct SizeFit {};
// struct SizeFill {};

// using Size = std::variant<float, SizeFit, SizeFill>;

// inline constexpr SizeFit fit{};
// inline constexpr SizeFill fill{};

inline constexpr float fit = -1.0f;
inline constexpr float fill = -2.0f;

// struct Size2D {
//   Size x = fit;
//   Size y = fit;
// };

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

// struct Overflow {
//   int Hidden = 0;
// };
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

constexpr size_t GPU_INSTANCE_SIZE = 416;

struct styleConfig {
  // ========================================================================
  // GPU-MAPPED BLIT ZONE (Exact byte alignment matching your std430 shader)
  // ========================================================================

  // INFO: GPU only data starts here
  // Offset 0
  alignas(8) math::vec2<float> pos{0.0f, 0.0f};
  alignas(8) math::vec2<float> size{fit, fit};

  // Offset 16
  alignas(16) math::vec4<float> backgroundColor = math::vec4<float>::all(1);

  // Offset 32
  alignas(16) math::vec4<float> radius;

  // Offset 48
  alignas(4) float opacity = 1.0f;
  alignas(4) uint32_t shapeType = 0;
  alignas(4) float strokeWidth = 0.0f;
  alignas(4) uint32_t strokePosition = 2;

  // Offset 64
  alignas(4) float dotGap = 0.0f;
  alignas(4) float dotSize = 0.0f;
  alignas(4) uint32_t textureIndex = 0;
  alignas(4) uint32_t isRadialUniform = 1;

  // Offset 80
  alignas(8) math::vec2<float> uvMin{0.0f, 0.0f};
  alignas(8) math::vec2<float> uvMax{1.0f, 1.0f};

  // Offset 96
  alignas(16) math::vec4<float> strokeColor{0.3f, 0.3f, 0.3f, 1.0f};

  // Offset 112
  alignas(4) uint32_t gradientType = 0;
  alignas(4) float gradientDirection = 0.0f;
  alignas(8) math::vec2<float> gradientCenter{
      0.5f, 0.5f}; // Fits perfectly on 8-byte boundary [120-127]

  // Offset 128
  alignas(4) float gradientRadius = 0.5f;
  alignas(4) uint32_t gradientStopOffset = 0;
  alignas(4) uint32_t gradientStopCount = 0;
  // alignas(4) uint32_t _inlineStopsFlag = 1;

  // Offset 144 -> 160 PADDING (Forces your array to land exactly at 160)

  static constexpr size_t GPU_INSTANCE_SIZE = 160;
  // Offset 160 -> 288 (8 stops * 16 bytes)
  // alignas(16) GradientStop gradientStops[8];
  // INFO: GPU only data ends here

  // ========================================================================
  // CPU-ONLY ZONE BEGINS HERE (Offset 288)
  // ========================================================================

  EdgeInsets margin;
  EdgeInsets padding;
  math::vec2<float> gap{0.0f, 0.0f};
  FlexDirection flexDirection = FlexDirection::Column;

  math::vec4<float> textColor = math::vec4<float>{0, 0, 0, 1};

  Overflow overflow = Overflow::Hidden;

  std::vector<GradientStop> gradientStops;

  // INFO: Linear Gradient Direction in Radians

  // Radial gradient
  ui::font::Font *font = nullptr;
  int fontSize = 16;
  ui::font::TextStyleBit styleFlag = ui::font::TextStyleBit::Regular;
  int tracking = 0;
  int maxWidth = 0;

  constexpr styleConfig &SetPos(const math::vec2<float> &val) {
    pos = val;
    return *this;
  }
  constexpr styleConfig &SetSize(const math::vec2<float> &val) {
    size = val;
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
    gradientType = static_cast<float>(val);
    return *this;
  }
  styleConfig &SetGradientStops(const std::vector<GradientStop> &stops) {
    gradientStops = stops;
    return *this;
  }

  constexpr styleConfig &SetLinearGradDirection(
      const float angle,
      const GradientDirectionUnit unit = GradientDirectionUnit::Rad) {

    if (unit == GradientDirectionUnit::Deg) {
      gradientDirection = angle * (3.14159265358979323846f / 180.0f);
    } else {
      gradientDirection = angle;
    }

    return *this;
  }

  constexpr styleConfig &SetRadialGradCenter(const math::vec2<float> &center) {
    gradientCenter = center;
    return *this;
  }

  constexpr styleConfig &SetTextColor(const math::vec4<float> &val) {
    textColor = val;
    return *this;
  }
  constexpr styleConfig &SetBGColor(const math::vec4<float> &val) {
    backgroundColor = val;
    return *this;
  }
  constexpr styleConfig &SetRadius(const CornerRadius &val) {
    radius = val;
    return *this;
  }
  constexpr styleConfig &SetShape(ShapeType val) {
    shapeType = static_cast<uint32_t>(val);
    return *this;
  }
  constexpr styleConfig &SetOpacity(float opacity) {
    this->opacity = opacity;
    return *this;
  }
  constexpr styleConfig &SetOverflow(Overflow val) {
    overflow = val;
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

  constexpr styleConfig &SetFont(ui::font::Font *val) {
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
