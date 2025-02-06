#ifndef _SCENE_H_
#define _SCENE_H_

#include "engine/Camera3D.h"
#include "engine/ModelLoading/ModelRefactored.h"
#include "engine/VulkanContext.h"

#include <vector>

const uint32_t MAX_POINT_LIGHTS = 2;
// const uint32_t MAX_POINT_LIGHTS = 10;
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

  VkDescriptorSet cameraUBODescriptorset;
  VkDescriptorSet lightsUBODescriptorset;

  std::vector<ModelDO> models;

  std::vector<PointLight> pointLights;
  DirectionalLight directionalLight;
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
