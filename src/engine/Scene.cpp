#include "Scene.h"
#include "engine/Buffers.h"
#include "engine/Lights.h"

#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include <iostream>

Scene::Scene(VulkanContext* vkContext)
  : vkContext(vkContext)
{
  // --------------------- Init Scene ---------------------
  directionalLight = new DirectionalLight(glm::vec4(-5.0f, 5.0f, -3.0f, 0.0),
                                          glm::vec4(10, 0, 0, 1));

  pointLights[0] =
    PointLight{ glm::vec4(0, 1, -10, 0), glm::vec4(5, 0.4, 0.1, 1) };
  pointLights[1] =
    PointLight{ glm::vec4(0, -2, -20, 0), glm::vec4(5, 1, 1, 1) };
  pointLights[2] = PointLight{ glm::vec4(0, -2, -30, 1), glm::vec4(0.5) };
  pointLights[3] =
    PointLight{ glm::vec4(0, -2, -30, 1), glm::vec4(5, 2, 3, 1) };
  pointLights[4] =
    PointLight{ glm::vec4(0, -2, -40, 1), glm::vec4(10, 0, 0, 1) };

  spotLights[0] = SpotLight{ glm::vec4(-5.0f, 5.0f, -3.0f, 0.0),
                             glm::vec4(0),
                             glm::vec4(10.0f, 10.0f, 10.0f, 1.0f),
                             glm::vec4(glm::cos(glm::radians(8.5f)),
                                       glm::cos(glm::radians(9.5f)),
                                       0,
                                       0) };

  // create light cubes for the lights
  std::string modelPath = MODEL_PATH;
  for (auto& pointLight : pointLights) {
    Model cube = Model(modelPath + "cube.glb",
                       vkContext,
                       pointLight.position,
                       glm::vec3(0.0),
                       0,
                       glm::vec3(0.1f));
    lightCubes.push_back(std::move(cube));
  }

  camera = new Camera3D(glm::vec3(0.0, 0.0, 3.0), glm::vec3(0.0, 0.0, -1.0));

  // create new skybox
  std::string texturePath = TEXTURE_PATH;
  std::array<std::string, 6> files = {
    texturePath + "skybox/right.jpg", texturePath + "skybox/left.jpg",
    texturePath + "skybox/top.jpg",   texturePath + "skybox/bottom.jpg",
    texturePath + "skybox/front.jpg", texturePath + "skybox/back.jpg"
  };
  skybox = new Skybox(vkContext, files);

  // finish initializing models and loading them, setup camera etc.
  Model plane = Model(modelPath + "for_demo/plane.glb",
                      vkContext,
                      glm::vec3(0, 1, 0),
                      glm::vec3(0),
                      0,
                      glm::vec3(50));
  models.push_back(std::move(plane));
  Model cube = Model(modelPath + "cube.glb", vkContext, glm::vec3(0, 0, 0));
  models.push_back(std::move(cube));
  // Model desk = Model(modelPath + "for_demo/prova_optimized.glb",
  //                    vkContext,
  //                    glm::vec3(0.0, 1.0f, -3.0f),
  //                    glm::vec3(0.0, 1.0, 0.0),
  //                    180.0f,
  //                    glm::vec3(1.0f));
  // models.push_back(std::move(desk));
  // Model rare = Model(modelPath + "for_demo/rare_logo/rare.glb",
  //                         vkContext,
  //                         glm::vec3(-1.85, -0.7, -19.5f),
  //                         glm::vec3(1.0, 0.0, 0.0),
  //                         -90.0f,
  //                         glm::vec3(0.1f));
  //  models.push_back(std::move(rare));

  // --------------------- Create Buffers ---------------------
  createBuffers();

  createDescriptors();
}

Scene::~Scene()
{
  // destroy camera
  delete camera;
  delete skybox;

  // destroy models
  models.clear();
  vkDestroyDescriptorSetLayout(
    vkContext->logicalDevice, Model::textureLayout, nullptr);

  // destroy layout
  vkDestroyDescriptorSetLayout(
    vkContext->logicalDevice, cameraUBOLayout, nullptr);
  vkDestroyDescriptorSetLayout(
    vkContext->logicalDevice, lightsUBOLayout, nullptr);
  vkDestroyDescriptorSetLayout(
    vkContext->logicalDevice, directionalLightSpaceLayout, nullptr);
  vkDestroyDescriptorSetLayout(
    vkContext->logicalDevice, directionalShadowMapLayout, nullptr);

  // destroy pool
  vkDestroyDescriptorPool(
    vkContext->logicalDevice, sceneDescriptorPool, nullptr);

  // destroy buffers
  vmaDestroyBuffer(
    vkContext->allocator, cameraBuffer.buffer, cameraBuffer.allocation);

  vmaDestroyBuffer(vkContext->allocator,
                   directionalLightBuffer.buffer,
                   directionalLightBuffer.allocation);

  vmaDestroyBuffer(vkContext->allocator,
                   directionalLightTransformBuffer.buffer,
                   directionalLightTransformBuffer.allocation);

  vmaDestroyBuffer(vkContext->allocator,
                   pointLightsBuffer.buffer,
                   pointLightsBuffer.allocation);

  vmaDestroyBuffer(
    vkContext->allocator, spotLightsBuffer.buffer, spotLightsBuffer.allocation);
}

