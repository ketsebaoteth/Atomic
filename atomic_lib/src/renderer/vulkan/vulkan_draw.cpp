#include "SDL3/SDL_video.h"
#include "renderer/font/freetype_layout.hpp"
#include "renderer/font/interface.hpp"
#include "renderer/style.hpp"
#include "renderer/vulkan/vulkan_renderer.hpp"
#include "windowing/interface.hpp"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace ui {

// void VulkanRenderer::add_rect(const math::vec2<float> &globalPosition,
//                               const math::vec2<float> &computedSize,
//                               const ui::styleConfig *style) {
//   if (!style)
//     return;
//
//   UIInstance instance{};
//   instance.pos = globalPosition;
//   instance.size = computedSize;
//   instance.color = style->backgroundColor;
//   instance.radius = style->radius;
//   instance.shapeType = static_cast<uint32_t>(style->shape);
//   instance.strokeWidth = style->strokeWidth;
//   instance.strokeColor = style->strokeColor;
//   instance.dotGap = style->dotGap;
//   instance.dotSize = style->dotSize;
//   instance.strokePosition = style->strokePosition;
//
//   m_ui_queue.push_back(instance);
// }

void VulkanRenderer::add_rect(const math::vec2<float> &globalPosition,
                              const math::vec2<float> &computedSize,
                              const ui::styleConfig *style) {
  if (!style)
    return;

  UIInstance instance{};

  // ---------------------------------
  // Base geometry
  // ---------------------------------

  instance.pos = globalPosition;
  instance.size = computedSize;

  // ---------------------------------
  // Fill / shape
  // ---------------------------------

  instance.backgroundColor = style->backgroundColor;

  instance.radius = style->radius;

  instance.shapeType = static_cast<uint32_t>(style->shape);

  // ---------------------------------
  // Stroke
  // ---------------------------------

  instance.strokeWidth = style->strokeWidth;

  instance.strokeColor = style->strokeColor;

  instance.strokePosition = style->strokePosition;

  instance.dotGap = style->dotGap;

  instance.dotSize = style->dotSize;

  // ---------------------------------
  // Texture defaults
  // ---------------------------------

  instance.textureIndex = 0;

  instance.uvMin = {0.0f, 0.0f};

  instance.uvMax = {1.0f, 1.0f};

  instance.opacity = style->opacity;
  instance.clipRect = style->clipRect;

  // ---------------------------------
  // Gradient
  // ---------------------------------

  instance.gradientType = static_cast<uint32_t>(style->gradientType);

  instance.gradientDirection = style->gradientDirection;

  instance.gradientCenter = style->gradientCenter;

  instance.gradientRadius = style->gradientRadius;

  // Store offset BEFORE append
  instance.gradientStopOffset = static_cast<uint32_t>(m_gradientStops.size());

  instance.gradientStopCount =
      static_cast<uint32_t>(style->gradientStops.size());

  // Append stops into global GPU buffer
  for (const auto &stop : style->gradientStops) {

    GradientStopGPU gpuStop{};

    gpuStop.position = stop.position;
    gpuStop.color = stop.color;

    m_gradientStops.push_back(gpuStop);
  }

  // ---------------------------------
  // Queue draw instance
  // ---------------------------------

  m_ui_queue.push_back(instance);
}

void VulkanRenderer::add_circle(const math::vec2<float> &globalPosition,
                                float radius, ui::styleConfig *style) {
  if (!style)
    return;

  style->radius = {radius, radius, radius, radius};
  math::vec2<float> diameterSize{radius * 2.0f, radius * 2.0f};

  add_rect(globalPosition, diameterSize, style);
}

