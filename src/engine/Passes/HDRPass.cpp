#include "engine/Passes/HDRPass.h"

#include "engine/RenderUtils.h"
#include "engine/Scene.h"

HDRPass::HDRPass(VulkanContext* vkContext,
                 const AttachmentConfig& attachmentConfig)
  : vkContext(vkContext)
{
  createAttachments(attachmentConfig.width, attachmentConfig.height);

  createRenderPass(attachmentConfig.swapchainImageFormat);

  createFrameBuffers();

  createDescriptors();

  createPipelines();
}

HDRPass::~HDRPass()
{
  vkDestroyDescriptorPool(
    vkContext->logicalDevice, mainDescriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(
    vkContext->logicalDevice, bloomDescriptorSetLayout, nullptr);

  vkDestroyPipeline(
    vkContext->logicalDevice, brightPointExtractionPipeline, nullptr);
  vkDestroyPipelineLayout(
    vkContext->logicalDevice, brightPointExtractionPipelineLayout, nullptr);

  vkDestroyPipeline(vkContext->logicalDevice, horizontalBloomPipeline, nullptr);
  vkDestroyPipelineLayout(
    vkContext->logicalDevice, horizontalBloomPipelineLayout, nullptr);

  vkDestroyPipeline(vkContext->logicalDevice, verticalBloomPipeline, nullptr);
  vkDestroyPipelineLayout(
    vkContext->logicalDevice, verticalBloomPipelineLayout, nullptr);

  vkDestroyPipeline(vkContext->logicalDevice, compositionPipeline, nullptr);
  vkDestroyPipelineLayout(
    vkContext->logicalDevice, compositionPipelineLayout, nullptr);

  vkDestroyRenderPass(vkContext->logicalDevice, bloomRenderPass, nullptr);
  vkDestroyRenderPass(
    vkContext->logicalDevice, presentationRenderPass, nullptr);

  vkDestroyFramebuffer(vkContext->logicalDevice, bloomFramebuffer, nullptr);
}

void
HDRPass::draw(VulkanSwapchain* vkSwapchain)
{
  // extract colors first!
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = bloomRenderPass;
  renderPassInfo.framebuffer = bloomFramebuffer;
  renderPassInfo.renderArea.offset = { 0, 0 };
  renderPassInfo.renderArea.extent = vkSwapchain->swapChainExtent;

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
  clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(
    vkSwapchain->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(vkSwapchain->commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    brightPointExtractionPipeline);

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
                          brightPointExtractionPipelineLayout,
                          0,
                          1,
                          &brightPointDescriptorSet,
                          0,
                          nullptr);
  vkCmdDraw(vkSwapchain->commandBuffer, 3, 1, 0, 0);

  // we now do out first horizontal bloom pass
  vkCmdNextSubpass(vkSwapchain->commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(vkSwapchain->commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    horizontalBloomPipeline);

  vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);

  vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          horizontalBloomPipelineLayout,
                          0,
                          1,
                          &horizontalBloomDescriptorSet,
                          0,
                          nullptr);
  vkCmdDraw(vkSwapchain->commandBuffer, 3, 1, 0, 0);

  // now we combine it inside the swapchain!
  renderPassInfo.renderPass = presentationRenderPass;
  renderPassInfo.framebuffer =
    vkSwapchain->swapChainFramebuffers[vkSwapchain->imageIndex];
  renderPassInfo.renderArea.offset = { 0, 0 };
  renderPassInfo.renderArea.extent = vkSwapchain->swapChainExtent;

  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearValues[0];

  vkCmdEndRenderPass(vkSwapchain->commandBuffer);
  vkCmdBeginRenderPass(
    vkSwapchain->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(vkSwapchain->commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    compositionPipeline);

  vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);

  vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          compositionPipelineLayout,
                          0,
                          1,
                          &brightPointDescriptorSet,
                          0,
                          nullptr);
  vkCmdDraw(vkSwapchain->commandBuffer, 3, 1, 0, 0);

  vkCmdBindPipeline(vkSwapchain->commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    verticalBloomPipeline);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          verticalBloomPipelineLayout,
                          0,
                          1,
                          &verticalBloomDescriptorSet,
                          0,
                          nullptr);
  vkCmdDraw(vkSwapchain->commandBuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(vkSwapchain->commandBuffer);
}

