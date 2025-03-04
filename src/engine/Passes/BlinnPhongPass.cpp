#include "engine/Passes/BlinnPhongPass.h"
#include "engine/ModelLoading/Model.h"
#include "engine/Vertex.h"

BlinnPhongPass::BlinnPhongPass(
  VulkanContext* vkContext,
  const std::array<AttachmentData, 16>& attachmentData,
  const Scene& scene,
  const uint32_t attachmentWidth,
  const uint32_t attachmentHeight)
  : IPassHelper(vkContext, scene)
{
  createAttachments(attachmentWidth, attachmentHeight);

  createRenderPass(attachmentData);

  createFrameBuffer(attachmentData);

  createMainPipeline(scene);
  createSkyboxPipeline(scene);
  createLightCubesPipeline(scene);
}

BlinnPhongPass::~BlinnPhongPass()
{
  vkDestroyRenderPass(vkContext->logicalDevice, renderPass, nullptr);

  vkDestroyPipeline(vkContext->logicalDevice, blinnPhongPipeline, nullptr);
  vkDestroyPipelineLayout(
    vkContext->logicalDevice, blinnPhongPipelineLayout, nullptr);

  vkDestroyPipeline(vkContext->logicalDevice, skyboxPipeline, nullptr);
  vkDestroyPipelineLayout(
    vkContext->logicalDevice, skyboxPipelineLayout, nullptr);

  vkDestroyPipeline(vkContext->logicalDevice, lightCubesPipeline, nullptr);
  vkDestroyPipelineLayout(
    vkContext->logicalDevice, lightCubesPipelineLayout, nullptr);

  delete hdrAttachment;
  vkDestroyFramebuffer(vkContext->logicalDevice, hdrFramebuffer, nullptr);
}

void
BlinnPhongPass::draw(VulkanSwapchain* vkSwapchain, const Scene& scene)
{
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass;
  renderPassInfo.framebuffer = hdrFramebuffer;
  renderPassInfo.renderArea.offset = { 0, 0 };
  renderPassInfo.renderArea.extent = vkSwapchain->swapChainExtent;

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = { { 0.21f, 0.68f, 0.8f, 1.0f } };
  clearValues[1].depthStencil = { 1.0f, 0 };

  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(
    vkSwapchain->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // -------------------- bind main pipeline --------------------
  vkCmdBindPipeline(vkSwapchain->commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    blinnPhongPipeline);

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
                          blinnPhongPipelineLayout,
                          0,
                          1,
                          &scene.cameraUBODescriptorset,
                          0,
                          nullptr);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          blinnPhongPipelineLayout,
                          1,
                          1,
                          &scene.lightsUBODescriptorset,
                          0,
                          nullptr);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          blinnPhongPipelineLayout,
                          3,
                          1,
                          &scene.directionalLightSpaceDescriptorSet,
                          0,
                          nullptr);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          blinnPhongPipelineLayout,
                          4,
                          1,
                          &scene.directionalShadowMapDescriptorSet,
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
                         blinnPhongPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT,
                         0,
                         64,
                         &pc);

      vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              blinnPhongPipelineLayout,
                              2,
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

  // -------------------- bind lightCubes pipeline --------------------
  vkCmdBindPipeline(vkSwapchain->commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    lightCubesPipeline);

  vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          lightCubesPipelineLayout,
                          0,
                          1,
                          &scene.cameraUBODescriptorset,
                          0,
                          nullptr);

  struct LightColor
  {
    glm::vec4 lightColor;
  };

  for (int i = 0; i < scene.lightCubes.size(); i++) {
    VkBuffer vertexBuffers[] = { scene.lightCubes[i].vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(
      vkSwapchain->commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(vkSwapchain->commandBuffer,
                         scene.lightCubes[i].indexBuffer,
                         0,
                         VK_INDEX_TYPE_UINT32);

    for (const auto& instance : scene.lightCubes[i].meshInstances) {
      PushConstant pc;
      pc.model = instance.transformation;
      vkCmdPushConstants(vkSwapchain->commandBuffer,
                         blinnPhongPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT,
                         0,
                         64,
                         &pc);

      LightColor lightColor;
      glm::vec3 color = scene.pointLights[i].color;
      lightColor.lightColor = glm::vec4(color, 1.0);
      vkCmdPushConstants(vkSwapchain->commandBuffer,
                         lightCubesPipelineLayout,
                         VK_SHADER_STAGE_FRAGMENT_BIT,
                         64,
                         16,
                         &lightColor);

      vkCmdDrawIndexed(vkSwapchain->commandBuffer,
                       instance.mesh->indexCount,
                       1,
                       instance.mesh->startIndex,
                       0,
                       0);
    }
  }

  // -------------------- bind skybox pipeline --------------------
  vkCmdBindPipeline(vkSwapchain->commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    skyboxPipeline);

  vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          skyboxPipelineLayout,
                          0,
                          1,
                          &scene.cameraUBODescriptorset,
                          0,
                          nullptr);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          skyboxPipelineLayout,
                          1,
                          1,
                          &scene.skybox->descriptorSet,
                          0,
                          nullptr);

  VkBuffer vertexBuffers[] = { scene.skybox->cube->vertexBuffer };
  VkDeviceSize offsets[] = { 0 };
  vkCmdBindVertexBuffers(
    vkSwapchain->commandBuffer, 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(vkSwapchain->commandBuffer,
                       scene.skybox->cube->indexBuffer,
                       0,
                       VK_INDEX_TYPE_UINT32);

  for (const auto& instance : scene.skybox->cube->meshInstances) {
    PushConstant pc;
    pc.model = instance.transformation;
    vkCmdPushConstants(vkSwapchain->commandBuffer,
                       skyboxPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       64,
                       &pc);

    vkCmdDrawIndexed(vkSwapchain->commandBuffer,
                     instance.mesh->indexCount,
                     1,
                     instance.mesh->startIndex,
                     0,
                     0);
  }

  vkCmdEndRenderPass(vkSwapchain->commandBuffer);
}

