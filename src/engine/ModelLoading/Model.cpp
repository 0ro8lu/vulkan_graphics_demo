#include "engine/GpuAllocator.h"
#include "engine/GpuDevice.h"
#include "engine/ImmediateCommands.h"
#include "engine/ModelLoading/Model.h"
#include "engine/SamplerCache.h"
#include "engine/VulkanCheck.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

static glm::mat4
AssimpToGlmMatrix(const aiMatrix4x4& from);

VkDescriptorSetLayout Model::s_textureLayout = VK_NULL_HANDLE;

Model::Model(const std::string& filePath,
             const GpuDevice& device,
             const GpuAllocator& allocator,
             const SamplerCache& samplerCache,
             const ImmediateCommands& immediateCommands,
             glm::vec3 pos,
             glm::vec3 rotationAxis,
             float rotationAngle,
             glm::vec3 scale)
  : m_device(&device)
  , m_allocator(&allocator)
  , m_samplerCache(&samplerCache)
  , m_immediateCommands(&immediateCommands)
{
  m_modelMatrix = glm::translate(m_modelMatrix, pos);
  if (rotationAngle != 0) {
    m_modelMatrix =
      glm::rotate(m_modelMatrix, glm::radians(rotationAngle), rotationAxis);
  }
  m_modelMatrix = glm::scale(m_modelMatrix, scale);

  loadModel(filePath);

  createVertexBuffer(sizeof(m_vertices[0]) * m_vertices.size());
  createIndexBuffer(sizeof(m_indices[0]) * m_indices.size());

  setupDescriptors();

  // Release vertex/index data now that it's uploaded to GPU
  std::vector<Vertex>().swap(m_vertices);
  std::vector<uint32_t>().swap(m_indices);
}

Model::~Model()
{
  cleanup();
}

Model::Model(Model&& other) noexcept
{
  m_modelMatrix = other.m_modelMatrix;
  m_meshInstances = std::move(other.m_meshInstances);
  m_uniqueMeshes = std::move(other.m_uniqueMeshes);

  m_vertices = std::move(other.m_vertices);
  m_vertexBuffer = std::exchange(other.m_vertexBuffer, {});

  m_indices = std::move(other.m_indices);
  m_indexBuffer = std::exchange(other.m_indexBuffer, {});

  m_device = std::exchange(other.m_device, nullptr);
  m_allocator = std::exchange(other.m_allocator, nullptr);
  m_samplerCache = std::exchange(other.m_samplerCache, nullptr);
  m_immediateCommands = std::exchange(other.m_immediateCommands, nullptr);
  m_descriptorPool = std::exchange(other.m_descriptorPool, VK_NULL_HANDLE);
}

Model&
Model::operator=(Model&& other) noexcept
{
  if (this != &other) {
    cleanup();

    m_modelMatrix = other.m_modelMatrix;
    m_meshInstances = std::move(other.m_meshInstances);
    m_uniqueMeshes = std::move(other.m_uniqueMeshes);

    m_vertices = std::move(other.m_vertices);
    m_vertexBuffer = std::exchange(other.m_vertexBuffer, {});

    m_indices = std::move(other.m_indices);
    m_indexBuffer = std::exchange(other.m_indexBuffer, {});

    m_device = std::exchange(other.m_device, nullptr);
    m_allocator = std::exchange(other.m_allocator, nullptr);
    m_samplerCache = std::exchange(other.m_samplerCache, nullptr);
    m_immediateCommands = std::exchange(other.m_immediateCommands, nullptr);
    m_descriptorPool = std::exchange(other.m_descriptorPool, VK_NULL_HANDLE);
  }
  return *this;
}

void
Model::applyTransform(const glm::mat4& transform)
{
  for (auto& instance : m_meshInstances) {
    instance.transformation = instance.transformation * transform;
  }
}

void
Model::rotate(float angle, glm::vec3 rotationAxis)
{
  glm::mat4 newMat =
    glm::rotate(glm::mat4(1.0), glm::radians(angle), rotationAxis);
  applyTransform(newMat);
}

void
Model::loadModel(const std::string& filePath)
{
  Assimp::Importer importer;
  unsigned int processFlags =
    aiProcess_FlipUVs |
    aiProcess_Triangulate | // Ensure all verticies are triangulated (each 3
                            // vertices are triangle)
    aiProcess_GenUVCoords;  // convert spherical, cylindrical, box and planar
                            // mapping to proper UVs

  const aiScene* scene = importer.ReadFile(filePath, processFlags);

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
      !scene->mRootNode) {
    std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
    return;
  }

  // preallocate vertices and indices vectors
  unsigned int totalNumVertices = 0;
  unsigned int totalNumIndices = 0;
  for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
    totalNumVertices += scene->mMeshes[i]->mNumVertices;
    totalNumIndices += scene->mMeshes[i]->mNumFaces *
                       3; // <== i assume every face has three vertices because
                          // of the TRIANGULATE option
  }

  m_meshInstances.reserve(scene->mNumMeshes);
  m_indices.reserve(totalNumIndices);
  m_vertices.reserve(totalNumVertices);

  size_t startIndex = 0;
  size_t startVertex = 0;

  processNode(scene->mRootNode, scene, startIndex, startVertex);
}