void
HDRPass::recreateAttachments(int width, int height)
{
  brightSpotsAttachment->resize(width, height);
  intermediateBloomAttachment->resize(width, height);

  vkDestroyFramebuffer(vkContext->logicalDevice, bloomFramebuffer, nullptr);

  createFrameBuffers();
}

// TODO: Should this be inside blinnphong?
void
HDRPass::updateDescriptors(
  const std::unique_ptr<FramebufferAttachment>& blinnPhongAttachment)
{
  {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView =
      blinnPhongAttachment->getView(); // <= hdr attachment from blin-phong
    imageInfo.sampler = blinnPhongAttachment->getSampler();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = brightPointDescriptorSet;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(
      vkContext->logicalDevice, 1, &descriptorWrite, 0, nullptr);
  }

  {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView =
      brightSpotsAttachment->getView(); // <= hdr attachment from blin-phong
    imageInfo.sampler = brightSpotsAttachment->getSampler();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = horizontalBloomDescriptorSet;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(
      vkContext->logicalDevice, 1, &descriptorWrite, 0, nullptr);
  }

  {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = intermediateBloomAttachment->getView();
    imageInfo.sampler = intermediateBloomAttachment->getSampler();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = verticalBloomDescriptorSet;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(
      vkContext->logicalDevice, 1, &descriptorWrite, 0, nullptr);
  }
}

