#include "engine/Passes/ShadowMapPass.h"

#include "engine/Lights.h"
#include "engine/Scene.h"
#include "engine/Vertex.h"

ShadowMapPass::ShadowMapPass(
  VulkanContext* vkContext,
  const std::array<AttachmentData, 16>& attachmentData,
  const Scene& scene,
  const uint32_t shadowMapWidth,
  const uint32_t shadowMapHeight)
  : IPassHelper(vkContext, scene)
  , shadowMapWidth(shadowMapWidth)
  , shadowMapHeight(shadowMapHeight)
{
  createShadowMaps(shadowMapWidth, shadowMapHeight);

  createDirectionalRenderPass(attachmentData);

  createFrameBuffers(attachmentData);

  createShadowMapPipeline();
}

ShadowMapPass::~ShadowMapPass()
{
  vkDestroyFramebuffer(
    vkContext->logicalDevice, directionalShadowMapFramebuffer, nullptr);
  vkDestroyFramebuffer(
    vkContext->logicalDevice, spotShadowMapFramebuffer, nullptr);

  vkDestroyRenderPass(vkContext->logicalDevice, shadowMapRenderPass, nullptr);

  vkDestroyPipeline(vkContext->logicalDevice, shadowMapPipeline, nullptr);
  vkDestroyPipelineLayout(
    vkContext->logicalDevice, shadowMapPipelineLayout, nullptr);

  if (directionalShadowMap) {
    delete directionalShadowMap;
  }

  if (spotPointShadowAtlas) {
    delete spotPointShadowAtlas;
  }
}

