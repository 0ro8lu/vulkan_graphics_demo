#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include "engine/Vertex.h"
#include "engine/VulkanContext.h"

#include "engine/Camera3D.h"
#include "engine/LightManager.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <string>
#include <vector>

struct VulkanBufferDefinition
{
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  void* mapped = nullptr;
  size_t size = 0;
};

struct CamLightShadowBundle
{
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

  VkDescriptorSetLayout cameraUBOLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout lightsUBOLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout directionalShadowmapLayout = VK_NULL_HANDLE;

  VkDescriptorSet cameraUBODescriptorset = VK_NULL_HANDLE;
  VkDescriptorSet lightsUBODescriptorset = VK_NULL_HANDLE;
  VkDescriptorSet shadowMapDescriptorSet = VK_NULL_HANDLE;

  VulkanBufferDefinition cameraBuffer = {};
  VulkanBufferDefinition pointLightsBuffer = {};
  VulkanBufferDefinition directionalLightBuffer = {};
  VulkanBufferDefinition spotLightsBuffer = {};
};

inline CamLightShadowBundle
createBaselineDescriptorsAndBuffers(VulkanContext* vkContext)
{
  CamLightShadowBundle camLightShadowBundle;

  // Buffers
  camLightShadowBundle.cameraBuffer.size = sizeof(CameraBuffer);

  camLightShadowBundle.cameraBuffer.mapped =
    vkContext->createBuffer(camLightShadowBundle.cameraBuffer.size,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VulkanContext::BufferType::STAGING_BUFFER,
                            camLightShadowBundle.cameraBuffer.buffer,
                            camLightShadowBundle.cameraBuffer.allocation);

  // --------------------- Light Buffers ---------------------
  camLightShadowBundle.directionalLightBuffer.size = sizeof(DirectionalLight);
  camLightShadowBundle.pointLightsBuffer.size =
    sizeof(PointLight) * MAX_POINT_LIGHTS;
  camLightShadowBundle.spotLightsBuffer.size =
    sizeof(SpotLight) * MAX_SPOT_LIGHTS;

  camLightShadowBundle.directionalLightBuffer.mapped = vkContext->createBuffer(
    camLightShadowBundle.directionalLightBuffer.size,
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    VulkanContext::BufferType::STAGING_BUFFER,
    camLightShadowBundle.directionalLightBuffer.buffer,
    camLightShadowBundle.directionalLightBuffer.allocation);

  camLightShadowBundle.pointLightsBuffer.mapped =
    vkContext->createBuffer(camLightShadowBundle.pointLightsBuffer.size,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VulkanContext::BufferType::STAGING_BUFFER,
                            camLightShadowBundle.pointLightsBuffer.buffer,
                            camLightShadowBundle.pointLightsBuffer.allocation);

  camLightShadowBundle.spotLightsBuffer.mapped =
    vkContext->createBuffer(camLightShadowBundle.spotLightsBuffer.size,
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VulkanContext::BufferType::STAGING_BUFFER,
                            camLightShadowBundle.spotLightsBuffer.buffer,
                            camLightShadowBundle.spotLightsBuffer.allocation);

  // Descriptors
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

  if (vkCreateDescriptorPool(vkContext->logicalDevice,
                             &poolInfo,
                             nullptr,
                             &camLightShadowBundle.descriptorPool) !=
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
                                    &camLightShadowBundle.cameraUBOLayout) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  // --------------------- Create Lights Layout ---------------------
  {
    std::array<VkDescriptorSetLayoutBinding, 3> bindings;
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].pImmutableSamplers = nullptr;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[2].binding = 2;
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
                                    &camLightShadowBundle.lightsUBOLayout) !=
        VK_SUCCESS) {
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

    if (vkCreateDescriptorSetLayout(
          vkContext->logicalDevice,
          &layoutInfoAttachmentWrite,
          nullptr,
          &camLightShadowBundle.directionalShadowmapLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  // --------------------- Create Descriptorset for Camera ---------------------
  {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = camLightShadowBundle.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &camLightShadowBundle.cameraUBOLayout;
    if (vkAllocateDescriptorSets(
          vkContext->logicalDevice,
          &allocInfo,
          &camLightShadowBundle.cameraUBODescriptorset) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }
    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = camLightShadowBundle.cameraBuffer.buffer;
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = sizeof(CameraBuffer);
    std::array<VkWriteDescriptorSet, 1> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = camLightShadowBundle.cameraUBODescriptorset;
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
  //--------------------- Create Descriptorset for Light UBOs
  //---------------------
  {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = camLightShadowBundle.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &camLightShadowBundle.lightsUBOLayout;

    if (vkAllocateDescriptorSets(
          vkContext->logicalDevice,
          &allocInfo,
          &camLightShadowBundle.lightsUBODescriptorset) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }

    VkDescriptorBufferInfo directionalLightBufferInfo{};
    directionalLightBufferInfo.buffer =
      camLightShadowBundle.directionalLightBuffer.buffer;
    directionalLightBufferInfo.offset = 0;
    directionalLightBufferInfo.range = sizeof(DirectionalLight);

    VkDescriptorBufferInfo pointLightBufferInfo{};
    pointLightBufferInfo.buffer = camLightShadowBundle.pointLightsBuffer.buffer;
    pointLightBufferInfo.offset = 0;
    pointLightBufferInfo.range = sizeof(PointLight) * MAX_POINT_LIGHTS;

    VkDescriptorBufferInfo spotLightBufferInfo{};
    spotLightBufferInfo.buffer = camLightShadowBundle.spotLightsBuffer.buffer;
    spotLightBufferInfo.offset = 0;
    spotLightBufferInfo.range = sizeof(SpotLight) * MAX_SPOT_LIGHTS;

    std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = camLightShadowBundle.lightsUBODescriptorset;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &directionalLightBufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = camLightShadowBundle.lightsUBODescriptorset;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &pointLightBufferInfo;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = camLightShadowBundle.lightsUBODescriptorset;
    descriptorWrites[2].dstBinding = 2;
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
  {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = camLightShadowBundle.descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &camLightShadowBundle.directionalShadowmapLayout;

    if (vkAllocateDescriptorSets(
          vkContext->logicalDevice,
          &allocInfo,
          &camLightShadowBundle.shadowMapDescriptorSet) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }
  }

  return camLightShadowBundle;
}

struct GraphicsPipelineConfig
{
  // Shaders
  std::string vertShaderName;
  bool useVertexInput = true;
  std::string fragShaderName;
  VkSpecializationInfo* vertSpecialization = nullptr;
  VkSpecializationInfo* fragSpecialization = nullptr;

  // Rasterization
  VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
  VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
  VkBool32 depthBiasEnable = VK_FALSE;
  VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  // Depth/Stencil
  VkBool32 depthTestEnable = VK_TRUE;
  VkBool32 depthWriteEnable = VK_TRUE;
  VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

  // MSAA & Blending
  VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
  VkBool32 blendEnable = VK_FALSE;
  VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
  VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;
  VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

  // Layout & Renderpass
  std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
  std::vector<VkPushConstantRange> pushConstantRanges;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  uint32_t subpass = 0;
};

struct PipelineResult
{
  VkPipeline pipeline;
  VkPipelineLayout layout;
};

inline PipelineResult
createPipeline(VulkanContext* vkContext, const GraphicsPipelineConfig& config)
{
  std::string shaderPath = SHADER_PATH;

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  std::vector<VkShaderModule> shaderModules;

  // 1. Dynamic Shader Stage Creation
  auto addStage = [&](const std::string& name,
                      VkShaderStageFlagBits stage,
                      VkSpecializationInfo* specInfo) {
    if (name.empty())
      return;

    auto code = vkContext->readShader(shaderPath + name);
    VkShaderModule module = vkContext->createShaderModule(code);
    shaderModules.push_back(module);

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = stage;
    stageInfo.module = module;
    stageInfo.pName = "main";
    stageInfo.pSpecializationInfo = specInfo;
    shaderStages.push_back(stageInfo);
  };

  addStage(config.vertShaderName,
           VK_SHADER_STAGE_VERTEX_BIT,
           config.vertSpecialization);
  addStage(config.fragShaderName,
           VK_SHADER_STAGE_FRAGMENT_BIT,
           config.fragSpecialization);

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  if (config.useVertexInput) {
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
  } else {
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
  }

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = config.polygonMode;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = config.cullMode;
  rasterizer.frontFace = config.frontFace;
  rasterizer.depthBiasEnable = config.depthBiasEnable;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = config.msaaSamples;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = config.depthTestEnable;
  depthStencil.depthWriteEnable = config.depthWriteEnable;
  depthStencil.depthCompareOp = config.depthCompareOp;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = config.blendEnable;

  colorBlendAttachment.blendEnable = config.blendEnable;
  colorBlendAttachment.colorBlendOp = config.colorBlendOp;
  colorBlendAttachment.srcColorBlendFactor = config.srcColorBlendFactor;
  colorBlendAttachment.dstColorBlendFactor = config.dstColorBlendFactor;
  colorBlendAttachment.alphaBlendOp = config.alphaBlendOp;
  colorBlendAttachment.srcAlphaBlendFactor = config.srcAlphaBlendFactor;
  colorBlendAttachment.dstAlphaBlendFactor = config.dstAlphaBlendFactor;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = config.fragShaderName.empty() ? 0 : 1;
  colorBlending.pAttachments =
    (colorBlending.attachmentCount > 0) ? &colorBlendAttachment : nullptr;

  VkPipelineDynamicStateCreateInfo dynamicState{};
  std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
                                                VK_DYNAMIC_STATE_SCISSOR };
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount =
    static_cast<uint32_t>(config.descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = config.descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount =
    static_cast<uint32_t>(config.pushConstantRanges.size());
  pipelineLayoutInfo.pPushConstantRanges = config.pushConstantRanges.data();

  PipelineResult result{};
  if (vkCreatePipelineLayout(vkContext->logicalDevice,
                             &pipelineLayoutInfo,
                             nullptr,
                             &result.layout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = result.layout;
  pipelineInfo.renderPass = config.renderPass;
  pipelineInfo.subpass = config.subpass;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(vkContext->logicalDevice,
                                VK_NULL_HANDLE,
                                1,
                                &pipelineInfo,
                                nullptr,
                                &result.pipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  for (auto module : shaderModules) {
    vkDestroyShaderModule(vkContext->logicalDevice, module, nullptr);
  }

  return result;
}

#endif