void
HDRPass::createFrameBuffers()
{
  std::array<VkImageView, 2> attachments = {
    brightSpotsAttachment->getView(), intermediateBloomAttachment->getView()
  };

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = bloomRenderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = brightSpotsAttachment->getWidth();
  framebufferInfo.height = brightSpotsAttachment->getHeight();
  framebufferInfo.layers = 1;

  if (vkCreateFramebuffer(vkContext->logicalDevice,
                          &framebufferInfo,
                          nullptr,
                          &bloomFramebuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to create framebuffer!");
  }
}

void
HDRPass::createAttachments(uint32_t width, uint32_t height)
{
  FramebufferAttachment::CreateInfo brightInfo{};
  brightInfo.width = width;
  brightInfo.height = height;
  brightInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  brightInfo.layerCount = 1;
  brightInfo.usage =
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  brightInfo.vkContext = vkContext;
  brightSpotsAttachment = FramebufferAttachment::create(brightInfo);

  FramebufferAttachment::CreateInfo intermInfo{};
  intermInfo.width = width;
  intermInfo.height = height;
  intermInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  intermInfo.layerCount = 1;
  intermInfo.usage =
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  intermInfo.vkContext = vkContext;
  intermediateBloomAttachment = FramebufferAttachment::create(intermInfo);
}

void
HDRPass::createRenderPass(VkFormat swapchainImageFormat)
{
  // bloom render pass
  // two attachments. two subpasses.
  {
    VkAttachmentDescription brightSpotsAttachmentDescriptor{};
    brightSpotsAttachmentDescriptor.format = brightSpotsAttachment->getFormat();
    brightSpotsAttachmentDescriptor.samples = VK_SAMPLE_COUNT_1_BIT;
    brightSpotsAttachmentDescriptor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    brightSpotsAttachmentDescriptor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    brightSpotsAttachmentDescriptor.stencilLoadOp =
      VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    brightSpotsAttachmentDescriptor.stencilStoreOp =
      VK_ATTACHMENT_STORE_OP_DONT_CARE;
    brightSpotsAttachmentDescriptor.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    brightSpotsAttachmentDescriptor.finalLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription intermediateBloomAttachmentDescriptor{};
    intermediateBloomAttachmentDescriptor.format =
      intermediateBloomAttachment->getFormat();
    intermediateBloomAttachmentDescriptor.samples = VK_SAMPLE_COUNT_1_BIT;
    intermediateBloomAttachmentDescriptor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    intermediateBloomAttachmentDescriptor.storeOp =
      VK_ATTACHMENT_STORE_OP_STORE;
    intermediateBloomAttachmentDescriptor.stencilLoadOp =
      VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    intermediateBloomAttachmentDescriptor.stencilStoreOp =
      VK_ATTACHMENT_STORE_OP_DONT_CARE;
    intermediateBloomAttachmentDescriptor.initialLayout =
      VK_IMAGE_LAYOUT_UNDEFINED;
    intermediateBloomAttachmentDescriptor.finalLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference brightSpotsAttachmentOutputReference;
    brightSpotsAttachmentOutputReference.attachment = 0;
    brightSpotsAttachmentOutputReference.layout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference brightSpotsAttachmentInputReference;
    brightSpotsAttachmentInputReference.attachment = 0;
    brightSpotsAttachmentInputReference.layout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference intermediateBloomAttachmentReference;
    intermediateBloomAttachmentReference.attachment = 1;
    intermediateBloomAttachmentReference.layout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::array<VkSubpassDescription, 2> subPassDescriptions{};
    subPassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPassDescriptions[0].colorAttachmentCount = 1;
    subPassDescriptions[0].pColorAttachments =
      &brightSpotsAttachmentOutputReference;

    subPassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPassDescriptions[1].colorAttachmentCount = 1;
    subPassDescriptions[1].pColorAttachments =
      &intermediateBloomAttachmentReference;
    subPassDescriptions[1].inputAttachmentCount = 1;
    subPassDescriptions[1].pInputAttachments =
      &brightSpotsAttachmentInputReference;

    std::array<VkSubpassDependency, 3> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // This dependency transitions the input attachment from color attachment to
    // shader read
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[2].srcSubpass = 1;
    dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[2].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {
      brightSpotsAttachmentDescriptor,
      intermediateBloomAttachmentDescriptor,
    };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount =
      static_cast<uint32_t>(subPassDescriptions.size());
    renderPassInfo.pSubpasses = subPassDescriptions.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(vkContext->logicalDevice,
                           &renderPassInfo,
                           nullptr,
                           &bloomRenderPass) != VK_SUCCESS) {
      throw std::runtime_error("failed to create render pass!");
    }
  }

  // render pass for presentation
  {
    VkAttachmentDescription swapchainAttachment{};
    swapchainAttachment.format = swapchainImageFormat;
    swapchainAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    swapchainAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    swapchainAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    swapchainAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

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

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &swapchainAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(vkContext->logicalDevice,
                           &renderPassInfo,
                           nullptr,
                           &presentationRenderPass) != VK_SUCCESS) {
      throw std::runtime_error("failed to create render pass!");
    }
  }
}

