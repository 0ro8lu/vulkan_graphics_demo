#include "engine/Renderer.h"
#include "engine/VulkanCheck.h"

Renderer::Renderer(GLFWwindow* window)
{
  m_contextOwner =
    std::make_unique<VulkanContext>(VulkanContext::Key{}, window);
  m_vkContext = m_contextOwner.get();
  m_vkSwapchain = std::make_unique<VulkanSwapchain>(
    VulkanSwapchain::Key{}, m_vkContext, window);

  m_frameUniforms = std::make_unique<FrameUniforms>(m_vkContext);
}

Renderer::~Renderer()
{
  waitIdle();
}

void
Renderer::waitIdle() const
{
  if (m_vkContext && m_vkContext->logicalDevice != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_vkContext->logicalDevice);
  }
}

void
Renderer::draw(const SceneRenderData& sceneRenderData, GLFWwindow* window)
{
  auto frame = m_vkSwapchain->beginFrame(window);
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

  m_vkSwapchain->submitFrame(*frame, window);
}

void
Renderer::buildRenderGraph()
{
  ShadowMapPass::AttachmentConfig shadowAttachmentConfig{};
  shadowAttachmentConfig.depthFormat = m_vkSwapchain->depthFormat;
  shadowAttachmentConfig.directionalAtlasSize = DIRECTIONAL_ATLAS_SIZE;
  shadowAttachmentConfig.spotPointAtlasSize = SPOT_POINT_ATLAS_SIZE;
  m_shadowMapPass =
    std::make_unique<ShadowMapPass>(m_vkContext, shadowAttachmentConfig);

  BlinnPhongPass::AttachmentConfig blinnPhongAttachmentConfig{};
  blinnPhongAttachmentConfig.depthFormat = m_vkSwapchain->depthFormat;
  blinnPhongAttachmentConfig.depthImageView = m_vkSwapchain->depthImageView;
  blinnPhongAttachmentConfig.width = m_vkSwapchain->width;
  blinnPhongAttachmentConfig.height = m_vkSwapchain->height;

  BlinnPhongPass::LayoutConfig blinnPhongLayoutConfig{};
  blinnPhongLayoutConfig.cameraLayout = m_frameUniforms->getCameraLayout();
  blinnPhongLayoutConfig.directionalShadowmapLayout =
    m_frameUniforms->getShadowMapLayout();
  blinnPhongLayoutConfig.lightsLayout = m_frameUniforms->getLightsLayout();

  m_blinnPhongPass = std::make_unique<BlinnPhongPass>(
    m_vkContext, blinnPhongAttachmentConfig, blinnPhongLayoutConfig);

  HDRPass::AttachmentConfig hdrAttachmentConfig{};
  hdrAttachmentConfig.swapchainImageFormat =
    m_vkSwapchain->swapChainImageFormat;
  hdrAttachmentConfig.width = m_vkSwapchain->width;
  hdrAttachmentConfig.height = m_vkSwapchain->height;

  m_hdrPass = std::make_unique<HDRPass>(m_vkContext, hdrAttachmentConfig);

  m_hdrPass->updateDescriptors(m_blinnPhongPass->m_hdrAttachment);

  m_blinnPhongPass->updateDescriptors(
    m_shadowMapPass->m_directionalShadowMap,
    m_shadowMapPass->m_spotPointShadowAtlas,
    m_frameUniforms->getShadowMapDescriptorSet());

  m_vkSwapchain->drawingPass = m_hdrPass->m_presentationRenderPass;

  m_vkSwapchain->createSwapChainFrameBuffer();
  m_vkSwapchain->onResize = [this](int width, int height) {
    m_blinnPhongPass->recreateAttachments(
      width, height, this->m_vkSwapchain->depthImageView);

    m_hdrPass->recreateAttachments(width, height);

    m_hdrPass->updateDescriptors(m_blinnPhongPass->m_hdrAttachment);
  };
}
