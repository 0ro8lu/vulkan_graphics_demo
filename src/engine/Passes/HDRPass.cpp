#include "engine/Passes/HDRPass.h"
#include "engine/VulkanCheck.h"

#include "engine/RenderUtils.h"

HDRPass::HDRPass(VulkanContext* vkContext,
                 const AttachmentConfig& attachmentConfig)
  : m_vkContext(vkContext)
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
    m_vkContext->logicalDevice, m_mainDescriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(
    m_vkContext->logicalDevice, m_bloomDescriptorSetLayout, nullptr);

  vkDestroyPipeline(
    m_vkContext->logicalDevice, m_brightPointExtractionPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_vkContext->logicalDevice, m_brightPointExtractionPipelineLayout, nullptr);

  vkDestroyPipeline(m_vkContext->logicalDevice, m_horizontalBloomPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_vkContext->logicalDevice, m_horizontalBloomPipelineLayout, nullptr);

  vkDestroyPipeline(m_vkContext->logicalDevice, m_verticalBloomPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_vkContext->logicalDevice, m_verticalBloomPipelineLayout, nullptr);

  vkDestroyPipeline(m_vkContext->logicalDevice, m_compositionPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_vkContext->logicalDevice, m_compositionPipelineLayout, nullptr);

  vkDestroyRenderPass(m_vkContext->logicalDevice, m_bloomRenderPass, nullptr);
  vkDestroyRenderPass(
    m_vkContext->logicalDevice, m_presentationRenderPass, nullptr);

  vkDestroyFramebuffer(m_vkContext->logicalDevice, m_bloomFramebuffer, nullptr);
}

void
HDRPass::draw(VulkanSwapchain* vkSwapchain)
{
  // extract colors first!
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = m_bloomRenderPass;
  renderPassInfo.framebuffer = m_bloomFramebuffer;
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
                    m_brightPointExtractionPipeline);

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
                          m_brightPointExtractionPipelineLayout,
                          0,
                          1,
                          &m_brightPointDescriptorSet,
                          0,
                          nullptr);
  vkCmdDraw(vkSwapchain->commandBuffer, 3, 1, 0, 0);

  // we now do out first horizontal bloom pass
  vkCmdNextSubpass(vkSwapchain->commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(vkSwapchain->commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_horizontalBloomPipeline);

  vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);

  vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_horizontalBloomPipelineLayout,
                          0,
                          1,
                          &m_horizontalBloomDescriptorSet,
                          0,
                          nullptr);
  vkCmdDraw(vkSwapchain->commandBuffer, 3, 1, 0, 0);

  // now we combine it inside the swapchain!
  renderPassInfo.renderPass = m_presentationRenderPass;
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
                    m_compositionPipeline);

  vkCmdSetViewport(vkSwapchain->commandBuffer, 0, 1, &viewport);

  vkCmdSetScissor(vkSwapchain->commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_compositionPipelineLayout,
                          0,
                          1,
                          &m_brightPointDescriptorSet,
                          0,
                          nullptr);
  vkCmdDraw(vkSwapchain->commandBuffer, 3, 1, 0, 0);

  vkCmdBindPipeline(vkSwapchain->commandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_verticalBloomPipeline);

  vkCmdBindDescriptorSets(vkSwapchain->commandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_verticalBloomPipelineLayout,
                          0,
                          1,
                          &m_verticalBloomDescriptorSet,
                          0,
                          nullptr);
  vkCmdDraw(vkSwapchain->commandBuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(vkSwapchain->commandBuffer);
}

void
HDRPass::recreateAttachments(int width, int height)
{
  m_brightSpotsAttachment->resize(width, height);
  m_intermediateBloomAttachment->resize(width, height);

  vkDestroyFramebuffer(m_vkContext->logicalDevice, m_bloomFramebuffer, nullptr);

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
    descriptorWrite.dstSet = m_brightPointDescriptorSet;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(
      m_vkContext->logicalDevice, 1, &descriptorWrite, 0, nullptr);
  }

  {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView =
      m_brightSpotsAttachment->getView(); // <= hdr attachment from blin-phong
    imageInfo.sampler = m_brightSpotsAttachment->getSampler();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_horizontalBloomDescriptorSet;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(
      m_vkContext->logicalDevice, 1, &descriptorWrite, 0, nullptr);
  }

  {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_intermediateBloomAttachment->getView();
    imageInfo.sampler = m_intermediateBloomAttachment->getSampler();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_verticalBloomDescriptorSet;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(
      m_vkContext->logicalDevice, 1, &descriptorWrite, 0, nullptr);
  }
}

