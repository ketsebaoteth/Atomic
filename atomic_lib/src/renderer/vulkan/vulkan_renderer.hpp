#pragma once

#include "math/vec.hpp"
#include "renderer/interface.hpp"
#include "renderer/style.hpp"
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace ui {

// vulkan_renderer.hpp
struct UIPushConstants {
  math::vec2<float> resolution;
};

struct UIInstance {
  alignas(8) math::vec2<float> pos;
  alignas(8) math::vec2<float> size;
  alignas(16) math::vec4<float> color;
  alignas(16) math::vec4<float> radius;
  alignas(4) uint32_t shapeType;

  alignas(4) float strokeWidth;
  alignas(4) uint32_t strokePosition;
  alignas(4) float dotGap;
  alignas(4) float dotSize;

  float _pad[3];

  alignas(16) math::vec4<float> strokeColor;
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
  void render_batch();
  void create_descriptor_set_layout();
  void create_descriptor_pool();
  void create_descriptor_set();

  uint32_t findMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);
  // -- drawing code --
  void add_rect(float x, float y, float w, float h, ui::styleConfig *style);
  void add_circle(float x, float y, float radius, ui::styleConfig *style);
  // drawing codes --
  VkShaderModule createShaderModule(const std::vector<char> &code);

  class Window *m_window;
  std::vector<UIInstance> m_ui_queue;
  VkBuffer m_storageBuffer;
  VkDeviceMemory m_storageBufferMemory;

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
