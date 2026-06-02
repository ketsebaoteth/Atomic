// #include "SDL3/SDL_video.h"
#include "renderer/font/freetype_layout.hpp"
#include "renderer/font/interface.hpp"
#include "renderer/style.hpp"
#include "renderer/vulkan/vulkan_renderer.hpp"
// #include "windowing/interface.hpp"
#include <cstdint>
#include <cstdio>
// #include <iostream>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace ui {

void VulkanRenderer::add_rect(const math::vec2<float> &globalPosition,
                              const math::vec2<float> &computedSize,
                              ui::styleConfig *style) {
  if (!style)
    return;

  style->styleConfigGPU.pos = globalPosition;
  style->styleConfigGPU.size = computedSize;

  m_ui_queue.push_back(style->styleConfigGPU);
}

void VulkanRenderer::add_circle(const math::vec2<float> &globalPosition,
                                float radius, ui::styleConfig *style) {
  if (!style)
    return;

  style->styleConfigGPU.radius = {radius, radius, radius, radius};
  math::vec2<float> diameterSize{radius * 2.0f, radius * 2.0f};

  add_rect(globalPosition, diameterSize, style);
}

void VulkanRenderer::add_text(const math::vec2<float> &globalPosition,
                              const std::string &text, ui::styleConfig *style,
                              float dpiScale) {
  if (!style) {
    printf("no font so not rendering");
    return;
  }

  ui::font::Font *activeFont = style->font
                                   ? getFont(style->font) // fontId lookup
                                   : m_default_font.get();

  if (!activeFont) {
    printf("no font set so not rendering");
    return;
  }

  float physicalFontSize = style->fontSize * dpiScale;
  float physicalMaxWidth = style->maxWidth * dpiScale;
  float physicalTracking = style->tracking * dpiScale;

  auto runs = font::TextLayoutEngine::parseRichText(text, physicalFontSize,
                                                    style->textColor);

  if (!runs.empty())
    runs[0].styleFlags = static_cast<uint8_t>(style->styleFlag);

  auto positionedGlyphs = font::TextLayoutEngine::calcLayout(
      runs, activeFont, physicalMaxWidth, physicalTracking);

  float fontAscender = activeFont->getAscender(physicalFontSize);

  for (const auto &pg : positionedGlyphs) {
    // styleConfig::StyleConfigGPU style->styleConfigGPU =
    // style->styleConfigGPU;

    style->styleConfigGPU.pos = {globalPosition.x + pg.rect.x,
                                 globalPosition.y + fontAscender + pg.rect.y};

    style->styleConfigGPU.size = {pg.rect.z, pg.rect.w};

    style->styleConfigGPU.backgroundColor = pg.color;

    style->styleConfigGPU.shapeType =
        static_cast<uint32_t>(ui::ShapeType::Text);

    style->styleConfigGPU.uvMin = {pg.uv.x, pg.uv.y};
    style->styleConfigGPU.uvMax = {pg.uv.z, pg.uv.w};

    style->styleConfigGPU.strokeWidth = pg.fontWeightOffset;
    // style->styleConfigGPU.opacity = style->opacity;

    // IMPORTANT: font is NOT stored in UIstyle->styleConfigGPU
    // font only affects glyph generation

    m_ui_queue.push_back(style->styleConfigGPU);
  }
}

void VulkanRenderer::add_image(const math::vec2<float> &globalPosition,
                               const math::vec2<float> &computedSize,
                               const std::string &path,
                               ui::styleConfig *style) {
  uint32_t textureId = get_or_create_texture(path);

  // styleConfig::StyleConfigGPU style->styleConfigGPU = style->styleConfigGPU;
  style->styleConfigGPU.pos = globalPosition;
  style->styleConfigGPU.size = computedSize;

  style->styleConfigGPU.shapeType =
      static_cast<uint32_t>(ui::ShapeType::Image); // SHAPE_IMAGE
  style->styleConfigGPU.textureIndex = textureId;

  m_ui_queue.push_back(style->styleConfigGPU);
}

} // namespace ui
