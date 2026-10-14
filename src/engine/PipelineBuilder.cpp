#include "engine/PipelineBuilder.h"
#include "engine/ShaderLoader.h"
#include "engine/Vertex.h"
#include "engine/VulkanCheck.h"

PipelineBuilder::PipelineBuilder()
{
  // Input assembly default: Triangle List
  m_inputAssembly.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  m_inputAssembly.primitiveRestartEnable = VK_FALSE;

  // Rasterizer default: Fill, Back-face culling, CCW front face
  m_rasterizer.sType =
    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  m_rasterizer.depthClampEnable = VK_FALSE;
  m_rasterizer.rasterizerDiscardEnable = VK_FALSE;
  m_rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  m_rasterizer.lineWidth = 1.0f;
  m_rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  m_rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  m_rasterizer.depthBiasEnable = VK_FALSE;

  // Multisampling default: 1x
  m_multisampling.sType =
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  m_multisampling.sampleShadingEnable = VK_FALSE;
  m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // Depth / Stencil default: Test & Write enabled, LESS_OR_EQUAL
  m_depthStencil.sType =
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  m_depthStencil.depthTestEnable = VK_TRUE;
  m_depthStencil.depthWriteEnable = VK_TRUE;
  m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  m_depthStencil.depthBoundsTestEnable = VK_FALSE;
  m_depthStencil.stencilTestEnable = VK_FALSE;

  // Dynamic states default: Viewport and Scissor
  m_dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  m_dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

  // Default to NO vertex input (fullscreen / post-process friendly)
  m_useVertexInput = false;
}

PipelineBuilder&
PipelineBuilder::setVertexShader(std::string_view shaderName,
                                 const VkSpecializationInfo* specialization)
{
  m_shaderStages.push_back(ShaderStage{
    .name = std::string(shaderName),
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .specialization = specialization,
  });
  return *this;
}

PipelineBuilder&
PipelineBuilder::setFragmentShader(std::string_view shaderName,
                                   const VkSpecializationInfo* specialization)
{
  m_shaderStages.push_back(ShaderStage{
    .name = std::string(shaderName),
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .specialization = specialization,
  });
  return *this;
}

PipelineBuilder&
PipelineBuilder::withDefaultMeshVertexInput()
{
  m_useVertexInput = true;
  m_vertexBindings.clear();
  m_vertexBindings.push_back(Vertex::getBindingDescription());
  auto attrs = Vertex::getAttributeDescriptions();
  m_vertexAttributes.assign(attrs.begin(), attrs.end());
  return *this;
}

PipelineBuilder&
PipelineBuilder::disableVertexInput()
{
  m_useVertexInput = false;
  m_vertexBindings.clear();
  m_vertexAttributes.clear();
  return *this;
}

PipelineBuilder&
PipelineBuilder::setVertexInput(
  std::span<const VkVertexInputBindingDescription> bindings,
  std::span<const VkVertexInputAttributeDescription> attributes)
{
  m_useVertexInput = true;
  m_vertexBindings.assign(bindings.begin(), bindings.end());
  m_vertexAttributes.assign(attributes.begin(), attributes.end());
  return *this;
}

PipelineBuilder&
PipelineBuilder::setInputAssembly(VkPrimitiveTopology topology,
                                  VkBool32 primitiveRestart)
{
  m_inputAssembly.topology = topology;
  m_inputAssembly.primitiveRestartEnable = primitiveRestart;
  return *this;
}

PipelineBuilder&
PipelineBuilder::setPolygonMode(VkPolygonMode mode)
{
  m_rasterizer.polygonMode = mode;
  return *this;
}

PipelineBuilder&
PipelineBuilder::setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
  m_rasterizer.cullMode = cullMode;
  m_rasterizer.frontFace = frontFace;
  return *this;
}

PipelineBuilder&
PipelineBuilder::setDepthBias(VkBool32 enable,
                              float constantFactor,
                              float clamp,
                              float slopeFactor)
{
  m_rasterizer.depthBiasEnable = enable;
  m_rasterizer.depthBiasConstantFactor = constantFactor;
  m_rasterizer.depthBiasClamp = clamp;
  m_rasterizer.depthBiasSlopeFactor = slopeFactor;
  return *this;
}

