#ifndef _HDR_PASS_H_
#define _HDR_PASS_H_

#include "IPassHelper.h"

class HDRPass : public IPassHelper
{
public:
  HDRPass(VulkanContext* vkContext,
          std::array<AttachmentData, 16> attachmentData,
          const Scene& scene,
          uint32_t attachmentWidth,
          uint32_t attachmentHeight);
  ~HDRPass();

  void draw(VulkanSwapchain* vkSwapchain, const Scene& scene) override;
  void recreateAttachments(
    int width,
    int height,
    std::array<AttachmentData, 16> attachmentData) override;
  void updateDescriptors(
    std::array<FramebufferAttachment*, 16> attachments) override;

  FramebufferAttachment* brightSpotsAttachment;
  FramebufferAttachment* intermediateBloomAttachment;

  VkFramebuffer bloomFramebuffer;

  VkRenderPass bloomRenderPass;
  VkRenderPass presentationRenderPass;

private:
  void createFrameBuffer(std::array<AttachmentData, 16> attachmentData);
  void createAttachments(uint32_t width, uint32_t height);

  void createRenderPass(std::array<AttachmentData, 16> attachmentData);

  VkDescriptorPool mainDescriptorPool;
  VkDescriptorSetLayout bloomDescriptorSetLayout;
  VkDescriptorSet brightPointDescriptorSet;
  VkDescriptorSet horizontalBloomDescriptorSet;
  VkDescriptorSet verticalBloomDescriptorSet;
  void createDescriptors();

  VkPipeline brightPointExtractionPipeline;
  VkPipelineLayout brightPointExtractionPipelineLayout;
  void createBrightPointExtractionPipeline();

  VkPipeline horizontalBloomPipeline;
  VkPipelineLayout horizontalBloomPipelineLayout;
  void createHorizontalBloomPipeline();

  VkPipeline verticalBloomPipeline;
  VkPipelineLayout verticalBloomPipelineLayout;
  void createVerticalBloomPipeline();

  VkPipeline compositionPipeline;
  VkPipelineLayout compositionPipelineLayout;
  void createCompositionPipeline();
};

#endif