void
ShadowMapPass::createShadowMaps(uint32_t width, uint32_t height)
{
  VkFormat depthFormat = vkContext->findSupportedFormat(
    { VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D16_UNORM,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT },
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  directionalShadowMap = new FramebufferAttachment(
    depthFormat,
    1,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    width,
    height,
    vkContext,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

  // TODO: if not spotligts, nor pointlights cast shadows, don't create it.
  spotPointShadowAtlas = new FramebufferAttachment(
    depthFormat,
    1,
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    ATLAS_SIZE,
    ATLAS_SIZE,
    vkContext);
}

void
ShadowMapPass::draw(VulkanSwapchain* vkSwapchain, const Scene& scene)
{
  // directional shadow
  if (scene.directionalLight->castsShadow()) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = shadowMapRenderPass;
    renderPassInfo.framebuffer = directionalShadowMapFramebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent.width = directionalShadowMap->width;
    renderPassInfo.renderArea.extent.height = directionalShadowMap->height;

    VkClearValue depthClearValue = { 1.0f, 0 };

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &depthClearValue;

    vkCmdBeginRenderPass(
      vkSwapchain->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // -------------------- bind main pipeline --------------------
    vkCmdBindPipeline(vkSwapchain->commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      shadowMapPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = directionalShadowMap->width;
    viewport.height = directionalShadowMap->width;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent.width = directionalShadowMap->width;
    scissor.extent.height = directionalShadowMap->height;
    scissor.offset.x = 0;
    scissor.offset.y = 0;

    vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

    // vkCmdSetDepthBias(vkSwapchain->commandBuffer, 1.5f, 0.0f, 2.0f);
    // vkCmdSetDepthBias(vkSwapchain->commandBuffer, 1.25f, 0.0f, 1.75f);

    struct PushConstant
    {
      glm::mat4 lightSpaceMatrix;
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
        pc.lightSpaceMatrix =
          scene.directionalLight->getTransform() * instance.transformation;

        vkCmdPushConstants(vkSwapchain->commandBuffer,
                           shadowMapPipelineLayout,
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
    }
    vkCmdEndRenderPass(vkSwapchain->commandBuffer);
  } else {
    // this is imprtant! we still have to transition the image layout even tho
    // we don't use it later on! Thanks daddy Vulkan :3
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = shadowMapRenderPass;
    renderPassInfo.framebuffer = directionalShadowMapFramebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent.width = directionalShadowMap->width;
    renderPassInfo.renderArea.extent.height = directionalShadowMap->height;

    VkClearValue depthClearValue = { 1.0f, 0 };

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
    renderPassInfo.renderPass = shadowMapRenderPass;
    renderPassInfo.framebuffer = spotShadowMapFramebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent.width = spotPointShadowAtlas->width;
    renderPassInfo.renderArea.extent.height = spotPointShadowAtlas->height;

    VkClearValue depthClearValue = { 1.0f, 0 };

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &depthClearValue;

    vkCmdBeginRenderPass(
      vkSwapchain->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // -------------------- bind main pipeline --------------------
    vkCmdBindPipeline(vkSwapchain->commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      shadowMapPipeline);

    for (const auto& spotlight : scene.spotLights) {

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

      for (auto& model : scene.models) {

        VkBuffer vertexBuffers[] = { model.vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(
          vkSwapchain->commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(vkSwapchain->commandBuffer,
                             model.indexBuffer,
                             0,
                             VK_INDEX_TYPE_UINT32);

        for (const auto& instance : model.meshInstances) {
          PushConstant pc;
          pc.lightSpaceMatrix =
            spotlight.getTransform() * instance.transformation;

          vkCmdPushConstants(vkSwapchain->commandBuffer,
                             shadowMapPipelineLayout,
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
      }
    }
    for (const auto& pointlight : scene.pointLights) {
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

        for (auto& model : scene.models) {

          VkBuffer vertexBuffers[] = { model.vertexBuffer };
          VkDeviceSize offsets[] = { 0 };
          vkCmdBindVertexBuffers(
            vkSwapchain->commandBuffer, 0, 1, vertexBuffers, offsets);

          vkCmdBindIndexBuffer(vkSwapchain->commandBuffer,
                               model.indexBuffer,
                               0,
                               VK_INDEX_TYPE_UINT32);

          for (const auto& instance : model.meshInstances) {
            PushConstant pc;
            pc.lightSpaceMatrix =
              pointlight.getTransform(side) * instance.transformation;

            vkCmdPushConstants(vkSwapchain->commandBuffer,
                               shadowMapPipelineLayout,
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
        }
      }
    }

    vkCmdEndRenderPass(vkSwapchain->commandBuffer);
  }
}

void
ShadowMapPass::createFrameBuffers(std::array<AttachmentData, 16> attachmentData)
{
  // for directional lights
  {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = shadowMapRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &directionalShadowMap->view;
    framebufferInfo.width = shadowMapWidth;
    framebufferInfo.height = shadowMapHeight;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(vkContext->logicalDevice,
                            &framebufferInfo,
                            nullptr,
                            &directionalShadowMapFramebuffer) != VK_SUCCESS) {
      throw std::runtime_error("failed to create framebuffer!");
    }
  }

  {
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = shadowMapRenderPass;
    // framebufferInfo.renderPass = spotShadowMapRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &spotPointShadowAtlas->view;
    framebufferInfo.width = ATLAS_SIZE;
    framebufferInfo.height = ATLAS_SIZE;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(vkContext->logicalDevice,
                            &framebufferInfo,
                            nullptr,
                            &spotShadowMapFramebuffer) != VK_SUCCESS) {
      throw std::runtime_error("failed to create framebuffer!");
    }
  }
}

void
ShadowMapPass::createDirectionalRenderPass(
  std::array<AttachmentData, 16> attachmentData)

{
  VkAttachmentDescription attachmentDescription{};
  attachmentDescription.format = spotPointShadowAtlas->format;
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

  if (vkCreateRenderPass(vkContext->logicalDevice,
                         &renderPassInfo,
                         nullptr,
                         &shadowMapRenderPass) != VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

// void
// ShadowMapPass::createSpotRenderPass(
//   std::array<AttachmentData, 16> attachmentData)
// {
//   VkAttachmentDescription attachmentDescription{};
//   attachmentDescription.format = spotPointShadowAtlas->format;
//   attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
//   attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
//   attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
//   attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//   attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//   attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//   attachmentDescription.finalLayout =
//     VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

//   VkAttachmentReference depthReference = {};
//   depthReference.attachment = 0;
//   depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

//   VkSubpassDescription subpass = {};
//   subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
//   subpass.colorAttachmentCount = 0;
//   subpass.pDepthStencilAttachment = &depthReference;

//   std::array<VkSubpassDependency, 2> dependencies;

//   dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
//   dependencies[0].dstSubpass = 0;
//   dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
//   dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
//   dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
//   dependencies[0].dstAccessMask =
//   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
//   dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

//   dependencies[1].srcSubpass = 0;
//   dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
//   dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
//   dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
//   dependencies[1].srcAccessMask =
//   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; dependencies[1].dstAccessMask
//   = VK_ACCESS_SHADER_READ_BIT; dependencies[1].dependencyFlags =
//   VK_DEPENDENCY_BY_REGION_BIT;

//   VkRenderPassCreateInfo renderPassInfo{};
//   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
//   renderPassInfo.attachmentCount = 1;
//   renderPassInfo.pAttachments = &attachmentDescription;
//   renderPassInfo.subpassCount = 1;
//   renderPassInfo.pSubpasses = &subpass;
//   renderPassInfo.dependencyCount =
//   static_cast<uint32_t>(dependencies.size()); renderPassInfo.pDependencies =
//   dependencies.data();

//   if (vkCreateRenderPass(vkContext->logicalDevice,
//                          &renderPassInfo,
//                          nullptr,
//                          &spotShadowMapRenderPass) != VK_SUCCESS) {
//     throw std::runtime_error("failed to create render pass!");
//   }
// }

void
ShadowMapPass::createShadowMapPipeline()
{
  std::string shaderPath = SHADER_PATH;
  auto vertShaderCode =
    readFile(shaderPath + "shadows/directional_shadow_vert.spv");

  VkShaderModule vertShaderModule =
    vkContext->createShaderModule(vertShaderCode);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

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

  VkPushConstantRange lightSpaceMatrixPCRange{};
  lightSpaceMatrixPCRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  lightSpaceMatrixPCRange.offset = 0;
  lightSpaceMatrixPCRange.size = 64;

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
  rasterizer.depthBiasEnable = VK_TRUE;

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

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;

  // std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
  //                                               VK_DYNAMIC_STATE_SCISSOR,
  //                                               VK_DYNAMIC_STATE_DEPTH_BIAS
  //                                               };
  std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
                                                VK_DYNAMIC_STATE_SCISSOR };

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &lightSpaceMatrixPCRange;

  if (vkCreatePipelineLayout(vkContext->logicalDevice,
                             &pipelineLayoutInfo,
                             nullptr,
                             &shadowMapPipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 1;
  pipelineInfo.pStages = &vertShaderStageInfo;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = shadowMapPipelineLayout;
  pipelineInfo.renderPass = shadowMapRenderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(vkContext->logicalDevice,
                                VK_NULL_HANDLE,
                                1,
                                &pipelineInfo,
                                nullptr,
                                &shadowMapPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  vkDestroyShaderModule(vkContext->logicalDevice, vertShaderModule, nullptr);
}