PipelineBuilder&
PipelineBuilder::setMultisampling(VkSampleCountFlagBits samples)
{
  m_multisampling.rasterizationSamples = samples;
  return *this;
}

PipelineBuilder&
PipelineBuilder::setDepthTest(VkBool32 depthTestEnable,
                              VkBool32 depthWriteEnable,
                              VkCompareOp depthCompareOp)
{
  m_depthStencil.depthTestEnable = depthTestEnable;
  m_depthStencil.depthWriteEnable = depthWriteEnable;
  m_depthStencil.depthCompareOp = depthCompareOp;
  return *this;
}

PipelineBuilder&
PipelineBuilder::addColorBlendAttachment(VkBool32 blendEnable,
                                         VkBlendFactor srcColor,
                                         VkBlendFactor dstColor,
                                         VkBlendOp colorOp,
                                         VkBlendFactor srcAlpha,
                                         VkBlendFactor dstAlpha,
                                         VkBlendOp alphaOp,
                                         VkColorComponentFlags writeMask)
{
  m_colorBlendAttachments.push_back(VkPipelineColorBlendAttachmentState{
    .blendEnable = blendEnable,
    .srcColorBlendFactor = srcColor,
    .dstColorBlendFactor = dstColor,
    .colorBlendOp = colorOp,
    .srcAlphaBlendFactor = srcAlpha,
    .dstAlphaBlendFactor = dstAlpha,
    .alphaBlendOp = alphaOp,
    .colorWriteMask = writeMask,
  });
  return *this;
}

