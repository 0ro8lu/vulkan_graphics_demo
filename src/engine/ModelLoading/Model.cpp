#include "engine/ModelLoading/Model.h"
#include "engine/ModelLoading/Mesh.h"
#include "engine/Vertex.h"

#include <assimp/postprocess.h>
#include <iostream>

VkDescriptorSetLayout Model::textureLayout = VK_NULL_HANDLE;

Model::Model(const std::string& filePath,
             VulkanContext* vkContext,
             glm::vec3 pos,
             glm::vec3 rotationAxis,
             float rotationAngle,
             glm::vec3 scale)
  : vkContext(vkContext)
{
  modelMatrix = glm::translate(modelMatrix, pos);
  if (rotationAngle != 0) {
    modelMatrix =
      glm::rotate(modelMatrix, glm::radians(rotationAngle), rotationAxis);
  }
  modelMatrix = glm::scale(modelMatrix, scale);

  loadModel(filePath);

  createVertexBuffer(sizeof(vertices[0]) * vertices.size());
  createIndexBuffer(sizeof(indices[0]) * indices.size());

  setupDescriptors();

  // clean the vectors by destroying their contents and releasing the memory of
  // the vector itself
  vertices.clear();
  std::vector<Vertex> tmpVertices = std::vector<Vertex>();
  vertices.swap(tmpVertices);

  indices.clear();
  std::vector<uint32_t> tmpIndices = std::vector<uint32_t>();
  indices.swap(tmpIndices);
}

Model::~Model()
{
  cleanup();
}

Model::Model(Model&& other) noexcept
{
  modelMatrix = other.modelMatrix;
  meshInstances = std::move(other.meshInstances);
  uniqueMeshes = std::move(other.uniqueMeshes);

  vertices = std::move(other.vertices);
  vertexBuffer = other.vertexBuffer;
  vertexBufferAllocation = other.vertexBufferAllocation;
  vertexBufferMemory = other.vertexBufferMemory;

  indices = std::move(other.indices);
  indexBuffer = other.indexBuffer;
  indexBufferAllocation = other.indexBufferAllocation;
  indexBufferMemory = other.indexBufferMemory;

  vkContext = other.vkContext;
  descriptorPool = other.descriptorPool;

  other.vertexBuffer = VK_NULL_HANDLE;
  other.vertexBufferAllocation = VK_NULL_HANDLE;
  other.vertexBufferMemory = VK_NULL_HANDLE;

  other.indexBuffer = VK_NULL_HANDLE;
  other.indexBufferAllocation = VK_NULL_HANDLE;
  other.indexBufferMemory = VK_NULL_HANDLE;

  other.vkContext = nullptr;
}

Model&
Model::operator=(Model&& other) noexcept
{
  if (this != &other) {
    cleanup();

    modelMatrix = other.modelMatrix;
    meshInstances = std::move(other.meshInstances);
    uniqueMeshes = std::move(other.uniqueMeshes);

    vertices = std::move(other.vertices);
    vertexBuffer = other.vertexBuffer;
    vertexBufferAllocation = other.vertexBufferAllocation;
    vertexBufferMemory = other.vertexBufferMemory;

    indices = std::move(other.indices);
    indexBuffer = other.indexBuffer;
    indexBufferAllocation = other.indexBufferAllocation;
    indexBufferMemory = other.indexBufferMemory;

    vkContext = other.vkContext;
  }
  return *this;
}

void
Model::applyTransform(const glm::mat4& transform)
{
  for (auto& instance : meshInstances) {
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

  meshInstances.reserve(scene->mNumMeshes);
  indices.reserve(totalNumIndices);
  vertices.reserve(totalNumVertices);

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
    modelMatrix * AssimpToGlmMatrix(node->mTransformation);

  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    aiMesh* assimpMesh = scene->mMeshes[node->mMeshes[i]];
    Mesh* mesh;

    auto it = uniqueMeshes.find(assimpMesh);
    if (it == uniqueMeshes.end()) {
      auto newMesh = processMesh(assimpMesh, scene, startIndex, startVertex);
      mesh = newMesh.get();
      uniqueMeshes[assimpMesh] = std::move(newMesh);

      startIndex += scene->mMeshes[node->mMeshes[i]]->mNumFaces * 3;
      startVertex += assimpMesh->mNumVertices;
    } else {
      mesh = it->second.get();
    }

    meshInstances.push_back({ nodeTransform, mesh });
  }

  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    processNode(node->mChildren[i], scene, startIndex, startVertex);
  }
}

