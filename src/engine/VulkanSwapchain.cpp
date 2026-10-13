#include "engine/VulkanSwapchain.h"

#include <iostream>

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
  createCommandBuffers();
}

VulkanSwapchain::~VulkanSwapchain()
{
  if (m_vkContext && m_vkContext->logicalDevice != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_vkContext->logicalDevice);
  }

  cleanSwapChain();

  vkDestroySurfaceKHR(m_vkContext->instance, m_surface, nullptr);

  std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> cmdBuffers{};
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    cmdBuffers[i] = m_frameSync[i].commandBuffer;

    if (m_frameSync[i].imageAvailableSemaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_vkContext->logicalDevice,
                         m_frameSync[i].imageAvailableSemaphore,
                         nullptr);
    }
    if (m_frameSync[i].inFlightFence != VK_NULL_HANDLE) {
      vkDestroyFence(
        m_vkContext->logicalDevice, m_frameSync[i].inFlightFence, nullptr);
    }
  }

  if (m_vkContext->commandPool != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(m_vkContext->logicalDevice,
                         m_vkContext->commandPool,
                         static_cast<uint32_t>(cmdBuffers.size()),
                         cmdBuffers.data());
  }
}

void
VulkanSwapchain::createSurface(GLFWwindow* window)
{
  VK_CHECK(glfwCreateWindowSurface(
    m_vkContext->instance, window, nullptr, &m_surface));
}

SwapChainSupportDetails
VulkanSwapchain::querySwapChainSupport(VkPhysicalDevice device)
{
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    device, m_surface, &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
    device, m_surface, &formatCount, nullptr);

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
VulkanSwapchain::createCommandBuffers()
{
  std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> cmdBuffers{};

  VkCommandBufferAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = m_vkContext->commandPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
  };

  VK_CHECK(vkAllocateCommandBuffers(
    m_vkContext->logicalDevice, &allocInfo, cmdBuffers.data()));

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    m_frameSync[i].commandBuffer = cmdBuffers[i];
  }
}

std::optional<FrameContext>
VulkanSwapchain::beginFrame(GLFWwindow* window)
{
  if (resized) {
    resized = false;
    recreateSwapChain(window);
    return std::nullopt;
  }

  const auto& currentSync = m_frameSync[currentFrame];

  // 1. Wait for this in-flight frame slot's previous GPU execution to complete
  VK_CHECK(vkWaitForFences(m_vkContext->logicalDevice,
                           1,
                           &currentSync.inFlightFence,
                           VK_TRUE,
                           UINT64_MAX));

  // 2. Acquire next swapchain image
  VkResult result = vkAcquireNextImageKHR(m_vkContext->logicalDevice,
                                          m_swapChain,
                                          UINT64_MAX,
                                          currentSync.imageAvailableSemaphore,
                                          VK_NULL_HANDLE,
                                          &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain(window);
    return std::nullopt;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    std::cerr
      << "[VulkanSwapchain]: Failed to acquire swap chain image! Result: "
      << result << '\n';
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  // 3. Reset and begin command buffer recording for this frame.
  // Note: inFlightFence is reset in submitFrame immediately prior to vkQueueSubmit
  // to avoid deadlock hazards if frame rendering is aborted before submission.
  VK_CHECK(vkResetCommandBuffer(currentSync.commandBuffer, 0));

  VkCommandBufferBeginInfo beginInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
  VK_CHECK(vkBeginCommandBuffer(currentSync.commandBuffer, &beginInfo));

  return FrameContext{
    .commandBuffer = currentSync.commandBuffer,
    .extent = swapChainExtent,
    .backbuffer = swapChainFramebuffers[imageIndex],
    .currentFrame = currentFrame,
    .imageIndex = imageIndex,
  };
}

void
VulkanSwapchain::submitFrame(const FrameContext& frameContext,
                             GLFWwindow* window)
{
  const auto& currentSync = m_frameSync[frameContext.currentFrame];

  VK_CHECK(vkEndCommandBuffer(frameContext.commandBuffer));

  VkSubmitInfo submitInfo{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
  };

  VkPipelineStageFlags waitStages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &currentSync.imageAvailableSemaphore;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &frameContext.commandBuffer;

  VkSemaphore signalSemaphores[] = {
    m_renderFinishedSemaphores[frameContext.imageIndex]
  };
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  // Reset fence immediately prior to queue submission.
  VK_CHECK(vkResetFences(
    m_vkContext->logicalDevice, 1, &currentSync.inFlightFence));

  VK_CHECK(vkQueueSubmit(
    m_vkContext->graphicsQueue, 1, &submitInfo, currentSync.inFlightFence));

  VkPresentInfoKHR presentInfo{
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = signalSemaphores,
    .swapchainCount = 1,
    .pSwapchains = &m_swapChain,
    .pImageIndices = &frameContext.imageIndex,
  };

  VkResult result = vkQueuePresentKHR(m_vkContext->presentQueue, &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      resized) {
    resized = false;
    recreateSwapChain(window);
  } else if (result != VK_SUCCESS) {
    std::cerr
      << "[VulkanSwapchain]: Failed to present swap chain image! Result: "
      << result << '\n';
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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
  vkGetSwapchainImagesKHR(m_vkContext->logicalDevice,
                          m_swapChain,
                          &imageCount,
                          m_swapChainImages.data());

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

  // Semaphores used with vkQueuePresentKHR must be indexed per swapchain image
  // because presentation engine usage is only retired upon re-acquiring the image.
  m_renderFinishedSemaphores.resize(m_swapChainImages.size());
  VkSemaphoreCreateInfo semaphoreInfo{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  for (size_t i = 0; i < m_swapChainImages.size(); i++) {
    VK_CHECK(vkCreateSemaphore(m_vkContext->logicalDevice,
                               &semaphoreInfo,
                               nullptr,
                               &m_renderFinishedSemaphores[i]));
  }
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
  swapChainFramebuffers.clear();

  // cleanup swapchain
  for (auto imageView : m_swapChainImageViews) {
    vkDestroyImageView(m_vkContext->logicalDevice, imageView, nullptr);
  }
  m_swapChainImageViews.clear();
  m_swapChainImages.clear();

  m_vkContext->destroyImageView(depthImageView);
  depthImageView = VK_NULL_HANDLE;
  m_vkContext->destroyImage(m_depthImage, m_depthAllocation);
  m_depthImage = VK_NULL_HANDLE;
  m_depthAllocation = VK_NULL_HANDLE;

  for (auto semaphore : m_renderFinishedSemaphores) {
    vkDestroySemaphore(m_vkContext->logicalDevice, semaphore, nullptr);
  }
  m_renderFinishedSemaphores.clear();

  vkDestroySwapchainKHR(m_vkContext->logicalDevice, m_swapChain, nullptr);
  m_swapChain = VK_NULL_HANDLE;
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

  std::cerr
    << "[VulkanSwapchain]: Failed to find supported format among candidates!\n";
  ENGINE_DEBUG_BREAK();
  std::abort();
}

void
VulkanSwapchain::createSyncObjects()
{
  VkSemaphoreCreateInfo semaphoreInfo{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fenceInfo{
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK(vkCreateSemaphore(m_vkContext->logicalDevice,
                               &semaphoreInfo,
                               nullptr,
                               &m_frameSync[i].imageAvailableSemaphore));
    VK_CHECK(vkCreateFence(m_vkContext->logicalDevice,
                           &fenceInfo,
                           nullptr,
                           &m_frameSync[i].inFlightFence));
  }
}
