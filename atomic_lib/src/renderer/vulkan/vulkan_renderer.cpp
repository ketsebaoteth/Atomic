#include "vulkan_renderer.hpp"
#include "math/vec.hpp"
#include "renderer/style.hpp"
#include "renderer/vulkan/vulkan_shader.hpp"
#include "windowing/interface.hpp"
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace ui {

VulkanRenderer::VulkanRenderer(Window *window) : m_window(window) {
  init_vulkan();
  create_swapchain();
  create_renderpass();
  create_framebuffers();
  create_descriptor_set_layout();
  create_pipeline_layout();
  create_graphics_pipeline();
  create_storage_buffer();
  create_descriptor_pool();
  create_descriptor_set();
  create_commandpool();
  create_commandbuffer();
  create_sync_objects();
}

VulkanRenderer::~VulkanRenderer() {
  // clean up
  vkDeviceWaitIdle(m_device);

  vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
  vkDestroyBuffer(m_device, m_storageBuffer, nullptr);
  vkFreeMemory(m_device, m_storageBufferMemory, nullptr);

  vkDestroySemaphore(m_device, m_image_available_sem, nullptr);
  vkDestroySemaphore(m_device, m_render_finished_sem, nullptr);
  vkDestroyFence(m_device, m_in_flight_fence, nullptr);

  vkDestroyCommandPool(m_device, m_commandPool, nullptr);

  for (auto framebuffer : m_swapchainFrameBuffers) {
    vkDestroyFramebuffer(m_device, framebuffer, nullptr);
  }

  for (auto imageview : m_swapchainImageViews) {
    vkDestroyImageView(m_device, imageview, nullptr);
  }
  vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

  vkDestroyRenderPass(m_device, m_renderPass, nullptr);

  vkDestroyDevice(m_device, nullptr);
  vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
  vkDestroyInstance(m_instance, nullptr);
}

static const char *vk_result_to_string(VkResult result) {
  switch (result) {
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "requested layer not present";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "requested extensions not present";
  default:
    return "UNKOWN VULKAN ERROR";
  }
}

// enumrates gpu devices and chooses best prefers discrite gpus for now
static VkPhysicalDevice
selectBestDevice(const std::vector<VkPhysicalDevice> &devices) {
  if (devices.empty())
    return VK_NULL_HANDLE;

  for (VkPhysicalDevice device : devices) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      return device;
    }
  }
  // fallback to whatever came up probably integrated gpu
  return devices[0];
}

static uint32_t findIndexOfGraphicsSupportingFamily(
    std::vector<VkQueueFamilyProperties> &queueFamily, uint32_t familyCount,
    VkInstance instance, VkPhysicalDevice device) {
  VkBool32 presentationSupport;
  for (uint32_t i = 0; i < familyCount; i++) {
    presentationSupport =
        SDL_Vulkan_GetPresentationSupport(instance, device, i);
    if (queueFamily[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
        presentationSupport) {
      return i;
    }
  }
  return -1;
}

void VulkanRenderer::init_vulkan() {
  VkApplicationInfo appinfo{VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            nullptr,
                            "atomic",
                            1,
                            "atomic",
                            1,
                            VK_API_VERSION_1_3};

  uint32_t extCount = 0;
  const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extCount);

  VkInstanceCreateInfo instanceCreateInfo{
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      nullptr,
      0,
      &appinfo,
      0,
      nullptr,
      extCount,
      extensions};

  if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance) != VK_SUCCESS)
    throw std::runtime_error(
        std::string("Failed to create vulkan instance! Error code: "));

  // connect vulkan instance to window
  if (!SDL_Vulkan_CreateSurface((SDL_Window *)m_window->get_native_handle(),
                                m_instance, nullptr, &m_surface)) {
    throw std::runtime_error("sdl failed to attach vulkan surface!");
  }

  uint32_t physicalDevicesCount = 0;
  vkEnumeratePhysicalDevices(m_instance, &physicalDevicesCount, nullptr);

  std::vector<VkPhysicalDevice> physicalDevices;
  physicalDevices.resize(physicalDevicesCount);
  VkResult enumurationResult = vkEnumeratePhysicalDevices(
      m_instance, &physicalDevicesCount, physicalDevices.data());
  m_physical_device = selectBestDevice(physicalDevices);
  uint32_t pQueueFamilyPropertiesCount = 0;
  // get the count first
  vkGetPhysicalDeviceQueueFamilyProperties(
      m_physical_device, &pQueueFamilyPropertiesCount, nullptr);
  std::vector<VkQueueFamilyProperties> pQueueFamilyProperties;
  // resize to the count
  pQueueFamilyProperties.resize(pQueueFamilyPropertiesCount);
  vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device,
                                           &pQueueFamilyPropertiesCount,
                                           pQueueFamilyProperties.data());
  float queuePriority = 1.0f;
  m_graphicsSupportingQueueFamilyIndex = findIndexOfGraphicsSupportingFamily(
      pQueueFamilyProperties, pQueueFamilyPropertiesCount, m_instance,
      m_physical_device);
  // vk logical device creation info
  VkDeviceQueueCreateInfo devicequeuecreateinfo{
      VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0,
      m_graphicsSupportingQueueFamilyIndex,       1,       &queuePriority};

  const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkPhysicalDeviceFeatures deviceFeatures{};
  VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                nullptr,
                                0,
                                1,
                                &devicequeuecreateinfo,
                                0,
                                nullptr,
                                1,
                                deviceExtensions,
                                &deviceFeatures};
  VkResult createDeviceResult =
      vkCreateDevice(m_physical_device, &createInfo, nullptr, &m_device);
  if (createDeviceResult != VK_SUCCESS) {
    throw std::runtime_error("coudnt create vulkan device!");
  }

  vkGetDeviceQueue(m_device, m_graphicsSupportingQueueFamilyIndex, 0,
                   &m_graphicsQueue);
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }
  return availableFormats[0];
}

