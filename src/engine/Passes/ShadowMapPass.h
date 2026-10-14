#pragma once

#include "engine/FramebufferAttachment.h"

struct SceneRenderData;
class GpuDevice;
class GpuAllocator;
class SamplerCache;

#include <memory>

class ShadowMapPass
{
public:
  struct AttachmentConfig;

  ShadowMapPass(const GpuDevice& device,
                const GpuAllocator& allocator,
                const SamplerCache& samplerCache,
                const AttachmentConfig& attachmentConfig);
  ~ShadowMapPass();

  ShadowMapPass(const ShadowMapPass&) = delete;
  ShadowMapPass(ShadowMapPass&& other) = delete;
  ShadowMapPass& operator=(const ShadowMapPass&) = delete;
  ShadowMapPass& operator=(ShadowMapPass&&) = delete;

  void draw(VkCommandBuffer commandBuffer, const SceneRenderData& scene);

  std::unique_ptr<FramebufferAttachment> m_directionalShadowMap = nullptr;
  std::unique_ptr<FramebufferAttachment> m_spotPointShadowAtlas = nullptr;

  VkFramebuffer m_directionalShadowMapFramebuffer;
  VkRenderPass m_shadowMapRenderPass;

  VkFramebuffer m_spotShadowMapFramebuffer;

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

  VkPipeline m_shadowMapPipeline;
  VkPipelineLayout m_shadowMapPipelineLayout;
  void createShadowMapPipeline();

  uint32_t m_directionalShadowmapSize;
  uint32_t m_spotPointShadowmapSize;

  const GpuDevice& m_device;
  const GpuAllocator& m_allocator;
  const SamplerCache& m_samplerCache;
};
