#ifndef _RENDERER_H_
#define _RENDERER_H_

#include "engine/Passes/BlinnPhongPass.h"
#include "engine/Passes/GBuffPass.h"
#include "engine/Passes/LightPass.h"
#include "engine/Passes/ShadowMapPass.h"
#include "engine/Passes/HDRPass.h"

#include "engine/Scene.h"
#include "engine/VulkanContext.h"
#include "engine/VulkanSwapchain.h"

class Renderer
{
public:
  Renderer(VulkanContext* vkContext, VulkanSwapchain* vkSwapchain, const Scene& scene);
  ~Renderer();

  void update(const Scene& scene);

  GBuffPass* gBufferPass;
  LightPass* lightPass;
  ShadowMapPass* shadowMapPass;
  BlinnPhongPass* blinnPhongPass;
  HDRPass* hdrPass;
  void draw(const Scene& scene);

private:

  VulkanContext* vkContext;
  VulkanSwapchain* vkSwapchain;

  VulkanBufferDefinition cameraBuffer;
  VulkanBufferDefinition pointLightsBuffer;
  VulkanBufferDefinition directionalLightBuffer;
  VulkanBufferDefinition spotLightsBuffer;
  void createBuffers();

  // descriptors
  VkDescriptorSetLayout cameraUBOLayout;
  VkDescriptorSetLayout lightsUBOLayout;
  VkDescriptorSetLayout directionalShadowMapLayout;
  
  VkDescriptorPool sceneDescriptorPool;

  VkDescriptorSet cameraUBODescriptorset;
  VkDescriptorSet lightsUBODescriptorset;
  VkDescriptorSet shadowMapDescriptorSet;
  void createDescriptors();
};

#endif
