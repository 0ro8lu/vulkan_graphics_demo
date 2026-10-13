#include "engine/Passes/BlinnPhongPass.h"
#include "engine/VulkanCheck.h"

#include "engine/PipelineBuilder.h"
#include "engine/SceneRenderData.h"

BlinnPhongPass::BlinnPhongPass(VulkanContext* vkContext,
                               const AttachmentConfig& attachmentConfig,
                               const LayoutConfig& layoutConfig)
  : m_vkContext(vkContext)
{
  createAttachments(attachmentConfig.width, attachmentConfig.height);

  createRenderPass(attachmentConfig.depthFormat);

  createFrameBuffer(attachmentConfig.depthImageView);

  createPipelines(layoutConfig);
}

BlinnPhongPass::~BlinnPhongPass()
{
  vkDestroyRenderPass(m_vkContext->logicalDevice, m_renderPass, nullptr);

  vkDestroyPipeline(m_vkContext->logicalDevice, m_blinnPhongPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_vkContext->logicalDevice, m_blinnPhongPipelineLayout, nullptr);

  vkDestroyPipeline(m_vkContext->logicalDevice, m_skyboxPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_vkContext->logicalDevice, m_skyboxPipelineLayout, nullptr);

  vkDestroyPipeline(m_vkContext->logicalDevice, m_lightCubesPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_vkContext->logicalDevice, m_lightCubesPipelineLayout, nullptr);

  vkDestroyFramebuffer(m_vkContext->logicalDevice, m_hdrFramebuffer, nullptr);
}

