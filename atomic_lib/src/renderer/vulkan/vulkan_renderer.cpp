#include "vulkan_renderer.hpp"
#include "math/vec.hpp"
#include "renderer/font/freetype_font.hpp"
#include "renderer/font/interface.hpp"
#include "renderer/style.hpp"
#include "renderer/vulkan/vulkan_shader.hpp"
#include "windowing/interface.hpp"
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
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
  // Clean up Font Atlas Views and Samplers
  if (m_fontAtlasSampler != VK_NULL_HANDLE)
    vkDestroySampler(m_device, m_fontAtlasSampler, nullptr);
  if (m_fontAtlasImageView != VK_NULL_HANDLE)
    vkDestroyImageView(m_device, m_fontAtlasImageView, nullptr);
  if (m_fontAtlasImage != VK_NULL_HANDLE)
    vkDestroyImage(m_device, m_fontAtlasImage, nullptr);
  if (m_fontAtlasMemory != VK_NULL_HANDLE)
    vkFreeMemory(m_device, m_fontAtlasMemory, nullptr);

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
  // ==========================================
  // UI VERIFICATION SUITE
  // ==========================================

  // 1. TEST ADD_RECT (A solid button background)
  ui::styleConfig rectStyle{};
  rectStyle.color = {0.2f, 0.6f, 0.9f, 1.0f};      // Bright blue
  rectStyle.radius = {12.0f, 12.0f, 12.0f, 12.0f}; // Subtle rounded corners
  rectStyle.shape = ShapeType::RoundedRect; // Map to your custom shape enum
                                            // (e.g. Rectangle/RoundedRect)
  rectStyle.strokeWidth = 0.0f;
  add_rect(100.0f, 100.0f, 250.0f, 60.0f, &rectStyle);

  // 2. TEST ADD_CIRCLE (Placed safely next to the rectangle)
  ui::styleConfig circleStyle{};
  circleStyle.color = {0.9f, 0.3f, 0.3f, 1.0f}; // Reddish circle
  circleStyle.shape =
      ShapeType::Circle;          // Map to your circle shape enum if applicable
  circleStyle.strokeWidth = 2.0f; // Test border tracing properties
  circleStyle.strokeColor = {1.0f, 1.0f, 1.0f, 1.0f};
  add_circle(
      400.0f, 100.0f, 30.0f,
      &circleStyle); // x=400, y=100, radius=30 (will draw 60x60 bounding box)

  // image test
  ui::styleConfig imageStyle{};
  imageStyle.color = {1.0f, 1.0f, 1.0f, 1.0f}; // Clear tint
  imageStyle.radius = {12.0f, 12.0f, 12.0f,
                       12.0f}; // Rounded borders match up perfectly
  imageStyle.shape = ShapeType::Image;

  add_image(100.0f, 200.0f, 1500.0f, 900.0f, "gta.jpg", &imageStyle);

  if (m_default_font) {
    ui::styleConfig textStyle{};
    textStyle.font = m_default_font;
    textStyle.fontSize = 24;
    textStyle.color = {1.0f, 1.0f, 1.0f, 1.0f};
    textStyle.styleFlag = font::TextStyleBit::Regular;
    textStyle.maxWidth = 800.0f;
    textStyle.tracking = 0.0f;

    add_text(120.0f, 120.0f, "Hello Vulkan UI!", &textStyle);
  }
  // ==========================================
  // DISPATCH BATCH & RECORD END
  // ==========================================

  // This physically updates your GPU buffers with data stored in m_ui_queue
  // and submits the vkCmdDrawIndexed calls.
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
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  // New binding slot for our texture lookup engine inside ui.frag
  VkDescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 1;
  samplerLayoutBinding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutBinding samplerLayoutBindingPic{};
  samplerLayoutBindingPic.binding = 2;
  samplerLayoutBindingPic.descriptorCount =
      16; // Match the array size in the shader
  samplerLayoutBindingPic.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBindingPic.pImmutableSamplers = nullptr;
  samplerLayoutBindingPic.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
      layoutBinding, samplerLayoutBinding, samplerLayoutBindingPic};

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr,
                                  &m_descriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

