#include "engine/HDRPass.h"

#include <vulkan/vulkan_core.h>

HDRPass::HDRPass(VulkanContext* vkContext,
                 std::array<AttachmentData, 16> attachmentData,
                 const Scene& scene,
                 uint32_t attachmentWidth,
                 uint32_t attachmentHeight)
  : IPassHelper(vkContext, scene)
{
  createAttachments(attachmentWidth, attachmentHeight);

  createRenderPass(attachmentData);

  createFrameBuffer(attachmentData);

  createDescriptors();

  createBrightPointExtractionPipeline();
  createHorizontalBloomPipeline();
  createVerticalBloomPipeline();
  createCompositionPipeline();
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

  delete brightSpotsAttachment;
  delete intermediateBloomAttachment;
  vkDestroyFramebuffer(vkContext->logicalDevice, bloomFramebuffer, nullptr);
}

void
HDRPass::draw(VulkanSwapchain* vkSwapchain, const Scene& scene)
{
  // extract colors first!
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = bloomRenderPass;
  renderPassInfo.framebuffer = bloomFramebuffer;
  renderPassInfo.renderArea.offset = { 0, 0 };
  renderPassInfo.renderArea.extent = vkSwapchain->swapChainExtent;

  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = { 0 };
  clearValues[1].color = { 0 };

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
HDRPass::recreateAttachments(int width,
                             int height,
                             std::array<AttachmentData, 16> attachmentData)
{
  brightSpotsAttachment->resize(width, height);
  intermediateBloomAttachment->resize(width, height);
  
  vkDestroyFramebuffer(vkContext->logicalDevice, bloomFramebuffer, nullptr);

  createFrameBuffer(attachmentData);
}

void
HDRPass::updateDescriptors(std::array<FramebufferAttachment*, 16> attachments)
{
  {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView =
      attachments[0]->view; // <= hdr attachment from blin-phong
    imageInfo.sampler = attachments[0]->sampler;

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
      brightSpotsAttachment->view; // <= hdr attachment from blin-phong
    imageInfo.sampler = brightSpotsAttachment->sampler;

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
    imageInfo.imageView =
      intermediateBloomAttachment->view; // <= hdr attachment from blin-phong
    imageInfo.sampler = intermediateBloomAttachment->sampler;

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
HDRPass::createFrameBuffer(std::array<AttachmentData, 16> attachmentData)
{
  std::array<VkImageView, 2> attachments = {
    brightSpotsAttachment->view, intermediateBloomAttachment->view
  };

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = bloomRenderPass;
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = brightSpotsAttachment->width;
  framebufferInfo.height = brightSpotsAttachment->height;
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
  brightSpotsAttachment = new FramebufferAttachment(
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    width,
    height,
    vkContext);

  intermediateBloomAttachment = new FramebufferAttachment(
    VK_FORMAT_R16G16B16A16_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    width,
    height,
    vkContext);
}

void
HDRPass::createRenderPass(std::array<AttachmentData, 16> attachmentData)
{
  // bloom render pass
  // two attachments. two subpasses.
  {
    VkAttachmentDescription brightSpotsAttachmentDescriptor{};
    brightSpotsAttachmentDescriptor.format = brightSpotsAttachment->format;
    brightSpotsAttachmentDescriptor.samples = VK_SAMPLE_COUNT_1_BIT;
    brightSpotsAttachmentDescriptor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    brightSpotsAttachmentDescriptor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    brightSpotsAttachmentDescriptor.stencilLoadOp =
      VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    brightSpotsAttachmentDescriptor.stencilStoreOp =
      VK_ATTACHMENT_STORE_OP_DONT_CARE;
    brightSpotsAttachmentDescriptor.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    brightSpotsAttachmentDescriptor.finalLayout =
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription intermediateBloomAttachmentDescriptor{};
    intermediateBloomAttachmentDescriptor.format =
      intermediateBloomAttachment->format;
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
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
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
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[2].srcSubpass = 0;
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
    swapchainAttachment.format = attachmentData[0].format;
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
HDRPass::createBrightPointExtractionPipeline()
{
  std::string shaderPath = SHADER_PATH;
  auto vertShaderCode = readFile(shaderPath + "bloom/bloom_vert.spv");
  auto fragShaderCode =
    readFile(shaderPath + "bloom/bright_point_extraction_frag.spv");

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

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

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
  rasterizer.cullMode = VK_CULL_MODE_NONE;
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
  depthStencil.depthTestEnable = VK_FALSE;
  depthStencil.depthWriteEnable = VK_FALSE;
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
  descriptorSetLayouts[0] = bloomDescriptorSetLayout;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount =
    static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

  if (vkCreatePipelineLayout(vkContext->logicalDevice,
                             &pipelineLayoutInfo,
                             nullptr,
                             &brightPointExtractionPipelineLayout) !=
      VK_SUCCESS) {
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
  pipelineInfo.layout = brightPointExtractionPipelineLayout;
  pipelineInfo.renderPass = bloomRenderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(vkContext->logicalDevice,
                                VK_NULL_HANDLE,
                                1,
                                &pipelineInfo,
                                nullptr,
                                &brightPointExtractionPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  vkDestroyShaderModule(vkContext->logicalDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(vkContext->logicalDevice, vertShaderModule, nullptr);
}

void
HDRPass::createHorizontalBloomPipeline()
{
  std::string shaderPath = SHADER_PATH;
  auto vertShaderCode = readFile(shaderPath + "bloom/bloom_vert.spv");
  auto fragShaderCode = readFile(shaderPath + "bloom/bloom_frag.spv");

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

  VkSpecializationMapEntry specializationMapEntry{};
  specializationMapEntry.constantID = 0;
  specializationMapEntry.offset = 0;
  specializationMapEntry.size = sizeof(uint32_t);

  uint32_t dir = 0;
  VkSpecializationInfo specializationInfo{};
  specializationInfo.mapEntryCount = 1;
  specializationInfo.pMapEntries = &specializationMapEntry;
  specializationInfo.dataSize = sizeof(uint32_t);
  specializationInfo.pData = &dir;

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";
  fragShaderStageInfo.pSpecializationInfo = &specializationInfo;

  VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo,
                                                     fragShaderStageInfo };

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

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
  rasterizer.cullMode = VK_CULL_MODE_NONE;
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
  depthStencil.depthTestEnable = VK_FALSE;
  depthStencil.depthWriteEnable = VK_FALSE;
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
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.colorWriteMask = 0xF;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;

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
  descriptorSetLayouts[0] = bloomDescriptorSetLayout;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount =
    static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

  if (vkCreatePipelineLayout(vkContext->logicalDevice,
                             &pipelineLayoutInfo,
                             nullptr,
                             &horizontalBloomPipelineLayout) != VK_SUCCESS) {
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
  pipelineInfo.layout = horizontalBloomPipelineLayout;
  pipelineInfo.renderPass = bloomRenderPass;
  pipelineInfo.subpass = 1;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(vkContext->logicalDevice,
                                VK_NULL_HANDLE,
                                1,
                                &pipelineInfo,
                                nullptr,
                                &horizontalBloomPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  vkDestroyShaderModule(vkContext->logicalDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(vkContext->logicalDevice, vertShaderModule, nullptr);
}

void
HDRPass::createVerticalBloomPipeline()
{
  std::string shaderPath = SHADER_PATH;
  auto vertShaderCode = readFile(shaderPath + "bloom/bloom_vert.spv");
  auto fragShaderCode = readFile(shaderPath + "bloom/bloom_frag.spv");

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

  VkSpecializationMapEntry specializationMapEntry{};
  specializationMapEntry.constantID = 0;
  specializationMapEntry.offset = 0;
  specializationMapEntry.size = sizeof(uint32_t);

  uint32_t dir = 1;
  VkSpecializationInfo specializationInfo{};
  specializationInfo.mapEntryCount = 1;
  specializationInfo.pMapEntries = &specializationMapEntry;
  specializationInfo.dataSize = sizeof(uint32_t);
  specializationInfo.pData = &dir;

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";
  fragShaderStageInfo.pSpecializationInfo = &specializationInfo;

  VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo,
                                                     fragShaderStageInfo };

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

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
  rasterizer.cullMode = VK_CULL_MODE_NONE;
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
  depthStencil.depthTestEnable = VK_FALSE;
  depthStencil.depthWriteEnable = VK_FALSE;
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
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.colorWriteMask = 0xF;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;

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
  descriptorSetLayouts[0] = bloomDescriptorSetLayout;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount =
    static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

  if (vkCreatePipelineLayout(vkContext->logicalDevice,
                             &pipelineLayoutInfo,
                             nullptr,
                             &verticalBloomPipelineLayout) != VK_SUCCESS) {
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
  pipelineInfo.layout = verticalBloomPipelineLayout;
  pipelineInfo.renderPass = presentationRenderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(vkContext->logicalDevice,
                                VK_NULL_HANDLE,
                                1,
                                &pipelineInfo,
                                nullptr,
                                &verticalBloomPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  vkDestroyShaderModule(vkContext->logicalDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(vkContext->logicalDevice, vertShaderModule, nullptr);
}

void
HDRPass::createCompositionPipeline()
{
  std::string shaderPath = SHADER_PATH;
  auto vertShaderCode = readFile(shaderPath + "bloom/bloom_vert.spv");
  auto fragShaderCode = readFile(shaderPath + "bloom/composition_frag.spv");

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

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

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
  rasterizer.cullMode = VK_CULL_MODE_NONE;
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
  depthStencil.depthTestEnable = VK_FALSE;
  depthStencil.depthWriteEnable = VK_FALSE;
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
  descriptorSetLayouts[0] = bloomDescriptorSetLayout;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount =
    static_cast<uint32_t>(descriptorSetLayouts.size());
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

  if (vkCreatePipelineLayout(vkContext->logicalDevice,
                             &pipelineLayoutInfo,
                             nullptr,
                             &compositionPipelineLayout) != VK_SUCCESS) {
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
  pipelineInfo.layout = compositionPipelineLayout;
  pipelineInfo.renderPass = presentationRenderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(vkContext->logicalDevice,
                                VK_NULL_HANDLE,
                                1,
                                &pipelineInfo,
                                nullptr,
                                &compositionPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }

  vkDestroyShaderModule(vkContext->logicalDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(vkContext->logicalDevice, vertShaderModule, nullptr);
}
