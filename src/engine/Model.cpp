#include "Model.h"
#include "engine/Texture.h"
#include "engine/Vertex.h"
#include "engine/VulkanDevice.h"
#include <assimp/material.h>
#include <utility>
#include <vulkan/vulkan_core.h>

VkDescriptorSetLayout Model::textureLayout = VK_NULL_HANDLE;

Model::Model(const std::string& filePath,
		VulkanDevice* vulkanDevice,
		glm::vec3 pos,
        glm::vec3 rotationAxis,
		float rotationAngle,
		glm::vec3 scale) :
vulkanDevice(vulkanDevice), filePath(filePath){
	
	loadModel(filePath);
    
    std::cout << indices.size();

	createVertexBuffer(sizeof(vertices[0]) * vertices.size());
	createIndexBuffer(sizeof(indices[0]) * indices.size());

	modelMatrix = glm::translate(modelMatrix, pos);
	modelMatrix = glm::rotate(modelMatrix, glm::radians(rotationAngle), rotationAxis);
    modelMatrix = glm::scale(modelMatrix, scale);

	// create the descriptor layout and descriptor sets
	setupDescriptors();
}

Model::~Model() {
	cleanup();
}

Model::Model(Model&& other) noexcept {
	modelMatrix = other.modelMatrix;
	meshes = std::move(other.meshes);

	vertices = std::move(other.vertices);
	vertexBuffer = other.vertexBuffer;
	vertexBufferAllocation = other.vertexBufferAllocation;
	vertexBufferMemory = other.vertexBufferMemory;

	indices = std::move(other.indices);
	indexBuffer = other.indexBuffer;
	indexBufferAllocation = other.indexBufferAllocation;
	indexBufferMemory = other.indexBufferMemory;

	vulkanDevice = other.vulkanDevice;
	descriptorPool = other.descriptorPool;
    
    other.vertexBuffer = VK_NULL_HANDLE;
    other.vertexBufferAllocation = VK_NULL_HANDLE;
    other.vertexBufferMemory = VK_NULL_HANDLE;
    
    other.indexBuffer = VK_NULL_HANDLE;
    other.indexBufferAllocation = VK_NULL_HANDLE;
    other.indexBufferMemory = VK_NULL_HANDLE;
    
    other.vulkanDevice = nullptr;
    other.descriptorPool = VK_NULL_HANDLE;
}

Model& Model::operator=(Model&& other) noexcept {
	if(this != &other) {
		cleanup();

		modelMatrix = other.modelMatrix;
		meshes = std::move(other.meshes);

		vertices = std::move(other.vertices);
		vertexBuffer = other.vertexBuffer;
		vertexBufferAllocation = other.vertexBufferAllocation;
		vertexBufferMemory = other.vertexBufferMemory;

		indices = std::move(other.indices);
		indexBuffer = other.indexBuffer;
		indexBufferAllocation = other.indexBufferAllocation;
		indexBufferMemory = other.indexBufferMemory;

		vulkanDevice = other.vulkanDevice;
		descriptorPool = other.descriptorPool;
	}
	return *this;
}

void Model::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, bool shouldRenderTexture) {
    
    struct PushConstant{
        glm::mat4 model;
    };

    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    PushConstant pc;
    pc.model = modelMatrix;
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &pc);
    
	for(Mesh& mesh : meshes) {
		mesh.draw(commandBuffer, pipelineLayout, shouldRenderTexture);
	}
}