PipelineBuilder&
PipelineBuilder::addAdditiveBlendAttachment()
{
  return addColorBlendAttachment(
    VK_TRUE,
    VK_BLEND_FACTOR_ONE,
    VK_BLEND_FACTOR_ONE,
    VK_BLEND_OP_ADD,
    VK_BLEND_FACTOR_ONE,
    VK_BLEND_FACTOR_ONE,
    VK_BLEND_OP_ADD,
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
}

PipelineBuilder&
PipelineBuilder::addAlphaBlendAttachment()
{
  return addColorBlendAttachment(
    VK_TRUE,
    VK_BLEND_FACTOR_SRC_ALPHA,
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    VK_BLEND_OP_ADD,
    VK_BLEND_FACTOR_ONE,
    VK_BLEND_FACTOR_ZERO,
    VK_BLEND_OP_ADD,
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
}

PipelineBuilder&
PipelineBuilder::clearColorBlendAttachments()
{
  m_colorBlendAttachments.clear();
  return *this;
}

PipelineBuilder&
PipelineBuilder::setDynamicStates(std::span<const VkDynamicState> states)
{
  m_dynamicStates.assign(states.begin(), states.end());
  return *this;
}

PipelineBuilder&
PipelineBuilder::setDescriptorSetLayouts(
  std::span<const VkDescriptorSetLayout> layouts)
{
  m_descriptorSetLayouts.assign(layouts.begin(), layouts.end());
  return *this;
}

PipelineBuilder&
PipelineBuilder::setPushConstantRanges(
  std::span<const VkPushConstantRange> ranges)
{
  m_pushConstantRanges.assign(ranges.begin(), ranges.end());
  return *this;
}

PipelineBuilder&
PipelineBuilder::setRenderPass(VkRenderPass renderPass, uint32_t subpass)
{
  m_renderPass = renderPass;
  m_subpass = subpass;
  return *this;
}

PipelineBuilder&
PipelineBuilder::setPipelineLayout(VkPipelineLayout layout)
{
  m_existingLayout = layout;
  return *this;
}

PipelineResult
PipelineBuilder::build(VkDevice device)
{
  // 1. Shaders
  std::array<VkPipelineShaderStageCreateInfo, 4> stageInfos{};
  std::array<VkShaderModule, 4> modulesToDestroy{};
  uint32_t stageCount = 0;

  bool hasFragmentShader = false;

  for (const auto& stage : m_shaderStages) {
    if (stage.name.empty()) {
      continue;
    }

    VkShaderModule module = ShaderLoader::loadShaderModule(device, stage.name);
    modulesToDestroy[stageCount] = module;

    stageInfos[stageCount] = VkPipelineShaderStageCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = stage.stage,
      .module = module,
      .pName = "main",
      .pSpecializationInfo = stage.specialization,
    };
    stageCount++;

    if (stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      hasFragmentShader = true;
    }
  }

  // 2. Vertex Input
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount =
      m_useVertexInput ? static_cast<uint32_t>(m_vertexBindings.size()) : 0u,
    .pVertexBindingDescriptions =
      m_useVertexInput ? m_vertexBindings.data() : nullptr,
    .vertexAttributeDescriptionCount =
      m_useVertexInput ? static_cast<uint32_t>(m_vertexAttributes.size()) : 0u,
    .pVertexAttributeDescriptions =
      m_useVertexInput ? m_vertexAttributes.data() : nullptr,
  };

  // 3. Viewport & Scissor (Dynamic)
  VkPipelineViewportStateCreateInfo viewportState{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports = nullptr,
    .scissorCount = 1,
    .pScissors = nullptr,
  };

  // 4. Color Blending
  VkPipelineColorBlendAttachmentState defaultOpaqueAttachment{
    .blendEnable = VK_FALSE,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  const bool useDefaultAttachment =
    m_colorBlendAttachments.empty() && hasFragmentShader;
  const uint32_t blendAttachmentCount =
    useDefaultAttachment ? 1u
                         : static_cast<uint32_t>(m_colorBlendAttachments.size());
  const VkPipelineColorBlendAttachmentState* pBlendAttachments =
    useDefaultAttachment
      ? &defaultOpaqueAttachment
      : (m_colorBlendAttachments.empty() ? nullptr
                                         : m_colorBlendAttachments.data());

  VkPipelineColorBlendStateCreateInfo colorBlending{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = blendAttachmentCount,
    .pAttachments = pBlendAttachments,
  };

  // 5. Dynamic States
  VkPipelineDynamicStateCreateInfo dynamicStateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size()),
    .pDynamicStates = m_dynamicStates.data(),
  };

  // 6. Pipeline Layout
  PipelineResult result{};
  if (m_existingLayout != VK_NULL_HANDLE) {
    result.layout = m_existingLayout;
  } else {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = static_cast<uint32_t>(m_descriptorSetLayouts.size()),
      .pSetLayouts =
        m_descriptorSetLayouts.empty() ? nullptr : m_descriptorSetLayouts.data(),
      .pushConstantRangeCount =
        static_cast<uint32_t>(m_pushConstantRanges.size()),
      .pPushConstantRanges =
        m_pushConstantRanges.empty() ? nullptr : m_pushConstantRanges.data(),
    };
    VK_CHECK(
      vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &result.layout));
  }

  // 7. Graphics Pipeline Creation
  VkGraphicsPipelineCreateInfo pipelineInfo{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = stageCount,
    .pStages = stageInfos.data(),
    .pVertexInputState = &vertexInputInfo,
    .pInputAssemblyState = &m_inputAssembly,
    .pViewportState = &viewportState,
    .pRasterizationState = &m_rasterizer,
    .pMultisampleState = &m_multisampling,
    .pDepthStencilState = &m_depthStencil,
    .pColorBlendState = &colorBlending,
    .pDynamicState = &dynamicStateInfo,
    .layout = result.layout,
    .renderPass = m_renderPass,
    .subpass = m_subpass,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1,
  };

  VK_CHECK(vkCreateGraphicsPipelines(
    device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &result.pipeline));

  // 8. Clean up temporary shader modules
  for (uint32_t i = 0; i < stageCount; ++i) {
    vkDestroyShaderModule(device, modulesToDestroy[i], nullptr);
  }

  return result;
}
