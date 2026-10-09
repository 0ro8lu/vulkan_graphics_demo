#include "engine/VulkanCheck.h"
#include "engine/Renderer.h"

Renderer::Renderer(GLFWwindow* window)
{
  m_contextOwner =
    std::make_unique<VulkanContext>(VulkanContext::Key{}, window);
  m_vkContext = m_contextOwner.get();
  m_vkSwapchain = std::make_unique<VulkanSwapchain>(
    VulkanSwapchain::Key{}, m_vkContext, window);

  m_camLightShadowBundle = createBaselineDescriptorsAndBuffers(m_vkContext);
}

Renderer::~Renderer()
{
  vkDestroyDescriptorSetLayout(m_vkContext->logicalDevice,
                               m_camLightShadowBundle.cameraUBOLayout,
                               nullptr);
  vkDestroyDescriptorSetLayout(m_vkContext->logicalDevice,
                               m_camLightShadowBundle.lightsUBOLayout,
                               nullptr);
  vkDestroyDescriptorSetLayout(
    m_vkContext->logicalDevice,
    m_camLightShadowBundle.directionalShadowmapLayout,
    nullptr);

  // TODO: use this when the resource manager comes into place
  // vkDestroyDescriptorSetLayout(
  //   m_vkContext->logicalDevice, m_camLightShadowBundle.skyboxLayout,
  //   nullptr);

  m_vkContext->destroyBuffer(m_camLightShadowBundle.cameraBuffer.buffer,
                             m_camLightShadowBundle.cameraBuffer.allocation);

  m_vkContext->destroyBuffer(
    m_camLightShadowBundle.directionalLightBuffer.buffer,
    m_camLightShadowBundle.directionalLightBuffer.allocation);

  m_vkContext->destroyBuffer(
    m_camLightShadowBundle.pointLightsBuffer.buffer,
    m_camLightShadowBundle.pointLightsBuffer.allocation);

  m_vkContext->destroyBuffer(
    m_camLightShadowBundle.spotLightsBuffer.buffer,
    m_camLightShadowBundle.spotLightsBuffer.allocation);

  vkDestroyDescriptorPool(
    m_vkContext->logicalDevice, m_camLightShadowBundle.descriptorPool, nullptr);
}

void
Renderer::waitIdle() const
{
  if (m_vkContext && m_vkContext->logicalDevice != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_vkContext->logicalDevice);
  }
}

void
Renderer::update(const Scene& scene)
{
  if (scene.spotLights.size() > MAX_SPOT_LIGHTS) {
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  if (scene.directionalLight.has_value()) {
    uint8_t* directionalMapped = reinterpret_cast<uint8_t*>(
      m_camLightShadowBundle.directionalLightBuffer.mapped);

    memcpy(directionalMapped,
           &scene.directionalLight.value(),
           m_camLightShadowBundle.directionalLightBuffer.size);
  }

  if (!scene.pointLights.empty()) {
    uint8_t* pointMapped = reinterpret_cast<uint8_t*>(
      m_camLightShadowBundle.pointLightsBuffer.mapped);
    memcpy(pointMapped,
           scene.pointLights.data(),
           m_camLightShadowBundle.pointLightsBuffer.size);
  }

  if (!scene.spotLights.empty()) {
    uint8_t* spotMapped = reinterpret_cast<uint8_t*>(
      m_camLightShadowBundle.spotLightsBuffer.mapped);
    memcpy(spotMapped,
           scene.spotLights.data(),
           m_camLightShadowBundle.spotLightsBuffer.size);
  }

  CameraBuffer cb;

  cb.view = scene.camera->getCameraMatrix();
  cb.proj = scene.camera->getCameraProjectionMatrix();
  cb.cameraPos = glm::vec4(scene.camera->getCameraPos(), 1);

  memcpy(m_camLightShadowBundle.cameraBuffer.mapped, &cb, sizeof(CameraBuffer));
}

void
Renderer::draw(const Scene& scene, GLFWwindow* window)
{
  m_vkSwapchain->prepareFrame(window);

  m_shadowMapPass->draw(m_vkSwapchain.get(), scene);
  m_blinnPhongPass->draw(m_vkSwapchain.get(),
                         scene,
                         m_camLightShadowBundle.cameraUBODescriptorset,
                         m_camLightShadowBundle.lightsUBODescriptorset,
                         m_camLightShadowBundle.shadowMapDescriptorSet);
  m_hdrPass->draw(m_vkSwapchain.get());

  m_vkSwapchain->submitFrame(window);
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
  blinnPhongLayoutConfig.cameraLayout = m_camLightShadowBundle.cameraUBOLayout;
  blinnPhongLayoutConfig.directionalShadowmapLayout =
    m_camLightShadowBundle.directionalShadowmapLayout;
  blinnPhongLayoutConfig.lightsLayout = m_camLightShadowBundle.lightsUBOLayout;

  m_blinnPhongPass = std::make_unique<BlinnPhongPass>(
    m_vkContext, blinnPhongAttachmentConfig, blinnPhongLayoutConfig);

  HDRPass::AttachmentConfig hdrAttachmentConfig{};
  hdrAttachmentConfig.swapchainImageFormat =
    m_vkSwapchain->swapChainImageFormat;
  hdrAttachmentConfig.width = m_vkSwapchain->width;
  hdrAttachmentConfig.height = m_vkSwapchain->height;

  m_hdrPass = std::make_unique<HDRPass>(m_vkContext, hdrAttachmentConfig);

  m_hdrPass->updateDescriptors(m_blinnPhongPass->hdrAttachment);

  m_blinnPhongPass->updateDescriptors(
    m_shadowMapPass->directionalShadowMap,
    m_shadowMapPass->spotPointShadowAtlas,
    m_camLightShadowBundle.shadowMapDescriptorSet);

  m_vkSwapchain->drawingPass = m_hdrPass->presentationRenderPass;

  m_vkSwapchain->createSwapChainFrameBuffer();
  m_vkSwapchain->onResize = [this](int width, int height) {
    m_blinnPhongPass->recreateAttachments(
      width, height, this->m_vkSwapchain->depthImageView);

    m_hdrPass->recreateAttachments(width, height);

    m_hdrPass->updateDescriptors(m_blinnPhongPass->hdrAttachment);
  };
}
