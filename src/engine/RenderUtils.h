#pragma once

#include "engine/VulkanContext.h"

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
  // VkDescriptorSetLayout skyboxLayout = VK_NULL_HANDLE;

  VkDescriptorSet cameraUBODescriptorset = VK_NULL_HANDLE;
  VkDescriptorSet lightsUBODescriptorset = VK_NULL_HANDLE;
  VkDescriptorSet shadowMapDescriptorSet = VK_NULL_HANDLE;

  VulkanBufferDefinition cameraBuffer = {};
  VulkanBufferDefinition pointLightsBuffer = {};
  VulkanBufferDefinition directionalLightBuffer = {};
  VulkanBufferDefinition spotLightsBuffer = {};
};

CamLightShadowBundle
createBaselineDescriptorsAndBuffers(VulkanContext* vkContext);

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

PipelineResult
createPipeline(VulkanContext* vkContext, const GraphicsPipelineConfig& config);

std::vector<char>
readShader(const std::string& filename);
