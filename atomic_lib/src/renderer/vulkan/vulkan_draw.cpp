
#include "renderer/style.hpp"
#include "renderer/vulkan/vulkan_renderer.hpp"
#include <vulkan/vulkan_core.h>
namespace ui {

void VulkanRenderer::add_rect(float x, float y, float w, float h,
                              ui::styleConfig *style) {
  UIInstance instance;
  instance.pos = {x, y};
  instance.size = {w, h};
  instance.color = style->color;
  instance.radius = style->radius;
  instance.shapeType = static_cast<uint32_t>(style->shape);
  instance.strokeWidth = style->strokeWidth;
  instance.strokeColor = style->strokeColor;
  instance.dotGap = style->dotGap;
  instance.dotSize = style->dotSize;
  instance.strokePosition = style->strokePosition;
  m_ui_queue.push_back(instance);
}

void VulkanRenderer::add_circle(float x, float y, float radius,
                                ui::styleConfig *style) {
  style->radius = {radius, radius, radius, radius};
  add_rect(x, y, radius * 2, radius * 2, style);
}
} // namespace ui