void VulkanRenderer::add_text(const math::vec2<float> &globalPosition,
                              const std::string &text,
                              const ui::styleConfig *style, float dpiScale) {
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
    UIInstance instance{};

    instance.pos = {globalPosition.x + pg.rect.x,
                    globalPosition.y + fontAscender + pg.rect.y};

    instance.size = {pg.rect.z, pg.rect.w};

    instance.backgroundColor = pg.color;

    instance.shapeType = static_cast<uint32_t>(ui::ShapeType::Text);

    instance.uvMin = {pg.uv.x, pg.uv.y};
    instance.uvMax = {pg.uv.z, pg.uv.w};

    instance.strokeWidth = pg.fontWeightOffset;
    instance.opacity = style->opacity;
    instance.clipRect = style->clipRect;

    // IMPORTANT: font is NOT stored in UIInstance
    // font only affects glyph generation

    m_ui_queue.push_back(instance);
  }
}
// void VulkanRenderer::add_text(const math::vec2<float> &globalPosition,
//                               const std::string &text,
//                               const ui::styleConfig *style, float dpiScale) {
//   if (!style) {
//     printf("no font so not rendering");
//     return;
//   }
//
//   ui::font::Font *activeFont = style->font
//                                    ? static_cast<ui::font::Font
//                                    *>(style->font) : m_default_font.get();
//
//   if (!activeFont) {
//     std::cerr << "Renderer Warning: Dropping text draw call due to missing "
//                  "Font asset."
//               << std::endl;
//     return;
//   }
//
//   // Pure single-source-of-truth configuration pass passed explicitly from
//   // SDLWindow
//   float physicalFontSize = style->fontSize * dpiScale;
//   float physicalMaxWidth = style->maxWidth * dpiScale;
//   float physicalTracking = style->tracking * dpiScale;
//
//   std::vector<font::TextRun> runs = font::TextLayoutEngine::parseRichText(
//       text, physicalFontSize, style->backgroundColor);
//
//   if (!runs.empty()) {
//     runs[0].styleFlags = static_cast<uint8_t>(style->styleFlag);
//   }
//
//   std::vector<ui::font::PositionedGlyph> positionedGlyphs =
//       font::TextLayoutEngine::calcLayout(runs, activeFont, physicalMaxWidth,
//                                          physicalTracking);
//
//   // NOTE: If getAscender() inside your freetype_font layer already reflects
//   the
//   // internal FreeType face metrics scaled by physicalFontSize, do not
//   multiply
//   // by dpiScale again.
//   float fontAscender = activeFont->getAscender(physicalFontSize);
//
//   for (const auto &pg : positionedGlyphs) {
//     UIInstance instance{};
//     instance.pos = {globalPosition.x + pg.rect.x,
//                     globalPosition.y + fontAscender + pg.rect.y};
//     instance.size = {pg.rect.z, pg.rect.w};
//     instance.color = pg.color;
//
//     instance.shapeType = 2; // SHAPE_TEXT
//     instance.uvMin = {pg.uv.x, pg.uv.y};
//     instance.uvMax = {pg.uv.z, pg.uv.w};
//
//     instance.strokeWidth = pg.fontWeightOffset;
//
//     m_ui_queue.push_back(instance);
//   }
// }

void VulkanRenderer::add_image(const math::vec2<float> &globalPosition,
                               const math::vec2<float> &computedSize,
                               const std::string &path,
                               const ui::styleConfig *style) {
  uint32_t textureId = get_or_create_texture(path);

  UIInstance instance{};
  instance.pos = globalPosition;
  instance.size = computedSize;
  instance.backgroundColor = style ? style->backgroundColor
                                   : math::vec4<float>{1.0f, 1.0f, 1.0f, 1.0f};
  instance.radius =
      style ? style->radius : math::vec4<float>{0.0f, 0.0f, 0.0f, 0.0f};

  instance.shapeType = 3; // SHAPE_IMAGE
  instance.textureIndex = textureId;

  instance.uvMin = {0.0f, 0.0f};
  instance.uvMax = {1.0f, 1.0f};

  instance.strokeWidth = style ? style->strokeWidth : 0.0f;
  instance.strokeColor =
      style ? style->strokeColor : math::vec4<float>{0.0f, 0.0f, 0.0f, 0.0f};
  instance.strokePosition = style ? style->strokePosition : 0;
  instance.dotGap = style ? style->dotGap : 0.0f;
  instance.dotSize = style ? style->dotSize : 0.0f;
  instance.opacity = style ? style->opacity : 1.0f;
  instance.clipRect = style ? style->clipRect : math::vec4<float>{-10000.0f, -10000.0f, 100000.0f, 100000.0f};

  m_ui_queue.push_back(instance);
}

} // namespace ui