void
HDRPass::createFrameBuffers()
{
  std::array<VkImageView, 2> attachments = {
    m_brightSpotsAttachment->getView(), m_intermediateBloomAttachment->getView()
  };

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = m_bloomRenderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = m_brightSpotsAttachment->getWidth();
  framebufferInfo.height = m_brightSpotsAttachment->getHeight();
  framebufferInfo.layers = 1;

  VK_CHECK(vkCreateFramebuffer(
    m_vkContext->logicalDevice, &framebufferInfo, nullptr, &m_bloomFramebuffer));
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
  brightInfo.vkContext = m_vkContext;
  m_brightSpotsAttachment = FramebufferAttachment::create(brightInfo);

  FramebufferAttachment::CreateInfo intermInfo{};
  intermInfo.width = width;
  intermInfo.height = height;
  intermInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  intermInfo.layerCount = 1;
  intermInfo.usage =
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  intermInfo.vkContext = m_vkContext;
  m_intermediateBloomAttachment = FramebufferAttachment::create(intermInfo);
}

void
HDRPass::createRenderPass(VkFormat swapchainImageFormat)
{
  // bloom render pass
  // two attachments. two subpasses.
  {
    VkAttachmentDescription brightSpotsAttachmentDescriptor{};
    brightSpotsAttachmentDescriptor.format =
      m_brightSpotsAttachment->getFormat();
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
      m_intermediateBloomAttachment->getFormat();
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

    VK_CHECK(vkCreateRenderPass(
      m_vkContext->logicalDevice, &renderPassInfo, nullptr, &m_bloomRenderPass));
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

    VK_CHECK(vkCreateRenderPass(m_vkContext->logicalDevice,
                                &renderPassInfo,
                                nullptr,
                                &m_presentationRenderPass));
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

  VK_CHECK(vkCreateDescriptorPool(
    m_vkContext->logicalDevice, &poolInfo, nullptr, &m_mainDescriptorPool));

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

  VK_CHECK(vkCreateDescriptorSetLayout(m_vkContext->logicalDevice,
                                       &layoutInfoAttachmentWrite,
                                       nullptr,
                                       &m_bloomDescriptorSetLayout));

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_mainDescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_bloomDescriptorSetLayout;

  VK_CHECK(vkAllocateDescriptorSets(
    m_vkContext->logicalDevice, &allocInfo, &m_brightPointDescriptorSet));
  VK_CHECK(vkAllocateDescriptorSets(
    m_vkContext->logicalDevice, &allocInfo, &m_horizontalBloomDescriptorSet));
  VK_CHECK(vkAllocateDescriptorSets(
    m_vkContext->logicalDevice, &allocInfo, &m_verticalBloomDescriptorSet));
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
    config.descriptorSetLayouts = { m_bloomDescriptorSetLayout };
    config.pushConstantRanges = {};
    config.renderPass = m_bloomRenderPass;
    config.subpass = 0;

    auto result = createPipeline(m_vkContext, config);
    m_brightPointExtractionPipeline = result.pipeline;
    m_brightPointExtractionPipelineLayout = result.layout;
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

    config.descriptorSetLayouts = { m_bloomDescriptorSetLayout };
    config.pushConstantRanges = {};
    config.renderPass = m_bloomRenderPass;
    config.subpass = 1;

    auto result = createPipeline(m_vkContext, config);
    m_horizontalBloomPipeline = result.pipeline;
    m_horizontalBloomPipelineLayout = result.layout;
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

    config.descriptorSetLayouts = { m_bloomDescriptorSetLayout };
    config.pushConstantRanges = {};
    config.renderPass = m_presentationRenderPass;
    config.subpass = 0;

    auto result = createPipeline(m_vkContext, config);
    m_verticalBloomPipeline = result.pipeline;
    m_verticalBloomPipelineLayout = result.layout;
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

    config.descriptorSetLayouts = { m_bloomDescriptorSetLayout };
    config.pushConstantRanges = {};
    config.renderPass = m_presentationRenderPass;
    config.subpass = 0;

    auto result = createPipeline(m_vkContext, config);
    m_compositionPipeline = result.pipeline;
    m_compositionPipelineLayout = result.layout;
  }
}
