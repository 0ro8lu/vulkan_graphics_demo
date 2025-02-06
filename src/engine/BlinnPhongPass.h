#ifndef _BLINN_PHONG_PASS_H_
#define _BLINN_PHONG_PASS_H_

#include "engine/IPassHelper.h"
#include "engine/Scene.h"
#include "engine/VulkanSwapchain.h"

#include <vulkan/vulkan_core.h>

class BlinnPhongPass : public IPassHelper
{
public:
  BlinnPhongPass(VulkanContext* vkContext);
  ~BlinnPhongPass();

  void init(VulkanSwapchain* vulkanSwapchain, const Scene& scene) override;
  void draw(VulkanSwapchain* vkSwapchain, const Scene& scene) override;

  VkRenderPass renderPass;

private:
  void createRenderPass(VulkanSwapchain* vkSwapchain);

  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;
  void createPipeline(const Scene& scene);

  // std::vector<VkDescriptorSet> descriptorSets;
};

#endif
