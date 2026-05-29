#pragma once
#include "layout/element.hpp"
#include "layout/elements/text.hpp"
#include "math/vec.hpp"
#include "renderer/style.hpp"
#include <algorithm>
#include <variant>
#include <vector>

namespace ui {

class AtomicEngine {
public:
  AtomicEngine() = default;
  ~AtomicEngine() = default;

  void ProcessLayout(IElement *rootNode, const math::vec2<float> &canvasBounds,
                     ui::font::Font *defaultFont, float dpiScale = 1.0f) {
    if (!rootNode)
      return;

    m_contextDefaultFont = defaultFont;
    m_globalDpiScale =
        dpiScale; // Keep track of physical-to-logical ratio globally

    ExecSizingPass(rootNode, canvasBounds);

    math::vec2<float> rootStartingPosition = rootNode->GetStyle().pos;
    ExecPositionPass(rootNode, rootStartingPosition);

    m_linearizedRenderCache.clear();
    FlattenHierarchy(rootNode);
  }

  const std::vector<IElement *> &GetRenderCache() const {
    return m_linearizedRenderCache;
  }

private:
  ui::font::Font *m_contextDefaultFont;
  float m_globalDpiScale = 1.0f;

  void ExecSizingPass(IElement *node,
                      const math::vec2<float> &parentAllocation) {
    if (!node)
      return;

    const ui::styleConfig &style = node->GetStyle();
    LayoutAccumulation &metrics = node->GetLayoutMetrics();

    // Leaf nodes (Text layout evaluation)
    // Leaf nodes (Text layout evaluation)
    if (node->GetType() == ElementType::TEXT) {
      auto *textNode = static_cast<ui::TextElement *>(node);
      ui::font::Font *activeFont =
          style.font ? reinterpret_cast<ui::font::Font *>(style.font)
                     : m_contextDefaultFont;

      // Scale font size parameter inline before running atlas glyph queries
      float scaledFontSize = style.fontSize * m_globalDpiScale;

      // FIX: Pass m_globalDpiScale here so internal max-width limits scale
      // matching the font
      math::vec2<float> intrinsicSize = textNode->ComputeIntrinsicBounds(
          activeFont, scaledFontSize, m_globalDpiScale);

      if (std::holds_alternative<ui::SizeFit>(style.size.x)) {
        metrics.computed_size.x = intrinsicSize.x;
      } else if (std::holds_alternative<float>(style.size.x)) {
        metrics.computed_size.x =
            std::get<float>(style.size.x) * m_globalDpiScale;
      }

      if (std::holds_alternative<ui::SizeFit>(style.size.y)) {
        metrics.computed_size.y = intrinsicSize.y;
      } else if (std::holds_alternative<float>(style.size.y)) {
        metrics.computed_size.y =
            std::get<float>(style.size.y) * m_globalDpiScale;
      }
      return;
    }
    bool isRow = (style.flexDirection == FlexDirection::Row);
    size_t childCount = node->GetChildren().size();
    size_t activeGaps = (childCount > 1) ? (childCount - 1) : 0;

    // Scale padding measurements up to physical pixels
    float paddingX =
        (style.padding.left + style.padding.right) * m_globalDpiScale;
    float paddingY =
        (style.padding.top + style.padding.bottom) * m_globalDpiScale;

    float currentBoundaryX =
        std::holds_alternative<float>(style.size.x)
            ? (std::get<float>(style.size.x) * m_globalDpiScale)
            : parentAllocation.x;
    float currentBoundaryY =
        std::holds_alternative<float>(style.size.y)
            ? (std::get<float>(style.size.y) * m_globalDpiScale)
            : parentAllocation.y;

    math::vec2<float> currentInnerCapacity = {
        std::max(0.0f, currentBoundaryX - paddingX),
        std::max(0.0f, currentBoundaryY - paddingY)};

    // =================================================================
    // PASS 1A: Pre-Pass (Resolve SizeFit nodes first)
    // =================================================================
    for (const auto &child : node->GetChildren()) {
      const auto &childStyle = child->GetStyle();
      const auto &mainSize = isRow ? childStyle.size.x : childStyle.size.y;

      if (std::holds_alternative<ui::SizeFit>(mainSize)) {
        math::vec2<float> fitBudget = currentInnerCapacity;
        ExecSizingPass(child.get(), fitBudget);
      }
    }

    // =================================================================
    // PASS 1B: Main-Axis Space Evaluation & Accumulation
    // =================================================================
    float totalFixedMainAxis = 0.0f;
    size_t fillCountMainAxis = 0;

    for (const auto &child : node->GetChildren()) {
      const auto &childStyle = child->GetStyle();
      const auto &mainSize = isRow ? childStyle.size.x : childStyle.size.y;

      // Scale up margins dynamically
      float marginTotal =
          isRow ? (childStyle.margin.left + childStyle.margin.right)
                : (childStyle.margin.top + childStyle.margin.bottom);
      marginTotal *= m_globalDpiScale;

      if (std::holds_alternative<float>(mainSize)) {
        totalFixedMainAxis +=
            (std::get<float>(mainSize) * m_globalDpiScale) + marginTotal;
      } else if (std::holds_alternative<ui::SizeFit>(mainSize)) {
        float resolvedMain = isRow ? child->GetLayoutMetrics().computed_size.x
                                   : child->GetLayoutMetrics().computed_size.y;
        totalFixedMainAxis += resolvedMain + marginTotal;
      } else if (std::holds_alternative<ui::SizeFill>(mainSize)) {
        totalFixedMainAxis += marginTotal;
        fillCountMainAxis++;
      }
    }

    // Scale layout step gaps
    float gapsMain =
        activeGaps * (isRow ? style.gap.x : style.gap.y) * m_globalDpiScale;
    float innerMainCapacity =
        isRow ? currentInnerCapacity.x : currentInnerCapacity.y;

    float rawRemainingSpace = innerMainCapacity - totalFixedMainAxis - gapsMain;
    float availableSpaceForFill = std::max(0.0f, rawRemainingSpace);

    float fillSlice = (fillCountMainAxis > 0)
                          ? (availableSpaceForFill / fillCountMainAxis)
                          : 0.0f;

    // =================================================================
    // PASS 2 & 3: Budget Determination & Deep Child Recursion
    // =================================================================
    for (const auto &child : node->GetChildren()) {
      const auto &childStyle = child->GetStyle();
      const auto &mainSize = isRow ? childStyle.size.x : childStyle.size.y;

      if (std::holds_alternative<ui::SizeFit>(mainSize))
        continue;

      math::vec2<float> childAllocationBudget;

      // Handle X-Axis
      if (std::holds_alternative<float>(childStyle.size.x)) {
        childAllocationBudget.x =
            std::get<float>(childStyle.size.x) * m_globalDpiScale;
      } else if (std::holds_alternative<ui::SizeFill>(childStyle.size.x)) {
        childAllocationBudget.x = isRow ? fillSlice : currentInnerCapacity.x;
      } else {
        childAllocationBudget.x = currentInnerCapacity.x;
      }

      // Handle Y-Axis
      if (std::holds_alternative<float>(childStyle.size.y)) {
        childAllocationBudget.y =
            std::get<float>(childStyle.size.y) * m_globalDpiScale;
      } else if (std::holds_alternative<ui::SizeFill>(childStyle.size.y)) {
        childAllocationBudget.y = !isRow ? fillSlice : currentInnerCapacity.y;
      } else {
        childAllocationBudget.y = currentInnerCapacity.y;
      }

      childAllocationBudget.x =
          std::min(childAllocationBudget.x, currentInnerCapacity.x);
      childAllocationBudget.y =
          std::min(childAllocationBudget.y, currentInnerCapacity.y);

      ExecSizingPass(child.get(), childAllocationBudget);
    }

    // =================================================================
    // PASS 4 & 5: Accumulate Outward & Resolve Metrics
    // =================================================================
    math::vec2<float> contentSize{0.0f, 0.0f};

    for (size_t i = 0; i < childCount; ++i) {
      IElement *child = node->GetChildren()[i].get();
      const auto &childMetrics = child->GetLayoutMetrics();
      const auto &childStyle = child->GetStyle();

      float childWidthWithMargin =
          childMetrics.computed_size.x +
          ((childStyle.margin.left + childStyle.margin.right) *
           m_globalDpiScale);
      float childHeightWithMargin =
          childMetrics.computed_size.y +
          ((childStyle.margin.top + childStyle.margin.bottom) *
           m_globalDpiScale);

      if (isRow) {
        contentSize.x += childWidthWithMargin;
        contentSize.y = std::max(contentSize.y, childHeightWithMargin);
      } else {
        contentSize.y += childHeightWithMargin;
        contentSize.x = std::max(contentSize.x, childWidthWithMargin);
      }
    }

    if (childCount > 0) {
      if (isRow)
        contentSize.x += gapsMain;
      else
        contentSize.y += gapsMain;
    }

    auto resolveFinalMetric =
        [this, &style](const ui::Size &sizeVariant, float contentVal,
                       float paddingTotal, float parentAlloc) -> float {
      if (std::holds_alternative<float>(sizeVariant)) {
        return std::get<float>(sizeVariant) * m_globalDpiScale;
      }
      if (std::holds_alternative<ui::SizeFit>(sizeVariant)) {
        return contentVal + paddingTotal;
      }
      return parentAlloc;
    };

    metrics.computed_size.x = resolveFinalMetric(style.size.x, contentSize.x,
                                                 paddingX, parentAllocation.x);
    metrics.computed_size.y = resolveFinalMetric(style.size.y, contentSize.y,
                                                 paddingY, parentAllocation.y);
  }