static VkPresentModeKHR
chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &presentModes) {
  for (const auto &presentMode : presentModes) {
    if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return presentMode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                                   Window *window) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    math::vec2<int> win_size;
    SDL_GetWindowSizeInPixels((SDL_Window *)window->get_native_handle(),
                              &win_size.x, &win_size.y);

    VkExtent2D actualExtent = win_size;

    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    return actualExtent;
  }
}

void VulkanRenderer::create_swapchain() {
  VkSurfaceCapabilitiesKHR surfaceCapablities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface,
                                            &surfaceCapablities);
  uint32_t pSurfaceFormatsCount;
  std::vector<VkSurfaceFormatKHR> surfaceFormats;
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface,
                                       &pSurfaceFormatsCount, nullptr);
  surfaceFormats.resize(pSurfaceFormatsCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface,
                                       &pSurfaceFormatsCount,
                                       surfaceFormats.data());
  uint32_t presentModeCount = 0;
  std::vector<VkPresentModeKHR> presentmodes;
  vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface,
                                            &presentModeCount, nullptr);
  presentmodes.resize(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_physical_device, m_surface, &presentModeCount, presentmodes.data());

  // choosing what we want
  VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(surfaceFormats);
  VkPresentModeKHR presentMode = chooseSwapPresentMode(presentmodes);
  VkExtent2D extent = chooseSwapExtent(surfaceCapablities, m_window);

  uint32_t imageCount = surfaceCapablities.minImageCount + 1;

  if (surfaceCapablities.maxImageCount > 0 &&
      imageCount > surfaceCapablities.maxImageCount) {
    imageCount = surfaceCapablities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m_surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) !=
      VK_SUCCESS) {
    throw std::runtime_error("coudnt create vulkan swapchain");
  }

  uint32_t actualImageCount = 0;
  vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualImageCount, nullptr);

  m_swapchainImages.resize(actualImageCount);
  vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualImageCount,
                          m_swapchainImages.data());

  m_swapchain_format = surfaceFormat.format;
  m_swapchainExtent = extent;

  m_swapchainImageViews.resize(m_swapchainImages.size());

  for (size_t i = 0; i < m_swapchainImages.size(); i++) {
    VkImageViewCreateInfo viewinfo{};
    viewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewinfo.image = m_swapchainImages[i];
    viewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewinfo.format = m_swapchain_format;

    viewinfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewinfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewinfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewinfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewinfo.subresourceRange.baseMipLevel = 0;
    viewinfo.subresourceRange.levelCount = 1;
    viewinfo.subresourceRange.baseArrayLayer = 0;
    viewinfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device, &viewinfo, nullptr,
                          &m_swapchainImageViews[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create image views");
    }
  }
}

void VulkanRenderer::create_renderpass() {
  VkAttachmentDescription colorAttachement{};
  colorAttachement.format = m_swapchain_format;
  colorAttachement.samples = VK_SAMPLE_COUNT_1_BIT;

  colorAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  colorAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  colorAttachement.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachement.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachementRef{};
  colorAttachementRef.attachment = 0;
  colorAttachementRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachementRef;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachement;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

void VulkanRenderer::create_framebuffers() {
  m_swapchainFrameBuffers.resize(m_swapchainImageViews.size());

  for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
    VkImageView attachments[] = {m_swapchainImageViews[i]};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_swapchainExtent.width;
    framebufferInfo.height = m_swapchainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr,
                            &m_swapchainFrameBuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("coudnt create frame buffer");
    }
  }
}

