#ifndef _SCENE_H_
#define _SCENE_H_

#include "engine/Buffers.h"
#include "engine/Camera3D.h"
#include "engine/ModelLoading/Model.h"
#include "engine/Skybox.h"
#include "engine/VulkanContext.h"

#include "engine/Lights.h"

#include <array>

const uint32_t MAX_POINT_LIGHTS = 5;
const uint32_t MAX_SPOT_LIGHTS = 10;

class Scene
{
public:
  Scene(VulkanContext* vkContext);
  ~Scene();

  void update(); // used for sending data to GPU in case camera or lights have
                 // changed.

  VkDescriptorSetLayout cameraUBOLayout;
  VkDescriptorSetLayout lightsUBOLayout;
  VkDescriptorSetLayout directionalLightSpaceLayout;
  VkDescriptorSetLayout directionalShadowMapLayout;

  VkDescriptorSet cameraUBODescriptorset;
  VkDescriptorSet lightsUBODescriptorset;
  VkDescriptorSet directionalLightSpaceDescriptorSet;
  VkDescriptorSet directionalShadowMapDescriptorSet;

  std::vector<Model> models;
  std::vector<Model> lightCubes;

  Skybox* skybox;

  std::array<PointLight, MAX_POINT_LIGHTS> pointLights;
  DirectionalLight* directionalLight;
  std::array<SpotLight, MAX_SPOT_LIGHTS> spotLights;

  Camera3D* camera;

private:
  VulkanContext* vkContext;

  VkDescriptorPool sceneDescriptorPool;

  void createDescriptors();

  VulkanBufferDefinition cameraBuffer;
  VulkanBufferDefinition pointLightsBuffer;
  VulkanBufferDefinition directionalLightBuffer;
  VulkanBufferDefinition directionalLightTransformBuffer;
  VulkanBufferDefinition spotLightsBuffer;
  void createBuffers();
};

#endif
