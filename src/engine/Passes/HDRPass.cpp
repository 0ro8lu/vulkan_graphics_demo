#include "engine/Passes/HDRPass.h"
#include "engine/GpuAllocator.h"
#include "engine/GpuDevice.h"
#include "engine/PipelineBuilder.h"
#include "engine/SamplerCache.h"
#include "engine/VulkanCheck.h"

HDRPass::HDRPass(const GpuDevice& device,
                 const GpuAllocator& allocator,
                 const SamplerCache& samplerCache,
                 const AttachmentConfig& attachmentConfig)
  : m_device(device)
  , m_allocator(allocator)
  , m_samplerCache(samplerCache)
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
    m_device.device(), m_mainDescriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(
    m_device.device(), m_bloomDescriptorSetLayout, nullptr);

  vkDestroyPipeline(
    m_device.device(), m_brightPointExtractionPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_device.device(), m_brightPointExtractionPipelineLayout, nullptr);

  vkDestroyPipeline(m_device.device(), m_horizontalBloomPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_device.device(), m_horizontalBloomPipelineLayout, nullptr);

  vkDestroyPipeline(m_device.device(), m_verticalBloomPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_device.device(), m_verticalBloomPipelineLayout, nullptr);

  vkDestroyPipeline(m_device.device(), m_compositionPipeline, nullptr);
  vkDestroyPipelineLayout(
    m_device.device(), m_compositionPipelineLayout, nullptr);

  vkDestroyRenderPass(m_device.device(), m_brightPointRenderPass, nullptr);
  vkDestroyRenderPass(m_device.device(), m_horizontalBloomRenderPass, nullptr);
  vkDestroyRenderPass(
    m_device.device(), m_presentationRenderPass, nullptr);

  vkDestroyFramebuffer(m_device.device(), m_brightPointFramebuffer, nullptr);
  vkDestroyFramebuffer(m_device.device(), m_horizontalBloomFramebuffer, nullptr);
}