void VulkanRenderer::create_commandpool() {
  VkCommandPoolCreateInfo commandpoolinfo{};
  commandpoolinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandpoolinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  commandpoolinfo.queueFamilyIndex = m_graphicsSupportingQueueFamilyIndex;

  if (vkCreateCommandPool(m_device, &commandpoolinfo, nullptr,
                          &m_commandPool) != VK_SUCCESS) {
    throw std::runtime_error("unable to create commandpool !");
  }
}

void VulkanRenderer::create_commandbuffer() {
  VkCommandBufferAllocateInfo allocinfo{};
  allocinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocinfo.commandPool = m_commandPool;
  allocinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocinfo.commandBufferCount = 1;

  if (vkAllocateCommandBuffers(m_device, &allocinfo, &m_commandBuffer) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void VulkanRenderer::create_sync_objects() {
  VkSemaphoreCreateInfo seaminfo{};
  seaminfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceinfo{};
  fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  if (vkCreateSemaphore(m_device, &seaminfo, nullptr, &m_image_available_sem) !=
          VK_SUCCESS ||
      vkCreateSemaphore(m_device, &seaminfo, nullptr, &m_render_finished_sem) !=
          VK_SUCCESS ||
      vkCreateFence(m_device, &fenceinfo, nullptr, &m_in_flight_fence) !=
          VK_SUCCESS) {
    throw std::runtime_error("failed to create synchronization objects!");
  }
}

void VulkanRenderer::begin_frame() {
  vkWaitForFences(m_device, 1, &m_in_flight_fence, VK_TRUE, UINT64_MAX);
  vkResetFences(m_device, 1, &m_in_flight_fence);

  vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                        m_image_available_sem, VK_NULL_HANDLE,
                        &m_currentImageIndex);
  vkResetCommandBuffer(m_commandBuffer, 0);
  VkCommandBufferBeginInfo begininfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  vkBeginCommandBuffer(m_commandBuffer, &begininfo);

  // recording
  VkClearValue clearColor = {{{1.0f, 1.0f, 1.0f, 1.0f}}};
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_renderPass;
  renderPassInfo.framebuffer = m_swapchainFrameBuffers[m_currentImageIndex];
  renderPassInfo.renderArea.extent = m_swapchainExtent;
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = m_swapchainExtent;

  vkCmdBeginRenderPass(m_commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  // ui staff
  ui::styleConfig myStyle;
  myStyle.color = {0.9f, 0.9f, 0.9f, 1.0f};      // black
  myStyle.radius = {20.0f, 20.0f, 20.0f, 20.0f}; // Rounded corners

  // Draw a button-sized rect in the middle of the screen
  add_rect(100.0f, 100.0f, 200.0f, 60.0f, &myStyle);
  ui::styleConfig circleStyle;
  circleStyle.color = {0.0f, 0.0f, 0.0f, 1.0f};
  // add_circle(100.0f, 160.0f, 100.0f, &circleStyle);

  render_batch();
  vkCmdEndRenderPass(m_commandBuffer);

  vkEndCommandBuffer(m_commandBuffer);
}

void VulkanRenderer::end_frame() {
  VkSubmitInfo submitinfo{};
  submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitsemaphore[] = {m_image_available_sem};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitinfo.waitSemaphoreCount = 1;
  submitinfo.pWaitSemaphores = waitsemaphore;
  submitinfo.pWaitDstStageMask = waitStages;

  submitinfo.commandBufferCount = 1;
  submitinfo.pCommandBuffers = &m_commandBuffer;

  VkSemaphore signalSemaphore[] = {m_render_finished_sem};
  submitinfo.signalSemaphoreCount = 1;
  submitinfo.pSignalSemaphores = signalSemaphore;

  if (vkQueueSubmit(m_graphicsQueue, 1, &submitinfo, m_in_flight_fence) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to submit draw commandbuffer");
  }

  VkPresentInfoKHR presentinfo{};
  presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentinfo.waitSemaphoreCount = 1;
  presentinfo.pWaitSemaphores = signalSemaphore;

  VkSwapchainKHR swapChains[] = {m_swapchain};
  presentinfo.swapchainCount = 1;
  presentinfo.pSwapchains = swapChains;
  presentinfo.pImageIndices = &m_currentImageIndex;

  vkQueuePresentKHR(m_graphicsQueue, &presentinfo);
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter,
                                        VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    // 1. Check if this memory type is allowed by the buffer (typeFilter)
    // 2. Check if this memory type has all the features we need (properties)
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanRenderer::create_descriptor_set_layout() {
  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = 0;
  layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  layoutBinding.descriptorCount = 1;
  layoutBinding.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT; // Vertex shader needs it

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &layoutBinding;

  vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
                              &m_descriptorSetLayout);
}

void VulkanRenderer::create_descriptor_pool() {
  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSize.descriptorCount = 1;

  VkDescriptorPoolCreateInfo poolInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = 1;

  vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool);
}

void VulkanRenderer::create_descriptor_set() {
  VkDescriptorSetAllocateInfo allocInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocInfo.descriptorPool = m_descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_descriptorSetLayout;

  vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet);

  // Now connect the Set to your actual Buffer
  VkDescriptorBufferInfo bufferInfo{};
  bufferInfo.buffer = m_storageBuffer;
  bufferInfo.offset = 0;
  bufferInfo.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet descriptorWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  descriptorWrite.dstSet = m_descriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo = &bufferInfo;

  vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
}

