#include "engine/Camera3D.h"
#include "engine/FrameBufferAttachment.h"
#include "engine/Model.h"
#include "engine/Skybox.h"
#include "engine/Vertex.h"
#include "engine/VulkanBase.h"
#include <vulkan/vulkan_core.h>

#ifdef WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#define CUTE_SOUND_IMPLEMENTATION
#define CUTE_SOUND_SCALAR_MODE
#include <cute_sound.h>

#define VMA_IMPLEMENTATION
#define VMA_DEBUG_DETECT_CORRUPTION 1
#define VMA_DEBUG_MARGIN 16
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#define VMA_RECORD_ALLOCATION_CALL_STACK 1
#define VMA_DEBUG_LOG
#include <vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#include <ext/matrix_clip_space.hpp>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// TODO: remove this and all the vectors
const int MAX_FRAMES_IN_FLIGHT = 1;

// this needs to be aligned with the one in the shader
const unsigned int NR_POINT_LIGHTS = 5;

// positions of the point lights
glm::vec3 pointLightPositions[] = { glm::vec3(0, 0, 0),
                                    glm::vec3(0.0f, -2.0f, 0.0f),
                                    glm::vec3(2.3f, -1.3f, -4.0f),
                                    glm::vec3(-4.0f, -2.0f, -12.0f),
                                    glm::vec3(0.0f, 0.0f, -3.0f) };

const unsigned int NR_OBJECTS = 10;

enum BufferType
{
  STAGING_BUFFER,
  GPU_BUFFER,
};