void
BlinnPhongPass::recreateAttachments(
  int width,
  int height,
  const std::array<AttachmentData, 16>& attachmentData)
{
  hdrAttachment->resize(width, height);
  vkDestroyFramebuffer(vkContext->logicalDevice, hdrFramebuffer, nullptr);
  createFrameBuffer(attachmentData);
}

void
BlinnPhongPass::updateDescriptors(
  const std::array<FramebufferAttachment*, 16>& attachments)
{
  VkDescriptorImageInfo shadowMapImageInfo{};
  shadowMapImageInfo.imageLayout =
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  shadowMapImageInfo.imageView = attachments[0]->view;
  shadowMapImageInfo.sampler = attachments[0]->sampler;

  std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

  descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrites[0].dstSet = scene.directionalShadowMapDescriptorSet;
  descriptorWrites[0].dstBinding = 0;
  descriptorWrites[0].dstArrayElement = 0;
  descriptorWrites[0].descriptorType =
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrites[0].descriptorCount = 1;
  descriptorWrites[0].pImageInfo = &shadowMapImageInfo;

  vkUpdateDescriptorSets(vkContext->logicalDevice,
                         static_cast<uint32_t>(descriptorWrites.size()),
                         descriptorWrites.data(),
                         0,
                         nullptr);
}

void
BlinnPhongPass::createFrameBuffer(std::array<AttachmentData, 16> attachmentData)
{
  std::array<VkImageView, 2> attachments = { hdrAttachment->view,
                                             attachmentData[0].view };

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = renderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = hdrAttachment->width;
  framebufferInfo.height = hdrAttachment->height;
  framebufferInfo.layers = 1;

  if (vkCreateFramebuffer(
        vkContext->logicalDevice, &framebufferInfo, nullptr, &hdrFramebuffer) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create framebuffer!");
  }
}

void
BlinnPhongPass::createAttachments(uint32_t width, uint32_t height)
{
  hdrAttachment = new FramebufferAttachment(
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    width,
    height,
    vkContext);
}

