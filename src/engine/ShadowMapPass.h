#ifndef _SHADOW_MAP_PASS_H_
#define _SHADOW_MAP_PASS_H_

#include "IPassHelper.h"

class ShadowMapPass : public IPassHelper
{
public:
  ShadowMapPass(VulkanContext* vkContext,
                const std::array<AttachmentData, 16>& attachmentData,
                const Scene& scene,
                const uint32_t shadowMapWidth,
                const uint32_t shadowMapHeight);
  ~ShadowMapPass();

  void draw(VulkanSwapchain* vkSwapchain, const Scene& scene) override;
  void recreateAttachments(
    int width,
    int height,
    const std::array<AttachmentData, 16>& attachmentData) override {};
  void updateDescriptors(
    const std::array<FramebufferAttachment*, 16>& attachments) override {};

  FramebufferAttachment* directionalShadowMap;

  VkFramebuffer directionalShadowMapFramebuffer;
  VkRenderPass directionalShadowMapRenderPass;

private:
  void createShadowMaps(uint32_t width, uint32_t height);
  void createFrameBuffer(std::array<AttachmentData, 16> attachmentData);
  void createRenderPass(std::array<AttachmentData, 16> attachmentData);

  VkPipeline directionalShadowMapPipeline;
  VkPipelineLayout directionalShadowMapPipelineLayout;
  void createDirectionalShadowMapPipeline();
};

#endif