void
HDRPass::draw(const FrameContext& frameContext)
{
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(frameContext.extent.width);
  viewport.height = static_cast<float>(frameContext.extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = { 0, 0 };
  scissor.extent = frameContext.extent;

  VkClearValue clearColor{};
  clearColor.color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

  // 1. Bright point extraction pass
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_brightPointRenderPass;
    renderPassInfo.framebuffer = m_brightPointFramebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = frameContext.extent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(
      frameContext.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(frameContext.commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_brightPointExtractionPipeline);
    vkCmdSetViewport(frameContext.commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(frameContext.commandBuffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(frameContext.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_brightPointExtractionPipelineLayout,
                            0,
                            1,
                            &m_brightPointDescriptorSet,
                            0,
                            nullptr);
    vkCmdDraw(frameContext.commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(frameContext.commandBuffer);
  }

  // 2. Horizontal bloom pass
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_horizontalBloomRenderPass;
    renderPassInfo.framebuffer = m_horizontalBloomFramebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = frameContext.extent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(
      frameContext.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(frameContext.commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_horizontalBloomPipeline);
    vkCmdSetViewport(frameContext.commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(frameContext.commandBuffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(frameContext.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_horizontalBloomPipelineLayout,
                            0,
                            1,
                            &m_horizontalBloomDescriptorSet,
                            0,
                            nullptr);
    vkCmdDraw(frameContext.commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(frameContext.commandBuffer);
  }

  // 3. Presentation pass (composition + vertical bloom additive blend)
  {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_presentationRenderPass;
    renderPassInfo.framebuffer = frameContext.backbuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = frameContext.extent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(
      frameContext.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(frameContext.commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_compositionPipeline);
    vkCmdSetViewport(frameContext.commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(frameContext.commandBuffer, 0, 1, &scissor);
    vkCmdBindDescriptorSets(frameContext.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_compositionPipelineLayout,
                            0,
                            1,
                            &m_brightPointDescriptorSet,
                            0,
                            nullptr);
    vkCmdDraw(frameContext.commandBuffer, 3, 1, 0, 0);

    vkCmdBindPipeline(frameContext.commandBuffer,
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_verticalBloomPipeline);
    vkCmdBindDescriptorSets(frameContext.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_verticalBloomPipelineLayout,
                            0,
                            1,
                            &m_verticalBloomDescriptorSet,
                            0,
                            nullptr);
    vkCmdDraw(frameContext.commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(frameContext.commandBuffer);
  }
}

void
HDRPass::recreateAttachments(int width, int height)
{
  m_brightSpotsAttachment->resize(width, height);
  m_intermediateBloomAttachment->resize(width, height);

  vkDestroyFramebuffer(m_device.device(), m_brightPointFramebuffer, nullptr);
  vkDestroyFramebuffer(m_device.device(), m_horizontalBloomFramebuffer, nullptr);

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
      m_device.device(), 1, &descriptorWrite, 0, nullptr);
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
      m_device.device(), 1, &descriptorWrite, 0, nullptr);
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
      m_device.device(), 1, &descriptorWrite, 0, nullptr);
  }
}

void
HDRPass::createFrameBuffers()
{
  // 1. Bright point framebuffer
  {
    VkImageView attachment = m_brightSpotsAttachment->getView();
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_brightPointRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &attachment;
    framebufferInfo.width = m_brightSpotsAttachment->getWidth();
    framebufferInfo.height = m_brightSpotsAttachment->getHeight();
    framebufferInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(
      m_device.device(), &framebufferInfo, nullptr, &m_brightPointFramebuffer));
  }

  // 2. Horizontal bloom framebuffer
  {
    VkImageView attachment = m_intermediateBloomAttachment->getView();
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_horizontalBloomRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &attachment;
    framebufferInfo.width = m_intermediateBloomAttachment->getWidth();
    framebufferInfo.height = m_intermediateBloomAttachment->getHeight();
    framebufferInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(
      m_device.device(), &framebufferInfo, nullptr, &m_horizontalBloomFramebuffer));
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
  brightInfo.device = &m_device;
  brightInfo.allocator = &m_allocator;
  brightInfo.samplerCache = &m_samplerCache;
  brightInfo.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  m_brightSpotsAttachment = FramebufferAttachment::create(brightInfo);

  FramebufferAttachment::CreateInfo intermInfo{};
  intermInfo.width = width;
  intermInfo.height = height;
  intermInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  intermInfo.layerCount = 1;
  intermInfo.usage =
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  intermInfo.device = &m_device;
  intermInfo.allocator = &m_allocator;
  intermInfo.samplerCache = &m_samplerCache;
  intermInfo.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  m_intermediateBloomAttachment = FramebufferAttachment::create(intermInfo);
}

void
HDRPass::createRenderPass(VkFormat swapchainImageFormat)
{
  // 1. Bright points render pass
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

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &brightSpotsAttachmentDescriptor;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(
      m_device.device(), &renderPassInfo, nullptr, &m_brightPointRenderPass));
  }

  // 2. Horizontal bloom render pass
  {
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

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &intermediateBloomAttachmentDescriptor;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(
      m_device.device(), &renderPassInfo, nullptr, &m_horizontalBloomRenderPass));
  }

  // 3. Presentation render pass (with fragment shader read dependency for sampled inputs)
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

    std::array<VkSubpassDependency, 2> dependencies{};
    // Incoming: wait for previous passes' sampled image writes to be visible
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstStageMask =
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask =
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Outgoing: ensure our writes complete before present
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].dstAccessMask = 0;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &swapchainAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(m_device.device(),
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
    m_device.device(), &poolInfo, nullptr, &m_mainDescriptorPool));

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

  VK_CHECK(vkCreateDescriptorSetLayout(m_device.device(),
                                       &layoutInfoAttachmentWrite,
                                       nullptr,
                                       &m_bloomDescriptorSetLayout));

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_mainDescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_bloomDescriptorSetLayout;

  VK_CHECK(vkAllocateDescriptorSets(
    m_device.device(), &allocInfo, &m_brightPointDescriptorSet));
  VK_CHECK(vkAllocateDescriptorSets(
    m_device.device(), &allocInfo, &m_horizontalBloomDescriptorSet));
  VK_CHECK(vkAllocateDescriptorSets(
    m_device.device(), &allocInfo, &m_verticalBloomDescriptorSet));
}

