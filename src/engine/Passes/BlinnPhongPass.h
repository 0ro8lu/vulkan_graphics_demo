#pragma once

#include "engine/FramebufferAttachment.h"
#include "engine/VulkanSwapchain.h"

class Scene;

#include <memory>

class BlinnPhongPass
{
public:
  struct AttachmentConfig;
  struct LayoutConfig;

  BlinnPhongPass(VulkanContext* vkContext,
                 const AttachmentConfig& attachmentConfig,
                 const LayoutConfig& layoutConfig);
  ~BlinnPhongPass();

  BlinnPhongPass(const BlinnPhongPass&) = delete;
  BlinnPhongPass(BlinnPhongPass&& other) = delete;
  BlinnPhongPass& operator=(const BlinnPhongPass&) = delete;
  BlinnPhongPass& operator=(BlinnPhongPass&&) = delete;

  void draw(VulkanSwapchain* vkSwapchain,
            const Scene& scene,
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

  std::unique_ptr<FramebufferAttachment> hdrAttachment;

  VkFramebuffer hdrFramebuffer;
  VkRenderPass renderPass;

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

  VkPipeline blinnPhongPipeline;
  VkPipelineLayout blinnPhongPipelineLayout;

  VkPipeline skyboxPipeline;
  VkPipelineLayout skyboxPipelineLayout;

  VkPipeline lightCubesPipeline;
  VkPipelineLayout lightCubesPipelineLayout;

  void createPipelines(const LayoutConfig& layoutConfig);

  VulkanContext* vkContext;
};