  void ExecPositionPass(IElement *node, const math::vec2<float> &globalOrigin, const math::vec4<float>& currentClip = {-10000.0f, -10000.0f, 100000.0f, 100000.0f}) {
    if (!node)
      return;

    LayoutAccumulation &metrics = node->GetLayoutMetrics();
    ui::styleConfig &style = node->GetStyle();

    metrics.global_position = globalOrigin + metrics.local_position;

    math::vec4<float> myClip = currentClip;
    if (style.overflow == Overflow::Hidden) {
       myClip.x = std::max(currentClip.x, metrics.global_position.x);
       myClip.y = std::max(currentClip.y, metrics.global_position.y);
       myClip.z = std::min(currentClip.z, metrics.global_position.x + metrics.computed_size.x);
       myClip.w = std::min(currentClip.w, metrics.global_position.y + metrics.computed_size.y);
    }
    style.clipRect = myClip;

    math::vec2<float> childCursorOffset{style.padding.left * m_globalDpiScale,
                                        style.padding.top * m_globalDpiScale};
    bool isRow = (style.flexDirection == FlexDirection::Row);

    for (const auto &child : node->GetChildren()) {
      LayoutAccumulation &childMetrics = child->GetLayoutMetrics();
      const auto &childStyle = child->GetStyle();

      if (isRow) {
        childCursorOffset.x += childStyle.margin.left * m_globalDpiScale;
        childMetrics.local_position = {
            childCursorOffset.x,
            childCursorOffset.y + (childStyle.margin.top * m_globalDpiScale)};

        childCursorOffset.x +=
            childMetrics.computed_size.x +
            ((childStyle.margin.right + style.gap.x) * m_globalDpiScale);
      } else {
        childCursorOffset.y += childStyle.margin.top * m_globalDpiScale;
        childMetrics.local_position = {
            childCursorOffset.x + (childStyle.margin.left * m_globalDpiScale),
            childCursorOffset.y};

        childCursorOffset.y +=
            childMetrics.computed_size.y +
            ((childStyle.margin.bottom + style.gap.y) * m_globalDpiScale);
      }

      ExecPositionPass(child.get(), metrics.global_position, style.clipRect);
    }
  }

  void FlattenHierarchy(IElement *node) {
    if (!node)
      return;
    m_linearizedRenderCache.push_back(node);
    for (const auto &child : node->GetChildren()) {
      FlattenHierarchy(child.get());
    }
  }

  std::vector<IElement *> m_linearizedRenderCache;
};

} // namespace ui
