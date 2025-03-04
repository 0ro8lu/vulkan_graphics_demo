#include "engine/Passes/GBuffPass.h"

GBuffPass::GBuffPass(VulkanContext* vkContext,
                     const std::array<AttachmentData, 16>& attachmentData,
                     const Scene& scene,
                     const uint32_t attachmentWidth,
                     const uint32_t attachmentHeight)
  : IPassHelper(vkContext, scene)
{
  createAttachments(attachmentWidth, attachmentHeight);

  createRenderPass(attachmentData);

  createFrameBuffer(attachmentData);

  createGBufferPipeline(scene);
}

GBuffPass::~GBuffPass()
{
  vkDestroyRenderPass(vkContext->logicalDevice, renderPass, nullptr);

  vkDestroyPipeline(vkContext->logicalDevice, gbufferPipeline, nullptr);
  vkDestroyPipelineLayout(
    vkContext->logicalDevice, gbufferPipelineLayout, nullptr);

  delete positionAttachment;
  delete normalAttachment;
  delete albedoAttachment;
  delete depthAttachment;

  vkDestroyFramebuffer(vkContext->logicalDevice, gbufferFramebuffer, nullptr);
}

void
GBuffPass::draw(VulkanSwapchain* vkSwapchain, const Scene& scene)
{
  /*TODO: implement Sascha Willem's optimization: we can draw similar meshes
   with the vkCmdDrawIndexed() and just do all of them at once ^^. BUT: we would
   have to modify the shaders slightly:
             - vec4 tmpPos = inPos + ubo.instancePos[gl_InstanceIndex];

   we would employ the index for accessing the vertices. maybe try that someday
   */

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass;
  renderPassInfo.framebuffer = gbufferFramebuffer;
  renderPassInfo.renderArea.offset = { 0, 0 };
  renderPassInfo.renderArea.extent = vkSwapchain->swapChainExtent;

  std::array<VkClearValue, 4> clearValues;
  clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
  clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
  clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
  clearValues[3].depthStencil = { 1.0f, 0 };

  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(
    vkSwapchain->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // -------------------- bind main pipeline --------------------
  vkCmdBindPipeline(vkSwapchain->commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    gbufferPipeline);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)vkSwapchain->swapChainExtent.width;
  viewport.height = (float)vkSwapchain->swapChainExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = { 0, 0 };
  scissor.extent = vkSwapchain->swapChainExtent;
  vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          gbufferPipelineLayout,
                          0,
                          1,
                          &scene.cameraUBODescriptorset,
                          0,
                          nullptr);

  struct PushConstant
  {
    glm::mat4 model;
  };

  for (auto& model : scene.models) {

    VkBuffer vertexBuffers[] = { model.vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(
      vkSwapchain->commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(
      vkSwapchain->commandBuffer, model.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    for (const auto& instance : model.meshInstances) {
      PushConstant pc;
      pc.model = instance.transformation;
      vkCmdPushConstants(vkSwapchain->commandBuffer,
                         gbufferPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT,
                         0,
                         64,
                         &pc);

      vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              gbufferPipelineLayout,
                              1,
                              1,
                              &instance.mesh->descriptorSet,
                              0,
                              nullptr);

      vkCmdDrawIndexed(vkSwapchain->commandBuffer,
                       instance.mesh->indexCount,
                       1,
                       instance.mesh->startIndex,
                       0,
                       0);
    }
  }

  vkCmdEndRenderPass(vkSwapchain->commandBuffer);
};

void
GBuffPass::recreateAttachments(
  int width,
  int height,
  const std::array<AttachmentData, 16>& attachmentData)
{
  positionAttachment->resize(width, height);
  normalAttachment->resize(width, height);
  albedoAttachment->resize(width, height);
  depthAttachment->resize(width, height);

  vkDestroyFramebuffer(vkContext->logicalDevice, gbufferFramebuffer, nullptr);
  createFrameBuffer(attachmentData);
}

void
GBuffPass::createFrameBuffer(std::array<AttachmentData, 16> attachmentData)
{
  std::array<VkImageView, 4> attachments = { positionAttachment->view,
                                             normalAttachment->view,
                                             albedoAttachment->view,
                                             depthAttachment->view };

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = renderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = positionAttachment->width;
  framebufferInfo.height = positionAttachment->height;
  framebufferInfo.layers = 1;

  if (vkCreateFramebuffer(vkContext->logicalDevice,
                          &framebufferInfo,
                          nullptr,
                          &gbufferFramebuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to create framebuffer!");
  }
}

void
GBuffPass::createAttachments(uint32_t width, uint32_t height)
{
  positionAttachment = new FramebufferAttachment(
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    width,
    height,
    vkContext);

  normalAttachment = new FramebufferAttachment(
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    width,
    height,
    vkContext);

  albedoAttachment = new FramebufferAttachment(
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    width,
    height,
    vkContext);

  VkFormat depthFormat = vkContext->findSupportedFormat(
    { VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT },
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  depthAttachment = new FramebufferAttachment(
    depthFormat,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    width,
    height,
    vkContext);
}

void
GBuffPass::createRenderPass(std::array<AttachmentData, 16> attachmentData)
{
  std::array<VkAttachmentDescription, 4> attachmentDescriptions;

  // attachment for position
  attachmentDescriptions[0].format = positionAttachment->format;
  attachmentDescriptions[0].flags = 0;
  attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachmentDescriptions[0].finalLayout =
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  // attachment for normal
  attachmentDescriptions[1].format = normalAttachment->format;
  attachmentDescriptions[1].flags = 0;
  attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachmentDescriptions[1].finalLayout =
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  // attachment for albedo
  attachmentDescriptions[2].format = albedoAttachment->format;
  attachmentDescriptions[2].flags = 0;
  attachmentDescriptions[2].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachmentDescriptions[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachmentDescriptions[2].finalLayout =
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  // attachment for depth
  attachmentDescriptions[3].format = depthAttachment->format;
  attachmentDescriptions[3].flags = 0;
  attachmentDescriptions[3].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescriptions[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescriptions[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescriptions[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescriptions[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachmentDescriptions[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachmentDescriptions[3].finalLayout =
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  std::array<VkAttachmentReference, 3> attachmentReferences;
  attachmentReferences[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
  attachmentReferences[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
  attachmentReferences[2] = { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 3;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount =
    static_cast<uint32_t>(attachmentReferences.size());
  subpass.pColorAttachments = attachmentReferences.data();
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  std::array<VkSubpassDependency, 4> dependencies;

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  dependencies[0].dstAccessMask =
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[1].srcAccessMask =
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[2].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[2].dstSubpass = 0;
  dependencies[2].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[2].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[2].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[2].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  dependencies[2].dependencyFlags = 0;

  dependencies[3].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[3].dstSubpass = 0;
  dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[3].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[3].srcAccessMask = 0;
  dependencies[3].dstAccessMask =
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
  dependencies[3].dependencyFlags = 0;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount =
    static_cast<uint32_t>(attachmentDescriptions.size());
  renderPassInfo.pAttachments = attachmentDescriptions.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  if (vkCreateRenderPass(
        vkContext->logicalDevice, &renderPassInfo, nullptr, &renderPass) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

void
GBuffPass::createGBufferPipeline(const Scene& scene)
{
  std::string shaderPath = SHADER_PATH;
  auto vertShaderCode = readFile(shaderPath + "gbuffer_vert.spv");
  auto fragShaderCode = readFile(shaderPath + "gbuffer_frag.spv");

  VkShaderModule vertShaderModule =
    vkContext->createShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule =
    vkContext->createShaderModule(fragShaderCode);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo,
                                                     fragShaderStageInfo };

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount =
    static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPushConstantRange modelPCRange{};
  modelPCRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  modelPCRange.offset = 0;
  modelPCRange.size = 64;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f; // Optional
  depthStencil.maxDepthBounds = 1.0f; // Optional
  depthStencil.stencilTestEnable = VK_FALSE;
  depthStencil.front = {}; // Optional
  depthStencil.back = {};  // Optional

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = {
    colorBlendAttachment, colorBlendAttachment, colorBlendAttachment
  };

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount =
    static_cast<uint32_t>(blendAttachmentStates.size());
  colorBlending.pAttachments = blendAttachmentStates.data();
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
                                                VK_DYNAMIC_STATE_SCISSOR };

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts;
  descriptorSetLayouts[0] = scene.cameraUBOLayout;
  descriptorSetLayouts[1] = Model::textureLayout;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount =
    static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &modelPCRange;

  if (vkCreatePipelineLayout(vkContext->logicalDevice,
                             &pipelineLayoutInfo,
                             nullptr,
                             &gbufferPipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = gbufferPipelineLayout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(vkContext->logicalDevice,
                                VK_NULL_HANDLE,
                                1,
                                &pipelineInfo,
                                nullptr,
                                &gbufferPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  vkDestroyShaderModule(vkContext->logicalDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(vkContext->logicalDevice, vertShaderModule, nullptr);
}
