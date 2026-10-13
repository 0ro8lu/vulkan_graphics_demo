#pragma once

#include "engine/ModelLoading/Mesh.h"
#include "engine/Vertex.h"
#include "engine/VulkanTypes.h"
#include <glm/mat4x4.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;

struct MeshInstance
{
  glm::mat4 transformation;
  Mesh* mesh;
};

class VulkanContext;

class Model final
{
public:
  Model(const std::string& filePath,
        VulkanContext* vulkanContext,
        glm::vec3 pos = glm::vec3(0),
        glm::vec3 rotationAxis = glm::vec3(0),
        float rotationAngle = 0.0f,
        glm::vec3 scale = glm::vec3(1));

  // find a way to copy this efficiently
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  Model(Model&& other) noexcept;
  Model& operator=(Model&& other) noexcept;

  ~Model();

  const glm::mat4& getModelMatrix() const { return m_modelMatrix; }

  void rotate(float angle, glm::vec3 rotationAxis);
  void translate(glm::vec3 position);
  void scale(glm::vec3 scale);

  std::unordered_map<aiMesh*, std::unique_ptr<Mesh>> m_uniqueMeshes;
  std::vector<MeshInstance> m_meshInstances;

  std::vector<Vertex> m_vertices;
  VulkanBuffer m_vertexBuffer{};

  std::vector<uint32_t> m_indices;
  VulkanBuffer m_indexBuffer{};

  static VkDescriptorSetLayout s_textureLayout;

private:
  VulkanContext* m_vkContext;

  glm::mat4 m_modelMatrix = glm::mat4(1.0);
  inline void applyTransform(const glm::mat4& transform);

  VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
  void setupDescriptors();

  void loadModel(const std::string& filePath);
  void processNode(aiNode* node,
                   const aiScene* scene,
                   size_t& startIndex,
                   size_t& startVertex);
  std::unique_ptr<Mesh> processMesh(aiMesh* mesh,
                                    const aiScene* scene,
                                    size_t& startIndex,
                                    size_t& startVertex);
  void loadTexture(aiMaterial* material,
                   Texture& texture,
                   const aiScene* scene,
                   int type);

  void createVertexBuffer(VkDeviceSize bufferSize);
  void createIndexBuffer(VkDeviceSize bufferSize);

  void cleanup();
};
