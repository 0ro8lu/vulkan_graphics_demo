#include "engine/Passes/ShadowMapPass.h"
#include "engine/VulkanCheck.h"

#include "engine/RenderUtils.h"
#include "engine/Scene.h"

ShadowMapPass::ShadowMapPass(VulkanContext* vkContext,
                             const AttachmentConfig& attachmentConfig)
  : m_directionalShadowmapSize(attachmentConfig.directionalAtlasSize)
  , m_spotPointShadowmapSize(attachmentConfig.spotPointAtlasSize)
  , m_vkContext(vkContext)
{
  createShadowMaps(attachmentConfig.depthFormat);

  createDirectionalRenderPass();

  createFrameBuffers();

  createShadowMapPipeline();
}

ShadowMapPass::~ShadowMapPass()
{
  vkDestroyFramebuffer(
    m_vkContext->logicalDevice, m_directionalShadowMapFramebuffer, nullptr);
  vkDestroyFramebuffer(
    m_vkContext->logicalDevice, m_spotShadowMapFramebuffer, nullptr);

  vkDestroyRenderPass(
    m_vkContext->logicalDevice, m_shadowMapRenderPass, nullptr);

  vkDestroyPipeline(m_vkContext->logicalDevice, m_shadowMapPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_vkContext->logicalDevice, m_shadowMapPipelineLayout, nullptr);
}

void
ShadowMapPass::createShadowMaps(VkFormat depthImageFormat)
{
  FramebufferAttachment::CreateInfo dirInfo{};
  dirInfo.width = m_directionalShadowmapSize;
  dirInfo.height = m_directionalShadowmapSize;
  dirInfo.format = depthImageFormat;
  dirInfo.layerCount = 1;
  dirInfo.usage =
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  dirInfo.vkContext = m_vkContext;
  dirInfo.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  m_directionalShadowMap = FramebufferAttachment::create(dirInfo);

  // TODO: if not spotligts, nor pointlights cast shadows, don't create it.
  FramebufferAttachment::CreateInfo spotInfo{};
  spotInfo.width = m_spotPointShadowmapSize;
  spotInfo.height = m_spotPointShadowmapSize;
  spotInfo.format = depthImageFormat;
  spotInfo.layerCount = 1;
  spotInfo.usage =
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  spotInfo.vkContext = m_vkContext;
  m_spotPointShadowAtlas = FramebufferAttachment::create(spotInfo);
}

