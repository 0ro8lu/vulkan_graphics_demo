#include "engine/Renderer.h"

Renderer::Renderer(VulkanContext* vkContext, VulkanSwapchain* vkSwapchain,  const Scene& scene) : vkContext(vkContext), vkSwapchain(vkSwapchain)
{
  // create buffers
  // createBuffers();
  // create descriptors
  // createDescriptors();

  // create render passes
  gBufferPass = new GBuffPass(vkContext, {}, scene, vkSwapchain->width, vkSwapchain->height);
  lightPass = new LightPass(vkContext, {gBufferPass->depthAttachment->view, gBufferPass->depthAttachment->format}, scene, vkSwapchain->width, vkSwapchain->height);
  shadowMapPass = new ShadowMapPass(vkContext, {}, scene, 4096, 4096); // <- 1024 is the fixed resolution i've chosen for the shadowmaps
  blinnPhongPass = new BlinnPhongPass(vkContext, {vkSwapchain->depthImageView, vkSwapchain->getDepthImageFormat()}, scene, vkSwapchain->width, vkSwapchain->height);
  hdrPass = new HDRPass(vkContext, {VK_NULL_HANDLE, vkSwapchain->getSwapChainImageFormat()}, scene, vkSwapchain->width, vkSwapchain->height);

  // update descriptors
  // lightPass.updateDescriptors({gbufferPass.positionAttachment, gbufferPass.normalAttachment, gbufferPass.albedoAttachment, shadowMapPass.directionalShadowMap});
  // hdrPass.updateDescriptors({lightPass.hdrAttachment});
  hdrPass->updateDescriptors({blinnPhongPass->hdrAttachment});
  blinnPhongPass->updateDescriptors({shadowMapPass->directionalShadowMap, shadowMapPass->spotPointShadowAtlas});

  vkSwapchain->drawingPass = hdrPass->presentationRenderPass;
  vkSwapchain->createSwapChainFrameBuffer();
  vkSwapchain->onResize = [this, vkSwapchain](int width, int height) {
    // gbufferPass.recreateAttachments(width, height, {});
    // lightPass.recreateAttachments(width, height, {gbufferPass.depthAttachment->view, gbufferPass.depthAttachment->format});
    blinnPhongPass->recreateAttachments(width, height, {vkSwapchain->depthImageView, vkSwapchain->getDepthImageFormat()});
    hdrPass->recreateAttachments(width, height, {VK_NULL_HANDLE, vkSwapchain->getSwapChainImageFormat()});

    // hdrPass.updateDescriptors({lightPass.hdrAttachment});
    hdrPass->updateDescriptors({blinnPhongPass->hdrAttachment});
    // lightPass.updateDescriptors({gbufferPass.positionAttachment, gbufferPass.normalAttachment, gbufferPass.albedoAttachment, shadowMapPass.directionalShadowMap});
  };
}

Renderer::~Renderer() {
    vkDestroyDescriptorSetLayout(
    vkContext->logicalDevice, cameraUBOLayout, nullptr);
  vkDestroyDescriptorSetLayout(
    vkContext->logicalDevice, lightsUBOLayout, nullptr);
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
                   pointLightsBuffer.buffer,
                   pointLightsBuffer.allocation);

  vmaDestroyBuffer(
    vkContext->allocator, spotLightsBuffer.buffer, spotLightsBuffer.allocation);

  delete gBufferPass;
  delete lightPass;

  delete shadowMapPass;
  delete blinnPhongPass;
  delete hdrPass;
}

void
Renderer::update(const Scene& scene)
{
  // update the buffers
  if(scene.directionalLight) {
    uint8_t* directionalMapped =
    reinterpret_cast<uint8_t*>(directionalLightBuffer.mapped);
    memcpy(directionalMapped, scene.directionalLight, directionalLightBuffer.size);
  }

  if(!scene.pointLights.empty()) {
    uint8_t* pointMapped = reinterpret_cast<uint8_t*>(pointLightsBuffer.mapped);
    memcpy(pointMapped, scene.pointLights.data(), pointLightsBuffer.size);
  }

  if (!scene.spotLights.empty()) {
    uint8_t* spotMapped = reinterpret_cast<uint8_t*>(spotLightsBuffer.mapped);
    memcpy(spotMapped, scene.spotLights.data(), spotLightsBuffer.size);
  }
}

void
Renderer::draw(const Scene& scene)
{
  vkSwapchain->prepareFrame();

  // gBufferPass->draw(vkSwapchain, scene);
  shadowMapPass->draw(vkSwapchain, scene);
  // lightPass->draw(vkSwapchain, scene);
  blinnPhongPass->draw(vkSwapchain, scene);
  hdrPass->draw(vkSwapchain, scene);

  vkSwapchain->submitFrame();
}

void Renderer::createBuffers() {
  cameraBuffer.size = sizeof(CameraBuffer);

  cameraBuffer.mapped =
    vkContext->createBuffer(cameraBuffer.size,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            BufferType::STAGING_BUFFER,
                            cameraBuffer.buffer,
                            cameraBuffer.allocation);

  // --------------------- Light Buffers ---------------------
  
  directionalLightBuffer.size = sizeof(DirectionalLight);
  pointLightsBuffer.size = sizeof(PointLight) * MAX_POINT_LIGHTS;
  spotLightsBuffer.size = sizeof(SpotLight) * MAX_SPOT_LIGHTS;

  directionalLightBuffer.mapped =
    vkContext->createBuffer(directionalLightBuffer.size,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            BufferType::STAGING_BUFFER,
                            directionalLightBuffer.buffer,
                            directionalLightBuffer.allocation);

  pointLightsBuffer.mapped =
    vkContext->createBuffer(pointLightsBuffer.size,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            BufferType::STAGING_BUFFER,
                            pointLightsBuffer.buffer,
                            pointLightsBuffer.allocation);

  spotLightsBuffer.mapped =
    vkContext->createBuffer(spotLightsBuffer.size,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            BufferType::STAGING_BUFFER,
                            spotLightsBuffer.buffer,
                            spotLightsBuffer.allocation);
}

void Renderer::createDescriptors()
{
  // --------------------- Create Pool ---------------------
  std::array<VkDescriptorPoolSize, 2> poolSizes;

  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount =
    4; // UBO, PointLights, DirectionalLights, SpotLights

  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount =
    2; // DirectionalShadowMap and spotPointShadowAtlas

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = 3;

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

  // --------------------- Create Shadow Map Layout ---------------------
  {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings;

    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].pImmutableSamplers = nullptr;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

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
    directionalLightBufferInfo.range = sizeof(DirectionalLight);

    VkDescriptorBufferInfo pointLightBufferInfo{};
    pointLightBufferInfo.buffer = pointLightsBuffer.buffer;
    pointLightBufferInfo.offset = 0;
    pointLightBufferInfo.range = sizeof(PointLight) * MAX_POINT_LIGHTS;

    VkDescriptorBufferInfo spotLightBufferInfo{};
    spotLightBufferInfo.buffer = spotLightsBuffer.buffer;
    spotLightBufferInfo.offset = 0;
    spotLightBufferInfo.range = sizeof(SpotLight) * MAX_SPOT_LIGHTS;
    // spotLightBufferInfo.range =
    //   sizeof(SpotLight::SpotLightBuffer) * MAX_SPOT_LIGHTS;

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
                                 &shadowMapDescriptorSet) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }
  }
}
