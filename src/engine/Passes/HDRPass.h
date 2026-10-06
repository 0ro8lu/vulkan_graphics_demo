#pragma once

#include "engine/FramebufferAttachment.h"
#include "engine/VulkanSwapchain.h"

class Scene;

#include <memory>

class HDRPass
{
public:
  struct AttachmentConfig;

  HDRPass(VulkanContext* vkContext, const AttachmentConfig& attachmentConfig);
  ~HDRPass();

  HDRPass(const HDRPass&) = delete;
  HDRPass(HDRPass&& other) = delete;
  HDRPass& operator=(const HDRPass&) = delete;
  HDRPass& operator=(HDRPass&&) = delete;

  void draw(VulkanSwapchain* vkSwapchain);
  void recreateAttachments(int width, int height);
  void updateDescriptors(
    const std::unique_ptr<FramebufferAttachment>& blinnPhongAttachment);

  std::unique_ptr<FramebufferAttachment> brightSpotsAttachment;
  std::unique_ptr<FramebufferAttachment> intermediateBloomAttachment;

  VkFramebuffer bloomFramebuffer;

  VkRenderPass bloomRenderPass;
  VkRenderPass presentationRenderPass;

  struct AttachmentConfig
  {
    VkFormat swapchainImageFormat;
    uint32_t width;
    uint32_t height;
  };

private:
  void createFrameBuffers();
  void createAttachments(uint32_t width, uint32_t height);

  void createRenderPass(VkFormat swapchainImageFormat);

  VkDescriptorPool mainDescriptorPool;
  VkDescriptorSetLayout bloomDescriptorSetLayout;
  VkDescriptorSet brightPointDescriptorSet;
  VkDescriptorSet horizontalBloomDescriptorSet;
  VkDescriptorSet verticalBloomDescriptorSet;
  void createDescriptors();

  VkPipeline brightPointExtractionPipeline;
  VkPipelineLayout brightPointExtractionPipelineLayout;

  VkPipeline horizontalBloomPipeline;
  VkPipelineLayout horizontalBloomPipelineLayout;

  VkPipeline verticalBloomPipeline;
  VkPipelineLayout verticalBloomPipelineLayout;

  VkPipeline compositionPipeline;
  VkPipelineLayout compositionPipelineLayout;
  void createPipelines();

  VulkanContext* vkContext;
};