void Model::loadModel(const std::string& filePath) {
	Assimp::Importer importer;
	unsigned int processFlags =
    aiProcess_FlipUVs |
	// aiProcess_CalcTangentSpace         | // calculate tangents and bitangents if possible
	// aiProcess_JoinIdenticalVertices    | // join identical vertices/ optimize indexing
	//aiProcess_ValidateDataStructure  | // perform a full validation of the loader's output
	aiProcess_Triangulate              | // Ensure all verticies are triangulated (each 3 vertices are triangle)
	// aiProcess_ConvertToLeftHanded      | // convert everything to D3D left handed space (by default right-handed, for OpenGL)
	// aiProcess_SortByPType              | // ?
	// aiProcess_ImproveCacheLocality     | // improve the cache locality of the output vertices
	// aiProcess_RemoveRedundantMaterials | // remove redundant materials
	// aiProcess_FindDegenerates          | // remove degenerated polygons from the import
	// aiProcess_FindInvalidData          | // detect invalid model data, such as invalid normal vectors
	 aiProcess_GenUVCoords              | // convert spherical, cylindrical, box and planar mapping to proper UVs
	// aiProcess_TransformUVCoords        | // preprocess UV transformations (scaling, translation ...)
	// aiProcess_FindInstances            | // search for instanced meshes and remove them by references to one master
	// aiProcess_LimitBoneWeights         | // limit bone weights to 4 per vertex
	// aiProcess_OptimizeMeshes           | // join small meshes, if possible;
	// aiProcess_SplitByBoneCount         | // split meshes with too many bones. Necessary for our (limited) hardware skinning shader
	0;

    const aiScene *scene = importer.ReadFile(filePath, processFlags);
    
	if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }

	//preallocate vertices and indices vectors
	unsigned int totalNumVertices = 0;
	unsigned int totalNumIndices = 0;
	for(unsigned int i = 0; i < scene->mNumMeshes; i++) {
        totalNumVertices += scene->mMeshes[i]->mNumVertices;
        totalNumIndices += scene->mMeshes[i]->mNumFaces * 3; // <== i assume every face has three vertices because of the TRIANGULATE option
	}

	// meshes.reserve(scene->mNumMeshes);
	// indices.reserve(totalNumIndices);
	// vertices.reserve(totalNumVertices);

	size_t startIndex = 0;
    size_t startVertex = 0;
	for(unsigned int i = 0; i < scene->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[i];
		processMesh(mesh, scene, startIndex, startVertex);

		startIndex += scene->mMeshes[i]->mNumFaces * 3;
        startVertex += mesh->mNumVertices;
	}
}

void Model::processMesh(aiMesh* mesh, const aiScene* scene, size_t startIndex, size_t startVertex) {
	for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
		Vertex vertex;

		// Position
        vertex.position.x = mesh->mVertices[i].x;
        vertex.position.y = -mesh->mVertices[i].y;
        vertex.position.z = mesh->mVertices[i].z;

		// Normal
        if(mesh->HasNormals()) {
            vertex.normals.x = mesh->mNormals[i].x;
            vertex.normals.y = -mesh->mNormals[i].y;
            vertex.normals.z = mesh->mNormals[i].z;
        }

		// Texture coordinates
		if(mesh->HasTextureCoords(0)) {
        // if(mesh->mTextureCoords[0]) {
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
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

        loadTexture(material, diffuseTexture, scene, aiTextureType_DIFFUSE);
        loadTexture(material, specularTexture, scene, aiTextureType_SPECULAR);
    } else {
        std::string path = TEXTURE_PATH;
        specularTexture = Texture(vulkanDevice, path + "specular.png");
        diffuseTexture = Texture(vulkanDevice, path + "diffuse.png");
    }
    
    Mesh myMesh = Mesh(vulkanDevice, mesh->mNumFaces * 3, startIndex, std::move(diffuseTexture), std::move(specularTexture));
    meshes.push_back(std::move(myMesh));

    for(unsigned int i = 0; i < mesh->mNumFaces; i++) {
		const aiFace& face = mesh->mFaces[i];
		indices.push_back(face.mIndices[0] + startVertex);
		indices.push_back(face.mIndices[1] + startVertex);
		indices.push_back(face.mIndices[2] + startVertex);
	}
}