void VulkanRenderer::render_batch() {
  if (m_ui_queue.empty())
    return;

  void *data;
  // since this is done every frame maybe mapping ones at the start of app and
  // unmapping at end might be better
  vkMapMemory(m_device, m_storageBufferMemory, 0,
              m_ui_queue.size() * sizeof(UIInstance), 0, &data);
  memcpy(data, m_ui_queue.data(), m_ui_queue.size() * sizeof(UIInstance));
  vkUnmapMemory(m_device, m_storageBufferMemory);

  vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_graphicsPipeline);

  VkViewport viewport{0.0f,
                      0.0f,
                      (float)m_swapchainExtent.width,
                      (float)m_swapchainExtent.height,
                      0.0f,
                      1.0f};
  vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{{0, 0}, m_swapchainExtent};
  vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

  UIPushConstants globals;
  globals.resolution = {(float)m_swapchainExtent.width,
                        (float)m_swapchainExtent.height};
  vkCmdPushConstants(m_commandBuffer, m_pipelineLayout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UIPushConstants),
                     &globals);

  vkCmdDraw(m_commandBuffer, 6, static_cast<uint32_t>(m_ui_queue.size()), 0, 0);

  m_ui_queue.clear();
}

void VulkanRenderer::create_storage_buffer() {
  VkDeviceSize bufferSize = sizeof(UIInstance) * 10000; // Room for 10k rects

  VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = bufferSize;
  bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_storageBuffer);

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_device, m_storageBuffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  vkAllocateMemory(m_device, &allocInfo, nullptr, &m_storageBufferMemory);
  vkBindBufferMemory(m_device, m_storageBuffer, m_storageBufferMemory, 0);
}

void VulkanRenderer::on_resize(int width, int height) {
  if (width == 0 || height == 0)
    return;
  vkDeviceWaitIdle(m_device);

  for (auto framebuffer : m_swapchainFrameBuffers) {
    vkDestroyFramebuffer(m_device, framebuffer, nullptr);
  }
  for (auto imageview : m_swapchainImageViews) {
    vkDestroyImageView(m_device, imageview, nullptr);
  }
  vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

  create_swapchain();
  create_framebuffers();
}
VkShaderModule
VulkanRenderer::createShaderModule(const std::vector<char> &code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module!");
  }
  return shaderModule;
}

void VulkanRenderer::create_pipeline_layout() {
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(UIPushConstants);

  VkPipelineLayoutCreateInfo pipelinelayoutinfo{};
  pipelinelayoutinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelinelayoutinfo.pushConstantRangeCount = 1;
  pipelinelayoutinfo.pPushConstantRanges = &pushConstantRange;
  pipelinelayoutinfo.setLayoutCount = 1;
  pipelinelayoutinfo.pSetLayouts = &m_descriptorSetLayout;
  if (vkCreatePipelineLayout(m_device, &pipelinelayoutinfo, nullptr,
                             &m_pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("coudnt create pipleineLayout");
  }
}

void VulkanRenderer::create_graphics_pipeline() {
  auto vertShaderCode = vulkan::shader::readFile("shaders/ui.vert.spv");
  auto fragShaderCode = vulkan::shader::readFile("shaders/ui.frag.spv");

  VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
  VkPipelineShaderStageCreateInfo vertshaderstageinfo{};
  vertshaderstageinfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertshaderstageinfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertshaderstageinfo.module = vertShaderModule;
  vertshaderstageinfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertshaderstageinfo,
                                                    fragShaderStageInfo};

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // Viewport and Scissor are dynamic (set during draw)
  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE; // UI is single-sided
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // Standard Alpha Blending for UI
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = m_pipelineLayout;
  pipelineInfo.renderPass = m_renderPass;
  pipelineInfo.subpass = 0;

  if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
  vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
}
} // namespace ui
