#include "engine/VulkanSwapchain.h"

#include <algorithm>
#include <array>
#include <stdexcept>

#include "GLFW/glfw3.h"
#include "engine/VulkanCheck.h"

void
VulkanSwapchain::framebufferResizeCallback(GLFWwindow* window, int, int)
{
  auto vulkanSwapchainPtr =
    static_cast<VulkanSwapchain*>(glfwGetWindowUserPointer(window));
  vulkanSwapchainPtr->resized = true;
}

VulkanSwapchain::VulkanSwapchain(Key,
                                 VulkanContext* vkContext,
                                 GLFWwindow* window)
  : m_vkContext(vkContext)
{
  glfwGetFramebufferSize(window, &width, &height);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

  createSurface(window);
  createVulkanSwapChain(window);
  createSyncObjects();
  createCommandBuffer();
}

VulkanSwapchain::~VulkanSwapchain()
{
  cleanSwapChain();

  vkDestroySurfaceKHR(m_vkContext->instance, m_surface, nullptr);

  for (size_t i = 0; i < m_swapChainImages.size(); i++) {
    vkDestroySemaphore(
      m_vkContext->logicalDevice, m_imageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(
      m_vkContext->logicalDevice, m_renderFinishedSemaphores[i], nullptr);
  }
  vkDestroyFence(m_vkContext->logicalDevice, m_inFlightFence, nullptr);
}

void
VulkanSwapchain::createSurface(GLFWwindow* window)
{
  VK_CHECK(
    glfwCreateWindowSurface(m_vkContext->instance, window, nullptr, &m_surface));
}

SwapChainSupportDetails
VulkanSwapchain::querySwapChainSupport(VkPhysicalDevice device)
{
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    device, m_surface, &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
      device, m_surface, &formatCount, details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(
    device, m_surface, &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      device, m_surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

void
VulkanSwapchain::createSwapChainFrameBuffer()
{
  swapChainFramebuffers.resize(m_swapChainImageViews.size());

  for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
    std::array<VkImageView, 1> attachments = { m_swapChainImageViews[i] };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = drawingPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = swapChainExtent.width;
    framebufferInfo.height = swapChainExtent.height;
    framebufferInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(m_vkContext->logicalDevice,
                                 &framebufferInfo,
                                 nullptr,
                                 &swapChainFramebuffers[i]));
  }
}

void
VulkanSwapchain::createCommandBuffer()
{
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_vkContext->commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  VK_CHECK(vkAllocateCommandBuffers(
    m_vkContext->logicalDevice, &allocInfo, &commandBuffer));
}

void
VulkanSwapchain::prepareFrame(GLFWwindow* window)
{
  vkWaitForFences(
    m_vkContext->logicalDevice, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);

  VkResult result =
    vkAcquireNextImageKHR(m_vkContext->logicalDevice,
                          m_swapChain,
                          UINT64_MAX,
                          m_imageAvailableSemaphores[currentFrame],
                          VK_NULL_HANDLE,
                          &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain(window);
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  vkResetFences(m_vkContext->logicalDevice, 1, &m_inFlightFence);
  vkResetCommandBuffer(commandBuffer, 0);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
}

void
VulkanSwapchain::submitFrame(GLFWwindow* window)
{
  VK_CHECK(vkEndCommandBuffer(commandBuffer));

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkPipelineStageFlags waitStages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[currentFrame];
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[currentFrame] };
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VK_CHECK(
    vkQueueSubmit(m_vkContext->graphicsQueue, 1, &submitInfo, m_inFlightFence));

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = { m_swapChain };
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = &imageIndex;

  VkResult result = vkQueuePresentKHR(m_vkContext->presentQueue, &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
    resized) {
    resized = false;
    recreateSwapChain(window);
  } else if (result != VK_SUCCESS) {
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  currentFrame = (currentFrame + 1) % m_imageAvailableSemaphores.size();
}

void
VulkanSwapchain::createVulkanSwapChain(GLFWwindow* window)
{
  SwapChainSupportDetails swapChainSupport =
    querySwapChainSupport(m_vkContext->physicalDevice);

  VkSurfaceFormatKHR surfaceFormat =
    chooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode =
    chooseSwapPresentMode(swapChainSupport.presentModes);
  VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
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

  QueueFamilyIndices indices = m_vkContext->queueFamilies;
  uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),
                                    indices.presentFamily.value() };

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  VK_CHECK(vkCreateSwapchainKHR(
    m_vkContext->logicalDevice, &createInfo, nullptr, &m_swapChain));

  vkGetSwapchainImagesKHR(
    m_vkContext->logicalDevice, m_swapChain, &imageCount, nullptr);
  m_swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(
    m_vkContext->logicalDevice, m_swapChain, &imageCount, m_swapChainImages.data());

  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent = extent;

  // finally create image views for swapchain
  m_swapChainImageViews.resize(m_swapChainImages.size());

  for (size_t i = 0; i < m_swapChainImages.size(); i++) {
    m_swapChainImageViews[i] = m_vkContext->createImageView(
      m_swapChainImages[i], swapChainImageFormat, 1, VK_IMAGE_ASPECT_COLOR_BIT);
  }

  // create depth resources
  depthFormat =
    findSupportedFormat({ VK_FORMAT_D32_SFLOAT,
                          VK_FORMAT_D32_SFLOAT_S8_UINT,
                          VK_FORMAT_D24_UNORM_S8_UINT },
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  m_depthImage =
    m_vkContext->createImage(swapChainExtent.width,
                             swapChainExtent.height,
                             depthFormat,
                             1,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             m_depthAllocation);
  depthImageView = m_vkContext->createImageView(
    m_depthImage, depthFormat, 1, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void
VulkanSwapchain::recreateSwapChain(GLFWwindow* window)
{
  int width = 0, height = 0;
  glfwGetFramebufferSize(window, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(m_vkContext->logicalDevice);
  cleanSwapChain();
  createVulkanSwapChain(window);
  createSwapChainFrameBuffer();

  onResize(swapChainExtent.width, swapChainExtent.height);
}

void
VulkanSwapchain::cleanSwapChain()
{
  for (auto framebuffer : swapChainFramebuffers) {
    vkDestroyFramebuffer(m_vkContext->logicalDevice, framebuffer, nullptr);
  }
  // cleanup swapchain
  for (auto imageView : m_swapChainImageViews) {
    vkDestroyImageView(m_vkContext->logicalDevice, imageView, nullptr);
  }

  m_vkContext->destroyImageView(depthImageView);
  m_vkContext->destroyImage(m_depthImage, m_depthAllocation);

  vkDestroySwapchainKHR(m_vkContext->logicalDevice, m_swapChain, nullptr);
}

VkSurfaceFormatKHR
VulkanSwapchain::chooseSwapSurfaceFormat(
  const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
  for (const auto& availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

VkPresentModeKHR
VulkanSwapchain::chooseSwapPresentMode(
  const std::vector<VkPresentModeKHR>& availablePresentModes)
{
  for (const auto& availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
VulkanSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                  GLFWwindow* window)
{
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent = { static_cast<uint32_t>(width),
                                static_cast<uint32_t>(height) };

    actualExtent.width = std::clamp(actualExtent.width,
                                    capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
                                     capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

VkFormat
VulkanSwapchain::findSupportedFormat(const std::vector<VkFormat>& candidates,
                                     VkImageTiling tiling,
                                     VkFormatFeatureFlags features)
{
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(
      m_vkContext->physicalDevice, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  ENGINE_DEBUG_BREAK();
  std::abort();
}

void
VulkanSwapchain::createSyncObjects()
{
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  m_imageAvailableSemaphores.resize(m_swapChainImages.size());
  m_renderFinishedSemaphores.resize(m_swapChainImages.size());
  for (size_t i = 0; i < m_swapChainImages.size(); i++) {
    VK_CHECK(vkCreateSemaphore(m_vkContext->logicalDevice,
                               &semaphoreInfo,
                               nullptr,
                               &m_imageAvailableSemaphores[i]));
    VK_CHECK(vkCreateSemaphore(m_vkContext->logicalDevice,
                               &semaphoreInfo,
                               nullptr,
                               &m_renderFinishedSemaphores[i]));
  }

  VK_CHECK(vkCreateFence(
    m_vkContext->logicalDevice, &fenceInfo, nullptr, &m_inFlightFence));
}