void VulkanRenderer::create_descriptor_pool() {
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSizes[0].descriptorCount = 1;

  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = 1 + 16;

  VkDescriptorPoolCreateInfo poolInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 1;

  if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}

void VulkanRenderer::create_descriptor_set() {
  VkDescriptorSetAllocateInfo allocInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocInfo.descriptorPool = m_descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_descriptorSetLayout;

  if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }

  VkDescriptorBufferInfo bufferInfo{};
  bufferInfo.buffer = m_storageBuffer;
  bufferInfo.offset = 0;
  bufferInfo.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet bufferWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  bufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  bufferWrite.dstSet = m_descriptorSet;
  bufferWrite.dstBinding = 0;
  bufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bufferWrite.descriptorCount = 1;
  bufferWrite.pBufferInfo = &bufferInfo;

  // Execute the initial storage buffer binding update pass
  vkUpdateDescriptorSets(m_device, 1, &bufferWrite, 0, nullptr);

  // Note: binding = 1 (fontAtlas) and binding = 2 (uiTexture[16]) are left
}

void VulkanRenderer::render_batch() {
  if (m_ui_queue.empty())
    return;

  // Track if we need to write changes to descriptor sets this frame
  bool fontAtlasChanged = false;

  if (m_default_font) {
    auto *concreteFont = static_cast<ui::font::FreeTypeFont *>(m_default_font);

    if (concreteFont->consumeTextureDirtyBit()) {
      const uint8_t *pixels = concreteFont->getRawPixels();
      uint32_t width = 1024; // Matches your FreeTypeFont defaults
      uint32_t height = 1024;

      // Upload the raw R8 single-channel data array to your Vulkan texture
      // memory handles
      create_texture_resource(pixels, width, height);

      fontAtlasChanged = true;
    }
  }

  // --- BINDING 1 UPDATE: Font Atlas Single Sampler ---
  if (fontAtlasChanged && m_fontAtlasImageView != VK_NULL_HANDLE) {
    VkDescriptorImageInfo fontImageInfo{};
    fontImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    fontImageInfo.imageView = m_fontAtlasImageView;
    fontImageInfo.sampler = m_fontAtlasSampler;

    VkWriteDescriptorSet fontSamplerWrite{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    fontSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    fontSamplerWrite.dstSet = m_descriptorSet;
    fontSamplerWrite.dstBinding = 1; // Explicitly targets binding = 1
    fontSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    fontSamplerWrite.descriptorCount = 1;
    fontSamplerWrite.pImageInfo = &fontImageInfo;

    vkUpdateDescriptorSets(m_device, 1, &fontSamplerWrite, 0, nullptr);
  }

  // --- BINDING 2 UPDATE: Disk Image Sampler Array ---
  // Run this if a new image was added, OR if the font atlas just loaded (since
  // it's our fallback view)
  if ((m_descriptorDirty || fontAtlasChanged) &&
      m_fontAtlasImageView != VK_NULL_HANDLE) {
    std::vector<VkDescriptorImageInfo> imageInfos(16);
    for (size_t i = 0; i < 16; ++i) {
      imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfos[i].sampler =
          m_fontAtlasSampler; // Share the UI filtering sampler properties

      // Fill slots with loaded textures, or fall back to font view to prevent
      // validation layers from complaining
      if (i < m_textureViews.size()) {
        imageInfos[i].imageView = m_textureViews[i];
      } else {
        imageInfos[i].imageView = m_fontAtlasImageView;
      }
    }

    VkWriteDescriptorSet arrayWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    arrayWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    arrayWrite.dstSet = m_descriptorSet;
    arrayWrite.dstBinding = 2; // Explicitly targets binding = 2 layout array
    arrayWrite.dstArrayElement = 0;
    arrayWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    arrayWrite.descriptorCount = 16;
    arrayWrite.pImageInfo = imageInfos.data();

    vkUpdateDescriptorSets(m_device, 1, &arrayWrite, 0, nullptr);

    // Only turn off the dirty flag when the update is safely submitted!
    m_descriptorDirty = false;
  }

  // --- CORE HOST DATA STORAGE STREAM AND DRAW COMMAND SEQUENCE ---
  void *data;
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

void VulkanRenderer::create_texture_resource(const uint8_t *pixels,
                                             uint32_t width, uint32_t height) {
  VkDeviceSize imageSize =
      width * height; // 1 byte per pixel for R8_UNORM (FreeType standard)

  // 1. Create a Staging Buffer to host the CPU pixel memory
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = imageSize;
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create staging buffer for font atlas!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingBufferMemory);
  vkBindBufferMemory(m_device, stagingBuffer, stagingBufferMemory, 0);

  // 2. Map staging buffer memory and copy the font pixel array directly
  void *data;
  vkMapMemory(m_device, stagingBufferMemory, 0, imageSize, 0, &data);
  std::memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(m_device, stagingBufferMemory);

  // 3. Create the actual VkImage Texture destination on the GPU hardware
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format =
      VK_FORMAT_R8_UNORM; // Matching our single-channel FreeType cache format
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

  if (vkCreateImage(m_device, &imageInfo, nullptr, &m_fontAtlasImage) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create font atlas image!");
  }

  vkGetImageMemoryRequirements(m_device, m_fontAtlasImage, &memRequirements);
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vkAllocateMemory(m_device, &allocInfo, nullptr, &m_fontAtlasMemory);
  vkBindImageMemory(m_device, m_fontAtlasImage, m_fontAtlasMemory, 0);

  // 4. Record and submit immediate execution commands for image layout
  // transitions and copy operations
  VkCommandBufferAllocateInfo cmdAllocInfo{};
  cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdAllocInfo.commandPool = m_commandPool;
  cmdAllocInfo.commandBufferCount = 1;

  VkCommandBuffer tempCommandBuffer;
  vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &tempCommandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(tempCommandBuffer, &beginInfo);

  // Barrier 1: UNDEFINED -> TRANSFER_DST_OPTIMAL
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_fontAtlasImage;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  vkCmdPipelineBarrier(tempCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  // Buffer to Image Copy Command
  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(tempCommandBuffer, stagingBuffer, m_fontAtlasImage,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // Barrier 2: TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(tempCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkEndCommandBuffer(tempCommandBuffer);

  // Submit to Graphic Queue instantly
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &tempCommandBuffer;

  vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_graphicsQueue); // Simple flush synchronize execution path

  // Cleanup resources used for transfer pipeline operations
  vkFreeCommandBuffers(m_device, m_commandPool, 1, &tempCommandBuffer);
  vkDestroyBuffer(m_device, stagingBuffer, nullptr);
  vkFreeMemory(m_device, stagingBufferMemory, nullptr);

  // ==========================================
  // CREATE IMAGE VIEW & SAMPLER FOR THE ATLAS
  // ==========================================
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = m_fontAtlasImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8_UNORM;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_fontAtlasImageView) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create font atlas image view!");
  }

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_fontAtlasSampler) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create font atlas sampler!");
  }
}