void
BlinnPhongPass::draw(VkCommandBuffer commandBuffer,
                     VkExtent2D extent,
                     const SceneRenderData& scene,
                     VkDescriptorSet cameraUBODescriptorset,
                     VkDescriptorSet lightsUBODescriptorset,
                     VkDescriptorSet shadowMapDescriptorSet)
{
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_renderPass;
  renderPassInfo.framebuffer = m_hdrFramebuffer;
  renderPassInfo.renderArea.offset = { 0, 0 };
  renderPassInfo.renderArea.extent = extent;

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = { { 0.21f, 0.68f, 0.8f, 1.0f } };
  clearValues[1].depthStencil = { 1.0f, 0 };

  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(
    commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  // -------------------- bind main pipeline --------------------
  vkCmdBindPipeline(commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_blinnPhongPipeline);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)extent.width;
  viewport.height = (float)extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = { 0, 0 };
  scissor.extent = extent;
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_blinnPhongPipelineLayout,
                          0,
                          1,
                          &cameraUBODescriptorset,
                          0,
                          nullptr);

  vkCmdBindDescriptorSets(commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_blinnPhongPipelineLayout,
                          1,
                          1,
                          &lightsUBODescriptorset,
                          0,
                          nullptr);

  vkCmdBindDescriptorSets(commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_blinnPhongPipelineLayout,
                          3,
                          1,
                          &shadowMapDescriptorSet,
                          0,
                          nullptr);

  struct PushConstant
  {
    glm::mat4 model;
  };

  for (const auto& model : scene.models) {

    VkBuffer vertexBuffers[] = { model.m_vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(
      commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(
      commandBuffer, model.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    for (const auto& instance : model.m_meshInstances) {
      PushConstant pc;
      pc.model = instance.transformation;
      vkCmdPushConstants(commandBuffer,
                         m_blinnPhongPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT,
                         0,
                         sizeof(PushConstant),
                         &pc);

      vkCmdBindDescriptorSets(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_blinnPhongPipelineLayout,
                              2,
                              1,
                              &instance.mesh->m_descriptorSet,
                              0,
                              nullptr);

      vkCmdDrawIndexed(commandBuffer,
                       instance.mesh->m_indexCount,
                       1,
                       instance.mesh->m_startIndex,
                       0,
                       0);
    }
  }

  // -------------------- bind lightCubes pipeline --------------------
  vkCmdBindPipeline(commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_lightCubesPipeline);

  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_lightCubesPipelineLayout,
                          0,
                          1,
                          &cameraUBODescriptorset,
                          0,
                          nullptr);

  struct LightColor
  {
    glm::vec4 lightColor;
  };

  for (size_t i = 0; i < scene.lightCubes.size(); i++) {
    VkBuffer vertexBuffers[] = { scene.lightCubes[i].m_vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(
      commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer,
                         scene.lightCubes[i].m_indexBuffer,
                         0,
                         VK_INDEX_TYPE_UINT32);

    for (const auto& instance : scene.lightCubes[i].m_meshInstances) {
      PushConstant pc;
      pc.model = instance.transformation;
      vkCmdPushConstants(commandBuffer,
                         m_lightCubesPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT,
                         0,
                         64,
                         &pc);

      LightColor lightColor;
      glm::vec3 color = scene.pointLights[i].getColor();
      lightColor.lightColor = glm::vec4(color, 1.0);
      vkCmdPushConstants(commandBuffer,
                         m_lightCubesPipelineLayout,
                         VK_SHADER_STAGE_FRAGMENT_BIT,
                         sizeof(PushConstant),
                         sizeof(LightColor),
                         &lightColor);

      vkCmdDrawIndexed(commandBuffer,
                       instance.mesh->m_indexCount,
                       1,
                       instance.mesh->m_startIndex,
                       0,
                       0);
    }
  }

  // -------------------- bind skybox pipeline --------------------
  if (scene.skybox != nullptr) {
    vkCmdBindPipeline(commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_skyboxPipeline);

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_skyboxPipelineLayout,
                            0,
                            1,
                            &cameraUBODescriptorset,
                            0,
                            nullptr);

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_skyboxPipelineLayout,
                            1,
                            1,
                            &scene.skybox->m_descriptorSet,
                            0,
                            nullptr);

    VkBuffer vertexBuffers[] = { scene.skybox->m_cube->m_vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(
      commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer,
                         scene.skybox->m_cube->m_indexBuffer,
                         0,
                         VK_INDEX_TYPE_UINT32);

    for (const auto& instance : scene.skybox->m_cube->m_meshInstances) {
      PushConstant pc;
      pc.model = instance.transformation;
      vkCmdPushConstants(commandBuffer,
                         m_skyboxPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT,
                         0,
                         sizeof(PushConstant),
                         &pc);

      vkCmdDrawIndexed(commandBuffer,
                       instance.mesh->m_indexCount,
                       1,
                       instance.mesh->m_startIndex,
                       0,
                       0);
    }
  }

  vkCmdEndRenderPass(commandBuffer);
}

void
BlinnPhongPass::recreateAttachments(int width,
                                    int height,
                                    VkImageView depthImageView)
{
  m_hdrAttachment->resize(width, height);
  vkDestroyFramebuffer(m_vkContext->logicalDevice, m_hdrFramebuffer, nullptr);
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

    vkUpdateDescriptorSets(m_vkContext->logicalDevice,
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

    vkUpdateDescriptorSets(m_vkContext->logicalDevice,
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(),
                           0,
                           nullptr);
  }
}

void
BlinnPhongPass::createFrameBuffer(VkImageView depthImageView)
{
  std::array<VkImageView, 2> attachments = { m_hdrAttachment->getView(),
                                             depthImageView };

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = m_renderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = m_hdrAttachment->getWidth();
  framebufferInfo.height = m_hdrAttachment->getHeight();
  framebufferInfo.layers = 1;

  VK_CHECK(vkCreateFramebuffer(
    m_vkContext->logicalDevice, &framebufferInfo, nullptr, &m_hdrFramebuffer));
}

void
BlinnPhongPass::createAttachments(uint32_t width, uint32_t height)
{
  FramebufferAttachment::CreateInfo hdrInfo{};
  hdrInfo.width = width;
  hdrInfo.height = height;
  hdrInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  hdrInfo.layerCount = 1;
  hdrInfo.usage =
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  hdrInfo.vkContext = m_vkContext;
  m_hdrAttachment = FramebufferAttachment::create(hdrInfo);
}

void
BlinnPhongPass::createRenderPass(VkFormat depthFormat)
{
  // attachment for HDR
  VkAttachmentDescription hdrAttachmentDescription{};
  hdrAttachmentDescription.format = m_hdrAttachment->getFormat();
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

  VK_CHECK(vkCreateRenderPass(
    m_vkContext->logicalDevice, &renderPassInfo, nullptr, &m_renderPass));
}

void
BlinnPhongPass::createPipelines(const LayoutConfig& layoutConfig)
{
  VkPushConstantRange modelPC{
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    .offset = 0,
    .size = 64,
  };

  // Main Blinn-Phong pipeline
  {
    std::array layouts{ layoutConfig.cameraLayout,
                        layoutConfig.lightsLayout,
                        Model::s_textureLayout,
                        layoutConfig.directionalShadowmapLayout };

    auto result = PipelineBuilder{}
                    .setVertexShader("texture_vert.spv")
                    .setFragmentShader("texture_frag.spv")
                    .withDefaultMeshVertexInput()
                    .setDescriptorSetLayouts(layouts)
                    .setPushConstantRanges(std::span(&modelPC, 1))
                    .setRenderPass(m_renderPass)
                    .build(m_vkContext->logicalDevice);

    m_blinnPhongPipeline = result.pipeline;
    m_blinnPhongPipelineLayout = result.layout;
  }

  // Skybox pipeline
  {
    std::array layouts{ layoutConfig.cameraLayout, Skybox::s_skyboxLayout };

    auto result = PipelineBuilder{}
                    .setVertexShader("skybox_vert.spv")
                    .setFragmentShader("skybox_frag.spv")
                    .withDefaultMeshVertexInput()
                    .setCullMode(VK_CULL_MODE_FRONT_BIT)
                    .setDescriptorSetLayouts(layouts)
                    .setPushConstantRanges(std::span(&modelPC, 1))
                    .setRenderPass(m_renderPass)
                    .build(m_vkContext->logicalDevice);

    m_skyboxPipeline = result.pipeline;
    m_skyboxPipelineLayout = result.layout;
  }

  // Light cubes pipeline
  {
    VkPushConstantRange colorPC{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 64,
      .size = 16,
    };
    std::array pushConstants{ modelPC, colorPC };
    std::array layouts{ layoutConfig.cameraLayout };

    auto result = PipelineBuilder{}
                    .setVertexShader("light_cube_vert.spv")
                    .setFragmentShader("light_cube_frag.spv")
                    .withDefaultMeshVertexInput()
                    .setDescriptorSetLayouts(layouts)
                    .setPushConstantRanges(pushConstants)
                    .setRenderPass(m_renderPass)
                    .build(m_vkContext->logicalDevice);

    m_lightCubesPipeline = result.pipeline;
    m_lightCubesPipelineLayout = result.layout;
  }
}