glm::mat4
Model::AssimpToGlmMatrix(const aiMatrix4x4& from)
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

    vertices.push_back(vertex);
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
  if (diffuseTexture.view == VK_NULL_HANDLE) {
    diffuseTexture = Texture(vkContext, path + "empty_diffuse.png");
  }
  if (specularTexture.view == VK_NULL_HANDLE) {
    specularTexture = Texture(vkContext, path + "empty_specular.png");
  }

  auto newMesh = std::make_unique<Mesh>(vkContext,
                                        mesh->mNumFaces * 3,
                                        startIndex,
                                        std::move(diffuseTexture),
                                        std::move(specularTexture));

  for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
    const aiFace& face = mesh->mFaces[i];
    indices.push_back(face.mIndices[0] + startVertex);
    indices.push_back(face.mIndices[1] + startVertex);
    indices.push_back(face.mIndices[2] + startVertex);
  }

  return newMesh;
}

void
Model::loadTexture(aiMaterial* material,
                   Texture& texture,
                   const aiScene* scene,
                   aiTextureType type)
{
  if (material->GetTextureCount(type) > 0) {
    aiString str;
    material->GetTexture(type, 0, &str);

    if (str.C_Str()[0] == '*') {
      // embedded texture
      int texIndex = atoi(&str.C_Str()[1]);

      aiTexture* aiTexture = scene->mTextures[texIndex];

      // mHeight == 0 means the texture is compressed
      if (aiTexture->mHeight == 0) {
        // Compressed texture data
        texture = Texture(vkContext,
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
      texture = Texture(vkContext, str.C_Str());
    }
  }
}

void
Model::setupDescriptors()
{
  // -------------------- DESCRIPTOR POOL --------------------
  VkDescriptorPoolSize poolSize;

  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = 2 * meshInstances.size();

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = meshInstances.size();

  if (vkCreateDescriptorPool(
        vkContext->logicalDevice, &poolInfo, nullptr, &descriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }

  // -------------------- DESCRIPTOR LAYOUT --------------------
  if (Model::textureLayout == VK_NULL_HANDLE) {
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

    if (vkCreateDescriptorSetLayout(vkContext->logicalDevice,
                                    &layoutInfo,
                                    nullptr,
                                    &Model::textureLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  // -------------------- DESCRIPTOR ALLOCATION --------------------
  for (const auto& uniqueMesh : uniqueMeshes) {
    uniqueMesh.second->createDescriptorSet(descriptorPool,
                                           Model::textureLayout);
  }
}

void
Model::createVertexBuffer(VkDeviceSize bufferSize)
{
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  VmaAllocation stagingBufferAllocation;
  void* data = vkContext->createBuffer(bufferSize,
                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       BufferType::STAGING_BUFFER,
                                       stagingBuffer,
                                       stagingBufferAllocation);

  memcpy(data, vertices.data(), (size_t)bufferSize);

  vkContext->createBuffer(bufferSize,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          BufferType::GPU_BUFFER,
                          vertexBuffer,
                          vertexBufferAllocation);

  vkContext->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

  vmaDestroyBuffer(
    vkContext->allocator, stagingBuffer, stagingBufferAllocation);
}

void
Model::createIndexBuffer(VkDeviceSize bufferSize)
{
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  VmaAllocation stagingBufferAllocation;
  void* data = vkContext->createBuffer(bufferSize,
                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       BufferType::STAGING_BUFFER,
                                       stagingBuffer,
                                       stagingBufferAllocation);

  memcpy(data, indices.data(), (size_t)bufferSize);

  vkContext->createBuffer(bufferSize,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                          BufferType::GPU_BUFFER,
                          indexBuffer,
                          indexBufferAllocation);

  vkContext->copyBuffer(stagingBuffer, indexBuffer, bufferSize);

  vmaDestroyBuffer(
    vkContext->allocator, stagingBuffer, stagingBufferAllocation);
}

void
Model::cleanup()
{
  if (vertexBuffer != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE &&
      descriptorPool != VK_NULL_HANDLE) {
    vmaDestroyBuffer(
      vkContext->allocator, vertexBuffer, vertexBufferAllocation);
    vmaDestroyBuffer(vkContext->allocator, indexBuffer, indexBufferAllocation);

    vkDestroyDescriptorPool(vkContext->logicalDevice, descriptorPool, nullptr);
  }
}
