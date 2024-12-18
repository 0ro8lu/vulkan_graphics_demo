#ifndef _MODEL_H_
#define _MODEL_H_

#include "Mesh.h"
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class Model {
public:
	// Model(VulkanDevice* vulkanDevice, const std::string& filePath);
	Model(const std::string& filePath,
		VulkanDevice* vulkanDevice,
		glm::vec3 pos = glm::vec3(0),
	    glm::vec3 rotationAxis = glm::vec3(0),
		float rotationAngle = 0.0f,
		glm::vec3 scale = glm::vec3(1));

	//find a way to copy this efficiently
	Model(const Model&) = delete;
  	Model& operator=(const Model&) = delete;

	Model(Model&& other) noexcept;
	Model& operator=(Model&& other) noexcept;

	Model(VulkanDevice* vulkanDevice); //todo remove this later
	~Model();

	glm::mat4 modelMatrix = glm::mat4(1.0);

	//TODO: add a copy constructor for being able to copy the same model multiple times by recycling both 
	// vertex and index buffers (so no extra allocations) and recycling the same textures.
	std::vector<Mesh> meshes;
	
	void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, bool shouldRenderTexture);

	std::vector<Vertex> vertices;
	VkBuffer vertexBuffer = VK_NULL_HANDLE;

	std::vector<uint32_t> indices;
	VkBuffer indexBuffer = VK_NULL_HANDLE;

	static VkDescriptorSetLayout textureLayout;

private:
	VulkanDevice* vulkanDevice;
    
    std::string filePath;
	
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	void setupDescriptors();

	void loadModel(const std::string& filePath);
	void processMesh(aiMesh* mesh, const aiScene* scene, size_t startIndex, size_t startVertex);
    void loadTexture(aiMaterial* material, Texture& texture, const aiScene* scene, aiTextureType type);

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
