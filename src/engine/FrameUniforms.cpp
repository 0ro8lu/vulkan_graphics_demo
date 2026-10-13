#include "engine/FrameUniforms.h"

#include "engine/Camera3D.h"
#include "engine/LightManager.h"
#include "engine/SceneRenderData.h"
#include "engine/VulkanCheck.h"
#include "engine/VulkanContext.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>

FrameUniforms::FrameUniforms(VulkanContext* vkContext)
  : m_vkContext(vkContext)
{
  assert(m_vkContext != nullptr);

  createDescriptorPool();
  createLayouts();
  createFrameResources();
  createShadowMapDescriptorSet();
}

FrameUniforms::~FrameUniforms()
{
  if (!m_vkContext || m_vkContext->logicalDevice == VK_NULL_HANDLE) {
    return;
  }

  for (auto& frame : m_frames) {
    destroyBuffer(frame.cameraBuffer);
    destroyBuffer(frame.directionalLightBuffer);
    destroyBuffer(frame.pointLightsBuffer);
    destroyBuffer(frame.spotLightsBuffer);
  }

  if (m_descriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(
      m_vkContext->logicalDevice, m_descriptorPool, nullptr);
    m_descriptorPool = VK_NULL_HANDLE;
  }

  if (m_cameraLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(
      m_vkContext->logicalDevice, m_cameraLayout, nullptr);
    m_cameraLayout = VK_NULL_HANDLE;
  }
  if (m_lightsLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(
      m_vkContext->logicalDevice, m_lightsLayout, nullptr);
    m_lightsLayout = VK_NULL_HANDLE;
  }
  if (m_shadowMapLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(
      m_vkContext->logicalDevice, m_shadowMapLayout, nullptr);
    m_shadowMapLayout = VK_NULL_HANDLE;
  }
}

VulkanBuffer
FrameUniforms::createMappedBuffer(VkDeviceSize size, VkBufferUsageFlags usage)
{
  return m_vkContext->createBuffer(size, usage, BufferType::STAGING_BUFFER);
}

void
FrameUniforms::destroyBuffer(VulkanBuffer& buffer) noexcept
{
  m_vkContext->destroyBuffer(buffer);
}

void
FrameUniforms::createDescriptorPool()
{
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  // 1 camera + 3 lights = 4 UBO descriptors per frame * 2 frames = 8 UBO
  // descriptors
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = 16;

  // 2 combined image samplers for directional shadow map and spot/point atlas
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = 8;

  VkDescriptorPoolCreateInfo poolInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = 8,
    .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
    .pPoolSizes = poolSizes.data(),
  };

  VK_CHECK(vkCreateDescriptorPool(
    m_vkContext->logicalDevice, &poolInfo, nullptr, &m_descriptorPool));
}

void
FrameUniforms::createLayouts()
{
  // 1. Camera Layout (binding 0: Vertex stage UBO)
  {
    VkDescriptorSetLayoutBinding cameraBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .pImmutableSamplers = nullptr,
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &cameraBinding,
    };

    VK_CHECK(vkCreateDescriptorSetLayout(
      m_vkContext->logicalDevice, &layoutInfo, nullptr, &m_cameraLayout));
  }

  // 2. Lights Layout (binding 0: Directional, 1: Point, 2: Spot)
  {
    std::array<VkDescriptorSetLayoutBinding, 3> lightBindings{};
    for (uint32_t i = 0; i < 3; ++i) {
      lightBindings[i] = {
        .binding = i,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr,
      };
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(lightBindings.size()),
      .pBindings = lightBindings.data(),
    };

    VK_CHECK(vkCreateDescriptorSetLayout(
      m_vkContext->logicalDevice, &layoutInfo, nullptr, &m_lightsLayout));
  }

  // 3. Shadow Map Layout (binding 0: Directional, binding 1: Spot/Point Atlas)
  {
    std::array<VkDescriptorSetLayoutBinding, 2> shadowBindings{};
    shadowBindings[0] = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr,
    };
    shadowBindings[1] = {
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr,
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(shadowBindings.size()),
      .pBindings = shadowBindings.data(),
    };

    VK_CHECK(vkCreateDescriptorSetLayout(
      m_vkContext->logicalDevice, &layoutInfo, nullptr, &m_shadowMapLayout));
  }
}

void
FrameUniforms::createFrameResources()
{
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    auto& frame = m_frames[i];

    // Create UBO allocations
    frame.cameraBuffer = createMappedBuffer(sizeof(CameraBuffer),
                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    frame.directionalLightBuffer = createMappedBuffer(
      sizeof(DirectionalLight), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    frame.pointLightsBuffer =
      createMappedBuffer(sizeof(PointLight) * MAX_POINT_LIGHTS,
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    frame.spotLightsBuffer = createMappedBuffer(
      sizeof(SpotLight) * MAX_SPOT_LIGHTS, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // Allocate & populate Camera descriptor set
    {
      VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_cameraLayout,
      };
      VK_CHECK(vkAllocateDescriptorSets(
        m_vkContext->logicalDevice, &allocInfo, &frame.cameraDescriptorSet));

      VkDescriptorBufferInfo bufferInfo{
        .buffer = frame.cameraBuffer.buffer,
        .offset = 0,
        .range = sizeof(CameraBuffer),
      };

      VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = frame.cameraDescriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &bufferInfo,
      };

      vkUpdateDescriptorSets(m_vkContext->logicalDevice, 1, &write, 0, nullptr);
    }

    // Allocate & populate Lights descriptor set
    {
      VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_lightsLayout,
      };
      VK_CHECK(vkAllocateDescriptorSets(
        m_vkContext->logicalDevice, &allocInfo, &frame.lightsDescriptorSet));

      VkDescriptorBufferInfo dirLightInfo{
        .buffer = frame.directionalLightBuffer.buffer,
        .offset = 0,
        .range = sizeof(DirectionalLight),
      };
      VkDescriptorBufferInfo pointLightInfo{
        .buffer = frame.pointLightsBuffer.buffer,
        .offset = 0,
        .range = sizeof(PointLight) * MAX_POINT_LIGHTS,
      };
      VkDescriptorBufferInfo spotLightInfo{
        .buffer = frame.spotLightsBuffer.buffer,
        .offset = 0,
        .range = sizeof(SpotLight) * MAX_SPOT_LIGHTS,
      };

      std::array<VkWriteDescriptorSet, 3> writes{};
      writes[0] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = frame.lightsDescriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &dirLightInfo,
      };
      writes[1] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = frame.lightsDescriptorSet,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &pointLightInfo,
      };
      writes[2] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = frame.lightsDescriptorSet,
        .dstBinding = 2,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &spotLightInfo,
      };

      vkUpdateDescriptorSets(m_vkContext->logicalDevice,
                             static_cast<uint32_t>(writes.size()),
                             writes.data(),
                             0,
                             nullptr);
    }
  }
}

