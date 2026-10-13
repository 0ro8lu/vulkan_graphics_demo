#pragma once

#include <array>
#include <cassert>
#include <span>
#include <string_view>
#include <vulkan/vulkan.h>

struct PipelineResult
{
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
};

template<typename T, size_t Capacity>
class FixedVector
{
public:
  constexpr FixedVector() = default;

  constexpr void push_back(const T& val)
  {
    assert(m_size < Capacity && "FixedVector capacity exceeded!");
    m_data[m_size++] = val;
  }

  constexpr void push_back(T&& val)
  {
    assert(m_size < Capacity && "FixedVector capacity exceeded!");
    m_data[m_size++] = std::move(val);
  }

  template<typename InputIt>
  constexpr void assign(InputIt first, InputIt last)
  {
    m_size = 0;
    for (auto it = first; it != last && m_size < Capacity; ++it) {
      m_data[m_size++] = *it;
    }
  }

  constexpr void clear() noexcept { m_size = 0; }

  [[nodiscard]] constexpr size_t size() const noexcept { return m_size; }
  [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }
  [[nodiscard]] constexpr const T* data() const noexcept { return m_data.data(); }
  [[nodiscard]] constexpr T* data() noexcept { return m_data.data(); }
  [[nodiscard]] constexpr const T& operator[](size_t i) const noexcept { return m_data[i]; }
  [[nodiscard]] constexpr T& operator[](size_t i) noexcept { return m_data[i]; }

  constexpr auto begin() noexcept { return m_data.begin(); }
  constexpr auto end() noexcept { return m_data.begin() + m_size; }
  constexpr auto begin() const noexcept { return m_data.begin(); }
  constexpr auto end() const noexcept { return m_data.begin() + m_size; }

private:
  std::array<T, Capacity> m_data{};
  size_t m_size{ 0 };
};

class PipelineBuilder
{
public:
  PipelineBuilder();

  // Shader Stages
  PipelineBuilder& setVertexShader(
    std::string_view shaderName,
    const VkSpecializationInfo* specialization = nullptr);

  PipelineBuilder& setFragmentShader(
    std::string_view shaderName,
    const VkSpecializationInfo* specialization = nullptr);

  // Vertex Input
  PipelineBuilder& withDefaultMeshVertexInput();
  PipelineBuilder& setDefaultVertexInput() { return withDefaultMeshVertexInput(); }
  PipelineBuilder& disableVertexInput();
  PipelineBuilder& setVertexInput(
    std::span<const VkVertexInputBindingDescription> bindings,
    std::span<const VkVertexInputAttributeDescription> attributes);

  // Input Assembly
  PipelineBuilder& setInputAssembly(
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    VkBool32 primitiveRestart = VK_FALSE);

  // Rasterizer
  PipelineBuilder& setPolygonMode(VkPolygonMode mode);
  PipelineBuilder& setCullMode(
    VkCullModeFlags cullMode,
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE);
  PipelineBuilder& setDepthBias(VkBool32 enable,
                                float constantFactor = 0.0f,
                                float clamp = 0.0f,
                                float slopeFactor = 0.0f);

  // Multisampling
  PipelineBuilder& setMultisampling(
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

  // Depth / Stencil
  PipelineBuilder& setDepthTest(
    VkBool32 depthTestEnable = VK_TRUE,
    VkBool32 depthWriteEnable = VK_TRUE,
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL);

  // Blending
  PipelineBuilder& addColorBlendAttachment(
    VkBool32 blendEnable = VK_FALSE,
    VkBlendFactor srcColor = VK_BLEND_FACTOR_ZERO,
    VkBlendFactor dstColor = VK_BLEND_FACTOR_ZERO,
    VkBlendOp colorOp = VK_BLEND_OP_ADD,
    VkBlendFactor srcAlpha = VK_BLEND_FACTOR_ZERO,
    VkBlendFactor dstAlpha = VK_BLEND_FACTOR_ZERO,
    VkBlendOp alphaOp = VK_BLEND_OP_ADD,
    VkColorComponentFlags writeMask = VK_COLOR_COMPONENT_R_BIT |
                                      VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT |
                                      VK_COLOR_COMPONENT_A_BIT);

  PipelineBuilder& addAdditiveBlendAttachment();
  PipelineBuilder& addAlphaBlendAttachment();
  PipelineBuilder& clearColorBlendAttachments();

  // Dynamic States
  PipelineBuilder& setDynamicStates(std::span<const VkDynamicState> states);

  // Layout & Pass
  PipelineBuilder& setDescriptorSetLayouts(
    std::span<const VkDescriptorSetLayout> layouts);
  PipelineBuilder& setPushConstantRanges(
    std::span<const VkPushConstantRange> ranges);
  PipelineBuilder& setRenderPass(VkRenderPass renderPass, uint32_t subpass = 0);
  PipelineBuilder& setPipelineLayout(VkPipelineLayout layout);

  // Build
  [[nodiscard]] PipelineResult build(VkDevice device);

private:
  struct ShaderStage
  {
    std::string_view name;
    VkShaderStageFlagBits stage;
    const VkSpecializationInfo* specialization = nullptr;
  };

  FixedVector<ShaderStage, 4> m_shaderStages;

  bool m_useVertexInput = false;
  FixedVector<VkVertexInputBindingDescription, 4> m_vertexBindings;
  FixedVector<VkVertexInputAttributeDescription, 16> m_vertexAttributes;

  VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{};
  VkPipelineRasterizationStateCreateInfo m_rasterizer{};
  VkPipelineMultisampleStateCreateInfo m_multisampling{};
  VkPipelineDepthStencilStateCreateInfo m_depthStencil{};

  FixedVector<VkPipelineColorBlendAttachmentState, 8> m_colorBlendAttachments;
  FixedVector<VkDynamicState, 8> m_dynamicStates;

  FixedVector<VkDescriptorSetLayout, 8> m_descriptorSetLayouts;
  FixedVector<VkPushConstantRange, 4> m_pushConstantRanges;

  VkRenderPass m_renderPass = VK_NULL_HANDLE;
  uint32_t m_subpass = 0;
  VkPipelineLayout m_existingLayout = VK_NULL_HANDLE;
};
