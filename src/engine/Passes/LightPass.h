#ifndef _LIGHT_PASS_H_
#define _LIGHT_PASS_H_

#include "engine/Passes/IPassHelper.h"
#include <vulkan/vulkan_core.h>

class LightPass : public IPassHelper
{
public:
  LightPass(VulkanContext* vkContext,
            const std::array<AttachmentData, 16>& attachmentData,
            const Scene& scene,
            const uint32_t attachmentWidth,
            const uint32_t attachmentHeight);
  ~LightPass();

  void draw(VulkanSwapchain* vkSwapchain, const Scene& scene) override;
  void recreateAttachments(
    int width,
    int height,
    const std::array<AttachmentData, 16>& attachmentData) override;
  void updateDescriptors(
    const std::array<FramebufferAttachment*, 16>& attachments) override;

  FramebufferAttachment* hdrAttachment;

  VkFramebuffer hdrFramebuffer;
  VkRenderPass renderPass;

private:
  void createFrameBuffer(std::array<AttachmentData, 16> attachmentData);
  void createAttachments(uint32_t width, uint32_t height);
  void createRenderPass(std::array<AttachmentData, 16> attachmentData);

  VkDescriptorPool descriptorPool;
  VkDescriptorSetLayout gbufferDescriptorLayout;
  VkDescriptorSet gbufferDescriptorSet;
  void createDescriptors();

  VkPipeline blinnPhongPipeline;
  VkPipelineLayout blinnPhongPipelineLayout;
  void createMainPipeline(const Scene& scene);

  VkPipeline skyboxPipeline;
  VkPipelineLayout skyboxPipelineLayout;
  void createSkyboxPipeline(const Scene& scene);

  VkPipeline lightCubesPipeline;
  VkPipelineLayout lightCubesPipelineLayout;
  void createLightCubesPipeline(const Scene& scene);
};

#endif