void Model::loadTexture(aiMaterial* material, Texture& texture, const aiScene* scene, aiTextureType type) {
    if(material->GetTextureCount(type) > 0) {
    	aiString str;
    	material->GetTexture(type, 0, &str);

		if(str.C_Str()[0] == '*') {
        	// It's an embedded texture
        	int texIndex = atoi(&str.C_Str()[1]); // Extract index after '*'

        	aiTexture* aiTexture = scene->mTextures[texIndex];

        	// Now, texture->pcData contains the texture data
        	// If mHeight == 0, the texture is compressed (e.g., PNG, JPG)
        	if(aiTexture->mHeight == 0) {
        	    // Compressed texture data
            	texture = Texture(vulkanDevice, reinterpret_cast<unsigned char*>(aiTexture->pcData), aiTexture->mWidth);
        	} else {
				// Uncompressed texture data (rare, but handle if needed)
            	// size_t size = aiTexture->mWidth * aiTexture->mHeight * 4; // Assuming RGBA
            	// texture = Texture(vulkanDevice, reinterpret_cast<unsigned char*>(aiTexture->pcData), size, aiTexture->mWidth, aiTexture->mHeight);
        	}
		} else {
			// Load texture from file path
    		texture = Texture(vulkanDevice, str.C_Str());
		}
	} else {
		// Load default texture
    	std::string path = TEXTURE_PATH;
    	texture = Texture(vulkanDevice, path + "diffuse.png");
	}
}

void Model::setupDescriptors() {

	// -------------------- DESCRIPTOR POOL --------------------
	VkDescriptorPoolSize poolSize;

    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 2 * meshes.size(); //TODO: change this to 2 * meshes.size(); as all meshes have specular and diffuse texture

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = meshes.size(); //TODO: change this to meshes.size() as all meshes will have a descriptor set from which to render from.

    if (vkCreateDescriptorPool(vulkanDevice->logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

	// -------------------- DESCRIPTOR LAYOUT --------------------
	VkDescriptorSetLayoutBinding diffuseLayoutBinding{};
    diffuseLayoutBinding.binding = 0;
    diffuseLayoutBinding.descriptorCount = 1;
    diffuseLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    diffuseLayoutBinding.pImmutableSamplers = nullptr;
    diffuseLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding specularLayoutBinding{};
    specularLayoutBinding.binding = 1;
    specularLayoutBinding.descriptorCount = 1;
    specularLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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

    if (vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &layoutInfo, nullptr, &Model::textureLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

	// -------------------- DESCRIPTOR ALLOCATION --------------------
	for (Mesh& mesh : meshes) {
		mesh.createDescriptorSet(descriptorPool, Model::textureLayout);
	}
}

void
Model::createVertexBuffer(VkDeviceSize bufferSize)
{
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  VmaAllocation stagingBufferAllocation;
  void* data = vulkanDevice->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VulkanDevice::BufferType::STAGING_BUFFER, stagingBuffer, stagingBufferAllocation);

  memcpy(data, vertices.data(), (size_t)bufferSize);

  vulkanDevice->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VulkanDevice::BufferType::GPU_BUFFER, vertexBuffer, vertexBufferAllocation);

  vulkanDevice->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

  vmaDestroyBuffer(vulkanDevice->allocator, stagingBuffer, stagingBufferAllocation);
}

void Model::createIndexBuffer(VkDeviceSize bufferSize) {
    
//    std::cout << indices.size() << std::endl;
//    for(int i = 0; i < indices.size(); i++) {
//        std::cout << indices[i] << std::endl;
//    }
    
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VmaAllocation stagingBufferAllocation;
	void* data = vulkanDevice->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VulkanDevice::BufferType::STAGING_BUFFER, stagingBuffer, stagingBufferAllocation);

	memcpy(data, indices.data(), (size_t) bufferSize);

	vulkanDevice->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VulkanDevice::BufferType::GPU_BUFFER, indexBuffer, indexBufferAllocation);

	vulkanDevice->copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	vmaDestroyBuffer(vulkanDevice->allocator, stagingBuffer, stagingBufferAllocation);
}

void Model::cleanup() {
	if(vertexBuffer != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE) {
		vmaDestroyBuffer(vulkanDevice->allocator, vertexBuffer, vertexBufferAllocation);
		vmaDestroyBuffer(vulkanDevice->allocator, indexBuffer, indexBufferAllocation);

		vkDestroyDescriptorPool(vulkanDevice->logicalDevice, descriptorPool, nullptr);
	}
}