void
FrameUniforms::createShadowMapDescriptorSet()
{
  VkDescriptorSetAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = m_descriptorPool,
    .descriptorSetCount = 1,
    .pSetLayouts = &m_shadowMapLayout,
  };
  VK_CHECK(vkAllocateDescriptorSets(
    m_vkContext->logicalDevice, &allocInfo, &m_shadowMapDescriptorSet));
}

void
FrameUniforms::update(uint32_t frameIndex,
                      const SceneRenderData& sceneRenderData)
{
  if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
    std::cerr << "[FrameUniforms]: Error: frameIndex " << frameIndex
              << " exceeds MAX_FRAMES_IN_FLIGHT (" << MAX_FRAMES_IN_FLIGHT
              << ")\n";
    ENGINE_DEBUG_BREAK();
    std::abort();
  }
  if (sceneRenderData.spotLights.size() > MAX_SPOT_LIGHTS) {
    std::cerr << "[FrameUniforms]: Error: spot light count "
              << sceneRenderData.spotLights.size()
              << " exceeds MAX_SPOT_LIGHTS (" << MAX_SPOT_LIGHTS << ")\n";
    ENGINE_DEBUG_BREAK();
    std::abort();
  }
  if (sceneRenderData.pointLights.size() > MAX_POINT_LIGHTS) {
    std::cerr << "[FrameUniforms]: Error: point light count "
              << sceneRenderData.pointLights.size()
              << " exceeds MAX_POINT_LIGHTS (" << MAX_POINT_LIGHTS << ")\n";
    ENGINE_DEBUG_BREAK();
    std::abort();
  }

  auto& frame = m_frames[frameIndex];

  // 1. Directional Light
  if (sceneRenderData.directionalLight != nullptr) {
    std::memcpy(frame.directionalLightBuffer.mapped,
                sceneRenderData.directionalLight,
                sizeof(DirectionalLight));
  } else {
    std::memset(
      frame.directionalLightBuffer.mapped, 0, sizeof(DirectionalLight));
  }

  // 2. Point Lights
  if (!sceneRenderData.pointLights.empty()) {
    const size_t copySize =
      std::min(sceneRenderData.pointLights.size() * sizeof(PointLight),
               static_cast<size_t>(frame.pointLightsBuffer.size));
    std::memcpy(frame.pointLightsBuffer.mapped,
                sceneRenderData.pointLights.data(),
                copySize);
    if (copySize < frame.pointLightsBuffer.size) {
      std::memset(static_cast<uint8_t*>(frame.pointLightsBuffer.mapped) +
                    copySize,
                  0,
                  frame.pointLightsBuffer.size - copySize);
    }
  } else {
    std::memset(
      frame.pointLightsBuffer.mapped, 0, frame.pointLightsBuffer.size);
  }

  // 3. Spot Lights
  if (!sceneRenderData.spotLights.empty()) {
    const size_t copySize =
      std::min(sceneRenderData.spotLights.size() * sizeof(SpotLight),
               static_cast<size_t>(frame.spotLightsBuffer.size));
    std::memcpy(frame.spotLightsBuffer.mapped,
                sceneRenderData.spotLights.data(),
                copySize);
    if (copySize < frame.spotLightsBuffer.size) {
      std::memset(static_cast<uint8_t*>(frame.spotLightsBuffer.mapped) +
                    copySize,
                  0,
                  frame.spotLightsBuffer.size - copySize);
    }
  } else {
    std::memset(frame.spotLightsBuffer.mapped, 0, frame.spotLightsBuffer.size);
  }

  // 4. Camera
  if (sceneRenderData.camera) {
    CameraBuffer cb{
      .view = sceneRenderData.camera->getCameraMatrix(),
      .proj = sceneRenderData.camera->getCameraProjectionMatrix(),
      .cameraPos = glm::vec4(sceneRenderData.camera->getCameraPos(), 1.0f),
    };
    std::memcpy(frame.cameraBuffer.mapped, &cb, sizeof(CameraBuffer));
  }
}

VkDescriptorSet
FrameUniforms::getCameraDescriptorSet(uint32_t frameIndex) const noexcept
{
  assert(frameIndex < MAX_FRAMES_IN_FLIGHT);
  return m_frames[frameIndex].cameraDescriptorSet;
}

VkDescriptorSet
FrameUniforms::getLightsDescriptorSet(uint32_t frameIndex) const noexcept
{
  assert(frameIndex < MAX_FRAMES_IN_FLIGHT);
  return m_frames[frameIndex].lightsDescriptorSet;
}
