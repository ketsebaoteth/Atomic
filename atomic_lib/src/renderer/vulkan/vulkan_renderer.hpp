#pragma once

#include "math/vec.hpp"
#include "renderer/assetloader/interface.hpp"
#include "renderer/font/interface.hpp"
#include "renderer/interface.hpp"
#include "renderer/style.hpp"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace ui {

// vulkan_renderer.hpp
struct UIPushConstants {
  math::vec2<float> resolution;
};

struct GradientStopGPU {
  alignas(16) math::vec4<float> color;
  alignas(4) float position;

  // std430 padding
  alignas(4) float _pad0 = 0.0f;
  alignas(4) float _pad1 = 0.0f;
  alignas(4) float _pad2 = 0.0f;
};

struct UIInstance {
  // ---------------------------------
  // Base geometry
  // ---------------------------------

  alignas(8) math::vec2<float> pos;
  alignas(8) math::vec2<float> size;

  // ---------------------------------
  // Fill / shape
  // ---------------------------------

  alignas(16) math::vec4<float> backgroundColor;
  alignas(16) math::vec4<float> radius;

  alignas(4) float opacity;
  alignas(4) uint32_t shapeType;

  // ---------------------------------
  // Stroke
  // ---------------------------------

  alignas(4) float strokeWidth;
  alignas(4) uint32_t strokePosition;
  alignas(4) float dotGap;
  alignas(4) float dotSize;

  // ---------------------------------
  // Texture
  // ---------------------------------

  alignas(4) uint32_t textureIndex;

  // occupies alignment gap
  alignas(4) uint32_t _padTex = 0;

  alignas(8) math::vec2<float> uvMin;
  alignas(8) math::vec2<float> uvMax;

  // ---------------------------------
  // Stroke color
  // ---------------------------------

  alignas(16) math::vec4<float> strokeColor;

  // ---------------------------------
  // Gradient
  // ---------------------------------

  alignas(4) uint32_t gradientType;

  // radians
  alignas(4) float gradientDirection;

  alignas(8) math::vec2<float> gradientCenter;

  alignas(4) float gradientRadius;

  // Offset into global gradient stop SSBO
  alignas(4) uint32_t gradientStopOffset;

  // Number of stops
  alignas(4) uint32_t gradientStopCount;

  // std430 alignment padding
  alignas(4) uint32_t _padGradient = 0;

  // Bounds Clip passing
  alignas(16) math::vec4<float> clipRect;
};

class VulkanRenderer : public Renderer {
public:
  static std::unique_ptr<Renderer> Create(class Window *window) {
    return std::make_unique<VulkanRenderer>(window);
  }

  VulkanRenderer(class Window *window);
  ~VulkanRenderer() override;

  void begin_frame() override;
  void end_frame() override;
  void on_resize(int width, int height) override;
  void set_default_font(std::unique_ptr<ui::font::Font> font) {
    m_default_font = std::move(font);
  }
  ui::font::Font *get_default_font() override { return m_default_font.get(); }
  ui::font::Font *getFont(ui::font::Font *font);

  void set_asset_loader(ui::asset::AssetLoader *loader) {
    m_asset_loader = loader;
  }
  // -- drawing code --
  void add_rect(const math::vec2<float> &globalPosition,
                const math::vec2<float> &computedSize,
                const ui::styleConfig *style) override;
  void add_circle(const math::vec2<float> &globalPosition, float radius,
                  ui::styleConfig *style) override;
  void add_text(const math::vec2<float> &globalPosition,
                const std::string &text, const ui::styleConfig *style,
                float dpiScale) override;
  void add_image(const math::vec2<float> &globalPosition,
                 const math::vec2<float> &computedSize, const std::string &path,
                 const ui::styleConfig *style) override;

private:
  void init_vulkan();
  void create_swapchain();
  void create_renderpass();
  void create_framebuffers();
  void create_commandpool();
  void create_commandbuffer();
  void create_sync_objects();
  void cleanup_swapchain();
  void create_pipeline_layout();
  void create_graphics_pipeline();
  void create_storage_buffer();
  void render_batch() override;
  void create_descriptor_set_layout();
  void create_descriptor_pool();
  void create_descriptor_set();
  std::unique_ptr<ui::font::Font> m_default_font = nullptr;

  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

  // Font Atlas Resources
  VkImage m_fontAtlasImage = VK_NULL_HANDLE;
  VkDeviceMemory m_fontAtlasMemory = VK_NULL_HANDLE;
  VkImageView m_fontAtlasImageView =
      VK_NULL_HANDLE; // You will need this to bind to your descriptor set!
  VkSampler m_fontAtlasSampler =
      VK_NULL_HANDLE; // To sample pixels in your fragment shader
  void create_texture_resource(const uint8_t *pixels, uint32_t width,
                               uint32_t height);
  // iamge texture tracking
  std::vector<VkImage> m_textureImages;
  std::vector<VkDeviceMemory> m_textureMemories;
  std::vector<VkImageView> m_textureViews;

  // small cache map to not always reload from disc
  std::unordered_map<std::string, uint32_t> m_imagePathToIdCache;
  VkSampler m_sharedSampler = VK_NULL_HANDLE;
  bool m_descriptorDirty = false;
  ui::asset::AssetLoader *m_asset_loader;
  uint32_t get_or_create_texture(const std::string &path);

  // shader loading utilities
  VkShaderModule createShaderModule(const std::vector<char> &code);

  class Window *m_window;
  std::vector<UIInstance> m_ui_queue;
  std::vector<GradientStopGPU> m_gradientStops;
  VkBuffer m_storageBuffer;
  VkDeviceMemory m_storageBufferMemory;
  VkBuffer m_gradientBuffer;
  VkDeviceMemory m_gradientBufferMemory;

  VkInstance m_instance = VK_NULL_HANDLE;
  VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VkQueue m_graphicsQueue = VK_NULL_HANDLE;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;

  // swap chain
  VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> m_swapchainImages;
  VkExtent2D m_swapchainExtent;
  std::vector<VkImageView> m_swapchainImageViews;
  std::vector<VkFramebuffer> m_swapchainFrameBuffers;
  VkFormat m_swapchain_format;
  VkCommandBuffer m_commandBuffer;
  uint32_t m_currentImageIndex = 0;
  VkRenderPass m_renderPass;
  VkCommandPool m_commandPool;
  uint32_t m_graphicsSupportingQueueFamilyIndex;
  VkPipelineLayout m_pipelineLayout;
  VkPipeline m_graphicsPipeline;

  VkDescriptorSetLayout m_descriptorSetLayout;
  VkDescriptorPool m_descriptorPool;
  VkDescriptorSet m_descriptorSet;
  // sync object related
  VkSemaphore m_image_available_sem;
  VkSemaphore m_render_finished_sem;
  VkFence m_in_flight_fence;
};
} // namespace ui
