#ifndef _GBUFF_PASS_H_
#define _GBUFF_PASS_H_

#include "engine/FramebufferAttachment.h"
#include "engine/Passes/IPassHelper.h"

#include "engine/Vertex.h"

class GBuffPass : public IPassHelper
{
public:
  GBuffPass(VulkanContext* vkContext,
            const std::array<AttachmentData, 16>& attachmentData,
            const Scene& scene,
            const uint32_t attachmentWidth,
            const uint32_t attachmentHeight);
  ~GBuffPass();

  void draw(VulkanSwapchain* vkSwapchain, const Scene& scene) override;
  void recreateAttachments(
    int width,
    int height,
    const std::array<AttachmentData, 16>& attachmentData) override;
  void updateDescriptors(
    const std::array<FramebufferAttachment*, 16>& attachments) override {};

  FramebufferAttachment* positionAttachment;
  FramebufferAttachment* normalAttachment;
  FramebufferAttachment* albedoAttachment;
  FramebufferAttachment* depthAttachment;

  VkFramebuffer gbufferFramebuffer;
  VkRenderPass renderPass;

private:
  void createFrameBuffer(std::array<AttachmentData, 16> attachmentData);
  void createAttachments(uint32_t width, uint32_t height);
  void createRenderPass(std::array<AttachmentData, 16> attachmentData);

  VkPipeline gbufferPipeline;
  VkPipelineLayout gbufferPipelineLayout;
  void createGBufferPipeline(const Scene& scene);
};

#endif