void
Scene::createDescriptors()
{
  // --------------------- Create Pool ---------------------
  std::array<VkDescriptorPoolSize, 2> poolSizes;

  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount =
    5; // UBO, PointLights, DirectionalLights, SpotLights, LSM

  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = 1; // ShadowMap

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 4;

  if (vkCreateDescriptorPool(
        vkContext->logicalDevice, &poolInfo, nullptr, &sceneDescriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }

  // --------------------- Create Camera Layout ---------------------
  {
    std::array<VkDescriptorSetLayoutBinding, 1> bindings;
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfoAttachmentWrite{};
    layoutInfoAttachmentWrite.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoAttachmentWrite.bindingCount =
      static_cast<uint32_t>(bindings.size());
    layoutInfoAttachmentWrite.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vkContext->logicalDevice,
                                    &layoutInfoAttachmentWrite,
                                    nullptr,
                                    &cameraUBOLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  // --------------------- Create Lights Layout ---------------------
  {
    std::array<VkDescriptorSetLayoutBinding, 3> bindings;
    bindings[0].binding = 1;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 2;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].pImmutableSamplers = nullptr;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[2].binding = 3;
    bindings[2].descriptorCount = 1;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].pImmutableSamplers = nullptr;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfoAttachmentWrite{};
    layoutInfoAttachmentWrite.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoAttachmentWrite.bindingCount =
      static_cast<uint32_t>(bindings.size());
    layoutInfoAttachmentWrite.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vkContext->logicalDevice,
                                    &layoutInfoAttachmentWrite,
                                    nullptr,
                                    &lightsUBOLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  // --------------------- Create Light Space Layout ---------------------
  {
    std::array<VkDescriptorSetLayoutBinding, 1> bindings;
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfoAttachmentWrite{};
    layoutInfoAttachmentWrite.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoAttachmentWrite.bindingCount =
      static_cast<uint32_t>(bindings.size());
    layoutInfoAttachmentWrite.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vkContext->logicalDevice,
                                    &layoutInfoAttachmentWrite,
                                    nullptr,
                                    &directionalLightSpaceLayout) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  // --------------------- Create Shadow Map Layout ---------------------
  {
    std::array<VkDescriptorSetLayoutBinding, 1> bindings;
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfoAttachmentWrite{};
    layoutInfoAttachmentWrite.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoAttachmentWrite.bindingCount =
      static_cast<uint32_t>(bindings.size());
    layoutInfoAttachmentWrite.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vkContext->logicalDevice,
                                    &layoutInfoAttachmentWrite,
                                    nullptr,
                                    &directionalShadowMapLayout) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  // --------------------- Create Descriptorset for Camera ---------------------
  {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = sceneDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &cameraUBOLayout;

    if (vkAllocateDescriptorSets(vkContext->logicalDevice,
                                 &allocInfo,
                                 &cameraUBODescriptorset) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }

    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = cameraBuffer.buffer;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = sizeof(CameraBuffer);

    std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = cameraUBODescriptorset;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

    vkUpdateDescriptorSets(vkContext->logicalDevice,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(),
                           0,
                           nullptr);
  }

  // --------------------- Create Descriptorset for Light UBOs
  // ---------------------
  {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = sceneDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &lightsUBOLayout;

    if (vkAllocateDescriptorSets(vkContext->logicalDevice,
                                 &allocInfo,
                                 &lightsUBODescriptorset) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }

    VkDescriptorBufferInfo directionalLightBufferInfo{};
    directionalLightBufferInfo.buffer = directionalLightBuffer.buffer;
    directionalLightBufferInfo.offset = 0;
    directionalLightBufferInfo.range =
      sizeof(DirectionalLight::DirectionalLightBuffer);

    VkDescriptorBufferInfo pointLightBufferInfo{};
    pointLightBufferInfo.buffer = pointLightsBuffer.buffer;
    pointLightBufferInfo.offset = 0;
    pointLightBufferInfo.range = sizeof(PointLight) * MAX_POINT_LIGHTS;

    VkDescriptorBufferInfo spotLightBufferInfo{};
    spotLightBufferInfo.buffer = spotLightsBuffer.buffer;
    spotLightBufferInfo.offset = 0;
    spotLightBufferInfo.range = sizeof(SpotLight);

    std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = lightsUBODescriptorset;
    descriptorWrites[0].dstBinding = 1;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &directionalLightBufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = lightsUBODescriptorset;
    descriptorWrites[1].dstBinding = 2;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &pointLightBufferInfo;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = lightsUBODescriptorset;
    descriptorWrites[2].dstBinding = 3;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &spotLightBufferInfo;

    vkUpdateDescriptorSets(vkContext->logicalDevice,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(),
                           0,
                           nullptr);
  }

  // --------------------- Create Descriptorset for Light Space Matrix
  // ---------------------
  {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = sceneDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &directionalLightSpaceLayout;

    if (vkAllocateDescriptorSets(vkContext->logicalDevice,
                                 &allocInfo,
                                 &directionalLightSpaceDescriptorSet) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }

    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = directionalLightTransformBuffer.buffer;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range =
      sizeof(DirectionalLight::DirectionalLightTransfrom);

    std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = directionalLightSpaceDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

    vkUpdateDescriptorSets(vkContext->logicalDevice,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(),
                           0,
                           nullptr);
  }

  // --------------------- Create Descriptorset for Shadow Map
  // ---------------------
  {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = sceneDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &directionalShadowMapLayout;

    if (vkAllocateDescriptorSets(vkContext->logicalDevice,
                                 &allocInfo,
                                 &directionalShadowMapDescriptorSet) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }
  }
}

void
Scene::createBuffers()
{
  // --------------------- Camera Buffer ---------------------
  VkDeviceSize uboBufferSize = sizeof(CameraBuffer);

  cameraBuffer.mapped =
    vkContext->createBuffer(uboBufferSize,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            BufferType::STAGING_BUFFER,
                            cameraBuffer.buffer,
                            cameraBuffer.allocation);

  // --------------------- Light Buffers ---------------------
  VkDeviceSize directionalLightBufferSize =
    sizeof(DirectionalLight::DirectionalLightBuffer);
  VkDeviceSize directionalLightTransformBufferSize =
    sizeof(DirectionalLight::DirectionalLightTransfrom);

  VkDeviceSize pointLightBufferSize = sizeof(PointLight) * MAX_POINT_LIGHTS;
  VkDeviceSize spotLightBufferSize = sizeof(SpotLight) * MAX_SPOT_LIGHTS;

  directionalLightBuffer.mapped =
    vkContext->createBuffer(directionalLightBufferSize,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            BufferType::STAGING_BUFFER,
                            directionalLightBuffer.buffer,
                            directionalLightBuffer.allocation);
  directionalLightTransformBuffer.mapped =
    vkContext->createBuffer(directionalLightTransformBufferSize,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            BufferType::STAGING_BUFFER,
                            directionalLightTransformBuffer.buffer,
                            directionalLightTransformBuffer.allocation);

  pointLightsBuffer.mapped =
    vkContext->createBuffer(pointLightBufferSize,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            BufferType::STAGING_BUFFER,
                            pointLightsBuffer.buffer,
                            pointLightsBuffer.allocation);
  spotLightsBuffer.mapped =
    vkContext->createBuffer(spotLightBufferSize,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            BufferType::STAGING_BUFFER,
                            spotLightsBuffer.buffer,
                            spotLightsBuffer.allocation);

  uint8_t* directionalMapped =
    reinterpret_cast<uint8_t*>(directionalLightBuffer.mapped);
  memcpy(directionalMapped,
         &directionalLight->directionalLightBuffer,
         directionalLightBufferSize);

  uint8_t* directionalTransformMapped =
    reinterpret_cast<uint8_t*>(directionalLightTransformBuffer.mapped);
  memcpy(directionalTransformMapped,
         &directionalLight->directionalLightTransform,
         directionalLightTransformBufferSize);

  uint8_t* pointMapped = reinterpret_cast<uint8_t*>(pointLightsBuffer.mapped);
  memcpy(pointMapped, &pointLights, pointLightBufferSize);

  uint8_t* spotMapped = reinterpret_cast<uint8_t*>(spotLightsBuffer.mapped);
  memcpy(spotMapped, &spotLights, spotLightBufferSize);
}

void
Scene::update()
{
  // update camera data
  CameraBuffer cb;

  // position, target, up
  cb.view = camera->getCameraMatrix();
  cb.proj = camera->getCameraProjectionMatrix();
  cb.cameraPos = glm::vec4(camera->getCameraPos(), 1);

  models[1].rotate(1.0, glm::vec3(1.0, 0.5, 0.3));

  spotLights[0].position = glm::vec4(camera->getCameraPos(), 1.0);
  spotLights[0].direction = glm::vec4(camera->getCameraFront(), 1.0);

  // auto tmp = camera->getCameraFront();
  // std::cout << tmp.x << " " << tmp.y << " " << tmp.z << std::endl;

  memcpy(cameraBuffer.mapped, &cb, sizeof(CameraBuffer));

  uint8_t* spotMapped = reinterpret_cast<uint8_t*>(spotLightsBuffer.mapped);
  memcpy(spotMapped, &spotLights, sizeof(SpotLight) * MAX_SPOT_LIGHTS);
}