uint32_t VulkanRenderer::get_or_create_texture(const std::string &path) {
  if (!m_asset_loader) {
    throw std::runtime_error("[VulkanRenderer] No asset loader attached!");
  }

  auto it = m_imagePathToIdCache.find(path);
  if (it != m_imagePathToIdCache.end()) {
    return it->second;
  }

  auto image_data = m_asset_loader->load<ui::asset::ImageAsset>(path);
  if (!image_data) {
    std::cerr << "[Vulkan] Failed to load image from path: " << path
              << std::endl;
    return 0; // Fallback to index 0
  }

  VkDeviceSize imageSize =
      image_data->width * image_data->height * 4; // 4 channels (RGBA)

  uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(
                           std::max(image_data->width, image_data->height)))) +
                       1;
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = imageSize;
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create image staging buffer!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingBufferMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate image staging buffer memory!");
  }

  vkBindBufferMemory(m_device, stagingBuffer, stagingBufferMemory, 0);

  void *mappedData;
  vkMapMemory(m_device, stagingBufferMemory, 0, imageSize, 0, &mappedData);
  std::memcpy(mappedData, image_data->pixels.data(),
              static_cast<size_t>(imageSize));
  vkUnmapMemory(m_device, stagingBufferMemory);

  VkImage newImage;
  VkDeviceMemory newMemory;
  VkImageView newView;

  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = image_data->width;
  imageInfo.extent.height = image_data->height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT |
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

  if (vkCreateImage(m_device, &imageInfo, nullptr, &newImage) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics texture image!");
  }

  vkGetImageMemoryRequirements(m_device, newImage, &memRequirements);

  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (vkAllocateMemory(m_device, &allocInfo, nullptr, &newMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "failed to allocate graphics texture image memory!");
  }

  vkBindImageMemory(m_device, newImage, newMemory, 0);

  VkCommandBufferAllocateInfo cmdAllocInfo{};
  cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdAllocInfo.commandPool = m_commandPool;
  cmdAllocInfo.commandBufferCount = 1;

  VkCommandBuffer tempCmdBuffer;
  vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &tempCmdBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(tempCmdBuffer, &beginInfo);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = newImage;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1; // Transitioning level by level

  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  vkCmdPipelineBarrier(tempCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  // 2. Copy the uncompressed staging buffer contents directly into Mip Level 0
  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {image_data->width, image_data->height, 1};

  vkCmdCopyBufferToImage(tempCmdBuffer, stagingBuffer, newImage,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  // 3. Downsample level by level using vkCmdBlitImage
  int32_t mipWidth = image_data->width;
  int32_t mipHeight = image_data->height;

  for (uint32_t i = 1; i < mipLevels; i++) {
    // Transition previous level (i - 1) to TRANSFER_SRC so we can read from it
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(tempCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    // Setup blit coordinates (downscaling by half each step)
    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.layerCount = 1;

    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                          mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.layerCount = 1;

    // Transition current destination level (i) from Undefined to TRANSFER_DST
    barrier.subresourceRange.baseMipLevel = i;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(tempCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    // Execute the linear blit on the GPU graphics queue
    vkCmdBlitImage(
        tempCmdBuffer, newImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, newImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(tempCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

    if (mipWidth > 1)
      mipWidth /= 2;
    if (mipHeight > 1)
      mipHeight /= 2;
  }

  // Finally, transition the very last remaining mip level to SHADER_READ_ONLY
  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(tempCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkEndCommandBuffer(tempCmdBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &tempCmdBuffer;

  vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_graphicsQueue);

  vkFreeCommandBuffers(m_device, m_commandPool, 1, &tempCmdBuffer);
  vkDestroyBuffer(m_device, stagingBuffer, nullptr);
  vkFreeMemory(m_device, stagingBufferMemory, nullptr);
  // =================================================================
  // REPLACE UNTIL HERE
  // =================================================================
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = newImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(m_device, &viewInfo, nullptr, &newView) != VK_SUCCESS) {
    throw std::runtime_error(
        "failed to create image view for loaded texture asset!");
  }

  m_textureImages.push_back(newImage);
  m_textureMemories.push_back(newMemory);
  m_textureViews.push_back(newView);

  uint32_t textureId = static_cast<uint32_t>(m_textureViews.size() - 1);
  m_imagePathToIdCache[path] = textureId;
  m_descriptorDirty = true;

  return textureId;
}

} // namespace ui