void
BlinnPhongPass::createRenderPass(std::array<AttachmentData, 16> attachmentData)
{
  // attachment for HDR
  VkAttachmentDescription hdrAttachmentDescription{};
  hdrAttachmentDescription.format = hdrAttachment->format;
  hdrAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
  hdrAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  hdrAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  hdrAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  hdrAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  hdrAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  hdrAttachmentDescription.finalLayout =
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  // attachment for depth
  VkAttachmentDescription depthAttachment{};
  depthAttachment.format = attachmentData[0].format;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout =
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference hdrAttachmentRef{};
  hdrAttachmentRef.attachment = 0;
  hdrAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &hdrAttachmentRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  std::array<VkAttachmentDescription, 2> attachments = {
    hdrAttachmentDescription, depthAttachment
  };

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(
        vkContext->logicalDevice, &renderPassInfo, nullptr, &renderPass) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

void
BlinnPhongPass::createMainPipeline(const Scene& scene)
{
  std::string shaderPath = SHADER_PATH;
  auto vertShaderCode = readFile(shaderPath + "texture_vert.spv");
  auto fragShaderCode = readFile(shaderPath + "texture_frag.spv");

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

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
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

  std::array<VkDescriptorSetLayout, 5> descriptorSetLayouts;
  descriptorSetLayouts[0] = scene.cameraUBOLayout;
  descriptorSetLayouts[1] = scene.lightsUBOLayout;
  descriptorSetLayouts[2] = Model::textureLayout;
  descriptorSetLayouts[3] = scene.directionalLightSpaceLayout;
  descriptorSetLayouts[4] = scene.directionalShadowMapLayout;

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
                             &blinnPhongPipelineLayout) != VK_SUCCESS) {
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
  pipelineInfo.layout = blinnPhongPipelineLayout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(vkContext->logicalDevice,
                                VK_NULL_HANDLE,
                                1,
                                &pipelineInfo,
                                nullptr,
                                &blinnPhongPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  vkDestroyShaderModule(vkContext->logicalDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(vkContext->logicalDevice, vertShaderModule, nullptr);
}

void
BlinnPhongPass::createSkyboxPipeline(const Scene& scene)
{
  std::string shaderPath = SHADER_PATH;
  auto vertShaderCode = readFile(shaderPath + "skybox_vert.spv");
  auto fragShaderCode = readFile(shaderPath + "skybox_frag.spv");

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
  rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
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

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
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
  descriptorSetLayouts[1] = scene.skybox->skyboxLayout;

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
                             &skyboxPipelineLayout) != VK_SUCCESS) {
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
  pipelineInfo.layout = skyboxPipelineLayout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(vkContext->logicalDevice,
                                VK_NULL_HANDLE,
                                1,
                                &pipelineInfo,
                                nullptr,
                                &skyboxPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  vkDestroyShaderModule(vkContext->logicalDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(vkContext->logicalDevice, vertShaderModule, nullptr);
}

void
BlinnPhongPass::createLightCubesPipeline(const Scene& scene)
{
  std::string shaderPath = SHADER_PATH;
  auto vertShaderCode = readFile(shaderPath + "light_cube_vert.spv");
  auto fragShaderCode = readFile(shaderPath + "light_cube_frag.spv");

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

  std::array<VkPushConstantRange, 2> pcRanges;
  pcRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pcRanges[0].offset = 0;
  pcRanges[0].size = 64;

  pcRanges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  pcRanges[1].offset = 64;
  pcRanges[1].size = 16;

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

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
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

  std::array<VkDescriptorSetLayout, 1> descriptorSetLayouts;
  descriptorSetLayouts[0] = scene.cameraUBOLayout;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount =
    static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount =
    static_cast<uint32_t>(pcRanges.size());
  pipelineLayoutInfo.pPushConstantRanges = pcRanges.data();

  if (vkCreatePipelineLayout(vkContext->logicalDevice,
                             &pipelineLayoutInfo,
                             nullptr,
                             &lightCubesPipelineLayout) != VK_SUCCESS) {
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
  pipelineInfo.layout = lightCubesPipelineLayout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(vkContext->logicalDevice,
                                VK_NULL_HANDLE,
                                1,
                                &pipelineInfo,
                                nullptr,
                                &lightCubesPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  vkDestroyShaderModule(vkContext->logicalDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(vkContext->logicalDevice, vertShaderModule, nullptr);
}