void
HDRPass::createPipelines()
{
  // Bright point extraction pipeline
  {
    auto result = PipelineBuilder{}
                    .setVertexShader("bloom/bloom_vert.spv")
                    .setFragmentShader("bloom/bright_point_extraction_frag.spv")
                    .disableVertexInput()
                    .setCullMode(VK_CULL_MODE_NONE)
                    .setDepthTest(VK_FALSE, VK_FALSE)
                    .setDescriptorSetLayouts(
                      std::span(&m_bloomDescriptorSetLayout, 1))
                    .setRenderPass(m_brightPointRenderPass, 0)
                    .build(m_device.device());

    m_brightPointExtractionPipeline = result.pipeline;
    m_brightPointExtractionPipelineLayout = result.layout;
  }

  // Horizontal bloom pipeline
  {
    VkSpecializationMapEntry specEntry{
      .constantID = 0,
      .offset = 0,
      .size = sizeof(uint32_t),
    };

    uint32_t dir = 0; // horizontal
    VkSpecializationInfo specInfo{
      .mapEntryCount = 1,
      .pMapEntries = &specEntry,
      .dataSize = sizeof(uint32_t),
      .pData = &dir,
    };

    auto result = PipelineBuilder{}
                    .setVertexShader("bloom/bloom_vert.spv")
                    .setFragmentShader("bloom/bloom_frag.spv", &specInfo)
                    .disableVertexInput()
                    .setCullMode(VK_CULL_MODE_NONE)
                    .setDepthTest(VK_FALSE, VK_FALSE)
                    .setDescriptorSetLayouts(
                      std::span(&m_bloomDescriptorSetLayout, 1))
                    .setRenderPass(m_horizontalBloomRenderPass, 0)
                    .build(m_device.device());

    m_horizontalBloomPipeline = result.pipeline;
    m_horizontalBloomPipelineLayout = result.layout;
  }

  // Vertical bloom pipeline
  {
    VkSpecializationMapEntry specEntry{
      .constantID = 0,
      .offset = 0,
      .size = sizeof(uint32_t),
    };

    uint32_t dir = 1; // vertical
    VkSpecializationInfo specInfo{
      .mapEntryCount = 1,
      .pMapEntries = &specEntry,
      .dataSize = sizeof(uint32_t),
      .pData = &dir,
    };

    auto result = PipelineBuilder{}
                    .setVertexShader("bloom/bloom_vert.spv")
                    .setFragmentShader("bloom/bloom_frag.spv", &specInfo)
                    .disableVertexInput()
                    .setCullMode(VK_CULL_MODE_NONE)
                    .setDepthTest(VK_FALSE, VK_FALSE)
                    .addAdditiveBlendAttachment()
                    .setDescriptorSetLayouts(
                      std::span(&m_bloomDescriptorSetLayout, 1))
                    .setRenderPass(m_presentationRenderPass, 0)
                    .build(m_device.device());

    m_verticalBloomPipeline = result.pipeline;
    m_verticalBloomPipelineLayout = result.layout;
  }

  // Composition Pipeline
  {
    auto result = PipelineBuilder{}
                    .setVertexShader("bloom/bloom_vert.spv")
                    .setFragmentShader("bloom/composition_frag.spv")
                    .disableVertexInput()
                    .setCullMode(VK_CULL_MODE_NONE)
                    .setDepthTest(VK_FALSE, VK_FALSE)
                    .setDescriptorSetLayouts(
                      std::span(&m_bloomDescriptorSetLayout, 1))
                    .setRenderPass(m_presentationRenderPass, 0)
                    .build(m_device.device());

    m_compositionPipeline = result.pipeline;
    m_compositionPipelineLayout = result.layout;
  }
}
