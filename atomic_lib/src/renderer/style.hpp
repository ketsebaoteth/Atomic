#pragma once

#include "math/vec.hpp"
#include <cstdint>

namespace ui {

enum class ShapeType : uint32_t {
  RoundedRect = 0,
  Circle = 1,
  Triangle = 2,
  Ring = 3
};

struct styleConfig {
  math::vec4<float> color;
  math::vec4<float> radius;
  ShapeType shape = ShapeType::RoundedRect;

  float strokeWidth = 1.0f;
  math::vec4<float> strokeColor = {0.3f, 0.3f, 0.3f, 1.0f};
  float dotGap = 0.0f; // 0.0 means solid line
  float dotSize = 0.0f;
  uint32_t strokePosition = 2; // 0-inner, 1-center, 2-out
};

} // namespace ui