void
ShadowMapPass::draw(VulkanSwapchain* vkSwapchain, const Scene& scene)
{
  // directional shadow
  if (scene.m_directionalLight->castsShadow()) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_shadowMapRenderPass;
    renderPassInfo.framebuffer = m_directionalShadowMapFramebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent.width = m_directionalShadowMap->getWidth();
    renderPassInfo.renderArea.extent.height =
      m_directionalShadowMap->getHeight();

    VkClearValue depthClearValue = { .depthStencil = { 1.0f, 0 } };

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &depthClearValue;

    vkCmdBeginRenderPass(
      vkSwapchain->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // -------------------- bind main pipeline --------------------
    vkCmdBindPipeline(vkSwapchain->commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_shadowMapPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = m_directionalShadowMap->getWidth();
    viewport.height = m_directionalShadowMap->getHeight();
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent.width = m_directionalShadowMap->getWidth();
    scissor.extent.height = m_directionalShadowMap->getHeight();
    scissor.offset.x = 0;
    scissor.offset.y = 0;

    vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

    struct PushConstant
    {
      glm::mat4 lightSpaceMatrix;
    };

    for (auto& model : scene.m_models) {

      VkBuffer vertexBuffers[] = { model.m_vertexBuffer };
      VkDeviceSize offsets[] = { 0 };
      vkCmdBindVertexBuffers(
        vkSwapchain->commandBuffer, 0, 1, vertexBuffers, offsets);

      vkCmdBindIndexBuffer(
        vkSwapchain->commandBuffer, model.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

      for (const auto& instance : model.m_meshInstances) {
        PushConstant pc;
        pc.lightSpaceMatrix =
          scene.m_directionalLight->getTransform() * instance.transformation;

        vkCmdPushConstants(vkSwapchain->commandBuffer,
                           m_shadowMapPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(PushConstant),
                           &pc);

        vkCmdDrawIndexed(vkSwapchain->commandBuffer,
                         instance.mesh->m_indexCount,
                         1,
                         instance.mesh->m_startIndex,
                         0,
                         0);
      }
    }
    vkCmdEndRenderPass(vkSwapchain->commandBuffer);
  } else {
    // this is imprtant! we still have to transition the image layout even tho
    // we don't use it later on! Thanks daddy Vulkan :3
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_shadowMapRenderPass;
    renderPassInfo.framebuffer = m_directionalShadowMapFramebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent.width = m_directionalShadowMap->getWidth();
    renderPassInfo.renderArea.extent.height =
      m_directionalShadowMap->getHeight();

    VkClearValue depthClearValue = { .depthStencil = { 1.0f, 0 } };

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &depthClearValue;

    vkCmdBeginRenderPass(
      vkSwapchain->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(vkSwapchain->commandBuffer);
  }

  // spotlight and pointlight shadows
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_shadowMapRenderPass;
    renderPassInfo.framebuffer = m_spotShadowMapFramebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent.width = m_spotPointShadowAtlas->getWidth();
    renderPassInfo.renderArea.extent.height =
      m_spotPointShadowAtlas->getHeight();

    VkClearValue depthClearValue = { .depthStencil = { 1.0f, 0 } };

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &depthClearValue;

    vkCmdBeginRenderPass(
      vkSwapchain->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // -------------------- bind main pipeline --------------------
    vkCmdBindPipeline(vkSwapchain->commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_shadowMapPipeline);

    for (const auto& spotlight : scene.m_spotLights) {

      if (!spotlight.castsShadow()) {
        continue;
      }

      VkViewport viewport{};
      viewport.x = spotlight.getAtlasCoordinatesPixel().x;
      viewport.y = spotlight.getAtlasCoordinatesPixel().y;
      viewport.width = spotlight.getAtlasCoordinatesPixel().z;
      viewport.height = spotlight.getAtlasCoordinatesPixel().w;
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);

      VkRect2D scissor{};
      scissor.offset.x = spotlight.getAtlasCoordinatesPixel().x;
      scissor.offset.y = spotlight.getAtlasCoordinatesPixel().y;
      scissor.extent.width = spotlight.getAtlasCoordinatesPixel().z;
      scissor.extent.height = spotlight.getAtlasCoordinatesPixel().w;

      vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

      // vkCmdSetDepthBias(vkSwapchain->commandBuffer, 1.5f, 0.0f, 2.0f);
      // vkCmdSetDepthBias(vkSwapchain->commandBuffer, 1.25f, 0.0f, 1.75f);

      struct PushConstant
      {
        glm::mat4 lightSpaceMatrix;
      };

      for (auto& model : scene.m_models) {

        VkBuffer vertexBuffers[] = { model.m_vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(
          vkSwapchain->commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(vkSwapchain->commandBuffer,
                             model.m_indexBuffer,
                             0,
                             VK_INDEX_TYPE_UINT32);

        for (const auto& instance : model.m_meshInstances) {
          PushConstant pc;
          pc.lightSpaceMatrix =
            spotlight.getTransform() * instance.transformation;

          vkCmdPushConstants(vkSwapchain->commandBuffer,
                             m_shadowMapPipelineLayout,
                             VK_SHADER_STAGE_VERTEX_BIT,
                             0,
                             sizeof(PushConstant),
                             &pc);

          vkCmdDrawIndexed(vkSwapchain->commandBuffer,
                           instance.mesh->m_indexCount,
                           1,
                           instance.mesh->m_startIndex,
                           0,
                           0);
        }
      }
    }
    for (const auto& pointlight : scene.m_pointLights) {
      if (!pointlight.castsShadow()) {
        continue;
      }

      for (int i = 0; i <= PointLight::BACK; i++) {
        PointLight::Side side = static_cast<PointLight::Side>(i);

        VkViewport viewport{};
        viewport.x = pointlight.getAtlasCoordinatesPixel(side).x;
        viewport.y = pointlight.getAtlasCoordinatesPixel(side).y;
        viewport.width = pointlight.getAtlasCoordinatesPixel(side).z;
        viewport.height = pointlight.getAtlasCoordinatesPixel(side).w;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset.x = pointlight.getAtlasCoordinatesPixel(side).x;
        scissor.offset.y = pointlight.getAtlasCoordinatesPixel(side).y;
        scissor.extent.width = pointlight.getAtlasCoordinatesPixel(side).z;
        scissor.extent.height = pointlight.getAtlasCoordinatesPixel(side).w;

        vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

        // vkCmdSetDepthBias(vkSwapchain->commandBuffer, 1.5f, 0.0f, 2.0f);
        // vkCmdSetDepthBias(vkSwapchain->commandBuffer, 1.25f, 0.0f, 1.75f);

        struct PushConstant
        {
          glm::mat4 lightSpaceMatrix;
        };

        for (auto& model : scene.m_models) {

          VkBuffer vertexBuffers[] = { model.m_vertexBuffer };
          VkDeviceSize offsets[] = { 0 };
          vkCmdBindVertexBuffers(
            vkSwapchain->commandBuffer, 0, 1, vertexBuffers, offsets);

          vkCmdBindIndexBuffer(vkSwapchain->commandBuffer,
                               model.m_indexBuffer,
                               0,
                               VK_INDEX_TYPE_UINT32);

          for (const auto& instance : model.m_meshInstances) {
            PushConstant pc;
            pc.lightSpaceMatrix =
              pointlight.getTransform(side) * instance.transformation;

            vkCmdPushConstants(vkSwapchain->commandBuffer,
                               m_shadowMapPipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(PushConstant),
                               &pc);

            vkCmdDrawIndexed(vkSwapchain->commandBuffer,
                             instance.mesh->m_indexCount,
                             1,
                             instance.mesh->m_startIndex,
                             0,
                             0);
          }
        }
      }
    }
    vkCmdEndRenderPass(vkSwapchain->commandBuffer);
  }
}

void
ShadowMapPass::createFrameBuffers()
{
  // for directional lights
  {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_shadowMapRenderPass;
    framebufferInfo.attachmentCount = 1;
    VkImageView dirView = m_directionalShadowMap->getView();
    framebufferInfo.pAttachments = &dirView;
    framebufferInfo.width = m_directionalShadowmapSize;
    framebufferInfo.height = m_directionalShadowmapSize;
    framebufferInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(m_vkContext->logicalDevice,
                                 &framebufferInfo,
                                 nullptr,
                                 &m_directionalShadowMapFramebuffer));
  }

  {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_shadowMapRenderPass;
    framebufferInfo.attachmentCount = 1;
    VkImageView spotView = m_spotPointShadowAtlas->getView();
    framebufferInfo.pAttachments = &spotView;
    framebufferInfo.width = m_spotPointShadowmapSize;
    framebufferInfo.height = m_spotPointShadowmapSize;
    framebufferInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(m_vkContext->logicalDevice,
                                 &framebufferInfo,
                                 nullptr,
                                 &m_spotShadowMapFramebuffer));
  }
}

void
ShadowMapPass::createDirectionalRenderPass()
{
  VkAttachmentDescription attachmentDescription{};
  attachmentDescription.format = m_spotPointShadowAtlas->getFormat();
  attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachmentDescription.finalLayout =
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkAttachmentReference depthReference = {};
  depthReference.attachment = 0;
  depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 0;
  subpass.pDepthStencilAttachment = &depthReference;

  std::array<VkSubpassDependency, 2> dependencies;

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &attachmentDescription;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  VK_CHECK(vkCreateRenderPass(m_vkContext->logicalDevice,
                              &renderPassInfo,
                              nullptr,
                              &m_shadowMapRenderPass));
}

void
ShadowMapPass::createShadowMapPipeline()
{
  VkPushConstantRange lightSpaceMatrixPCRange{};
  lightSpaceMatrixPCRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  lightSpaceMatrixPCRange.offset = 0;
  lightSpaceMatrixPCRange.size = 64;

  GraphicsPipelineConfig config{};
  config.vertShaderName = "shadows/directional_shadow_vert.spv";
  config.cullMode = VK_CULL_MODE_FRONT_BIT;
  config.depthBiasEnable = VK_TRUE;
  config.pushConstantRanges = { lightSpaceMatrixPCRange };
  config.renderPass = m_shadowMapRenderPass;

  auto result = createPipeline(m_vkContext, config);
  m_shadowMapPipeline = result.pipeline;
  m_shadowMapPipelineLayout = result.layout;
}