void
Model::processNode(aiNode* node,
                   const aiScene* scene,
                   size_t& startIndex,
                   size_t& startVertex)
{
  glm::mat4 nodeTransform =
    m_modelMatrix * AssimpToGlmMatrix(node->mTransformation);

  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    aiMesh* assimpMesh = scene->mMeshes[node->mMeshes[i]];
    Mesh* mesh;

    auto it = m_uniqueMeshes.find(assimpMesh);
    if (it == m_uniqueMeshes.end()) {
      auto newMesh = processMesh(assimpMesh, scene, startIndex, startVertex);
      mesh = newMesh.get();
      m_uniqueMeshes[assimpMesh] = std::move(newMesh);

      startIndex += scene->mMeshes[node->mMeshes[i]]->mNumFaces * 3;
      startVertex += assimpMesh->mNumVertices;
    } else {
      mesh = it->second.get();
    }

    m_meshInstances.push_back({ nodeTransform, mesh });
  }

  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    processNode(node->mChildren[i], scene, startIndex, startVertex);
  }
}

static glm::mat4
AssimpToGlmMatrix(const aiMatrix4x4& from)
{
  glm::mat4 to;
  to[0][0] = from.a1;
  to[1][0] = from.a2;
  to[2][0] = from.a3;
  to[3][0] = from.a4;
  to[0][1] = from.b1;
  to[1][1] = from.b2;
  to[2][1] = from.b3;
  to[3][1] = from.b4;
  to[0][2] = from.c1;
  to[1][2] = from.c2;
  to[2][2] = from.c3;
  to[3][2] = from.c4;
  to[0][3] = from.d1;
  to[1][3] = from.d2;
  to[2][3] = from.d3;
  to[3][3] = from.d4;
  return to;
}

std::unique_ptr<Mesh>
Model::processMesh(aiMesh* mesh,
                   const aiScene* scene,
                   size_t& startIndex,
                   size_t& startVertex)
{
  for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
    Vertex vertex;

    // Position
    glm::vec4 position = glm::vec4(
      mesh->mVertices[i].x, -mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
    vertex.position = glm::vec3(position);

    // Normal
    if (mesh->HasNormals()) {
      glm::vec4 normal = glm::vec4(
        mesh->mNormals[i].x, -mesh->mNormals[i].y, mesh->mNormals[i].z, 0.0f);
      vertex.normals = glm::normalize(glm::vec3(normal));
    }

    // Texture coordinates
    if (mesh->HasTextureCoords(0)) {
      vertex.texCoords.x = mesh->mTextureCoords[0][i].x;
      vertex.texCoords.y = mesh->mTextureCoords[0][i].y;
    } else {
      vertex.texCoords = glm::vec2(0.0f, 0.0f);
    }

    m_vertices.push_back(vertex);
  }

  // If we do have textures
  Texture diffuseTexture;
  Texture specularTexture;
  if (mesh->mMaterialIndex >= 0) {
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

    loadTexture(material, diffuseTexture, scene, aiTextureType_DIFFUSE);
    loadTexture(material, specularTexture, scene, aiTextureType_SPECULAR);
  }

  std::string path = TEXTURE_PATH;
  if (diffuseTexture.m_view == VK_NULL_HANDLE) {
    diffuseTexture = Texture(*m_device,
                             *m_allocator,
                             *m_samplerCache,
                             *m_immediateCommands,
                             path + "empty_diffuse.png");
  }
  if (specularTexture.m_view == VK_NULL_HANDLE) {
    specularTexture = Texture(*m_device,
                              *m_allocator,
                              *m_samplerCache,
                              *m_immediateCommands,
                              path + "empty_specular.png");
  }

  auto newMesh = std::make_unique<Mesh>(*m_device,
                                        mesh->mNumFaces * 3,
                                        startIndex,
                                        std::move(diffuseTexture),
                                        std::move(specularTexture));

  for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
    const aiFace& face = mesh->mFaces[i];
    m_indices.push_back(face.mIndices[0] + startVertex);
    m_indices.push_back(face.mIndices[1] + startVertex);
    m_indices.push_back(face.mIndices[2] + startVertex);
  }

  return newMesh;
}

