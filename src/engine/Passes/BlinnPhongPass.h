#pragma once

#include "engine/FramebufferAttachment.h"

struct SceneRenderData;

#include <memory>

class GpuDevice;
class GpuAllocator;
class SamplerCache;

class BlinnPhongPass
{
public:
  struct AttachmentConfig;
  struct LayoutConfig;

  BlinnPhongPass(const GpuDevice& device,
                 const GpuAllocator& allocator,
                 const SamplerCache& samplerCache,
                 const AttachmentConfig& attachmentConfig,
                 const LayoutConfig& layoutConfig);
  ~BlinnPhongPass();

  BlinnPhongPass(const BlinnPhongPass&) = delete;
  BlinnPhongPass(BlinnPhongPass&& other) = delete;
  BlinnPhongPass& operator=(const BlinnPhongPass&) = delete;
  BlinnPhongPass& operator=(BlinnPhongPass&&) = delete;

  void draw(VkCommandBuffer commandBuffer,
            VkExtent2D extent,
            const SceneRenderData& scene,
            VkDescriptorSet cameraUBODescriptorset,
            VkDescriptorSet lightsUBODescriptorset,
            VkDescriptorSet shadowMapDescriptorSet);
  void recreateAttachments(int attachmentWidth,
                           int attachmentHeight,
                           VkImageView depthImageView);
  void updateDescriptors(
    const std::unique_ptr<FramebufferAttachment>& directionalShadowmap,
    const std::unique_ptr<FramebufferAttachment>& spotPointShadowAtlas,
    VkDescriptorSet shadowMapDescriptorSet);

  std::unique_ptr<FramebufferAttachment> m_hdrAttachment;

  VkFramebuffer m_hdrFramebuffer;
  VkRenderPass m_renderPass;

  struct AttachmentConfig
  {
    VkFormat depthFormat;
    VkImageView depthImageView;
    uint32_t width;
    uint32_t height;
  };

  struct LayoutConfig
  {
    VkDescriptorSetLayout cameraLayout;
    VkDescriptorSetLayout lightsLayout;
    VkDescriptorSetLayout directionalShadowmapLayout;
    // VkDescriptorSetLayout skyboxLayout;
  };

private:
  void createFrameBuffer(VkImageView depthImageView);
  void createAttachments(uint32_t attachmentWidth, uint32_t attachmentHeight);
  void createRenderPass(VkFormat depthImageFormat);

  VkPipeline m_blinnPhongPipeline;
  VkPipelineLayout m_blinnPhongPipelineLayout;

  VkPipeline m_skyboxPipeline;
  VkPipelineLayout m_skyboxPipelineLayout;

  VkPipeline m_lightCubesPipeline;
  VkPipelineLayout m_lightCubesPipelineLayout;

  void createPipelines(const LayoutConfig& layoutConfig);

  const GpuDevice& m_device;
  const GpuAllocator& m_allocator;
  const SamplerCache& m_samplerCache;
};