void
DestroyDebugUtilsMessengerEXT(VkInstance instance,
                              VkDebugUtilsMessengerEXT debugMessenger,
                              const VkAllocationCallbacks* pAllocator)
{
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
    instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

struct UniformBufferObject
{
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
  alignas(16) glm::vec4 cameraPos;
};

struct DirectionalLight
{
  glm::vec4 direction;
};

struct PointLight
{
  glm::vec4 position;
};

struct SpotLight
{
  glm::vec4 direction;
  glm::vec4 position;
};

class DemoApplication : public VulkanBase
{
public:
  DemoApplication()
  {
    camera.reset(
      new Camera3D(glm::vec3(0.0, 0.0, 3.0), glm::vec3(0.0, 0.0, -1.0)));
  }

  void run()
  {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  GLFWwindow* window;
  std::unique_ptr<Camera3D> camera;
  std::vector<Model> models;
  std::unique_ptr<Skybox> skybox;
  std::vector<Model> lightModels;

  VmaAllocator allocator;

  VkSwapchainKHR swapChain;
  std::vector<VkImage> swapChainImages;
  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;
  std::vector<VkImageView> swapChainImageViews;
  std::vector<VkFramebuffer> swapChainFramebuffers;

  FrameBufferAttachment* depthAttachment;
  FrameBufferAttachment* colorAttachment;
  VkSampler colorAttachmentSampler;

  VkRenderPass renderPass;

  VkDescriptorPool mainDescriptorPool;

  VkDescriptorSetLayout descriptorSetLayoutAttachmentWrite;
  std::vector<VkDescriptorSet> descriptorSetsAttachmentWrite;

  VkDescriptorSetLayout descriptorSetLayoutAttachmentRead;
  VkDescriptorSet descriptorSetsAttachmentRead;

  VkPipelineLayout mainPipelineLayout;
  VkPipeline mainGraphicsPipeline;
  VkPipelineLayout lightCubePipelineLayout;
  VkPipeline lightCubePipeline;
  VkPipelineLayout skyboxPipelineLayout;
  VkPipeline skyboxPipeline;
  VkPipelineLayout postProcessingPipelineLayout;
  VkPipeline postProcessingPipeline;

  VkCommandPool commandPool;

  std::vector<VkBuffer> uniformBuffers;
  std::vector<VmaAllocation> uniformBuffersAllocation;
  std::vector<void*> uniformBuffersMapped;

  std::vector<glm::mat4> lightMat;

  // lights
  std::vector<VkBuffer> directionalLightBuffers;
  std::vector<VmaAllocation> directionalLightBuffersAllocation;
  std::vector<void*> directionalLightBuffersMapped;

  std::vector<VkBuffer> pointLightBuffers;
  std::vector<VmaAllocation> pointLightBuffersAllocation;
  std::vector<void*> pointLightBuffersMapped;

  std::vector<VkBuffer> spotLightBuffers;
  std::vector<VmaAllocation> spotLightBuffersAllocation;
  std::vector<void*> spotLightBuffersMapped;

  std::vector<VkCommandBuffer> commandBuffers;

  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;
  uint32_t currentFrame = 0;

  bool framebufferResized = false;

  size_t dynamicAlignmentUBO;

  cs_audio_source_t* voice;

  void initWindow()
  {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  }

  static void framebufferResizeCallback(GLFWwindow* window,
                                        int width,
                                        int height)
  {
    auto app =
      reinterpret_cast<DemoApplication*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
  }

  virtual void getEnabledFeatures()
  {
    if (deviceFeatures.samplerAnisotropy) {
      deviceFeatures.samplerAnisotropy = VK_TRUE;
    }
  }

  void initVulkan()
  {
    createInstance();
    setupDebugMessenger();
    createSurface();
    selectPhysicalDevice();
    createLogicalDevice();
    createVMAAllocator();
    createCommandPool();
    createVulkanDevice();
    
    createSwapChain();
    createImageViews();
    createIntermediateAttachment();
    createRenderPass();
    prepareModels();
    prepareAudio();
    createDepthResources();
    createFramebuffers();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSetLayout();
    createDescriptorSets();
    createGraphicsPipelines();
    createCommandBuffers();
    createSyncObjects();
  }

  void mainLoop()
  {
    constexpr auto frame_duration = std::chrono::microseconds(8333);
    auto last_frame_time = std::chrono::steady_clock::now();

    // int frame_count = 0;
    // auto fps_time_start = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window)) {

      auto frame_start = std::chrono::steady_clock::now();
      std::chrono::duration<float> delta_time = frame_start - last_frame_time;
      last_frame_time = frame_start;

      glfwPollEvents();

      if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
      }

      updateCamera(delta_time.count());

      cs_update(0);

      drawFrame();

      // frame_count++;

      // auto now = std::chrono::steady_clock::now();
      // std::chrono::duration<float> elapsed_fps_time = now - fps_time_start;

      // if (elapsed_fps_time.count() >= 1.0f) {
      //     float fps = frame_count / elapsed_fps_time.count();
      //     std::cout << "FPS: " << fps << std::endl;

      //     // Reset the FPS counter and timer
      //     frame_count = 0;
      //     fps_time_start = now;
      // }

      auto frame_end = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        frame_end - frame_start);

      auto sleep_duration = frame_duration - elapsed;

      if (sleep_duration > std::chrono::microseconds::zero()) {
        std::this_thread::sleep_for(sleep_duration);
      }
    }

    vkDeviceWaitIdle(logicalDevice);
  }

  void cleanupSwapChain()
  {
    for (auto framebuffer : swapChainFramebuffers) {
      vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
    }

    for (auto imageView : swapChainImageViews) {
      vkDestroyImageView(logicalDevice, imageView, nullptr);
    }

    delete depthAttachment;
    delete colorAttachment;
    vkDestroySampler(logicalDevice, colorAttachmentSampler, nullptr);

    vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
  }

  void cleanup()
  {
    cleanupSwapChain();

    vkDestroyPipeline(logicalDevice, mainGraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, mainPipelineLayout, nullptr);

    vkDestroyPipeline(logicalDevice, skyboxPipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, skyboxPipelineLayout, nullptr);

    vkDestroyPipeline(logicalDevice, lightCubePipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, lightCubePipelineLayout, nullptr);

    vkDestroyPipeline(logicalDevice, postProcessingPipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, postProcessingPipelineLayout, nullptr);

    vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      // for object transformations
      vmaDestroyBuffer(
        allocator, uniformBuffers[i], uniformBuffersAllocation[i]);

      // for lights
      vmaDestroyBuffer(allocator,
                       directionalLightBuffers[i],
                       directionalLightBuffersAllocation[i]);
      vmaDestroyBuffer(
        allocator, pointLightBuffers[i], pointLightBuffersAllocation[i]);
      vmaDestroyBuffer(
        allocator, spotLightBuffers[i], spotLightBuffersAllocation[i]);
    }

    models.clear();
    lightModels.clear();
    skybox.reset();

    vkDestroyDescriptorPool(logicalDevice, mainDescriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(
      logicalDevice, descriptorSetLayoutAttachmentWrite
  , nullptr);
    vkDestroyDescriptorSetLayout(logicalDevice, Model::textureLayout, nullptr);
    vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayoutAttachmentRead, nullptr);

    vmaDestroyAllocator(allocator);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
      vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
      vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

    vkDestroyDevice(logicalDevice, nullptr);

    if (enableValidationLayers) {
      DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();

    cs_free_audio_source(voice);
  }

  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features)
  {
    for (VkFormat format : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

      if (tiling == VK_IMAGE_TILING_LINEAR &&
          (props.linearTilingFeatures & features) == features) {
        return format;
      } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                 (props.optimalTilingFeatures & features) == features) {
        return format;
      }
    }
    throw std::runtime_error("failed to find supported format!");
  }

  VkFormat findDepthFormat()
  {
    return findSupportedFormat({ VK_FORMAT_D32_SFLOAT,
                                 VK_FORMAT_D32_SFLOAT_S8_UINT,
                                 VK_FORMAT_D24_UNORM_S8_UINT },
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  }

  bool hasStencilComponent(VkFormat format)
  {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           format == VK_FORMAT_D24_UNORM_S8_UINT;
  }

  void updateCamera(float deltaTime)
  {
    const float cameraSpeed = deltaTime * 2.5f; // adjust accordingly

    glm::vec3 cameraPos = camera->getCameraPos();
    glm::vec3 cameraFront = camera->getCameraFront();
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      cameraPos -=
        glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      cameraPos +=
        glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
      camera->pitch -= 40 * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
      camera->pitch += 40 * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
      camera->yaw -= 40 * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
      camera->yaw += 40 * deltaTime;

    if (camera->pitch > 89.0f)
      camera->pitch = 89.0f;
    if (camera->pitch < -89.0f)
      camera->pitch = -89.0f;

    camera->setPos(cameraPos);
    camera->setDirection(cameraFront);

    camera->update();
  }

  void recreateSwapChain()
  {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }

    vkDeviceWaitIdle(logicalDevice);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createFramebuffers();

    createIntermediateAttachment();
  }

  void createVMAAllocator()
  {
    VmaAllocatorCreateInfo createInfo = {};
    createInfo.physicalDevice = physicalDevice;
    createInfo.device = logicalDevice;
    createInfo.instance = instance;

    if (vmaCreateAllocator(&createInfo, &allocator) != VK_SUCCESS) {
      throw std::runtime_error("failed to create vma allocator!");
    }
  }

  void createSurface()
  {
    // TODO: make sure the queue supports present mode.
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create window surface!");
    }
  }

  void createSwapChain()
  {
    SwapChainSupportDetails swapChainSupport =
      querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat =
      chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode =
      chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
      imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(),
                                      indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(
      logicalDevice, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
  }

  void createIntermediateAttachment()
  {
    colorAttachment =
      new FrameBufferAttachment(swapChainImageFormat,
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                swapChainExtent.width,
                                swapChainExtent.height,
                                vulkanDevice);
    //TODO: create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 1.0;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    
    if (vkCreateSampler(
        logicalDevice, &samplerInfo, nullptr, &colorAttachmentSampler) !=
      VK_SUCCESS) {
      throw std::runtime_error("failed to create texture sampler!");
    }
  }

  void createImageViews()
  {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
      swapChainImageViews[i] = vulkanDevice->createImageView(
        swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
  }

  void createRenderPass()
  {
    std::array<VkAttachmentDescription, 3> attachments{};

    attachments[0].format = swapChainImageFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format = swapChainImageFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    attachments[2].format = findDepthFormat();
	attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkSubpassDescription,2> subpassDescriptions{};

    /*
		First subpass
		Fill the color and depth attachments
	*/
	VkAttachmentReference colorReference = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depthReference = { 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescriptions[0].colorAttachmentCount = 1;
	subpassDescriptions[0].pColorAttachments = &colorReference;
	subpassDescriptions[0].pDepthStencilAttachment = &depthReference;

    /*
		Second subpass
		Input attachment read and swap chain color attachment write
	*/

	// Color reference (target) for this sub pass is the swap chain color attachment
	VkAttachmentReference colorReferenceSwapchain = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	subpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescriptions[1].colorAttachmentCount = 1;
	subpassDescriptions[1].pColorAttachments = &colorReferenceSwapchain;

	// Color and depth attachment written to in first sub pass will be used as input attachments to be read in the fragment shader
	VkAttachmentReference inputReferences[2];
	inputReferences[0] = { 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	inputReferences[1] = { 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

	// Use the attachments filled in the first pass as input attachments
	subpassDescriptions[1].inputAttachmentCount = 2;
	subpassDescriptions[1].pInputAttachments = inputReferences;

    /*
		Subpass dependencies for layout transitions
	*/
	std::array<VkSubpassDependency, 3> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// This dependency transitions the input attachment from color attachment to shader read
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[2].srcSubpass = 0;
	dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = static_cast<uint32_t>(subpassDescriptions.size());
	renderPassInfo.pSubpasses = subpassDescriptions.data();
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(
          logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
      throw std::runtime_error("failed to create render pass!");
    }
  }

  void createDescriptorSetLayout()
  {
    /* Descriptor layout for Attachment Write */
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding directionalLightLayoutBinding{};
    directionalLightLayoutBinding.binding = 4;
    directionalLightLayoutBinding.descriptorCount = 1;
    directionalLightLayoutBinding.descriptorType =
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    directionalLightLayoutBinding.pImmutableSamplers = nullptr;
    directionalLightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding pointLightLayoutBinding{};
    pointLightLayoutBinding.binding = 5;
    pointLightLayoutBinding.descriptorCount = 1;
    pointLightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pointLightLayoutBinding.pImmutableSamplers = nullptr;
    pointLightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding spotLightLayoutBinding{};
    spotLightLayoutBinding.binding = 6;
    spotLightLayoutBinding.descriptorCount = 1;
    spotLightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    spotLightLayoutBinding.pImmutableSamplers = nullptr;
    spotLightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
      uboLayoutBinding,
      directionalLightLayoutBinding,
      pointLightLayoutBinding,
      spotLightLayoutBinding
    };

    VkDescriptorSetLayoutCreateInfo layoutInfoAttachmentWrite{};

    layoutInfoAttachmentWrite.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoAttachmentWrite.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfoAttachmentWrite.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(
          logicalDevice, &layoutInfoAttachmentWrite, nullptr, &descriptorSetLayoutAttachmentWrite) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }

    /* Descriptor layout for Attachment Read */
    VkDescriptorSetLayoutBinding attachmentBinding{};
    attachmentBinding.binding = 0;
    attachmentBinding.descriptorCount = 1;
    attachmentBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    attachmentBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfoAttachmentRead{};

    layoutInfoAttachmentRead.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoAttachmentRead.bindingCount = 1;
    layoutInfoAttachmentRead.pBindings = &attachmentBinding;

    if (vkCreateDescriptorSetLayout(
          logicalDevice, &layoutInfoAttachmentRead, nullptr, &descriptorSetLayoutAttachmentRead) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  void createVulkanDevice()
  {
    vulkanDevice = new VulkanDevice(
      physicalDevice, logicalDevice, allocator, commandPool, graphicsQueue);
  }

  void createGraphicsPipelines()
  {
    std::string shaderPath = SHADER_PATH;

    VkDescriptorSetLayout mainLayouts[] = { descriptorSetLayoutAttachmentWrite
  ,
                                            Model::textureLayout };
    createPipeline(shaderPath + "post_processing_texture/built/texture_vert.spv",
                   shaderPath + "post_processing_texture/built/texture_frag.spv",
                   mainPipelineLayout,
                   mainGraphicsPipeline,
                   mainLayouts,
                   2,
                   0);
    VkDescriptorSetLayout lightLayouts[] = { descriptorSetLayoutAttachmentWrite
   };
    createPipeline(shaderPath + "post_processing_texture/built/light_cube_vert.spv",
                   shaderPath + "post_processing_texture/built/light_cube_frag.spv",
                   lightCubePipelineLayout,
                   lightCubePipeline,
                   lightLayouts,
                   1,
                   0);
    VkDescriptorSetLayout skyboxLayouts[] = { descriptorSetLayoutAttachmentWrite
  ,
                                              Skybox::skyboxLayout };
    createPipeline(shaderPath + "post_processing_texture/built/skybox_vert.spv",
                   shaderPath + "post_processing_texture/built/skybox_frag.spv",
                   skyboxPipelineLayout,
                   skyboxPipeline,
                   skyboxLayouts,
                   2,
                   0,
                   VK_CULL_MODE_FRONT_BIT);

        VkDescriptorSetLayout postProcessingLayout[] = { descriptorSetLayoutAttachmentRead
   };

    createPipeline(shaderPath + "post_processing_texture/built/postprocessing_vert.spv",
                   shaderPath + "post_processing_texture/built/postprocessing_frag.spv",
                   postProcessingPipelineLayout,
                   postProcessingPipeline,
                   postProcessingLayout,
                   1,
                   1, 
                   VK_CULL_MODE_NONE,
                   false);
  }

  void createPipeline(std::string vertexShader,
                      std::string fragmentShader,
                      VkPipelineLayout& pipelineLayout,
                      VkPipeline& pipeline,
                      VkDescriptorSetLayout* pLayouts,
                      uint32_t numLayouts,
                      uint32_t subPass,
                      VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
                      bool usesVertices = true)
  {
    auto vertShaderCode = readFile(vertexShader);
    auto fragShaderCode = readFile(fragmentShader);

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

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
    if(usesVertices) {

      auto bindingDescription = Vertex::getBindingDescription();
      auto attributeDescriptions = Vertex::getAttributeDescriptions();

      vertexInputInfo.vertexBindingDescriptionCount = 1;
      vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
      vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
      vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    } 

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
    rasterizer.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = cullMode;
    // rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
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
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
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

    // allocate push constant
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    range.offset = 0;
    range.size = 64;

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount =
      static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = numLayouts;
    pipelineLayoutInfo.pSetLayouts = pLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &range;

    if (vkCreatePipelineLayout(
          logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
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
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = subPass;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(logicalDevice,
                                  VK_NULL_HANDLE,
                                  1,
                                  &pipelineInfo,
                                  nullptr,
                                  &pipeline) != VK_SUCCESS) {
      throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
    vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
  }

  void createFramebuffers()
  {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
      std::array<VkImageView, 3> attachments = { swapChainImageViews[i],
                                                 colorAttachment->view,
                                                 depthAttachment->view };

      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = renderPass;
      framebufferInfo.attachmentCount =
        static_cast<uint32_t>(attachments.size());
      framebufferInfo.pAttachments = attachments.data();
      framebufferInfo.width = swapChainExtent.width;
      framebufferInfo.height = swapChainExtent.height;
      framebufferInfo.layers = 1;

      if (vkCreateFramebuffer(logicalDevice,
                              &framebufferInfo,
                              nullptr,
                              &swapChainFramebuffers[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed to create framebuffer!");
      }
    }
  }

  void createCommandPool()
  {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create graphics command pool!");
    }
  }

  void createDepthResources()
  {
    VkFormat depthFormat = findDepthFormat();

    depthAttachment =
      new FrameBufferAttachment(depthFormat,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                swapChainExtent.width,
                                swapChainExtent.height,
                                vulkanDevice);
  }

  void prepareModels()
  {
    glm::vec3 cubePositions[] = {
      glm::vec3(0.0f, 0.0f, 0.0f),    glm::vec3(2.0f, 5.0f, -15.0f),
      glm::vec3(-1.5f, -2.2f, -2.5f), glm::vec3(-3.8f, -2.0f, -12.3f),
      glm::vec3(2.4f, -0.4f, -3.5f),  glm::vec3(-1.7f, 3.0f, -7.5f),
      glm::vec3(1.3f, -2.0f, -2.5f),  glm::vec3(1.5f, 2.0f, -2.5f),
      glm::vec3(1.5f, 0.2f, -1.5f),   glm::vec3(-1.3f, 1.0f, -1.5f)
    };

    std::string modelPath = MODEL_PATH;

    Model plane = Model(modelPath + "for_demo/plane.glb",
                        vulkanDevice,
                        glm::vec3(0.0, 1.0f, -3.0f),
                        glm::vec3(0),
                        0,
                        glm::vec3(100.0f));
    models.push_back(std::move(plane));
    // Model desk = Model(modelPath + "for_demo/prova_optimized.glb",
    //                    vulkanDevice,
    //                    glm::vec3(0.0, 1.0f, -3.0f),
    //                    glm::vec3(0.0, 1.0, 0.0),
    //                    180.0f,
    //                    glm::vec3(1.0f));
    // Model desk = Model(modelPath + "for_demo/prova.glb",
    //                    vulkanDevice,
    //                    glm::vec3(0.0, 1.0f, -3.0f),
    //                    glm::vec3(0.0, 1.0, 0.0),
    //                    180.0f,
    //                    glm::vec3(1.0f));
    // models.push_back(std::move(desk));

    Model rare = Model(modelPath + "for_demo/rare_logo/rare.glb",
                       vulkanDevice,
                       glm::vec3(-1.85, -0.7, -19.5f),
                       glm::vec3(1.0, 0.0, 0.0),
                       -90.0f,
                       glm::vec3(0.1f));
    models.push_back(std::move(rare));

    for (int i = 0; i < NR_POINT_LIGHTS; i++) {
      Model cube = Model(modelPath + "cube.glb",
                         vulkanDevice,
                         pointLightPositions[i],
                         glm::vec3(0.0),
                         0,
                         glm::vec3(0.1f));
      lightModels.push_back(std::move(cube));
    }

    std::string texturePath = TEXTURE_PATH;
    std::array<std::string, 6> files = {
      texturePath + "skybox/right.jpg", texturePath + "skybox/left.jpg",
      texturePath + "skybox/top.jpg",   texturePath + "skybox/bottom.jpg",
      texturePath + "skybox/front.jpg", texturePath + "skybox/back.jpg"
    };

    skybox = std::make_unique<Skybox>(vulkanDevice, files);
  }

  void prepareAudio()
  {

#ifdef WIN32
    HWND handle = glfwGetWin32Window(window);
    cs_init(handle, 44100, 1024, NULL);
#else
    cs_init(NULL, 44100, 1024, NULL);
#endif

    std::string soundPath = SOUND_PATH;
    soundPath = soundPath + "Heartbreak Of The Century.wav";
    voice = cs_load_wav(soundPath.c_str(), NULL);

    cs_sound_params_t params = cs_sound_params_default();
    cs_playing_sound_t voice_snd = CUTE_PLAYING_SOUND_INVALID;

    cs_play_sound(voice, params);
  }

  void createUniformBuffers()
  {
    VkDeviceSize uboBufferSize = sizeof(UniformBufferObject);

    VkDeviceSize directionalLightBufferSize = sizeof(DirectionalLight);
    VkDeviceSize pointLightBufferSize = sizeof(PointLight) * NR_POINT_LIGHTS;
    VkDeviceSize spotLightBufferSize = sizeof(SpotLight);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersAllocation.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    // for the lights
    directionalLightBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    directionalLightBuffersAllocation.resize(MAX_FRAMES_IN_FLIGHT);
    directionalLightBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    pointLightBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    pointLightBuffersAllocation.resize(MAX_FRAMES_IN_FLIGHT);
    pointLightBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    spotLightBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    spotLightBuffersAllocation.resize(MAX_FRAMES_IN_FLIGHT);
    spotLightBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      uniformBuffersMapped[i] = createBuffer(uboBufferSize,
                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                             BufferType::STAGING_BUFFER,
                                             uniformBuffers[i],
                                             uniformBuffersAllocation[i]);

      // for the lights
      directionalLightBuffersMapped[i] =
        createBuffer(directionalLightBufferSize,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     BufferType::STAGING_BUFFER,
                     directionalLightBuffers[i],
                     directionalLightBuffersAllocation[i]);
      pointLightBuffersMapped[i] =
        createBuffer(pointLightBufferSize,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     BufferType::STAGING_BUFFER,
                     pointLightBuffers[i],
                     pointLightBuffersAllocation[i]);
      spotLightBuffersMapped[i] =
        createBuffer(spotLightBufferSize,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     BufferType::STAGING_BUFFER,
                     spotLightBuffers[i],
                     spotLightBuffersAllocation[i]);
    }

    for (int i = 0; i < NR_POINT_LIGHTS; i++) {
      glm::mat4 model = glm::mat4(1.0);
      model = glm::translate(model, pointLightPositions[i]);
      model = glm::scale(model, glm::vec3(0.2f));
      lightMat.push_back(model);
    }

    // ------------------ LIGHTS ------------------

    std::array<PointLight, NR_POINT_LIGHTS> pointLights{};

    for (int i = 0; i < NR_POINT_LIGHTS; i++) {
      pointLights[i].position = glm::vec4(pointLightPositions[i], 1.0);
    }

    // point lights
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      uint8_t* mappedUniform =
        reinterpret_cast<uint8_t*>(pointLightBuffersMapped[i]);
      memcpy(mappedUniform, &pointLights, pointLightBufferSize);
    }

    DirectionalLight directionalLight;
    directionalLight.direction = glm::vec4(0.2f, -1.0f, 0.3f, 0.0);

    // directional lights
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      uint8_t* mappedUniform =
        reinterpret_cast<uint8_t*>(directionalLightBuffersMapped[i]);
      memcpy(mappedUniform, &directionalLight, sizeof(directionalLight));
    }
  }

  void createDescriptorPool()
  {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 4;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 2;

    if (vkCreateDescriptorPool(
          logicalDevice, &poolInfo, nullptr, &mainDescriptorPool) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor pool!");
    }
  }

  void createDescriptorSets()
  {
    /* descriptor sets for attachmentWrite */
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                               descriptorSetLayoutAttachmentWrite
                                            );
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mainDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSetsAttachmentWrite.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(logicalDevice,
                                 &allocInfo,
                                 descriptorSetsAttachmentWrite.data()) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      VkDescriptorBufferInfo uniformBufferInfo{};
      uniformBufferInfo.buffer = uniformBuffers[i];
      uniformBufferInfo.offset = 0;
      uniformBufferInfo.range = sizeof(UniformBufferObject);

      VkDescriptorBufferInfo directionalLightBufferInfo{};
      directionalLightBufferInfo.buffer = directionalLightBuffers[i];
      directionalLightBufferInfo.offset = 0;
      directionalLightBufferInfo.range = sizeof(DirectionalLight);

      VkDescriptorBufferInfo pointLightBufferInfo{};
      pointLightBufferInfo.buffer = pointLightBuffers[i];
      pointLightBufferInfo.offset = 0;
      pointLightBufferInfo.range = sizeof(PointLight);

      VkDescriptorBufferInfo spotLightBufferInfo{};
      spotLightBufferInfo.buffer = spotLightBuffers[i];
      spotLightBufferInfo.offset = 0;
      spotLightBufferInfo.range = sizeof(SpotLight);

      std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

      descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[0].dstSet = descriptorSetsAttachmentWrite[i];
      descriptorWrites[0].dstBinding = 0;
      descriptorWrites[0].dstArrayElement = 0;
      descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrites[0].descriptorCount = 1;
      descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

      descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[1].dstSet = descriptorSetsAttachmentWrite[i];
      descriptorWrites[1].dstBinding = 4;
      descriptorWrites[1].dstArrayElement = 0;
      descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrites[1].descriptorCount = 1;
      descriptorWrites[1].pBufferInfo = &directionalLightBufferInfo;

      descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[2].dstSet = descriptorSetsAttachmentWrite[i];
      descriptorWrites[2].dstBinding = 5;
      descriptorWrites[2].dstArrayElement = 0;
      descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrites[2].descriptorCount = 1;
      descriptorWrites[2].pBufferInfo = &pointLightBufferInfo;

      descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[3].dstSet = descriptorSetsAttachmentWrite[i];
      descriptorWrites[3].dstBinding = 6;
      descriptorWrites[3].dstArrayElement = 0;
      descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrites[3].descriptorCount = 1;
      descriptorWrites[3].pBufferInfo = &spotLightBufferInfo;

      vkUpdateDescriptorSets(logicalDevice,
                             static_cast<uint32_t>(descriptorWrites.size()),
                             descriptorWrites.data(),
                             0,
                             nullptr);
    }

    /* descriptor sets for attachmentRead */
    VkDescriptorSetAllocateInfo allocInfoRead{};
    allocInfoRead.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfoRead.descriptorPool = mainDescriptorPool;
    allocInfoRead.descriptorSetCount = 1;
    allocInfoRead.pSetLayouts = &descriptorSetLayoutAttachmentRead;

    if (vkAllocateDescriptorSets(logicalDevice,
                                 &allocInfoRead,
                                 &descriptorSetsAttachmentRead) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = colorAttachment->view;
    imageInfo.sampler = colorAttachmentSampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSetsAttachmentRead;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(logicalDevice,
                           1,
                           &descriptorWrite,
                           0,
                           nullptr);

  }

  void* createBuffer(VkDeviceSize size,
                     VkBufferUsageFlags usage,
                     BufferType bufferType,
                     VkBuffer& buffer,
                     VmaAllocation& allocation)
  {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    switch (bufferType) {
      case BufferType::STAGING_BUFFER:
        allocCreateInfo.flags =
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT;
        break;
      case BufferType::GPU_BUFFER:
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocCreateInfo.priority = 1.0f;
        break;
    }

    VmaAllocationInfo allocInfo;
    if (vmaCreateBuffer(allocator,
                        &bufferInfo,
                        &allocCreateInfo,
                        &buffer,
                        &allocation,
                        &allocInfo) != VK_SUCCESS) {
      throw std::runtime_error("failed to create buffer!");
    }

    return allocInfo.pMappedData;
  }

  VkCommandBuffer beginSingleTimeCommands()
  {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
  }

  void endSingleTimeCommands(VkCommandBuffer commandBuffer)
  {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
  }

  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
  {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
  }

  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
  {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) &&
          (memProperties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
        return i;
      }
    }

    throw std::runtime_error("failed to find suitable memory type!");
  }

  void createCommandBuffers()
  {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(
          logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate command buffers!");
    }
  }

  void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
  {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
      throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = { { 0.21f, 0.68f, 0.8f, 1.0f } };
    clearValues[1].color = { { 0.21f, 0.68f, 0.8f, 1.0f } };
    clearValues[2].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(
      commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // -------------------- bind main pipeline --------------------
    vkCmdBindPipeline(
      commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainGraphicsPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            mainPipelineLayout,
                            0,
                            1,
                            &descriptorSetsAttachmentWrite[currentFrame],
                            0,
                            nullptr);

    for (auto& model : models) {
      model.draw(commandBuffer, mainPipelineLayout, true);
    }

    // -------------------- bind light_cube pipeline --------------------
    vkCmdBindPipeline(
      commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightCubePipeline);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    for (auto& lightModel : lightModels) {
      lightModel.draw(commandBuffer, lightCubePipelineLayout, false);
    }

    // -------------------- RENDER SKYBOX --------------------

    vkCmdBindPipeline(
      commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);

    skybox->draw(
      commandBuffer, skyboxPipelineLayout, camera->getCameraMatrix());

    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(
      commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postProcessingPipeline);

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            postProcessingPipelineLayout,
                            0,
                            1,
                            &descriptorSetsAttachmentRead,
                            0,
                            nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
      throw std::runtime_error("failed to record command buffer!");
    }
  }

  void createSyncObjects()
  {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      if (vkCreateSemaphore(logicalDevice,
                            &semaphoreInfo,
                            nullptr,
                            &imageAvailableSemaphores[i]) != VK_SUCCESS ||
          vkCreateSemaphore(logicalDevice,
                            &semaphoreInfo,
                            nullptr,
                            &renderFinishedSemaphores[i]) != VK_SUCCESS ||
          vkCreateFence(
            logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) !=
            VK_SUCCESS) {
        throw std::runtime_error(
          "failed to create synchronization objects for a frame!");
      }
    }
  }

  void updateUniformBuffer(uint32_t currentImage)
  {
    UniformBufferObject sUBO{};

    // position, target, up
    sUBO.view = camera->getCameraMatrix();
    sUBO.proj = glm::perspective(
      glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
    sUBO.cameraPos = glm::vec4(camera->getCameraPos().x,
                               camera->getCameraPos().y,
                               camera->getCameraPos().z,
                               0.0);

    // make the Rare logo spin. i know, this should get its own index but i'm
    // running low on time
    models[models.size() - 1].rotate(1.0f, glm::vec3(0.0, 0.0, 1.0));
    // glm::mat4 rareModelMatrix = models[models.size() - 1].modelMatrix;
    // rareModelMatrix = glm::rotate(
    //   rareModelMatrix, glm::radians(1.0f), glm::vec3(0.0, 0.0, 1.0));
    // models[models.size() - 1].modelMatrix = rareModelMatrix;

    memcpy(uniformBuffersMapped[currentImage], &sUBO, sizeof(sUBO));
  }

  void drawFrame()
  {
    vkWaitForFences(
      logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result =
      vkAcquireNextImageKHR(logicalDevice,
                            swapChain,
                            UINT64_MAX,
                            imageAvailableSemaphores[currentFrame],
                            VK_NULL_HANDLE,
                            &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapChain();
      return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
    }

    updateUniformBuffer(currentFrame);

    vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(commandBuffers[currentFrame],
                         /*VkCommandBufferResetFlagBits*/ 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(
          graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        framebufferResized) {
      framebufferResized = false;
      recreateSwapChain();
    } else if (result != VK_SUCCESS) {
      throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  VkShaderModule createShaderModule(const std::vector<char>& code)
  {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(
          logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
      throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
  }

  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats)
  {
    for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
          availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return availableFormat;
      }
    }

    return availableFormats[0];
  }

  VkPresentModeKHR chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes)
  {
    for (const auto& availablePresentMode : availablePresentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return availablePresentMode;
      }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
  }

  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
  {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    } else {
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);

      VkExtent2D actualExtent = { static_cast<uint32_t>(width),
                                  static_cast<uint32_t>(height) };

      actualExtent.width = std::clamp(actualExtent.width,
                                      capabilities.minImageExtent.width,
                                      capabilities.maxImageExtent.width);
      actualExtent.height = std::clamp(actualExtent.height,
                                       capabilities.minImageExtent.height,
                                       capabilities.maxImageExtent.height);

      return actualExtent;
    }
  }

  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
  {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
      device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
      details.formats.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(
        device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
      details.presentModes.resize(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
  }

  static std::vector<char> readFile(const std::string& filename)
  {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      throw std::runtime_error("failed to open file! " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
  }
};

int
main()
{
  DemoApplication app;

  try {
    app.run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