void
HDRPass::createDescriptors()
{
  // create descr pool
  VkDescriptorPoolSize poolSize;
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = 3;

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = 3;

  if (vkCreateDescriptorPool(
        vkContext->logicalDevice, &poolInfo, nullptr, &mainDescriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }

  // create descr layout
  std::array<VkDescriptorSetLayoutBinding, 1> bindings;
  bindings[0].binding = 0;
  bindings[0].descriptorCount = 1;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[0].pImmutableSamplers = nullptr;
  bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfoAttachmentWrite{};
  layoutInfoAttachmentWrite.sType =
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfoAttachmentWrite.bindingCount =
    static_cast<uint32_t>(bindings.size());
  layoutInfoAttachmentWrite.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(vkContext->logicalDevice,
                                  &layoutInfoAttachmentWrite,
                                  nullptr,
                                  &bloomDescriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = mainDescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &bloomDescriptorSetLayout;

  if (vkAllocateDescriptorSets(vkContext->logicalDevice,
                               &allocInfo,
                               &brightPointDescriptorSet) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }
  if (vkAllocateDescriptorSets(vkContext->logicalDevice,
                               &allocInfo,
                               &horizontalBloomDescriptorSet) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }
  if (vkAllocateDescriptorSets(vkContext->logicalDevice,
                               &allocInfo,
                               &verticalBloomDescriptorSet) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }
}

void
HDRPass::createPipelines()
{
  // Bright point extraction pipeline
  {
    GraphicsPipelineConfig config{};
    config.vertShaderName = "bloom/bloom_vert.spv";
    config.useVertexInput = false;
    config.fragShaderName = "bloom/bright_point_extraction_frag.spv";

    config.cullMode = VK_CULL_MODE_NONE;

    config.depthTestEnable = VK_FALSE;
    config.depthWriteEnable = VK_FALSE;

    // Layout & Renderpass
    config.descriptorSetLayouts = { bloomDescriptorSetLayout };
    config.pushConstantRanges = {};
    config.renderPass = bloomRenderPass;
    config.subpass = 0;

    auto result = createPipeline(vkContext, config);
    brightPointExtractionPipeline = result.pipeline;
    brightPointExtractionPipelineLayout = result.layout;
  }

  // Horizontal bloom pipeline
  {
    VkSpecializationMapEntry specEntry{};
    specEntry.constantID = 0;
    specEntry.offset = 0;
    specEntry.size = sizeof(uint32_t);

    uint32_t dir = 0; // horizontal
    VkSpecializationInfo specInfo{};
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specEntry;
    specInfo.dataSize = sizeof(uint32_t);
    specInfo.pData = &dir;

    GraphicsPipelineConfig config{};

    config.fragSpecialization = &specInfo;

    config.vertShaderName = "bloom/bloom_vert.spv";
    config.fragShaderName = "bloom/bloom_frag.spv";
    config.useVertexInput = false;

    config.cullMode = VK_CULL_MODE_NONE;

    config.depthTestEnable = VK_FALSE;
    config.depthWriteEnable = VK_FALSE;

    config.descriptorSetLayouts = { bloomDescriptorSetLayout };
    config.pushConstantRanges = {};
    config.renderPass = bloomRenderPass;
    config.subpass = 1;

    auto result = createPipeline(vkContext, config);
    horizontalBloomPipeline = result.pipeline;
    horizontalBloomPipelineLayout = result.layout;
  }

  // Vertical bloom pipeline
  {
    VkSpecializationMapEntry specEntry{};
    specEntry.constantID = 0;
    specEntry.offset = 0;
    specEntry.size = sizeof(uint32_t);

    uint32_t dir = 1; // vertical
    VkSpecializationInfo specInfo{};
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specEntry;
    specInfo.dataSize = sizeof(uint32_t);
    specInfo.pData = &dir;

    GraphicsPipelineConfig config{};
    config.fragSpecialization = &specInfo;

    config.vertShaderName = "bloom/bloom_vert.spv";
    config.fragShaderName = "bloom/bloom_frag.spv";
    config.useVertexInput = false;

    config.cullMode = VK_CULL_MODE_NONE;

    config.depthTestEnable = VK_FALSE;
    config.depthWriteEnable = VK_FALSE;

    config.blendEnable = VK_TRUE;
    config.colorBlendOp = VK_BLEND_OP_ADD;
    config.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    config.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    config.alphaBlendOp = VK_BLEND_OP_ADD;
    config.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    config.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;

    config.descriptorSetLayouts = { bloomDescriptorSetLayout };
    config.pushConstantRanges = {};
    config.renderPass = presentationRenderPass;
    config.subpass = 0;

    auto result = createPipeline(vkContext, config);
    verticalBloomPipeline = result.pipeline;
    verticalBloomPipelineLayout = result.layout;
  }

  // Composition Pipeline
  {
    GraphicsPipelineConfig config{};

    config.vertShaderName = "bloom/bloom_vert.spv";
    config.fragShaderName = "bloom/composition_frag.spv";
    config.useVertexInput = false;

    config.cullMode = VK_CULL_MODE_NONE;

    config.depthTestEnable = VK_FALSE;
    config.depthWriteEnable = VK_FALSE;

    config.blendEnable = VK_FALSE;

    config.descriptorSetLayouts = { bloomDescriptorSetLayout };
    config.pushConstantRanges = {};
    config.renderPass = presentationRenderPass;
    config.subpass = 0;

    auto result = createPipeline(vkContext, config);
    compositionPipeline = result.pipeline;
    compositionPipelineLayout = result.layout;
  }
}
