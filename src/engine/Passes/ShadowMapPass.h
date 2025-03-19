#ifndef _SHADOW_MAP_PASS_H_
#define _SHADOW_MAP_PASS_H_

#include "engine/FramebufferAttachment.h"
#include "engine/Passes/IPassHelper.h"

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

  FramebufferAttachment* directionalShadowMap = nullptr;
  FramebufferAttachment* spotPointShadowAtlas = nullptr;

  VkFramebuffer directionalShadowMapFramebuffer;
  VkRenderPass shadowMapRenderPass;

  VkFramebuffer spotShadowMapFramebuffer;

private:
  void createShadowMaps(uint32_t width, uint32_t height);

  void createFrameBuffers(std::array<AttachmentData, 16> attachmentData);

  void createDirectionalRenderPass(
    std::array<AttachmentData, 16> attachmentData);
  // void createSpotRenderPass(std::array<AttachmentData, 16> attachmentData);

  VkPipeline shadowMapPipeline;
  VkPipelineLayout shadowMapPipelineLayout;
  void createShadowMapPipeline();

  // VkPipeline spotShadowMapPipeline;
  // VkPipelineLayout spotShadowMapPipelineLayout;
  // void createSpotShadowMapPipeline();

  uint32_t shadowMapWidth;
  uint32_t shadowMapHeight;
};

#endif
