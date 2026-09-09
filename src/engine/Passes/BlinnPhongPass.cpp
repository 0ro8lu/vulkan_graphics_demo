#include "engine/Passes/BlinnPhongPass.h"

#include "engine/RenderUtils.h"
#include "engine/Scene.h"

BlinnPhongPass::BlinnPhongPass(VulkanContext* vkContext,
                               const AttachmentConfig& attachmentConfig,
                               const LayoutConfig& layoutConfig)
  : vkContext(vkContext)
{
  createAttachments(attachmentConfig.width, attachmentConfig.height);

  createRenderPass(attachmentConfig.depthFormat);

  createFrameBuffer(attachmentConfig.depthImageView);

  createPipelines(layoutConfig.camera,
                  layoutConfig.lights,
                  layoutConfig.directionalShadowmap);
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

  vkDestroyFramebuffer(vkContext->logicalDevice, hdrFramebuffer, nullptr);
}

void
BlinnPhongPass::draw(VulkanSwapchain* vkSwapchain,
                     const Scene& scene,
                     VkDescriptorSet cameraUBODescriptorset,
                     VkDescriptorSet lightsUBODescriptorset,
                     VkDescriptorSet shadowMapDescriptorSet)
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
                          &cameraUBODescriptorset,
                          0,
                          nullptr);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          blinnPhongPipelineLayout,
                          1,
                          1,
                          &lightsUBODescriptorset,
                          0,
                          nullptr);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          blinnPhongPipelineLayout,
                          3,
                          1,
                          &shadowMapDescriptorSet,
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
                         sizeof(PushConstant),
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
                          &cameraUBODescriptorset,
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
                         lightCubesPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT,
                         0,
                         64,
                         &pc);

      LightColor lightColor;
      glm::vec3 color = scene.pointLights[i].getColor();
      lightColor.lightColor = glm::vec4(color, 1.0);
      vkCmdPushConstants(vkSwapchain->commandBuffer,
                         lightCubesPipelineLayout,
                         VK_SHADER_STAGE_FRAGMENT_BIT,
                         sizeof(PushConstant),
                         sizeof(LightColor),
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
                          &cameraUBODescriptorset,
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
                       sizeof(PushConstant),
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
BlinnPhongPass::recreateAttachments(int width,
                                    int height,
                                    VkImageView depthImageView)
{
  hdrAttachment->resize(width, height);
  vkDestroyFramebuffer(vkContext->logicalDevice, hdrFramebuffer, nullptr);
  createFrameBuffer(depthImageView);
}

// TODO: this should be in Shadowmap Pass?
void
BlinnPhongPass::updateDescriptors(
  const std::unique_ptr<FramebufferAttachment>& directionalShadowmap,
  const std::unique_ptr<FramebufferAttachment>& spotPointShadowAtlas,
  VkDescriptorSet shadowMapDescriptorSet)
{
  {
    VkDescriptorImageInfo shadowMapImageInfo{};
    shadowMapImageInfo.imageLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    shadowMapImageInfo.imageView = directionalShadowmap->getView();
    shadowMapImageInfo.sampler = directionalShadowmap->getSampler();

    std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = shadowMapDescriptorSet;
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

  {
    VkDescriptorImageInfo shadowAtlasImageInfo{};
    shadowAtlasImageInfo.imageLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    shadowAtlasImageInfo.imageView = spotPointShadowAtlas->getView();
    shadowAtlasImageInfo.sampler = spotPointShadowAtlas->getSampler();

    std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = shadowMapDescriptorSet;
    descriptorWrites[0].dstBinding = 1;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &shadowAtlasImageInfo;

    vkUpdateDescriptorSets(vkContext->logicalDevice,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(),
                           0,
                           nullptr);
  }
}

void
BlinnPhongPass::createFrameBuffer(VkImageView depthImageView)
{
  std::array<VkImageView, 2> attachments = { hdrAttachment->getView(),
                                             depthImageView };

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = renderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = hdrAttachment->getWidth();
  framebufferInfo.height = hdrAttachment->getHeight();
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
  FramebufferAttachment::CreateInfo hdrInfo{};
  hdrInfo.width = width;
  hdrInfo.height = height;
  hdrInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  hdrInfo.layerCount = 1;
  hdrInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  hdrInfo.vkContext = vkContext;
  hdrAttachment = FramebufferAttachment::create(hdrInfo);
}

void
BlinnPhongPass::createRenderPass(VkFormat depthFormat)
{
  // attachment for HDR
  VkAttachmentDescription hdrAttachmentDescription{};
  hdrAttachmentDescription.format = hdrAttachment->getFormat();
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
  depthAttachment.format = depthFormat;
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
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
BlinnPhongPass::createPipelines(
  VkDescriptorSetLayout cameraLayout,
  VkDescriptorSetLayout lightsLayout,
  VkDescriptorSetLayout directionalShadowmapLayout)
{
  VkPushConstantRange modelPC{};
  modelPC.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  modelPC.offset = 0;
  modelPC.size = 64;

  // Main Blinn-Phong pipeline
  {
    GraphicsPipelineConfig config{};
    config.vertShaderName = "texture_vert.spv";
    config.fragShaderName = "texture_frag.spv";
    config.descriptorSetLayouts = { cameraLayout,
                                    lightsLayout,
                                    Model::textureLayout,
                                    directionalShadowmapLayout };
    config.pushConstantRanges = { modelPC };
    config.renderPass = renderPass;

    auto result = createPipeline(vkContext, config);
    blinnPhongPipeline = result.pipeline;
    blinnPhongPipelineLayout = result.layout;
  }

  // Skybox pipeline
  {
    GraphicsPipelineConfig config{};
    config.vertShaderName = "skybox_vert.spv";
    config.fragShaderName = "skybox_frag.spv";

    config.cullMode = VK_CULL_MODE_FRONT_BIT;

    config.descriptorSetLayouts = { cameraLayout, Skybox::skyboxLayout };
    config.pushConstantRanges = { modelPC };
    config.renderPass = renderPass;
    config.subpass = 0;

    auto result = createPipeline(vkContext, config);
    skyboxPipeline = result.pipeline;
    skyboxPipelineLayout = result.layout;
  }

  // Light cubes pipeline
  {
    VkPushConstantRange colorPC{};
    colorPC.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    colorPC.offset = 64;
    colorPC.size = 16;

    GraphicsPipelineConfig config{};
    config.vertShaderName = "light_cube_vert.spv";
    config.fragShaderName = "light_cube_frag.spv";

    config.descriptorSetLayouts = { cameraLayout };
    config.pushConstantRanges = { modelPC, colorPC };
    config.renderPass = renderPass;
    config.subpass = 0;

    auto result = createPipeline(vkContext, config);
    lightCubesPipeline = result.pipeline;
    lightCubesPipelineLayout = result.layout;
  }
}
