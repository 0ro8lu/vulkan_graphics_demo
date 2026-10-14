#include "engine/FrameScheduler.h"

#include "engine/Error.h"
#include "engine/GpuDevice.h"
#include "engine/VulkanCheck.h"
#include "engine/VulkanSwapchain.h"

FrameScheduler::FrameScheduler(const GpuDevice& device)
  : m_device(device)
{
  createCommandBuffers();
  createSyncObjects();
}

FrameScheduler::~FrameScheduler()
{
  if (m_device.device() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_device.device());
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (m_slots[i].imageAvailableSemaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(
        m_device.device(), m_slots[i].imageAvailableSemaphore, nullptr);
      m_slots[i].imageAvailableSemaphore = VK_NULL_HANDLE;
    }
    if (m_slots[i].inFlightFence != VK_NULL_HANDLE) {
      vkDestroyFence(m_device.device(), m_slots[i].inFlightFence, nullptr);
      m_slots[i].inFlightFence = VK_NULL_HANDLE;
    }
  }

  for (auto semaphore : m_renderFinishedSemaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_device.device(), semaphore, nullptr);
    }
  }
  m_renderFinishedSemaphores.clear();

  if (m_commandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_device.device(), m_commandPool, nullptr);
    m_commandPool = VK_NULL_HANDLE;
  }
}

void
FrameScheduler::createCommandBuffers()
{
  VkCommandPoolCreateInfo poolInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = m_device.queueFamilies().graphicsFamily.value(),
  };

  VK_CHECK(vkCreateCommandPool(
    m_device.device(), &poolInfo, nullptr, &m_commandPool));

  std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> cmdBuffers{};
  VkCommandBufferAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = m_commandPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
  };

  VK_CHECK(
    vkAllocateCommandBuffers(m_device.device(), &allocInfo, cmdBuffers.data()));

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    m_slots[i].commandBuffer = cmdBuffers[i];
  }
}

void
FrameScheduler::createSyncObjects()
{
  VkSemaphoreCreateInfo semaphoreInfo{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  VkFenceCreateInfo fenceInfo{
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK(vkCreateSemaphore(m_device.device(),
                               &semaphoreInfo,
                               nullptr,
                               &m_slots[i].imageAvailableSemaphore));

    VK_CHECK(vkCreateFence(
      m_device.device(), &fenceInfo, nullptr, &m_slots[i].inFlightFence));
  }
}

void
FrameScheduler::ensureRenderFinishedSemaphores(size_t imageCount)
{
  if (m_renderFinishedSemaphores.size() == imageCount) {
    return;
  }

  for (auto semaphore : m_renderFinishedSemaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_device.device(), semaphore, nullptr);
    }
  }
  m_renderFinishedSemaphores.clear();
  m_renderFinishedSemaphores.resize(imageCount, VK_NULL_HANDLE);

  VkSemaphoreCreateInfo semaphoreInfo{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  for (size_t i = 0; i < imageCount; ++i) {
    VK_CHECK(vkCreateSemaphore(m_device.device(),
                               &semaphoreInfo,
                               nullptr,
                               &m_renderFinishedSemaphores[i]));
  }
}

std::optional<FrameContext>
FrameScheduler::beginFrame(VulkanSwapchain& swapchain)
{
  ensureRenderFinishedSemaphores(swapchain.imageCount());

  const auto& currentSlot = m_slots[m_currentFrame];

  // 1. Wait for this in-flight slot's previous GPU execution to complete
  VK_CHECK(vkWaitForFences(
    m_device.device(), 1, &currentSlot.inFlightFence, VK_TRUE, UINT64_MAX));

  // 2. Acquire next swapchain image
  auto imageIndexOpt =
    swapchain.acquireNextImage(currentSlot.imageAvailableSemaphore);
  if (!imageIndexOpt) {
    return std::nullopt;
  }
  const uint32_t imageIndex = *imageIndexOpt;

  // 3. Reset and begin command buffer recording for this frame
  VK_CHECK(vkResetCommandBuffer(currentSlot.commandBuffer, 0));

  VkCommandBufferBeginInfo beginInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
  VK_CHECK(vkBeginCommandBuffer(currentSlot.commandBuffer, &beginInfo));

  return FrameContext{
    .commandBuffer = currentSlot.commandBuffer,
    .extent = swapchain.extent(),
    .backbuffer = swapchain.framebuffer(imageIndex),
    .currentFrame = m_currentFrame,
    .imageIndex = imageIndex,
  };
}

bool
FrameScheduler::submitFrame(VulkanSwapchain& swapchain,
                            const FrameContext& frameContext)
{
  const auto& currentSlot = m_slots[frameContext.currentFrame];

  VK_CHECK(vkEndCommandBuffer(frameContext.commandBuffer));

  const VkPipelineStageFlags waitStages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  const VkSemaphore waitSemaphores[] = {
    currentSlot.imageAvailableSemaphore
  };
  const VkSemaphore signalSemaphores[] = {
    m_renderFinishedSemaphores[frameContext.imageIndex]
  };

  VkSubmitInfo submitInfo{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = waitSemaphores,
    .pWaitDstStageMask = waitStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &frameContext.commandBuffer,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = signalSemaphores,
  };

  // Reset fence immediately prior to queue submission
  VK_CHECK(vkResetFences(m_device.device(), 1, &currentSlot.inFlightFence));

  VK_CHECK(vkQueueSubmit(
    m_device.graphicsQueue(), 1, &submitInfo, currentSlot.inFlightFence));

  bool presentOk = swapchain.present(
    frameContext.imageIndex,
    m_renderFinishedSemaphores[frameContext.imageIndex]);

  m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  return presentOk;
}
