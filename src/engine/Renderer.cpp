#include "engine/Renderer.h"
#include "engine/Window.h"

Renderer::Renderer(const Window& window)
{
  m_instance = std::make_unique<VulkanInstance>();
  m_surface = m_instance->createSurface(window);

  m_device =
    std::make_unique<GpuDevice>(GpuDevice::Key{}, m_instance->instance(), m_surface);
  m_allocator = std::make_unique<GpuAllocator>(*m_device);
  m_samplerCache = std::make_unique<SamplerCache>(*m_device);
  m_immediateCommands = std::make_unique<ImmediateCommands>(*m_device);

  const auto initialExtent = window.framebufferSize();
  m_vkSwapchain = std::make_unique<VulkanSwapchain>(
    VulkanSwapchain::Key{},
    *m_device,
    *m_allocator,
    m_surface,
    VkExtent2D{ initialExtent.width, initialExtent.height });

  m_frameScheduler = std::make_unique<FrameScheduler>(*m_device);

  m_frameUniforms = std::make_unique<FrameUniforms>(*m_device, *m_allocator);
}

Renderer::~Renderer()
{
  waitIdle();

  m_hdrPass.reset();
  m_blinnPhongPass.reset();
  m_shadowMapPass.reset();
  m_frameUniforms.reset();
  m_frameScheduler.reset();
  m_vkSwapchain.reset();

  if (m_surface != VK_NULL_HANDLE) {
    m_instance->destroySurface(m_surface);
    m_surface = VK_NULL_HANDLE;
  }
}

void
Renderer::waitIdle() const
{
  if (m_device && m_device->device() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_device->device());
  }
}

void
Renderer::draw(const SceneRenderData& sceneRenderData)
{
  auto frame = m_frameScheduler->beginFrame(*m_vkSwapchain);
  if (!frame) {
    return;
  }

  m_frameUniforms->update(frame->currentFrame, sceneRenderData);

  m_shadowMapPass->draw(frame->commandBuffer, sceneRenderData);
  m_blinnPhongPass->draw(
    frame->commandBuffer,
    frame->extent,
    sceneRenderData,
    m_frameUniforms->getCameraDescriptorSet(frame->currentFrame),
    m_frameUniforms->getLightsDescriptorSet(frame->currentFrame),
    m_frameUniforms->getShadowMapDescriptorSet());
  m_hdrPass->draw(*frame);

  bool presented = m_frameScheduler->submitFrame(*m_vkSwapchain, *frame);
  if (!presented) {
    onResize(m_vkSwapchain->width(), m_vkSwapchain->height());
  }
}

void
Renderer::buildRenderGraph()
{
  ShadowMapPass::AttachmentConfig shadowAttachmentConfig{};
  shadowAttachmentConfig.depthFormat = m_vkSwapchain->depthFormat;
  shadowAttachmentConfig.directionalAtlasSize = DIRECTIONAL_ATLAS_SIZE;
  shadowAttachmentConfig.spotPointAtlasSize = SPOT_POINT_ATLAS_SIZE;
  m_shadowMapPass = std::make_unique<ShadowMapPass>(
    *m_device, *m_allocator, *m_samplerCache, shadowAttachmentConfig);

  BlinnPhongPass::AttachmentConfig blinnPhongAttachmentConfig{};
  blinnPhongAttachmentConfig.depthFormat = m_vkSwapchain->depthFormat;
  blinnPhongAttachmentConfig.depthImageView = m_vkSwapchain->depthImageView;
  blinnPhongAttachmentConfig.width = m_vkSwapchain->width();
  blinnPhongAttachmentConfig.height = m_vkSwapchain->height();

  BlinnPhongPass::LayoutConfig blinnPhongLayoutConfig{};
  blinnPhongLayoutConfig.cameraLayout = m_frameUniforms->getCameraLayout();
  blinnPhongLayoutConfig.directionalShadowmapLayout =
    m_frameUniforms->getShadowMapLayout();
  blinnPhongLayoutConfig.lightsLayout = m_frameUniforms->getLightsLayout();

  m_blinnPhongPass = std::make_unique<BlinnPhongPass>(
    *m_device,
    *m_allocator,
    *m_samplerCache,
    blinnPhongAttachmentConfig,
    blinnPhongLayoutConfig);

  HDRPass::AttachmentConfig hdrAttachmentConfig{};
  hdrAttachmentConfig.swapchainImageFormat =
    m_vkSwapchain->swapChainImageFormat;
  hdrAttachmentConfig.width = m_vkSwapchain->width();
  hdrAttachmentConfig.height = m_vkSwapchain->height();

  m_hdrPass = std::make_unique<HDRPass>(
    *m_device, *m_allocator, *m_samplerCache, hdrAttachmentConfig);

  m_hdrPass->updateDescriptors(m_blinnPhongPass->m_hdrAttachment);

  m_blinnPhongPass->updateDescriptors(
    m_shadowMapPass->m_directionalShadowMap,
    m_shadowMapPass->m_spotPointShadowAtlas,
    m_frameUniforms->getShadowMapDescriptorSet());

  m_vkSwapchain->drawingPass = m_hdrPass->m_presentationRenderPass;

  m_vkSwapchain->createSwapChainFrameBuffer();
}

void
Renderer::onResize(uint32_t width, uint32_t height)
{
  if (width == 0 || height == 0) {
    return;
  }

  VkExtent2D newExtent{ width, height };
  m_vkSwapchain->recreateSwapChain(newExtent);

  if (m_blinnPhongPass) {
    m_blinnPhongPass->recreateAttachments(
      width, height, m_vkSwapchain->depthImageView);
  }

  if (m_hdrPass) {
    m_hdrPass->recreateAttachments(width, height);
    if (m_blinnPhongPass) {
      m_hdrPass->updateDescriptors(m_blinnPhongPass->m_hdrAttachment);
    }
  }
}
