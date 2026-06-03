#pragma once
#include "layout/element.hpp"
#include "renderer/style.hpp"
#include <memory>
#include <utility>
#include <vector>

namespace ui {

class DivElement : public IElement {
public:
  DivElement(const styleConfig &style,
             std::vector<std::unique_ptr<IElement>> &&initChildren)
      : m_style(style) {
    m_children.reserve(initChildren.size());

    for (auto &child : initChildren) {
      child->SetParent(this);
      m_children.push_back(std::move(child));
    }
  };
  ~DivElement() override = default;

  ElementType GetType() const override { return ElementType::DIV; }
  // const ui::styleConfig &GetStyle() const override { return m_style; }
  ui::styleConfig &GetStyle() override { return m_style; }

  const ui::LayoutAccumulation &GetLayoutMetrics() const override {
    return m_geometry;
  }
  ui::LayoutAccumulation &GetLayoutMetrics() override { return m_geometry; }

  IElement *GetParent() const override { return m_parent; }
  void SetParent(IElement *parent) override { m_parent = parent; }

  const std::vector<std::unique_ptr<IElement>> &GetChildren() const override {
    return m_children;
  }

  void AddChild(std::unique_ptr<IElement> child) override {
    child->SetParent(this);
    m_children.push_back(std::move(child));
  }

private:
  ui::styleConfig m_style;
  ui::LayoutAccumulation m_geometry;
  IElement *m_parent = nullptr;
  std::vector<std::unique_ptr<IElement>> m_children;
};

inline std::unique_ptr<IElement>
Div(const styleConfig &cfg,
    std::vector<std::unique_ptr<IElement>> &&children = {}) {
  return std::make_unique<DivElement>(cfg, std::move(children));
}

template <typename... Args>
inline std::unique_ptr<IElement> Div(const styleConfig &style,
                                     Args &&...children) {
  std::vector<std::unique_ptr<IElement>> vec;
  vec.reserve(sizeof...(children));

  // Fold expression to move each child into the vector sequentially
  (vec.push_back(std::forward<Args>(children)), ...);

  return std::make_unique<DivElement>(style, std::move(vec));
}

} // namespace ui
