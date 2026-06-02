#pragma once
#include "layout/element.hpp"
#include "renderer/font/freetype_layout.hpp"
#include "renderer/style.hpp"
#include <memory>
#include <string>
#include <utility>
// #include <variant>
#include <vector>

namespace ui {

class TextElement : public IElement {
public:
  // Updated constructor to cleanly take both style configuration and content
  explicit TextElement(const styleConfig &cfg, std::string text)
      : m_style(cfg), m_textContent(std::move(text)) {}

  ~TextElement() override = default;

  ElementType GetType() const override { return ElementType::TEXT; }

  // const ui::styleConfig &GetStyle() const override { return m_style; }
  ui::styleConfig &GetStyle() override { return m_style; }

  const LayoutAccumulation &GetLayoutMetrics() const override {
    return m_geometry;
  }
  LayoutAccumulation &GetLayoutMetrics() override { return m_geometry; }

  IElement *GetParent() const override { return m_parent; }
  void SetParent(IElement *parent) override { m_parent = parent; }

  // Leaf nodes explicitly return empty structures for structural queries
  const std::vector<std::unique_ptr<IElement>> &GetChildren() const override {
    return m_emptyChildren;
  }
  void
  AddChild(std::unique_ptr<IElement> child) override { /* No-op: leaf node */ }

  const std::string &GetText() const { return m_textContent; }
  void SetText(std::string text) { m_textContent = std::move(text); }
  // Add this inside public: block of TextElement (layout/elements/text.hpp)
  // In layout/elements/text.hpp (or text.cpp)
  math::vec2<float> ComputeIntrinsicBounds(ui::font::Font *activeFont,
                                           float physicalFontSize,
                                           float dpiScale) const {
    // Scale your max width constraint up to physical pixels to match physical
    // font sizing
    float physicalMaxWidth = m_style.maxWidth * dpiScale;
    float physicalTracking = m_style.tracking * dpiScale;

    return font::TextLayoutEngine::measureString(
        m_textContent, activeFont, physicalFontSize, physicalMaxWidth,
        physicalTracking);
  }

private:
  ui::styleConfig m_style;
  LayoutAccumulation m_geometry;
  IElement *m_parent = nullptr;
  std::string m_textContent;

  inline static const std::vector<std::unique_ptr<IElement>> m_emptyChildren{};
};

// Base factory variant for a single string literal or pre-built std::string
inline std::unique_ptr<IElement> Text(const styleConfig &style,
                                      std::string text = "") {
  return std::make_unique<TextElement>(style, std::move(text));
}

// Overload for cases where you might pass multiple strings, tokens, or
// variables to concatenate
template <typename... Args>
inline std::unique_ptr<IElement> Text(const styleConfig &style,
                                      Args &&...args) {
  // Fold expression strings append into a single stack-allocated buffer
  std::string consolidated = (std::string("") + ... + std::forward<Args>(args));
  return std::make_unique<TextElement>(style, std::move(consolidated));
}

} // namespace ui
