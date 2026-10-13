#pragma once

#include "engine/FramebufferAttachment.h"
#include "engine/FrameContext.h"

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

  void draw(const FrameContext& frameContext);
  void recreateAttachments(int width, int height);
  void updateDescriptors(
    const std::unique_ptr<FramebufferAttachment>& blinnPhongAttachment);

  std::unique_ptr<FramebufferAttachment> m_brightSpotsAttachment;
  std::unique_ptr<FramebufferAttachment> m_intermediateBloomAttachment;

  VkFramebuffer m_bloomFramebuffer;

  VkRenderPass m_bloomRenderPass;
  VkRenderPass m_presentationRenderPass;

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

  VkDescriptorPool m_mainDescriptorPool;
  VkDescriptorSetLayout m_bloomDescriptorSetLayout;
  VkDescriptorSet m_brightPointDescriptorSet;
  VkDescriptorSet m_horizontalBloomDescriptorSet;
  VkDescriptorSet m_verticalBloomDescriptorSet;
  void createDescriptors();

  VkPipeline m_brightPointExtractionPipeline;
  VkPipelineLayout m_brightPointExtractionPipelineLayout;

  VkPipeline m_horizontalBloomPipeline;
  VkPipelineLayout m_horizontalBloomPipelineLayout;

  VkPipeline m_verticalBloomPipeline;
  VkPipelineLayout m_verticalBloomPipelineLayout;

  VkPipeline m_compositionPipeline;
  VkPipelineLayout m_compositionPipelineLayout;
  void createPipelines();

  VulkanContext* m_vkContext;
};