void
Model::loadTexture(aiMaterial* material,
                   Texture& texture,
                   const aiScene* scene,
                   int type)
{
  aiTextureType aiType = static_cast<aiTextureType>(type);
  if (material->GetTextureCount(aiType) > 0) {
    aiString str;
    material->GetTexture(aiType, 0, &str);

    if (str.C_Str()[0] == '*') {
      // embedded texture
      int texIndex = atoi(&str.C_Str()[1]);

      aiTexture* aiTexture = scene->mTextures[texIndex];

      // mHeight == 0 means the texture is compressed
      if (aiTexture->mHeight == 0) {
        // Compressed texture data
        texture = Texture(*m_device,
                          *m_allocator,
                          *m_samplerCache,
                          *m_immediateCommands,
                          reinterpret_cast<unsigned char*>(aiTexture->pcData),
                          aiTexture->mWidth);
      } else {
        // Uncompressed texture data
        // size_t size = aiTexture->mWidth * aiTexture->mHeight * 4; // Assuming
        // RGBA texture = Texture(vulkanDevice, reinterpret_cast<unsigned
        // char*>(aiTexture->pcData), size, aiTexture->mWidth,
        // aiTexture->mHeight);
      }
    } else {
      // Load texture from file path
      texture = Texture(*m_device,
                        *m_allocator,
                        *m_samplerCache,
                        *m_immediateCommands,
                        str.C_Str());
    }
  }
}

void
Model::setupDescriptors()
{
  // -------------------- DESCRIPTOR POOL --------------------
  VkDescriptorPoolSize poolSize;

  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = 2 * m_meshInstances.size();

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = m_meshInstances.size();

  VK_CHECK(vkCreateDescriptorPool(
    m_device->device(), &poolInfo, nullptr, &m_descriptorPool));

  // -------------------- DESCRIPTOR LAYOUT --------------------
  if (Model::s_textureLayout == VK_NULL_HANDLE) {
    VkDescriptorSetLayoutBinding diffuseLayoutBinding{};
    diffuseLayoutBinding.binding = 0;
    diffuseLayoutBinding.descriptorCount = 1;
    diffuseLayoutBinding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    diffuseLayoutBinding.pImmutableSamplers = nullptr;
    diffuseLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding specularLayoutBinding{};
    specularLayoutBinding.binding = 1;
    specularLayoutBinding.descriptorCount = 1;
    specularLayoutBinding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    specularLayoutBinding.pImmutableSamplers = nullptr;
    specularLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
      diffuseLayoutBinding,
      specularLayoutBinding,
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(
      m_device->device(), &layoutInfo, nullptr, &Model::s_textureLayout));
  }

  // -------------------- DESCRIPTOR ALLOCATION --------------------
  for (const auto& uniqueMesh : m_uniqueMeshes) {
    uniqueMesh.second->createDescriptorSet(m_descriptorPool,
                                           Model::s_textureLayout);
  }
}

void
Model::createVertexBuffer(VkDeviceSize bufferSize)
{
  VulkanBuffer stagingBuffer = m_allocator->createBuffer(
    bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferType::STAGING_BUFFER);

  memcpy(
    stagingBuffer.mapped, m_vertices.data(), static_cast<size_t>(bufferSize));

  m_vertexBuffer = m_allocator->createBuffer(
    bufferSize,
    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    BufferType::GPU_BUFFER);

  m_immediateCommands->copyBuffer(
    stagingBuffer.buffer, m_vertexBuffer.buffer, bufferSize);
  m_allocator->destroyBuffer(stagingBuffer);
}

void
Model::createIndexBuffer(VkDeviceSize bufferSize)
{
  VulkanBuffer stagingBuffer = m_allocator->createBuffer(
    bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferType::STAGING_BUFFER);

  memcpy(
    stagingBuffer.mapped, m_indices.data(), static_cast<size_t>(bufferSize));

  m_indexBuffer = m_allocator->createBuffer(bufferSize,
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                            BufferType::GPU_BUFFER);

  m_immediateCommands->copyBuffer(
    stagingBuffer.buffer, m_indexBuffer.buffer, bufferSize);
  m_allocator->destroyBuffer(stagingBuffer);
}

void
Model::cleanup()
{
  if (m_allocator) {
    m_allocator->destroyBuffer(m_vertexBuffer);
    m_allocator->destroyBuffer(m_indexBuffer);
  }

  if (m_device && m_descriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(m_device->device(), m_descriptorPool, nullptr);
    m_descriptorPool = VK_NULL_HANDLE;
  }
}
