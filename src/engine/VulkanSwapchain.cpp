#include "engine/VulkanSwapchain.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

#include "GLFW/glfw3.h"
#include "engine/VulkanQueueFamiliesHelper.h"

void
VulkanSwapchain::framebufferResizeCallback(GLFWwindow* window,
                                           int width,
                                           int height)
{
  auto vulkanSwapchainPtr = (VulkanSwapchain*)glfwGetWindowUserPointer(window);
  vulkanSwapchainPtr->resized = true;
}

VulkanSwapchain::VulkanSwapchain(VulkanContext* vkContext, GLFWwindow* window)
  : window(window)
  , vkContext(vkContext)
{
  glfwGetFramebufferSize(window, &width, &height);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

VulkanSwapchain::~VulkanSwapchain()
{
  cleanSwapChain();

  vkDestroySurfaceKHR(vkContext->instance, surface, nullptr);

  vkDestroySemaphore(
    vkContext->logicalDevice, renderFinishedSemaphore, nullptr);
  vkDestroySemaphore(
    vkContext->logicalDevice, imageAvailableSemaphore, nullptr);
  vkDestroyFence(vkContext->logicalDevice, inFlightFence, nullptr);
}

void
VulkanSwapchain::createSurface()
{
  if (glfwCreateWindowSurface(vkContext->instance, window, nullptr, &surface) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
}

SwapChainSupportDetails
VulkanSwapchain::querySwapChainSupport(VkPhysicalDevice device)
{
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    device, surface, &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
      device, surface, &formatCount, details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(
    device, surface, &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      device, surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

void
VulkanSwapchain::createSwapChainFrameBuffer()
{
  swapChainFramebuffers.resize(swapChainImageViews.size());

  for (size_t i = 0; i < swapChainImageViews.size(); i++) {
    std::array<VkImageView, 1> attachments = { swapChainImageViews[i] };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = drawingPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = swapChainExtent.width;
    framebufferInfo.height = swapChainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(vkContext->logicalDevice,
                            &framebufferInfo,
                            nullptr,
                            &swapChainFramebuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create framebuffer!");
    }
  }
}

void
VulkanSwapchain::createCommandBuffer()
{
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = vkContext->commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  if (vkAllocateCommandBuffers(
        vkContext->logicalDevice, &allocInfo, &commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void
VulkanSwapchain::prepareFrame()
{
  vkWaitForFences(
    vkContext->logicalDevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

  VkResult result = vkAcquireNextImageKHR(vkContext->logicalDevice,
                                          swapChain,
                                          UINT64_MAX,
                                          imageAvailableSemaphore,
                                          VK_NULL_HANDLE,
                                          &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain();
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  vkResetFences(vkContext->logicalDevice, 1, &inFlightFence);
  vkResetCommandBuffer(commandBuffer, 0);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }
}

void
VulkanSwapchain::submitFrame()
{
  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
  VkPipelineStageFlags waitStages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(vkContext->graphicsQueue, 1, &submitInfo, inFlightFence) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to submit draw command buffer!");
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = { swapChain };
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = &imageIndex;

  VkResult result = vkQueuePresentKHR(vkContext->presentQueue, &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      resized == true) {
    resized = false;
    recreateSwapChain();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }
}

void
VulkanSwapchain::createSwapChain()
{
  SwapChainSupportDetails swapChainSupport =
    querySwapChainSupport(vkContext->physicalDevice);

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
  createInfo.surface = surface;

  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices =
    QueueFamilyIndices::findQueueFamilies(vkContext->physicalDevice, surface);
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

  if (vkCreateSwapchainKHR(
        vkContext->logicalDevice, &createInfo, nullptr, &swapChain) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain!");
  }

  vkGetSwapchainImagesKHR(
    vkContext->logicalDevice, swapChain, &imageCount, nullptr);
  swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(
    vkContext->logicalDevice, swapChain, &imageCount, swapChainImages.data());

  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent = extent;

  // finally create image views for swapchain
  swapChainImageViews.resize(swapChainImages.size());

  for (size_t i = 0; i < swapChainImages.size(); i++) {
    swapChainImageViews[i] = vkContext->createImageView(
      swapChainImages[i], swapChainImageFormat, 1, VK_IMAGE_ASPECT_COLOR_BIT);
  }

  // create depth resources
  depthFormat = vkContext->findSupportedFormat(
    { VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT },
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  depthImage =
    vkContext->createImage(swapChainExtent.width,
                           swapChainExtent.height,
                           depthFormat,
                           1,
                           VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                           VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                           depthAllocation);
  depthImageView = vkContext->createImageView(
    depthImage, depthFormat, 1, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void
VulkanSwapchain::recreateSwapChain()
{
  int width = 0, height = 0;
  glfwGetFramebufferSize(window, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(vkContext->logicalDevice);
  cleanSwapChain();
  createSwapChain();
  createSwapChainFrameBuffer();

  onResize(swapChainExtent.width, swapChainExtent.height);
  onResize(width, height);
}

void
VulkanSwapchain::cleanSwapChain()
{
  for (auto framebuffer : swapChainFramebuffers) {
    vkDestroyFramebuffer(vkContext->logicalDevice, framebuffer, nullptr);
  }
  // cleanup swapchain
  for (auto imageView : swapChainImageViews) {
    vkDestroyImageView(vkContext->logicalDevice, imageView, nullptr);
  }

  vmaDestroyImage(vkContext->allocator, depthImage, depthAllocation);
  vkDestroyImageView(vkContext->logicalDevice, depthImageView, nullptr);

  vkDestroySwapchainKHR(vkContext->logicalDevice, swapChain, nullptr);
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

void
VulkanSwapchain::createSyncObjects()
{
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  if (vkCreateSemaphore(vkContext->logicalDevice,
                        &semaphoreInfo,
                        nullptr,
                        &imageAvailableSemaphore) != VK_SUCCESS ||
      vkCreateSemaphore(vkContext->logicalDevice,
                        &semaphoreInfo,
                        nullptr,
                        &renderFinishedSemaphore) != VK_SUCCESS ||
      vkCreateFence(
        vkContext->logicalDevice, &fenceInfo, nullptr, &inFlightFence) !=
        VK_SUCCESS) {
    throw std::runtime_error(
      "failed to create synchronization objects for a frame!");
  }
}
