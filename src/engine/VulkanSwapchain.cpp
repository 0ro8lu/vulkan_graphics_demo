#include "engine/VulkanSwapchain.h"

#include <algorithm>
#include <array>
#include <limits>

#include "engine/Error.h"
#include "engine/VulkanCheck.h"

VulkanSwapchain::VulkanSwapchain(Key,
                                 const GpuDevice& device,
                                 const GpuAllocator& allocator,
                                 VkSurfaceKHR surface,
                                 VkExtent2D initialExtent)
  : m_device(device)
  , m_allocator(allocator)
  , m_surface(surface)
{
  createVulkanSwapChain(initialExtent, VK_NULL_HANDLE);
}

VulkanSwapchain::~VulkanSwapchain()
{
  if (m_device.device() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_device.device());
  }

  cleanTransientResources();

  if (m_swapChain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(m_device.device(), m_swapChain, nullptr);
    m_swapChain = VK_NULL_HANDLE;
  }
}

SwapChainSupportDetails
VulkanSwapchain::querySwapChainSupport(VkPhysicalDevice device) const
{
  SwapChainSupportDetails details{};

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    device, m_surface, &details.capabilities);

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
    device, m_surface, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
      device, m_surface, &formatCount, details.formats.data());
  }

  uint32_t presentModeCount = 0;
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
    std::array<VkImageView, 1> attachments = {
      m_swapChainImageViews[i],
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = drawingPass;
    framebufferInfo.attachmentCount =
      static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = swapChainExtent.width;
    framebufferInfo.height = swapChainExtent.height;
    framebufferInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(m_device.device(),
                                 &framebufferInfo,
                                 nullptr,
                                 &swapChainFramebuffers[i]));
  }
}

std::optional<uint32_t>
VulkanSwapchain::acquireNextImage(VkSemaphore imageAvailableSemaphore)
{
  uint32_t imageIndex = 0;
  VkResult result = vkAcquireNextImageKHR(m_device.device(),
                                          m_swapChain,
                                          UINT64_MAX,
                                          imageAvailableSemaphore,
                                          VK_NULL_HANDLE,
                                          &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    return std::nullopt;
  } else if (result != VK_SUCCESS) {
    ReportFatalError("Failed to acquire swap chain image!");
  }

  return imageIndex;
}

bool
VulkanSwapchain::present(uint32_t imageIndex, VkSemaphore waitSemaphore)
{
  VkPresentInfoKHR presentInfo{
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &waitSemaphore,
    .swapchainCount = 1,
    .pSwapchains = &m_swapChain,
    .pImageIndices = &imageIndex,
  };

  VkResult result = vkQueuePresentKHR(m_device.presentQueue(), &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    return false;
  } else if (result != VK_SUCCESS) {
    ReportFatalError("Failed to present swap chain image!");
  }

  return true;
}

void
VulkanSwapchain::createVulkanSwapChain(VkExtent2D extent,
                                       VkSwapchainKHR oldSwapchain)
{
  SwapChainSupportDetails swapChainSupport =
    querySwapChainSupport(m_device.physicalDevice());

  VkSurfaceFormatKHR surfaceFormat =
    chooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode =
    chooseSwapPresentMode(swapChainSupport.presentModes);
  VkExtent2D actualExtent =
    chooseSwapExtent(swapChainSupport.capabilities, extent);

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
  createInfo.imageExtent = actualExtent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  auto queueFamilies = m_device.queueFamilies();
  uint32_t queueFamilyIndices[] = { queueFamilies.graphicsFamily.value(),
                                    queueFamilies.presentFamily.value() };

  if (queueFamilies.graphicsFamily != queueFamilies.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  }

  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = oldSwapchain;

  VK_CHECK(vkCreateSwapchainKHR(
    m_device.device(), &createInfo, nullptr, &m_swapChain));

  vkGetSwapchainImagesKHR(m_device.device(), m_swapChain, &imageCount, nullptr);
  m_swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(
    m_device.device(), m_swapChain, &imageCount, m_swapChainImages.data());

  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent = actualExtent;

  // Create image views for swapchain
  m_swapChainImageViews.resize(m_swapChainImages.size());

  for (size_t i = 0; i < m_swapChainImages.size(); i++) {
    m_swapChainImageViews[i] = m_device.createImageView(
      m_swapChainImages[i], swapChainImageFormat, 1, VK_IMAGE_ASPECT_COLOR_BIT);
  }

  // Create depth resources using GpuDevice::findSupportedFormat
  static constexpr auto kDepthCandidates = std::to_array<VkFormat>({
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
  });

  depthFormat = m_device.findSupportedFormat(
    kDepthCandidates,
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  m_depthImage =
    m_allocator.createImage(swapChainExtent.width,
                            swapChainExtent.height,
                            depthFormat,
                            1,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            m_depthAllocation);
  depthImageView = m_device.createImageView(
    m_depthImage, depthFormat, 1, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void
VulkanSwapchain::recreateSwapChain(VkExtent2D newExtent)
{
  if (newExtent.width == 0 || newExtent.height == 0) {
    return;
  }

  vkDeviceWaitIdle(m_device.device());

  VkSwapchainKHR oldSwapchain = m_swapChain;
  cleanTransientResources();

  createVulkanSwapChain(newExtent, oldSwapchain);

  if (oldSwapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(m_device.device(), oldSwapchain, nullptr);
  }

  createSwapChainFrameBuffer();
}

void
VulkanSwapchain::cleanTransientResources()
{
  for (auto framebuffer : swapChainFramebuffers) {
    vkDestroyFramebuffer(m_device.device(), framebuffer, nullptr);
  }
  swapChainFramebuffers.clear();

  for (auto imageView : m_swapChainImageViews) {
    vkDestroyImageView(m_device.device(), imageView, nullptr);
  }
  m_swapChainImageViews.clear();
  m_swapChainImages.clear();

  if (depthImageView != VK_NULL_HANDLE) {
    m_device.destroyImageView(depthImageView);
    depthImageView = VK_NULL_HANDLE;
  }

  if (m_depthImage != VK_NULL_HANDLE) {
    m_allocator.destroyImage(m_depthImage, m_depthAllocation);
    m_depthImage = VK_NULL_HANDLE;
    m_depthAllocation = VK_NULL_HANDLE;
  }
}

VkSurfaceFormatKHR
VulkanSwapchain::chooseSwapSurfaceFormat(
  std::span<const VkSurfaceFormatKHR> availableFormats)
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
  std::span<const VkPresentModeKHR> availablePresentModes)
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
                                  VkExtent2D requestedExtent)
{
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    VkExtent2D extent = capabilities.currentExtent;
    extent.width = std::clamp(extent.width,
                              capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
                               capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
    return extent;
  }

  return VkExtent2D{
    .width = std::clamp(requestedExtent.width,
                        capabilities.minImageExtent.width,
                        capabilities.maxImageExtent.width),
    .height = std::clamp(requestedExtent.height,
                         capabilities.minImageExtent.height,
                         capabilities.maxImageExtent.height),
  };
}
