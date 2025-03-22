#ifndef _SCENE_H_
#define _SCENE_H_

#include "engine/Buffers.h"
#include "engine/Camera3D.h"
#include "engine/LightManager.h"
#include "engine/ModelLoading/Model.h"
#include "engine/Skybox.h"
#include "engine/VulkanContext.h"

class Scene
{
public:
  Scene(VulkanContext* vkContext);
  ~Scene();

  void update(); // used for sending data to GPU in case camera or lights have
                 // changed.

  VkDescriptorSetLayout cameraUBOLayout;
  VkDescriptorSetLayout lightsUBOLayout;
  VkDescriptorSetLayout directionalShadowMapLayout;

  VkDescriptorSet cameraUBODescriptorset;
  VkDescriptorSet lightsUBODescriptorset;
  VkDescriptorSet shadowMapDescriptorSet;

  std::vector<Model> models;
  std::vector<Model> lightCubes;

  Skybox* skybox;

  std::vector<PointLight> pointLights;
  DirectionalLight* directionalLight;
  std::vector<SpotLight> spotLights;

  Camera3D* camera;

private:
  VulkanContext* vkContext;

  VkDescriptorPool sceneDescriptorPool;

  void createDescriptors();

  VulkanBufferDefinition cameraBuffer;
  VulkanBufferDefinition pointLightsBuffer;
  VulkanBufferDefinition directionalLightBuffer;
  VulkanBufferDefinition spotLightsBuffer;
  void createBuffers();
};

#endif
