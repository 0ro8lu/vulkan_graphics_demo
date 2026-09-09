#pragma once

#include "engine/FramebufferAttachment.h"
#include "engine/VulkanSwapchain.h"

class Scene;

#include <memory>

struct SMPAttachmentConfig;

class ShadowMapPass
{
public:
  struct AttachmentConfig;

  ShadowMapPass(VulkanContext* vkContext,
                const AttachmentConfig& attachmentConfig);
  ~ShadowMapPass();

  ShadowMapPass(const ShadowMapPass&) = delete;
  ShadowMapPass(ShadowMapPass&& other) = delete;
  ShadowMapPass& operator=(const ShadowMapPass&) = delete;
  ShadowMapPass& operator=(ShadowMapPass&&) = delete;

  void draw(VulkanSwapchain* vkSwapchain, const Scene& scene);

  std::unique_ptr<FramebufferAttachment> directionalShadowMap = nullptr;
  std::unique_ptr<FramebufferAttachment> spotPointShadowAtlas = nullptr;

  VkFramebuffer directionalShadowMapFramebuffer;
  VkRenderPass shadowMapRenderPass;

  VkFramebuffer spotShadowMapFramebuffer;

  struct AttachmentConfig
  {
    VkFormat depthFormat;
    uint32_t directionalAtlasSize;
    uint32_t spotPointAtlasSize;
  };

private:
  void createShadowMaps(VkFormat depthImageFormat);

  void createFrameBuffers();

  void createDirectionalRenderPass();

  // void createSpotRenderPass(std::array<AttachmentData, 16> attachmentData);

  VkPipeline shadowMapPipeline;
  VkPipelineLayout shadowMapPipelineLayout;
  void createShadowMapPipeline();

  // VkPipeline spotShadowMapPipeline;
  // VkPipelineLayout spotShadowMapPipelineLayout;
  // void createSpotShadowMapPipeline();

  uint32_t directionalShadowmapSize;
  uint32_t spotPointShadowmapSize;

  VulkanContext* vkContext;
};

