#pragma once
#include "math/vec.hpp"
#include "renderer/style.hpp"
#include <memory>
#include <variant>
#include <vector>

namespace ui {

enum class ElementType : uint32_t { DIV = 0, TEXT = 1, IMAGE = 2 };

struct LayoutAccumulation {
  math::vec2<float> local_position{0.0f, 0.0f};
  math::vec2<float> global_position{0.0f, 0.0f};
  math::vec2<float> computed_size{0.0f, 0.0f};

  math::vec2<float> padding{0.0f, 0.0f};
  math::vec2<float> margin{0.0f, 0.0f};
  float flex_grow_weight{0.0f};

  math::vec4<float> clip_rect{-10000.0f, -10000.0f, 100000.0f, 100000.0f};
};

class IElement {
public:
  virtual ~IElement() = default;

  virtual ElementType GetType() const = 0;
  virtual const ui::styleConfig &GetStyle() const = 0;
  virtual ui::styleConfig &GetStyle() = 0;

  virtual const LayoutAccumulation &GetLayoutMetrics() const = 0;
  virtual LayoutAccumulation &GetLayoutMetrics() = 0;

  virtual IElement *GetParent() const = 0;
  virtual void SetParent(IElement *parent) = 0;

  virtual const std::vector<std::unique_ptr<IElement>> &GetChildren() const = 0;
  virtual void AddChild(std::unique_ptr<IElement> child) = 0;
};

} // namespace ui
