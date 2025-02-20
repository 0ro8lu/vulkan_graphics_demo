#ifndef _BLINN_PHONG_PASS_H_
#define _BLINN_PHONG_PASS_H_

#include "engine/FramebufferAttachment.h"
#include "engine/IPassHelper.h"
#include "engine/Scene.h"
#include "engine/VulkanSwapchain.h"

#include <vulkan/vulkan_core.h>

class BlinnPhongPass : public IPassHelper
{
public:
  BlinnPhongPass(VulkanContext* vkContext,
                 const std::array<AttachmentData, 16>& attachmentData,
                 const Scene& scene,
                 const uint32_t attachmentWidth,
                 const uint32_t attachmentHeight);
  ~BlinnPhongPass();

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
