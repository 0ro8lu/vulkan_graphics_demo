#ifndef _MODEL_H_
#define _MODEL_H_

#include "Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

struct MeshInstance
{
  glm::mat4 transformation;
  Mesh* mesh;
};

class Model
{
public:
  Model(const std::string& filePath,
        VulkanDevice* vulkanDevice,
        glm::vec3 pos = glm::vec3(0),
        glm::vec3 rotationAxis = glm::vec3(0),
        float rotationAngle = 0.0f,
        glm::vec3 scale = glm::vec3(1));

  // find a way to copy this efficiently
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  Model(Model&& other) noexcept;
  Model& operator=(Model&& other) noexcept;

  Model(VulkanDevice* vulkanDevice); // todo remove this later
  ~Model();

  const glm::mat4& getModelMatrix() const { return modelMatrix; }

  void rotate(float angle, glm::vec3 rotationAxis);
  void translate(glm::vec3 position);
  void scale(glm::vec3 scale);

  std::unordered_map<aiMesh*, std::unique_ptr<Mesh>> uniqueMeshes;
  std::vector<MeshInstance> meshInstances;

  void draw(VkCommandBuffer commandBuffer,
            VkPipelineLayout pipelineLayout,
            bool shouldRenderTexture = false);

  std::vector<Vertex> vertices;
  VkBuffer vertexBuffer = VK_NULL_HANDLE;

  std::vector<uint32_t> indices;
  VkBuffer indexBuffer = VK_NULL_HANDLE;

  static VkDescriptorSetLayout textureLayout;

private:
  VulkanDevice* vulkanDevice;

  std::string filePath;

  glm::mat4 modelMatrix = glm::mat4(1.0);
  inline void applyTransform(const glm::mat4& transform);

  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  void setupDescriptors();

  void loadModel(const std::string& filePath);
  void processNode(aiNode* node,
                   const aiScene* scene,
                   size_t& startIndex,
                   size_t& startVertex);
  glm::mat4 AssimpToGlmMatrix(const aiMatrix4x4& from);
  std::unique_ptr<Mesh> processMesh(aiMesh* mesh,
                                    const aiScene* scene,
                                    size_t& startIndex,
                                    size_t& startVertex);
  void loadTexture(aiMaterial* material,
                   Texture& texture,
                   const aiScene* scene,
                   aiTextureType type);
  void printMaterialTypes(const aiScene* scene);

  unsigned int vertexCount;

  VkDeviceMemory vertexBufferMemory;
  VmaAllocation vertexBufferAllocation;
  void createVertexBuffer(VkDeviceSize bufferSize);

  VkDeviceMemory indexBufferMemory;
  VmaAllocation indexBufferAllocation;
  void createIndexBuffer(VkDeviceSize bufferSize);

  void cleanup();
};

#endif
