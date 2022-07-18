/*
 * Vulkan Playground
 *
 * Copyright (C) Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <thread>
#include <mutex>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"
#include "VulkanglTFModel.h"
#include "VulkanBuffer.hpp"
#include "VulkanHeightmap.hpp"

#include "Pipeline.hpp"
#include "PipelineLayout.hpp"
#include "DescriptorSet.hpp"
#include "DescriptorSetLayout.hpp"
#include "RenderPass.hpp"
#include "DescriptorPool.hpp"
#include "Image.hpp"
#include "ImageView.hpp"
#include "frustum.hpp"
#include "HeightMapSettings.h"

#define ENABLE_VALIDATION false
#define FB_DIM 1024
#define SHADOWMAP_DIM 4096
#define SHADOW_MAP_CASCADE_COUNT 4

vks::VulkanDevice *defaultDevice;
VkQueue defaultQueue;
VkQueue transferQueue;
vks::Frustum frustum;
const float chunkDim = 241.0f;
float waterPosition = 1.75f;

HeightMapSettings heightMapSettings;

struct InstanceData {
	glm::vec3 pos;
	glm::vec3 scale;
	glm::vec3 rotation;
};

class TerrainChunk {
public:
	vks::HeightMap* heightMap = nullptr;
	vks::Buffer instanceBuffer;
	glm::ivec2 position;
	glm::vec3 center;
	glm::vec3 min;
	glm::vec3 max;
	int size;
	bool hasValidMesh = false;
	bool visible = false;
	int treeInstanceCount;
	
	TerrainChunk(glm::ivec2 coords, int size) : size(size) {
		position = coords;
		center = glm::vec3(0.0f);
		center.x = (float)coords.x * (float)size;
		center.z = (float)coords.y * (float)size;
		min = glm::vec3(center) - glm::vec3((float)size / 2.0f);
		max = glm::vec3(center) + glm::vec3((float)size / 2.0f);
		heightMap = new vks::HeightMap(defaultDevice, transferQueue);
	};

	void update() {

	}

	void updateHeightMap() {
		assert(heightMap);
		if (heightMap->vertexBuffer.buffer != VK_NULL_HANDLE) {
			heightMap->vertexBuffer.destroy();
			heightMap->indexBuffer.destroy();
		}
		heightMap->generate(
			heightMapSettings.seed,
			heightMapSettings.noiseScale,
			heightMapSettings.octaves,
			heightMapSettings.persistence,
			heightMapSettings.lacunarity,
			// @todo: base on offset instead of changing it
			heightMapSettings.offset);
		glm::vec3 scale = glm::vec3(1.0f, -heightMapSettings.heightScale, 1.0f); // @todo
		heightMap->generateMesh(
			scale,
			vks::HeightMap::topologyTriangles,
			heightMapSettings.levelOfDetail
		);
	}

	float getHeight(int x, int y) {
		assert(heightMap);
		return heightMap->getHeight(x, y);
	}

	void updateTrees() {
		assert(heightMap);
		if (instanceBuffer.buffer != VK_NULL_HANDLE) {
			instanceBuffer.destroy();
		}

		float topLeftX = (float)(vks::HeightMap::chunkSize - 1) / -2.0f;
		float topLeftZ = (float)(vks::HeightMap::chunkSize - 1) / 2.0f;

		std::vector<InstanceData> instanceData;

		// Random distribution

		const int dim = 30; // 24 241
		const int maxTreeCount = heightMapSettings.treeDensity * heightMapSettings.treeDensity;
		std::random_device rndDevice;
		std::default_random_engine prng(rndDevice());
		std::uniform_real_distribution<float> distribution(0, (float)(vks::HeightMap::chunkSize - 1));
		std::uniform_real_distribution<float> scaleDist(heightMapSettings.minTreeSize, heightMapSettings.maxTreeSize);
		std::uniform_real_distribution<float> rotDist(0.0, 1.0);

		for (int i = 0; i < maxTreeCount; i++) {
			float xPos = distribution(prng);
			float yPos = distribution(prng);
			int terrainX = round(xPos + 0.5f);
			int terrainY = round(yPos + 0.5f);
			float h1 = getHeight(terrainX - 1, terrainY);
			float h2 = getHeight(terrainX + 1, terrainY);
			float h3 = getHeight(terrainX, terrainY - 1);
			float h4 = getHeight(terrainX, terrainY + 1);
			float h = (h1 + h2 + h3 + h4) / 4.0f;
			if ((h <= waterPosition) || (h > 15.0f)) {
				continue;
			}
			InstanceData inst{};
			inst.pos = glm::vec3((float)topLeftX + xPos, -h, (float)topLeftZ - yPos);
			inst.scale = glm::vec3(scaleDist(prng));
			inst.rotation = glm::vec3(M_PI * rotDist(prng) * 0.035f, M_PI * rotDist(prng), M_PI * rotDist(prng) * 0.035f);
			instanceData.push_back(inst);
		}
		// Even distribution
		/*

		std::vector<InstanceData> instanceData;
		const int dim = 24; // 241
		const int maxTreeCount = dim * dim; // @todo
		std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);
		for (int x = 0; x < dim; x++) {
			for (int y = 0; y < dim; y++) {
				const float f = 10.1f;
				float xPos = (float)x * f + 5.0f;
				float yPos = (float)y * f + 5.0f;
				int terrainX = round(xPos + 0.5f);
				int terrainY = round(yPos + 0.5f);
				float h1 = getHeight(terrainX - 1, terrainY);
				float h2 = getHeight(terrainX + 1, terrainY);
				float h3 = getHeight(terrainX, terrainY - 1);
				float h4 = getHeight(terrainX, terrainY + 1);
				float h = (h1 + h2 + h3 + h4) / 4.0f;
				if ((h <= waterPosition) || (h > 15.0f)) {
					continue;
				}
				InstanceData inst{};
				inst.pos = glm::vec3((float)topLeftX + xPos, -h, (float)topLeftZ - yPos);
				instanceData.push_back(inst);
			}
		}
		*/

		treeInstanceCount = static_cast<uint32_t>(instanceData.size());
		vks::Buffer stagingBuffer;
		VK_CHECK_RESULT(defaultDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, instanceData.size() * sizeof(InstanceData), instanceData.data()));
		VK_CHECK_RESULT(defaultDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &instanceBuffer, stagingBuffer.size));
		defaultDevice->copyBuffer(&stagingBuffer, &instanceBuffer, transferQueue);
		stagingBuffer.destroy();
	}

	void draw(CommandBuffer* cb) {
		if (hasValidMesh) {
			heightMap->draw(cb->handle);
		}
	}
};

class InfiniteTerrain {
public:
	float maxViewDst = 300.0f;
	glm::vec2 viewerPosition;
	int chunkSize;
	int chunksVisibleInViewDistance;

	std::vector<TerrainChunk*> terrainChunks{};
	std::vector<TerrainChunk*> terrainChunkgsUpdateList{};

	InfiniteTerrain() {
		chunkSize = heightMapSettings.mapChunkSize - 1;
		chunksVisibleInViewDistance = round(maxViewDst / chunkSize);
	}

	bool chunkPresent(glm::ivec2 coords) {
		for (auto &chunk : terrainChunks) {
			if (chunk->position.x == coords.x && chunk->position.y == coords.y) {
				return true;
			}
		}
		return false;
	}

	TerrainChunk* getChunk(glm::ivec2 coords) {
		for (auto& chunk : terrainChunks) {
			if (chunk->position.x == coords.x && chunk->position.y == coords.y) {
				return chunk;
			}
		}
		return nullptr;
	}

	int getVisibleChunkCount() {
		int count = 0;
		for (auto& chunk : terrainChunks) {
			if (chunk->visible) {
				count++;
			}
		}
		return count;
	}

	bool updateVisibleChunks(Camera &camera) {

		bool res = false;
		int currentChunkCoordX = (int)round(viewerPosition.x / (float)chunkSize);
		int currentChunkCoordY = (int)round(viewerPosition.y / (float)chunkSize);
		for (int yOffset = -chunksVisibleInViewDistance; yOffset <= chunksVisibleInViewDistance; yOffset++) {
			for (int xOffset = -chunksVisibleInViewDistance; xOffset <= chunksVisibleInViewDistance; xOffset++) {
				glm::ivec2 viewedChunkCoord = glm::ivec2(currentChunkCoordX + xOffset, currentChunkCoordY + yOffset);
				if (chunkPresent(viewedChunkCoord)) {
					TerrainChunk* chunk = getChunk(viewedChunkCoord);
					chunk->visible = true;
				} else {
					int l = heightMapSettings.levelOfDetail;
					TerrainChunk* newChunk = new TerrainChunk(viewedChunkCoord, chunkSize);
					terrainChunks.push_back(newChunk);
					terrainChunkgsUpdateList.push_back(newChunk);
					heightMapSettings.levelOfDetail = l;
					std::cout << "Added new terrain chunk at " << viewedChunkCoord.x << " / " << viewedChunkCoord.y << "\n";
					std::cout << "Center is " << newChunk->center.x << " / " << newChunk->center.y << "\n";
					res = true;
				}
			}
		}

		// Update visibility
		for (auto& chunk : terrainChunks) {
			chunk->visible = frustum.checkBox(chunk->center, chunk->min, chunk->max);
		}

		return res;
	}

	void updateChunks() {
		for (auto& terrainChunk : terrainChunks) {
			int l = heightMapSettings.levelOfDetail;
			//heightMapSettings.levelOfDetail = 6;
			heightMapSettings.offset.x = (float)terrainChunk->position.x * (float)(chunkSize);
			heightMapSettings.offset.y = (float)terrainChunk->position.y * (float)(chunkSize);
			terrainChunk->updateHeightMap();
			terrainChunk->updateTrees();
			terrainChunk->hasValidMesh = true;
			heightMapSettings.levelOfDetail = l;
		}
	}

	void clear() {
		vkQueueWaitIdle(defaultQueue);
		vkQueueWaitIdle(transferQueue);
		for (auto& chunk : terrainChunks) {
			if (chunk->hasValidMesh) {
				chunk->heightMap->vertexBuffer.destroy();
				chunk->heightMap->indexBuffer.destroy();
				chunk->instanceBuffer.destroy();
			}
			delete chunk;
		}
		terrainChunks.resize(0);
	}

};

class VulkanExample : public VulkanExampleBase
{
public:
	bool debugDisplayReflection = false;
	bool debugDisplayRefraction = false;
	bool displayWaterPlane = true;
	bool displayWireFrame = false;
	bool renderShadows = true;
	bool fixFrustum = false;
	bool hasExtMemoryBudget = false;

	struct MemoryBudget {
		int heapCount;
		VkDeviceSize heapBudget[VK_MAX_MEMORY_HEAPS];
		VkDeviceSize heapUsage[VK_MAX_MEMORY_HEAPS];
		std::chrono::time_point<std::chrono::high_resolution_clock> lastUpdate;
	} memoryBudget{};

	//vks::HeightMap* heightMap;
	InfiniteTerrain infiniteTerrain;

	glm::vec4 lightPos;

	enum class SceneDrawType { sceneDrawTypeRefract, sceneDrawTypeReflect, sceneDrawTypeDisplay };
	enum class FramebufferType { Color, DepthStencil };

	const std::vector<std::string> treeModels = { 
		"spruce/spruce.gltf", 
		"pine/pine.gltf", 
		"fir/fir.gltf",
		"acacia/acacia.gltf",
		"beech/beech.gltf",
		"joshua/joshua.gltf",
		"tropical/tropical.gltf",
		"banana/banana.gltf",
		"willow/willow.gltf",
	};

	const std::vector<std::string> presets = {
		"default",
		"flat"
	};
	int32_t presetIndex = 0;

	struct CascadeDebug {
		bool enabled = false;
		int32_t cascadeIndex = 0;
		Pipeline* pipeline;
		PipelineLayout* pipelineLayout;
		DescriptorSet* descriptorSet;
		DescriptorSetLayout* descriptorSetLayout;
	} cascadeDebug;

	struct {
		Pipeline* debug;
		Pipeline* water;
		Pipeline* waterOffscreen;
		Pipeline* terrain;
		Pipeline* terrainOffscreen;
		Pipeline* sky;
		Pipeline* skyOffscreen;
		Pipeline* depthpass;
		Pipeline* depthpassTree;
		Pipeline* wireframe;
		Pipeline* tree;
		Pipeline* treeOffscreen;
	} pipelines;

	struct Textures {
		vks::Texture2D skySphere;
		vks::Texture2D waterNormalMap;
		vks::Texture2DArray terrainArray;
	} textures;

	std::vector<vks::Texture2D> skyspheres;
	int32_t skysphereIndex;

	// @todo: add some kind of basic asset manager
	struct Models {
		vkglTF::Model skysphere;
		vkglTF::Model plane;
		std::vector<vkglTF::Model> trees;
	} models;

	struct {
		vks::Buffer vsShared;
		vks::Buffer vsWater;
		vks::Buffer vsOffScreen;
		vks::Buffer vsDebugQuad;
		vks::Buffer terrain;
		vks::Buffer sky;
		vks::Buffer CSM;
		vks::Buffer params;
	} uniformBuffers;

	struct UBO {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightDir = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
	} uboShared, uboSky;

	struct UBOTerrain {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightDir = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
		glm::vec4 layers[TERRAIN_LAYER_COUNT];
	} uboTerrain;

	struct UBOCSM {
		float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];
		glm::mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];
		glm::mat4 inverseViewMat;
		glm::vec3 lightDir;
	} uboCSM;

	struct UBOWaterPlane {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 cameraPos;
		glm::vec4 lightDir;
		glm::vec4 color = glm::vec4(1.0f);
		float time;
	} uboWaterPlane;

	struct UniformDataParams {
		uint32_t shadows = 0;
		uint32_t fog = 1;
		uint32_t _pad0;
		uint32_t _pad1;
		glm::vec4 fogColor;
	} uniformDataParams;

	struct {
		PipelineLayout* debug;
		PipelineLayout* textured;
		PipelineLayout* terrain;
		PipelineLayout* sky;
		PipelineLayout* tree;
	} pipelineLayouts;

	DescriptorPool* descriptorPool;

	struct DescriptorSets {
		DescriptorSet* waterplane;
		DescriptorSet* debugquad;
		DescriptorSet* terrain;
		DescriptorSet* skysphere;
		DescriptorSet* sceneMatrices;
		DescriptorSet* sceneParams;
	} descriptorSets;

	struct {
		DescriptorSetLayout* textured;
		DescriptorSetLayout* terrain;
		DescriptorSetLayout* skysphere;
		// @todo
		DescriptorSetLayout* ubo;
		DescriptorSetLayout* images;
	} descriptorSetLayouts;

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkFramebuffer frameBuffer;
		ImageView* view;
		Image* image;
		VkDescriptorImageInfo descriptor;
	};
	struct OffscreenPass {
		int32_t width, height;
		FrameBufferAttachment reflection, refraction, depth;
		RenderPass* renderPass;
		VkSampler sampler;
	} offscreenPass;

	/* CSM */

	float cascadeSplitLambda = 0.95f;

	float zNear = 0.5f;
	float zFar = 1024.0f;

	// Resources of the depth map generation pass
	struct CascadePushConstBlock {
		glm::vec4 position;
		uint32_t cascadeIndex;
	};
	struct DepthPass {
		RenderPass* renderPass;
		PipelineLayout* pipelineLayout;
		VkPipeline pipeline;
		vks::Buffer uniformBuffer;
		DescriptorSetLayout* descriptorSetLayout;
		DescriptorSet* descriptorSet;
		struct UniformBlock {
			std::array<glm::mat4, SHADOW_MAP_CASCADE_COUNT> cascadeViewProjMat;
		} ubo;
	} depthPass;
	// Layered depth image containing the shadow cascade depths
	struct DepthImage {
		Image* image;
		ImageView* view;
		VkSampler sampler;
		void destroy(VkDevice device) {
			vkDestroySampler(device, sampler, nullptr);
		}
	} depth;

	// Contains all resources required for a single shadow map cascade
	struct Cascade {
		VkFramebuffer frameBuffer;
		DescriptorSet* descriptorSet;
		ImageView* view;
		float splitDepth;
		glm::mat4 viewProjMatrix;
		void destroy(VkDevice device) {
			vkDestroyFramebuffer(device, frameBuffer, nullptr);
		}
	};
	std::array<Cascade, SHADOW_MAP_CASCADE_COUNT> cascades;
	VkImageView cascadesView;
	VkFramebuffer cascadesFramebuffer;

	std::mutex lock_guard;

	void terrainUpdateThreadFn() {
		while (true) {
			if (infiniteTerrain.terrainChunkgsUpdateList.size() > 0) {
				std::lock_guard<std::mutex> guard(lock_guard);
				for (size_t i = 0; i < infiniteTerrain.terrainChunkgsUpdateList.size(); i++) {
					TerrainChunk* chunk = infiniteTerrain.terrainChunkgsUpdateList[i];
					heightMapSettings.offset.x = (float)chunk->position.x * (float)(chunk->size);
					heightMapSettings.offset.y = (float)chunk->position.y * (float)(chunk->size);
					chunk->updateHeightMap();
					chunk->updateTrees();
					chunk->min.y = chunk->heightMap->minHeight;
					chunk->max.y = chunk->heightMap->maxHeight;
					chunk->hasValidMesh = true;
				}
				std::cout << infiniteTerrain.terrainChunkgsUpdateList.size() << " Terrain chunks created\n";
				infiniteTerrain.terrainChunkgsUpdateList.clear();
			}
		}
	}

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Vulkan infinite terrain";
		camera.type = Camera::CameraType::firstperson;
		camera.setPerspective(45.0f, (float)width / (float)height, zNear, zFar);
		camera.movementSpeed = 7.5f * 5.0f;
		camera.rotationSpeed = 0.1f;
		settings.overlay = true;
		timerSpeed *= 0.05f;

		//camera.setPosition(glm::vec3(1021.0f, -32.0f, 526.0f));
		//camera.rotate(133.75f, 6.25f);

		// Frustumc ulld ebug
		//camera.setPosition(glm::vec3(83.86f, -17.9753284f, 90.0f));
//		camera.rotate(-318.62f, 5.0f);

		camera.setPosition(glm::vec3(0.0f, -25.0f, 0.0f));


		camera.update(0.0f);
		frustum.update(camera.matrices.perspective * camera.matrices.view);

		// The scene shader uses a clipping plane, so this feature has to be enabled
		enabledFeatures.shaderClipDistance = VK_TRUE;
		enabledFeatures.samplerAnisotropy = VK_TRUE;
		enabledFeatures.depthClamp = VK_TRUE;
		enabledFeatures.fillModeNonSolid = VK_TRUE;
		enabledFeatures11.multiview = VK_TRUE;

		// @todo
		float radius = 20.0f;
		lightPos = glm::vec4(-20.0f, -15.0f, -15.0f, 0.0f) * radius;
		uboTerrain.lightDir = glm::normalize(lightPos);

		// Spawn background thread that creates newly visible terrain chunkgs
		std::thread backgroundLoadingThread(&VulkanExample::terrainUpdateThreadFn, this);
		backgroundLoadingThread.detach();

		apiVersion = VK_API_VERSION_1_3;
		enabledDeviceExtensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);

		heightMapSettings.loadFromFile(getAssetPath() + "presets/default.txt");
		memcpy(uboTerrain.layers, heightMapSettings.textureLayers, sizeof(glm::vec4) * TERRAIN_LAYER_COUNT);

#if defined(_WIN32)
		//ShowCursor(false);
#endif
	}

	~VulkanExample()
	{
		vkDestroySampler(device, offscreenPass.sampler, nullptr);
		uniformBuffers.vsShared.destroy();
		uniformBuffers.vsWater.destroy();
		uniformBuffers.vsOffScreen.destroy();
		uniformBuffers.vsDebugQuad.destroy();
		uniformBuffers.params.destroy();
	}

	void createFrameBufferImage(FrameBufferAttachment& target, FramebufferType type)
	{
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageAspectFlags aspectMask;
		VkImageUsageFlags usageFlags;
		switch (type) {
		case FramebufferType::Color:
			format = swapChain.colorFormat;
			usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		case FramebufferType::DepthStencil:
			VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &format);
			usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
		}
		assert(format != VK_FORMAT_UNDEFINED);

		target.image = new Image(vulkanDevice);
		target.image->setType(VK_IMAGE_TYPE_2D);
		target.image->setFormat(format);
		target.image->setExtent({ (uint32_t)offscreenPass.width, (uint32_t)offscreenPass.height, 1 });
		target.image->setTiling(VK_IMAGE_TILING_OPTIMAL);
		target.image->setUsage(usageFlags);
		target.image->create();

		target.view = new ImageView(vulkanDevice);
		target.view->setType(VK_IMAGE_VIEW_TYPE_2D);
		target.view->setFormat(format);
		target.view->setSubResourceRange({ aspectMask, 0, 1, 0, 1});
		target.view->setImage(target.image);
		target.view->create();

		target.descriptor = { offscreenPass.sampler, target.view->handle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	}

	// Setup the offscreen framebuffer for rendering the mirrored scene
	// The color attachment of this framebuffer will then be used to sample from in the fragment shader of the final pass
	void prepareOffscreen()
	{
		// Find a suitable depth format
		VkFormat fbDepthFormat;
		VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &fbDepthFormat);
		assert(validDepthFormat);

		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		offscreenPass.renderPass = new RenderPass(device);
		offscreenPass.renderPass->setDimensions(FB_DIM, FB_DIM);

		offscreenPass.renderPass->addSubpassDescription({
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			1,
			&colorReference,
			nullptr,
			&depthReference,
			0,
			nullptr
		});

		// Color attachment
		offscreenPass.renderPass->addAttachmentDescription({
			0,
			swapChain.colorFormat,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});
		// Depth attachment
		offscreenPass.renderPass->addAttachmentDescription({
			0,
			fbDepthFormat,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		});
		// Subpass dependencies
		offscreenPass.renderPass->addSubpassDependency({
			VK_SUBPASS_EXTERNAL,
			0,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_DEPENDENCY_BY_REGION_BIT,
		});
		offscreenPass.renderPass->addSubpassDependency({
			0,
			VK_SUBPASS_EXTERNAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_DEPENDENCY_BY_REGION_BIT,
		});

		offscreenPass.renderPass->setColorClearValue(0, { 0.0f, 0.0f, 0.0f, 0.0f });
		offscreenPass.renderPass->setDepthStencilClearValue(1, 1.0f, 0);
		offscreenPass.renderPass->create();

		offscreenPass.width = FB_DIM;
		offscreenPass.height = FB_DIM;

		/* Renderpass */
	
		/* Shared sampler */

		VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = samplerInfo.addressModeU;
		samplerInfo.addressModeW = samplerInfo.addressModeU;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &offscreenPass.sampler));

		/* Framebuffer images */

		createFrameBufferImage(offscreenPass.refraction, FramebufferType::Color);
		createFrameBufferImage(offscreenPass.reflection, FramebufferType::Color);
		createFrameBufferImage(offscreenPass.depth, FramebufferType::DepthStencil);

		/* Framebuffers */

		VkImageView attachments[2];
		attachments[0] = offscreenPass.refraction.view->handle;
		attachments[1] = offscreenPass.depth.view->handle;

		VkFramebufferCreateInfo frameBufferCI = vks::initializers::framebufferCreateInfo();
		frameBufferCI.renderPass = offscreenPass.renderPass->handle;
		frameBufferCI.attachmentCount = 2;
		frameBufferCI.pAttachments = attachments;
		frameBufferCI.width = offscreenPass.width;
		frameBufferCI.height = offscreenPass.height;
		frameBufferCI.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCI, nullptr, &offscreenPass.refraction.frameBuffer));

		attachments[0] = offscreenPass.reflection.view->handle;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCI, nullptr, &offscreenPass.reflection.frameBuffer));
	}

	void drawScene(CommandBuffer* cb, SceneDrawType drawType)
	{
		// @todo: lower draw distance for reflection and refraction

		// @todo: rename to localMat
		struct PushConst {
			glm::mat4 scale = glm::mat4(1.0f);
			glm::vec4 clipPlane = glm::vec4(0.0f);
			uint32_t shadows = 1;
		} pushConst;
		if (drawType == SceneDrawType::sceneDrawTypeReflect) {
			pushConst.scale = glm::scale(pushConst.scale, glm::vec3(1.0f, -1.0f, 1.0f));
		}

		switch (drawType) {
		case SceneDrawType::sceneDrawTypeRefract:
			pushConst.clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, waterPosition);
			pushConst.shadows = 0;
			break;
		case SceneDrawType::sceneDrawTypeReflect:
			pushConst.clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, waterPosition);
			pushConst.shadows = 0;
			break;
		}

		bool offscreen = drawType != SceneDrawType::sceneDrawTypeDisplay;

		// Skysphere
		cb->bindPipeline(offscreen ? pipelines.skyOffscreen : pipelines.sky);
		cb->bindDescriptorSets(pipelineLayouts.sky, { descriptorSets.skysphere }, 0);
		cb->updatePushConstant(pipelineLayouts.sky, 0, &pushConst);
		models.skysphere.draw(cb->handle);

		// Terrain
		if (displayWireFrame) {
			cb->bindPipeline(pipelines.wireframe);
		} else {
			cb->bindPipeline(offscreen ? pipelines.terrainOffscreen : pipelines.terrain);
		}
		cb->bindDescriptorSets(pipelineLayouts.terrain, { descriptorSets.terrain }, 0);
		cb->bindDescriptorSets(pipelineLayouts.terrain, { descriptorSets.sceneParams }, 1);
		cb->updatePushConstant(pipelineLayouts.terrain, 0, &pushConst);
		for (auto& terrainChunk : infiniteTerrain.terrainChunks) {
			if (terrainChunk->visible && terrainChunk->hasValidMesh) {
				glm::vec3 pos = glm::vec3((float)terrainChunk->position.x, 0.0f, (float)terrainChunk->position.y) * glm::vec3(chunkDim - 1.0f, 0.0f, chunkDim - 1.0f);
				//pos.x = 0.0f;
				//pos.y = 0.0f;
				if (drawType == SceneDrawType::sceneDrawTypeReflect) {
					pos.y += waterPosition * 2.0f;
					vkCmdSetCullMode(cb->handle, VK_CULL_MODE_BACK_BIT);
				} else {
					vkCmdSetCullMode(cb->handle, VK_CULL_MODE_FRONT_BIT);
				}
				vkCmdPushConstants(cb->handle, pipelineLayouts.terrain->handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(glm::vec3), &pos);
				terrainChunk->draw(cb);
			}
		}

		// Water
		vkCmdSetCullMode(cb->handle, VK_CULL_MODE_BACK_BIT);
		if ((drawType == SceneDrawType::sceneDrawTypeDisplay) && (displayWaterPlane)) {
			cb->bindDescriptorSets(pipelineLayouts.textured, { descriptorSets.waterplane }, 0);
			cb->bindDescriptorSets(pipelineLayouts.textured, { descriptorSets.sceneParams }, 1);
			cb->bindPipeline(offscreen ? pipelines.waterOffscreen : pipelines.water);
			for (auto& terrainChunk : infiniteTerrain.terrainChunks) {
				if (terrainChunk->visible && terrainChunk->hasValidMesh) {
					glm::vec3 pos = glm::vec3((float)terrainChunk->position.x, -waterPosition, (float)terrainChunk->position.y) * glm::vec3(chunkDim - 1.0f, 1.0f, chunkDim - 1.0f);
					vkCmdPushConstants(cb->handle, pipelineLayouts.terrain->handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(glm::vec3), &pos);
					models.plane.draw(cb->handle);
				}
			}
		}

		// Trees
		vkCmdSetCullMode(cb->handle, VK_CULL_MODE_NONE);
		if (drawType != SceneDrawType::sceneDrawTypeRefract) {
			for (auto& terrainChunk : infiniteTerrain.terrainChunks) {
				if (terrainChunk->visible && terrainChunk->hasValidMesh) {
					VkDeviceSize offsets[1] = { 0 };
					vkCmdBindVertexBuffers(cb->handle, 1, 1, &terrainChunk->instanceBuffer.buffer, offsets);
					// @todo: offset pos by waterplane? also needed for terrain?
					cb->bindPipeline(offscreen ? pipelines.treeOffscreen : pipelines.tree);
					cb->bindDescriptorSets(pipelineLayouts.tree, { descriptorSets.sceneMatrices }, 0);
					cb->bindDescriptorSets(pipelineLayouts.tree, { descriptorSets.sceneParams }, 2);
					glm::vec3 pos = glm::vec3((float)terrainChunk->position.x, 0.0f, (float)terrainChunk->position.y) * glm::vec3(chunkDim - 1.0f, 0.0f, chunkDim - 1.0f);
					if (drawType == SceneDrawType::sceneDrawTypeReflect) {
						pos.y += waterPosition * 2.0f;
					}
					cb->updatePushConstant(pipelineLayouts.tree, 0, &pushConst);
					vkCmdPushConstants(cb->handle, pipelineLayouts.terrain->handle, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(glm::vec3), &pos);
					models.trees[heightMapSettings.treeModelIndex].draw(cb->handle, vkglTF::RenderFlags::BindImages, pipelineLayouts.tree->handle, 1, terrainChunk->treeInstanceCount);
				}
			}
		}
	}

	void drawShadowCasters(CommandBuffer* cb, uint32_t cascadeIndex = 0) {
		vks::Frustum cascadeFrustum;
		cascadeFrustum.update(cascades[cascadeIndex].viewProjMatrix);

		CascadePushConstBlock pushConst = { glm::vec4(0.0f), cascadeIndex };
		cb->bindPipeline(pipelines.depthpass);
		cb->bindDescriptorSets(depthPass.pipelineLayout, { depthPass.descriptorSet }, 0);
		//vkCmdSetCullMode(cb->handle, VK_CULL_MODE_FRONT_BIT);

		// Terrain
		// @todo: limit distance
		for (auto& terrainChunk : infiniteTerrain.terrainChunks) {
			//if (terrainChunk->visible && terrainChunk->hasValidMesh) {
			bool chunkVisible = terrainChunk->hasValidMesh && terrainChunk->visible;
			if (chunkVisible) {
				pushConst.position = glm::vec4((float)terrainChunk->position.x, 0.0f, (float)terrainChunk->position.y, 0.0f) * glm::vec4(chunkDim - 1.0f, 0.0f, chunkDim - 1.0f, 0.0f);
				cb->updatePushConstant(depthPass.pipelineLayout, 0, &pushConst);
				terrainChunk->draw(cb);
			}
		}
		// Trees
		// @todo: limit distance
		cb->bindDescriptorSets(depthPass.pipelineLayout, { depthPass.descriptorSet }, 0);
		cb->bindPipeline(pipelines.depthpassTree);
		for (auto& terrainChunk : infiniteTerrain.terrainChunks) {
			bool chunkVisible = terrainChunk->hasValidMesh && terrainChunk->visible;
			if (chunkVisible) {
				VkDeviceSize offsets[1] = { 0 };
				vkCmdBindVertexBuffers(cb->handle, 1, 1, &terrainChunk->instanceBuffer.buffer, offsets);
				pushConst.position = glm::vec4((float)terrainChunk->position.x, 0.0f, (float)terrainChunk->position.y, 0.0f) * glm::vec4(chunkDim - 1.0f, 0.0f, chunkDim - 1.0f, 0.0f);
				cb->updatePushConstant(depthPass.pipelineLayout, 0, &pushConst);
				models.trees[heightMapSettings.treeModelIndex].draw(cb->handle, vkglTF::RenderFlags::BindImages, depthPass.pipelineLayout->handle, 1, terrainChunk->treeInstanceCount);
			}
		}
	}

	/*
		CSM
	*/

	void prepareCSM()
	{
		VkFormat depthFormat;
		vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);

		/*
			Depth map renderpass
		*/

		VkAttachmentReference depthReference = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		depthPass.renderPass = new RenderPass(device);
		depthPass.renderPass->setDimensions(SHADOWMAP_DIM, SHADOWMAP_DIM);
		depthPass.renderPass->addSubpassDescription({
			0,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			0,
			nullptr,
			0,
			nullptr,
			nullptr,
			&depthReference,
			0,
			nullptr
			});
		// Depth attachment
		depthPass.renderPass->addAttachmentDescription({
			0,
			depthFormat,
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
			});
		// Subpass dependencies
		depthPass.renderPass->addSubpassDependency({
			VK_SUBPASS_EXTERNAL,
			0,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_DEPENDENCY_BY_REGION_BIT,
			});
		depthPass.renderPass->addSubpassDependency({
			0,
			VK_SUBPASS_EXTERNAL,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_DEPENDENCY_BY_REGION_BIT,
			});

		const uint32_t viewMask = 0b00001111;
		const uint32_t correlationMask = 0b00001111;

		VkRenderPassMultiviewCreateInfo renderPassMultiviewCI{};
		renderPassMultiviewCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
		renderPassMultiviewCI.subpassCount = 1;
		renderPassMultiviewCI.pViewMasks = &viewMask;
		renderPassMultiviewCI.correlationMaskCount = 1;
		renderPassMultiviewCI.pCorrelationMasks = &correlationMask;

		depthPass.renderPass->setMultiview(renderPassMultiviewCI);
		depthPass.renderPass->setDepthStencilClearValue(0, 1.0f, 0);
		depthPass.renderPass->create();

		/*
			Layered depth image and views
		*/
		depth.image = new Image(vulkanDevice);
		depth.image->setType(VK_IMAGE_TYPE_2D);
		depth.image->setFormat(depthFormat);
		depth.image->setExtent({ SHADOWMAP_DIM, SHADOWMAP_DIM, 1 });
		depth.image->setNumArrayLayers(SHADOW_MAP_CASCADE_COUNT);
		depth.image->setUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		depth.image->setTiling(VK_IMAGE_TILING_OPTIMAL);
		depth.image->create();

		// Full depth map view (all layers)
		depth.view = new ImageView(vulkanDevice);
		depth.view->setImage(depth.image);
		depth.view->setType(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
		depth.view->setFormat(depthFormat);
		depth.view->setSubResourceRange({ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, SHADOW_MAP_CASCADE_COUNT });
		depth.view->create();

		// One image and framebuffer per cascade
		//for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		//	// Image view for this cascade's layer (inside the depth map) this view is used to render to that specific depth image layer
		//	cascades[i].view = new ImageView(vulkanDevice);
		//	cascades[i].view->setImage(depth.image);
		//	cascades[i].view->setType(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
		//	cascades[i].view->setFormat(depthFormat);
		//	cascades[i].view->setSubResourceRange({ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, i, 1 });
		//	cascades[i].view->create();
		//	// Framebuffer
		//	VkFramebufferCreateInfo framebufferInfo = vks::initializers::framebufferCreateInfo();
		//	framebufferInfo.renderPass = depthPass.renderPass->handle;
		//	framebufferInfo.attachmentCount = 1;
		//	framebufferInfo.pAttachments = &cascades[i].view->handle;
		//	framebufferInfo.width = SHADOWMAP_DIM;
		//	framebufferInfo.height = SHADOWMAP_DIM;
		//	framebufferInfo.layers = 1;
		//	VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &cascades[i].frameBuffer));
		//}

		// Image view for this cascade's layer (inside the depth map) this view is used to render to that specific depth image layer
		VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		imageViewCI.format = depthFormat;
		imageViewCI.subresourceRange = {};
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.layerCount = SHADOW_MAP_CASCADE_COUNT;
		imageViewCI.image = depth.image->handle;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &cascadesView));

		//cascades[i].view = new ImageView(vulkanDevice);
		//cascades[i].view->setImage(depth.image);
		//cascades[i].view->setType(VK_IMAGE_VIEW_TYPE_2D_ARRAY);
		//cascades[i].view->setFormat(depthFormat);
		//cascades[i].view->setSubResourceRange({ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, i, 1 });
		//cascades[i].view->create();
		// 
		// Framebuffer
		VkImageView attachments[1];
		attachments[0] = cascadesView;

		VkFramebufferCreateInfo framebufferInfo = vks::initializers::framebufferCreateInfo();
		framebufferInfo.renderPass = depthPass.renderPass->handle;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = SHADOWMAP_DIM;
		framebufferInfo.height = SHADOWMAP_DIM;
		framebufferInfo.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &cascadesFramebuffer));

		// Shared sampler for cascade deoth reads
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &depth.sampler));
	}

	/*
		Calculate frustum split depths and matrices for the shadow map cascades
		Based on https://johanmedestrom.wordpress.com/2016/03/18/opengl-cascaded-shadow-maps/
	*/
	void updateCascades()
	{
		float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

		float nearClip = camera.getNearClip();
		float farClip = camera.getFarClip();
		float clipRange = farClip - nearClip;

		float minZ = nearClip;
		float maxZ = nearClip + clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate split depths based on view camera furstum
		// Based on method presentd in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
			float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
			float log = minZ * std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = cascadeSplitLambda * (log - uniform) + uniform;
			cascadeSplits[i] = (d - nearClip) / clipRange;
		}

		// Calculate orthographic projection matrix for each cascade
		float lastSplitDist = 0.0;
		for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
			float splitDist = cascadeSplits[i];

			glm::vec3 frustumCorners[8] = {
				glm::vec3(-1.0f,  1.0f, -1.0f),
				glm::vec3( 1.0f,  1.0f, -1.0f),
				glm::vec3( 1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f, -1.0f, -1.0f),
				glm::vec3(-1.0f,  1.0f,  1.0f),
				glm::vec3( 1.0f,  1.0f,  1.0f),
				glm::vec3( 1.0f, -1.0f,  1.0f),
				glm::vec3(-1.0f, -1.0f,  1.0f),
			};

			// Project frustum corners into world space
			glm::mat4 invCam = glm::inverse(camera.matrices.perspective * camera.matrices.view);
			for (uint32_t i = 0; i < 8; i++) {
				glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
				frustumCorners[i] = invCorner / invCorner.w;
			}

			for (uint32_t i = 0; i < 4; i++) {
				glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
				frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
				frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
			}

			// Get frustum center
			glm::vec3 frustumCenter = glm::vec3(0.0f);
			for (uint32_t i = 0; i < 8; i++) {
				frustumCenter += frustumCorners[i];
			}
			frustumCenter /= 8.0f;

			float radius = 0.0f;
			for (uint32_t i = 0; i < 8; i++) {
				float distance = glm::length(frustumCorners[i] - frustumCenter);
				radius = glm::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::vec3 lightDir = glm::normalize(-lightPos);
			glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
			glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

			// Store split distance and matrix in cascade
			cascades[i].splitDepth = (camera.getNearClip() + splitDist * clipRange) * -1.0f;
			cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

			lastSplitDist = cascadeSplits[i];
		}
	}

	void drawCSM(CommandBuffer *cb) {
		/*
			Generate depth map cascades

			Uses multiple passes with each pass rendering the scene to the cascade's depth image layer
			Could be optimized using a geometry shader (and layered frame buffer) on devices that support geometry shaders
		*/
		cb->setViewport(0, 0, (float)SHADOWMAP_DIM, (float)SHADOWMAP_DIM, 0.0f, 1.0f);
		cb->setScissor(0, 0, SHADOWMAP_DIM, SHADOWMAP_DIM);
		// One pass per cascade
		//// The layer that this pass renders to is defined by the cascade's image view (selected via the cascade's decsriptor set)
		//for (uint32_t j = 0; j < SHADOW_MAP_CASCADE_COUNT; j++) {
		//	cb->beginRenderPass(depthPass.renderPass, cascades[j].frameBuffer);
		//	drawShadowCasters(cb, j);
		//	cb->endRenderPass();
		//}

		cb->beginRenderPass(depthPass.renderPass, cascadesFramebuffer);
		drawShadowCasters(cb, 0);
		cb->endRenderPass();
	}

	/*
		Sample
	*/

	void loadSkySphere(const std::string filename)
	{
		vkQueueWaitIdle(queue);
		textures.skySphere.destroy();
		textures.skySphere.loadFromFile(getAssetPath() + "textures/" + filename, VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		descriptorSets.skysphere->updateDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textures.skySphere.descriptor);
	}

	void loadAssets()
	{
		models.skysphere.loadFromFile(getAssetPath() + "scenes/geosphere.gltf", vulkanDevice, queue);
		models.plane.loadFromFile(getAssetPath() + "scenes/plane.gltf", vulkanDevice, queue);
		models.trees.resize(treeModels.size());
		for (size_t i = 0; i < treeModels.size(); i++) {
			models.trees[i].loadFromFile(getAssetPath() + "scenes/trees/" + treeModels[i], vulkanDevice, queue, vkglTF::FileLoadingFlags::FlipY);
		}

		textures.skySphere.loadFromFile(getAssetPath() + "textures/skysphere2.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.terrainArray.loadFromFile(getAssetPath() + "textures/terrain_layers_01_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.waterNormalMap.loadFromFile(getAssetPath() + "textures/water_normal_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);

		VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();

		// Setup a repeating sampler for the terrain texture layers
		// @todo: Sampler class, that can e.g. check against device if anisotropy > max and then lower it
		// also enable aniso if max is > 1.0
		vkDestroySampler(device, textures.terrainArray.sampler, nullptr);
		samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = samplerInfo.addressModeU;
		samplerInfo.addressModeW = samplerInfo.addressModeU;
		samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = (float)textures.terrainArray.mipLevels;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		if (deviceFeatures.samplerAnisotropy)
		{
			samplerInfo.maxAnisotropy = 4.0f;
			samplerInfo.anisotropyEnable = VK_TRUE;
		}
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &textures.terrainArray.sampler));
		textures.terrainArray.descriptor.sampler = textures.terrainArray.sampler;
	}

	// Generate a terrain quad patch for feeding to the tessellation control shader
	void generateTerrain()
	{
		infiniteTerrain.viewerPosition = glm::vec2(camera.position.x, camera.position.z);
		infiniteTerrain.updateVisibleChunks(camera);
	}

	void updateHeightmap(bool firstRun)
	{
		infiniteTerrain.updateChunks();
		// @todo
		// terrainChunk->updateHeightMap();
	}

	void setupDescriptorPool()
	{
		// @todo: proper sizes
		descriptorPool = new DescriptorPool(device);
		descriptorPool->setMaxSets(16);
		descriptorPool->addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32);
		descriptorPool->addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32);
		descriptorPool->create();
	}

	void setupDescriptorSetLayout()
	{
		// Trees (@todo: glTF models in general)
		descriptorSetLayouts.ubo = new DescriptorSetLayout(device);
		descriptorSetLayouts.ubo->addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.ubo->create();

		// @todo: remove
		descriptorSetLayouts.images = new DescriptorSetLayout(device);
		descriptorSetLayouts.images->addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.images->create();

		// Shared (use all layout bindings)
		descriptorSetLayouts.textured = new DescriptorSetLayout(device);
		descriptorSetLayouts.textured->addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->addBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.textured->create();

		pipelineLayouts.textured = new PipelineLayout(device);
		pipelineLayouts.textured->addLayout(descriptorSetLayouts.textured);
		pipelineLayouts.textured->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.textured->addPushConstantRange(108, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineLayouts.textured->create();

		// Debug
		pipelineLayouts.debug = new PipelineLayout(device);
		pipelineLayouts.debug->addLayout(descriptorSetLayouts.textured);
		pipelineLayouts.debug->addPushConstantRange(sizeof(uint32_t), 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineLayouts.debug->create();

		// Terrain
		descriptorSetLayouts.terrain = new DescriptorSetLayout(device);
		descriptorSetLayouts.terrain->addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.terrain->addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.terrain->addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.terrain->addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.terrain->addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.terrain->create();

		pipelineLayouts.terrain = new PipelineLayout(device);
		pipelineLayouts.terrain->addLayout(descriptorSetLayouts.terrain);
		pipelineLayouts.terrain->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.terrain->addPushConstantRange(108, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineLayouts.terrain->create();

		pipelineLayouts.tree = new PipelineLayout(device);
		pipelineLayouts.tree->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.tree->addLayout(vkglTF::descriptorSetLayoutImage);
		pipelineLayouts.tree->addLayout(descriptorSetLayouts.ubo);
		pipelineLayouts.tree->addPushConstantRange(108, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		pipelineLayouts.tree->create();

		// Skysphere
		descriptorSetLayouts.skysphere = new DescriptorSetLayout(device);
		descriptorSetLayouts.skysphere->addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		descriptorSetLayouts.skysphere->addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayouts.skysphere->create();

		pipelineLayouts.sky = new PipelineLayout(device);
		pipelineLayouts.sky->addLayout(descriptorSetLayouts.skysphere);
		pipelineLayouts.sky->addPushConstantRange(sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(uint32_t), 0, VK_SHADER_STAGE_VERTEX_BIT);
		pipelineLayouts.sky->create();

		// Depth pass
		depthPass.descriptorSetLayout = new DescriptorSetLayout(device);
		depthPass.descriptorSetLayout->addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		depthPass.descriptorSetLayout->create();

		depthPass.pipelineLayout = new PipelineLayout(device);
		depthPass.pipelineLayout->addLayout(depthPass.descriptorSetLayout);
		depthPass.pipelineLayout->addLayout(vkglTF::descriptorSetLayoutImage);
		depthPass.pipelineLayout->addPushConstantRange(sizeof(CascadePushConstBlock), 0, VK_SHADER_STAGE_VERTEX_BIT);
		depthPass.pipelineLayout->create();

		// Cascade debug
		cascadeDebug.descriptorSetLayout = new DescriptorSetLayout(device);
		cascadeDebug.descriptorSetLayout->addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		cascadeDebug.descriptorSetLayout->create();

		cascadeDebug.pipelineLayout = new PipelineLayout(device);
		cascadeDebug.pipelineLayout->addLayout(cascadeDebug.descriptorSetLayout);
		cascadeDebug.pipelineLayout->addPushConstantRange(sizeof(glm::vec4) + sizeof(uint32_t), 0, VK_SHADER_STAGE_VERTEX_BIT);
		cascadeDebug.pipelineLayout->create();

	}

	void setupDescriptorSet()
	{
		VkDescriptorImageInfo depthMapDescriptor = vks::initializers::descriptorImageInfo(depth.sampler, depth.view->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

		// Water plane
		descriptorSets.waterplane = new DescriptorSet(device);
		descriptorSets.waterplane->setPool(descriptorPool);
		descriptorSets.waterplane->addLayout(descriptorSetLayouts.textured);
		descriptorSets.waterplane->addDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBuffers.vsWater.descriptor);
		descriptorSets.waterplane->addDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offscreenPass.refraction.descriptor);
		descriptorSets.waterplane->addDescriptor(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offscreenPass.reflection.descriptor);
		descriptorSets.waterplane->addDescriptor(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textures.waterNormalMap.descriptor);
		descriptorSets.waterplane->addDescriptor(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthMapDescriptor);
		descriptorSets.waterplane->addDescriptor(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBuffers.CSM.descriptor);
		descriptorSets.waterplane->create();
			   			   		
		// Debug quad
		descriptorSets.debugquad = new DescriptorSet(device);
		descriptorSets.debugquad->setPool(descriptorPool);
		descriptorSets.debugquad->addLayout(descriptorSetLayouts.textured);
		descriptorSets.debugquad->addDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offscreenPass.reflection.descriptor);
		descriptorSets.debugquad->addDescriptor(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &offscreenPass.refraction.descriptor);
		descriptorSets.debugquad->create();

		// Terrain
		descriptorSets.terrain = new DescriptorSet(device);
		descriptorSets.terrain->setPool(descriptorPool);
		descriptorSets.terrain->addLayout(descriptorSetLayouts.terrain);
		descriptorSets.terrain->addDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBuffers.terrain.descriptor);
		descriptorSets.terrain->addDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textures.terrainArray.descriptor);
		descriptorSets.terrain->addDescriptor(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textures.terrainArray.descriptor);
		descriptorSets.terrain->addDescriptor(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthMapDescriptor);
		descriptorSets.terrain->addDescriptor(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBuffers.CSM.descriptor);
		descriptorSets.terrain->create();

		// Skysphere
		descriptorSets.skysphere = new DescriptorSet(device);
		descriptorSets.skysphere->setPool(descriptorPool);
		descriptorSets.skysphere->addLayout(descriptorSetLayouts.skysphere);
		descriptorSets.skysphere->addDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBuffers.sky.descriptor);
		descriptorSets.skysphere->addDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textures.skySphere.descriptor);
		descriptorSets.skysphere->create();

		// @todo
		descriptorSets.sceneMatrices = new DescriptorSet(device);
		descriptorSets.sceneMatrices->setPool(descriptorPool);
		descriptorSets.sceneMatrices->addLayout(descriptorSetLayouts.ubo);
		descriptorSets.sceneMatrices->addDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBuffers.vsShared.descriptor);
		descriptorSets.sceneMatrices->create();

		descriptorSets.sceneParams = new DescriptorSet(device);
		descriptorSets.sceneParams->setPool(descriptorPool);
		descriptorSets.sceneParams->addLayout(descriptorSetLayouts.ubo);
		descriptorSets.sceneParams->addDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBuffers.params.descriptor);
		descriptorSets.sceneParams->create();

		// Shadow map cascades (one set per cascade)
		// @todo: Doesn't make sense, all refer to same depth
		for (auto i = 0; i < cascades.size(); i++) {
			VkDescriptorImageInfo cascadeImageInfo = vks::initializers::descriptorImageInfo(depth.sampler, depth.view->handle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
			cascades[i].descriptorSet = new DescriptorSet(device);
			cascades[i].descriptorSet->setPool(descriptorPool);
			cascades[i].descriptorSet->addLayout(descriptorSetLayouts.textured);
			cascades[i].descriptorSet->addDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &depthPass.uniformBuffer.descriptor);
			cascades[i].descriptorSet->addDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &cascadeImageInfo);
			cascades[i].descriptorSet->create();
		}

		// Depth pass
		depthPass.descriptorSet = new DescriptorSet(device);
		depthPass.descriptorSet->setPool(descriptorPool);
		depthPass.descriptorSet->addLayout(depthPass.descriptorSetLayout);
		depthPass.descriptorSet->addDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &depthPass.uniformBuffer.descriptor);
		depthPass.descriptorSet->create();

		// Cascade debug
		cascadeDebug.descriptorSet = new DescriptorSet(device);
		cascadeDebug.descriptorSet->setPool(descriptorPool);
		cascadeDebug.descriptorSet->addLayout(cascadeDebug.descriptorSetLayout);
		cascadeDebug.descriptorSet->addDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthMapDescriptor);
		cascadeDebug.descriptorSet->create();
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE,VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_CULL_MODE };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		// Vertex bindings and attributes
		// Terrain / shared
		const VkVertexInputBindingDescription vertexInputBinding = vks::initializers::vertexInputBindingDescription(0, sizeof(vks::HeightMap::Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
		const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vks::HeightMap::Vertex, pos)),
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vks::HeightMap::Vertex, normal)),
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(vks::HeightMap::Vertex, uv)),
			vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vks::HeightMap::Vertex, color)),
			vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32_SFLOAT, offsetof(vks::HeightMap::Vertex, terrainHeight)),
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = 1;
		vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		// glTF models
		VkPipelineVertexInputStateCreateInfo* vertexInputStateModel = vkglTF::Vertex::getPipelineVertexInputState({
			vkglTF::VertexComponent::Position, 
			vkglTF::VertexComponent::Normal, 
			vkglTF::VertexComponent::UV 
		});

		// Instanced
		VkPipelineVertexInputStateCreateInfo vertexInputStateModelInstanced = vks::initializers::pipelineVertexInputStateCreateInfo();
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

		bindingDescriptions = {
			vks::initializers::vertexInputBindingDescription(0, sizeof(vkglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
			vks::initializers::vertexInputBindingDescription(1, sizeof(InstanceData), VK_VERTEX_INPUT_RATE_INSTANCE)
		};

		attributeDescriptions = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6),
			vks::initializers::vertexInputAttributeDescription(1, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, pos)),
			vks::initializers::vertexInputAttributeDescription(1, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, scale)),
			vks::initializers::vertexInputAttributeDescription(1, 5, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, rotation)),
		};
		vertexInputStateModelInstanced.pVertexBindingDescriptions = bindingDescriptions.data();
		vertexInputStateModelInstanced.pVertexAttributeDescriptions = attributeDescriptions.data();
		vertexInputStateModelInstanced.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
		vertexInputStateModelInstanced.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());

		// Empty state (no input)
		VkPipelineVertexInputStateCreateInfo vertexInputStateEmpty = vks::initializers::pipelineVertexInputStateCreateInfo();

		VkGraphicsPipelineCreateInfo pipelineCI{};
		pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCI.pVertexInputState = &vertexInputState;
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;

		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		depthStencilState.depthTestEnable = VK_FALSE;

		// Debug
		pipelines.debug = new Pipeline(device);
		pipelines.debug->setCreateInfo(pipelineCI);
		pipelines.debug->setVertexInputState(&vertexInputStateEmpty);
		pipelines.debug->setCache(pipelineCache);
		pipelines.debug->setLayout(pipelineLayouts.debug);
		pipelines.debug->setRenderPass(renderPass);
		pipelines.debug->addShader(getAssetPath() + "shaders/quad.vert.spv");
		pipelines.debug->addShader(getAssetPath() + "shaders/quad.frag.spv");
		pipelines.debug->create();
		// Debug cascades
		cascadeDebug.pipeline = new Pipeline(device);
		cascadeDebug.pipeline->setCreateInfo(pipelineCI);
		cascadeDebug.pipeline->setVertexInputState(&vertexInputStateEmpty);
		cascadeDebug.pipeline->setCache(pipelineCache);
		cascadeDebug.pipeline->setLayout(cascadeDebug.pipelineLayout);
		cascadeDebug.pipeline->setRenderPass(renderPass);
		cascadeDebug.pipeline->addShader(getAssetPath() + "shaders/debug_csm.vert.spv");
		cascadeDebug.pipeline->addShader(getAssetPath() + "shaders/debug_csm.frag.spv");
		cascadeDebug.pipeline->create();

		depthStencilState.depthTestEnable = VK_TRUE;

		// Water
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		pipelines.water = new Pipeline(device);
		pipelines.water->setCreateInfo(pipelineCI);
		pipelines.water->setVertexInputState(vertexInputStateModel);
		pipelines.water->setCache(pipelineCache);
		pipelines.water->setLayout(pipelineLayouts.textured);
		pipelines.water->setRenderPass(renderPass);
		pipelines.water->addShader(getAssetPath() + "shaders/water.vert.spv");
		pipelines.water->addShader(getAssetPath() + "shaders/water.frag.spv");
		pipelines.water->create();
		// Offscreen
		pipelines.waterOffscreen = new Pipeline(device);
		pipelines.waterOffscreen->setCreateInfo(pipelineCI);
		pipelines.waterOffscreen->setVertexInputState(vertexInputStateModel);
		pipelines.waterOffscreen->setCache(pipelineCache);
		pipelines.waterOffscreen->setLayout(pipelineLayouts.textured);
		pipelines.waterOffscreen->setRenderPass(offscreenPass.renderPass);
		pipelines.waterOffscreen->addShader(getAssetPath() + "shaders/water.vert.spv");
		pipelines.waterOffscreen->addShader(getAssetPath() + "shaders/water.frag.spv");
		pipelines.waterOffscreen->create();

		// Terrain
		//rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		pipelines.terrain = new Pipeline(device);
		pipelines.terrain->setCreateInfo(pipelineCI);
		pipelines.terrain->setVertexInputState(&vertexInputState);
		pipelines.terrain->setCache(pipelineCache);
		pipelines.terrain->setLayout(pipelineLayouts.terrain);
		pipelines.terrain->setRenderPass(renderPass);
		pipelines.terrain->addShader(getAssetPath() + "shaders/terrain.vert.spv");
		pipelines.terrain->addShader(getAssetPath() + "shaders/terrain.frag.spv");
		pipelines.terrain->create();
		// Offscreen
		//rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		pipelines.terrainOffscreen = new Pipeline(device);
		pipelines.terrainOffscreen->setCreateInfo(pipelineCI);
		pipelines.terrainOffscreen->setVertexInputState(&vertexInputState);
		pipelines.terrainOffscreen->setCache(pipelineCache);
		pipelines.terrainOffscreen->setLayout(pipelineLayouts.terrain);
		pipelines.terrainOffscreen->setRenderPass(offscreenPass.renderPass);
		pipelines.terrainOffscreen->addShader(getAssetPath() + "shaders/terrain.vert.spv");
		pipelines.terrainOffscreen->addShader(getAssetPath() + "shaders/terrain.frag.spv");
		pipelines.terrainOffscreen->create();
		// Wireframe (@todo: offscreen)
		rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
		pipelines.wireframe = new Pipeline(device);
		pipelines.wireframe->setCreateInfo(pipelineCI);
		pipelines.wireframe->setVertexInputState(&vertexInputState);
		pipelines.wireframe->setCache(pipelineCache);
		pipelines.wireframe->setLayout(pipelineLayouts.terrain);
		pipelines.wireframe->setRenderPass(renderPass);
		pipelines.wireframe->addShader(getAssetPath() + "shaders/terrain.vert.spv");
		pipelines.wireframe->addShader(getAssetPath() + "shaders/terrain.frag.spv");
		pipelines.wireframe->create();

		// Sky
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		depthStencilState.depthWriteEnable = VK_FALSE;
		pipelines.sky = new Pipeline(device);
		pipelines.sky->setCreateInfo(pipelineCI);
		pipelines.sky->setVertexInputState(vertexInputStateModel);
		pipelines.sky->setCache(pipelineCache);
		pipelines.sky->setLayout(pipelineLayouts.sky);
		pipelines.sky->setRenderPass(renderPass);
		pipelines.sky->addShader(getAssetPath() + "shaders/skysphere.vert.spv");
		pipelines.sky->addShader(getAssetPath() + "shaders/skysphere.frag.spv");
		pipelines.sky->create();
		// Offscreen
		pipelines.skyOffscreen = new Pipeline(device);
		pipelines.skyOffscreen->setCreateInfo(pipelineCI);
		pipelines.skyOffscreen->setVertexInputState(vertexInputStateModel);
		pipelines.skyOffscreen->setCache(pipelineCache);
		pipelines.skyOffscreen->setLayout(pipelineLayouts.sky);
		pipelines.skyOffscreen->setRenderPass(offscreenPass.renderPass);
		pipelines.skyOffscreen->addShader(getAssetPath() + "shaders/skysphere.vert.spv");
		pipelines.skyOffscreen->addShader(getAssetPath() + "shaders/skysphere.frag.spv");
		pipelines.skyOffscreen->create();

		// Trees
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		depthStencilState.depthWriteEnable = VK_TRUE;

		// @todo: not sure: blend or discard
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		pipelines.tree = new Pipeline(device);
		pipelines.tree->setCreateInfo(pipelineCI);
		pipelines.tree->setVertexInputState(&vertexInputStateModelInstanced);
		pipelines.tree->setCache(pipelineCache);
		pipelines.tree->setLayout(pipelineLayouts.tree);
		pipelines.tree->setRenderPass(renderPass);
		pipelines.tree->addShader(getAssetPath() + "shaders/tree.vert.spv");
		pipelines.tree->addShader(getAssetPath() + "shaders/tree.frag.spv");
		pipelines.tree->create();
		// Offscreen
		pipelines.treeOffscreen = new Pipeline(device);
		pipelines.treeOffscreen->setCreateInfo(pipelineCI);
		pipelines.treeOffscreen->setVertexInputState(&vertexInputStateModelInstanced);
		pipelines.treeOffscreen->setCache(pipelineCache);
		pipelines.treeOffscreen->setLayout(pipelineLayouts.tree);
		pipelines.treeOffscreen->setRenderPass(offscreenPass.renderPass);
		pipelines.treeOffscreen->addShader(getAssetPath() + "shaders/tree.vert.spv");
		pipelines.treeOffscreen->addShader(getAssetPath() + "shaders/tree.frag.spv");
		pipelines.treeOffscreen->create();

		depthStencilState.depthWriteEnable = VK_TRUE;
		blendAttachmentState.blendEnable = VK_FALSE;

		// Shadow map depth pass
		colorBlendState.attachmentCount = 0;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		// Enable depth clamp (if available)
		rasterizationState.depthClampEnable = deviceFeatures.depthClamp;
		pipelines.depthpass = new Pipeline(device);
		pipelines.depthpass->setCreateInfo(pipelineCI);
		pipelines.depthpass->setVertexInputState(&vertexInputState);
		pipelines.depthpass->setCache(pipelineCache);
		pipelines.depthpass->setLayout(depthPass.pipelineLayout);
		pipelines.depthpass->setRenderPass(depthPass.renderPass);
		pipelines.depthpass->addShader(getAssetPath() + "shaders/depthpass.vert.spv");
		pipelines.depthpass->addShader(getAssetPath() + "shaders/terrain_depthpass.frag.spv");
		pipelines.depthpass->create();
		// Depth pres pass pipeline for glTF models
		pipelines.depthpassTree = new Pipeline(device);
		pipelines.depthpassTree->setCreateInfo(pipelineCI);
		pipelines.depthpassTree->setVertexInputState(&vertexInputStateModelInstanced);
		pipelines.depthpassTree->setCache(pipelineCache);
		pipelines.depthpassTree->setLayout(depthPass.pipelineLayout);
		pipelines.depthpassTree->setRenderPass(depthPass.renderPass);
		pipelines.depthpassTree->addShader(getAssetPath() + "shaders/tree_depthpass.vert.spv");
		pipelines.depthpassTree->addShader(getAssetPath() + "shaders/tree_depthpass.frag.spv");
		pipelines.depthpassTree->create();
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{		
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.vsShared, sizeof(uboShared)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.vsWater, sizeof(uboWaterPlane)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.vsOffScreen, sizeof(uboShared)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.vsDebugQuad, sizeof(uboShared)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.terrain, sizeof(uboTerrain)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.sky, sizeof(uboShared)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &depthPass.uniformBuffer, sizeof(depthPass.ubo)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.CSM, sizeof(uboCSM)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.params, sizeof(UniformDataParams)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.vsShared.map());
		VK_CHECK_RESULT(uniformBuffers.vsWater.map());
		VK_CHECK_RESULT(uniformBuffers.vsOffScreen.map());
		VK_CHECK_RESULT(uniformBuffers.vsDebugQuad.map());
		VK_CHECK_RESULT(uniformBuffers.terrain.map());
		VK_CHECK_RESULT(uniformBuffers.sky.map());
		VK_CHECK_RESULT(depthPass.uniformBuffer.map());
		VK_CHECK_RESULT(uniformBuffers.CSM.map());
		VK_CHECK_RESULT(uniformBuffers.params.map());

		updateUniformBuffers();
		updateUniformBufferOffscreen();
		updateUniformParams();
	}

	void updateUniformParams()
	{
		uniformDataParams.shadows = renderShadows;
		uniformDataParams.fogColor = glm::vec4(heightMapSettings.fogColor[0], heightMapSettings.fogColor[1], heightMapSettings.fogColor[2], 1.0f);
		memcpy(uniformBuffers.params.mapped, &uniformDataParams, sizeof(UniformDataParams));
	}

	void updateUniformBuffers()
	{
		float radius = 50.0f;
		lightPos = glm::vec4(20.0f, -15.0f, -15.0f, 0.0f) * radius;
		lightPos = glm::vec4(-20.0f, -15.0f, -15.0f, 0.0f) * radius;
		lightPos = glm::vec4(-20.0f, -15.0f, 20.0f, 0.0f) * radius;
		// @todo
		lightPos = glm::vec4(20.0f, -10.0f, 20.0f, 0.0f);
		lightPos = glm::vec4(-48.0f, -40.0f, 46.0f, 0.0f);

		//float angle = glm::radians(timer * 360.0f);
		//lightPos = glm::vec4(cos(angle) * radius, -15.0f, sin(angle) * radius, 0.0f);

		uboTerrain.lightDir = glm::normalize(-lightPos);
		uboWaterPlane.lightDir = glm::normalize(-lightPos);
		uboShared.lightDir = glm::normalize(-lightPos);

		uboShared.projection = camera.matrices.perspective;
		uboShared.model = camera.matrices.view * glm::mat4(1.0f);

		// Mesh
		memcpy(uniformBuffers.vsShared.mapped, &uboShared, sizeof(uboShared));

		// Mirror
		uboWaterPlane.projection = camera.matrices.perspective;
		uboWaterPlane.model = camera.matrices.view * glm::mat4(1.0f);
		//uboWaterPlane.model = glm::translate(uboWaterPlane.model, glm::vec3(0.0f, 0.25f, 0.0f));
		uboWaterPlane.cameraPos = glm::vec4(camera.position, 0.0f);
		uboWaterPlane.time = sin(glm::radians(timer * 360.0f));
		memcpy(uniformBuffers.vsWater.mapped, &uboWaterPlane, sizeof(uboWaterPlane));

		// Debug quad
		uboShared.projection = glm::ortho(4.0f, 0.0f, 0.0f, 4.0f*(float)height / (float)width, -1.0f, 1.0f);
		uboShared.model = glm::mat4(1.0f);
		memcpy(uniformBuffers.vsDebugQuad.mapped, &uboShared, sizeof(uboShared));

		updateUniformBufferTerrain();
		updateUniformBufferCSM();

		// Sky
		uboSky.projection = camera.matrices.perspective;
		uboSky.model = glm::mat4(glm::mat3(camera.matrices.view));
		uniformBuffers.sky.copyTo(&uboSky, sizeof(uboSky));
	}

	void updateUniformBufferTerrain() {
		uboTerrain.projection = camera.matrices.perspective;
		uboTerrain.model = camera.matrices.view;
		uniformBuffers.terrain.copyTo(&uboTerrain, sizeof(uboTerrain));
	}

	void updateUniformBufferCSM() {
		for (auto i = 0; i < cascades.size(); i++) {
			depthPass.ubo.cascadeViewProjMat[i] = cascades[i].viewProjMatrix;
		}
		memcpy(depthPass.uniformBuffer.mapped, &depthPass.ubo, sizeof(depthPass.ubo));

		for (auto i = 0; i < cascades.size(); i++) {
			uboCSM.cascadeSplits[i] = cascades[i].splitDepth;
			uboCSM.cascadeViewProjMat[i] = cascades[i].viewProjMatrix;
		}
		uboCSM.inverseViewMat = glm::inverse(camera.matrices.view);
		uboCSM.lightDir = normalize(-lightPos);
		memcpy(uniformBuffers.CSM.mapped, &uboCSM, sizeof(uboCSM));
	}

	void updateUniformBufferOffscreen()
	{
		uboShared.projection = camera.matrices.perspective;
		uboShared.model = camera.matrices.view * glm::mat4(1.0f);
		uboShared.model = glm::scale(uboShared.model, glm::vec3(1.0f, -1.0f, 1.0f));
		memcpy(uniformBuffers.vsOffScreen.mapped, &uboShared, sizeof(uboShared));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();

		defaultDevice = vulkanDevice;
		defaultQueue = queue;

		// We try to get a transfer queue for background uploads
		if (vulkanDevice->queueFamilyIndices.graphics != vulkanDevice->queueFamilyIndices.transfer) {
			std::cout << "Using dedicated transfer queue for background uploads\n";
			vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.transfer, 0, &transferQueue);
		} else {
			transferQueue = queue;
		}

		hasExtMemoryBudget = vulkanDevice->extensionSupported("VK_EXT_memory_budget");

		loadAssets();
		generateTerrain();
		prepareOffscreen();
		prepareCSM();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		
		// @todo
		infiniteTerrain.viewerPosition = glm::vec2(camera.position.x, camera.position.z);
		infiniteTerrain.updateVisibleChunks(camera);
		
		prepared = true;
	}

	void buildCommandBuffer(int index) 
	{
		CommandBuffer* cb = commandBuffers[index];
		cb->begin();

		// CSM
		if (renderShadows) {
			drawCSM(cb);
		}

		// Refraction
		{
			cb->beginRenderPass(offscreenPass.renderPass, offscreenPass.refraction.frameBuffer);
			cb->setViewport(0.0f, 0.0f, (float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
			cb->setScissor(0, 0, offscreenPass.width, offscreenPass.height);
			drawScene(cb, SceneDrawType::sceneDrawTypeRefract);
			cb->endRenderPass();
		}

		// Reflection
		{
			cb->beginRenderPass(offscreenPass.renderPass, offscreenPass.reflection.frameBuffer);
			cb->setViewport(0.0f, 0.0f, (float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
			cb->setScissor(0, 0, offscreenPass.width, offscreenPass.height);
			drawScene(cb, SceneDrawType::sceneDrawTypeReflect);
			cb->endRenderPass();
		}

		// Scene
		{
			cb->beginRenderPass(renderPass, frameBuffers[index]);
			cb->setViewport(0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
			cb->setScissor(0, 0, width, height);
			drawScene(cb, SceneDrawType::sceneDrawTypeDisplay);

			if (debugDisplayReflection) {
				uint32_t val0 = 0;
				cb->bindDescriptorSets(pipelineLayouts.textured, { descriptorSets.debugquad }, 0);
				cb->bindPipeline(pipelines.debug);
				cb->updatePushConstant(pipelineLayouts.debug, 0, &val0);
				cb->draw(6, 1, 0, 0);
			}

			if (debugDisplayRefraction) {
				uint32_t val1 = 1;
				cb->bindDescriptorSets(pipelineLayouts.textured, { descriptorSets.debugquad }, 0);
				cb->bindPipeline(pipelines.debug);
				cb->updatePushConstant(pipelineLayouts.debug, 0, &val1);
				cb->draw(6, 1, 0, 0);
			}

			if (cascadeDebug.enabled) {
				const CascadePushConstBlock pushConst = { glm::vec4(0.0f), cascadeDebug.cascadeIndex };
				cb->bindDescriptorSets(cascadeDebug.pipelineLayout, { cascadeDebug.descriptorSet }, 0);
				cb->bindPipeline(cascadeDebug.pipeline);
				cb->updatePushConstant(cascadeDebug.pipelineLayout, 0, &pushConst);
				cb->draw(6, 1, 0, 0);
			}

			if (UIOverlay.visible) {
				drawUI(cb->handle);
			}

			cb->endRenderPass();
		}
		cb->end();
	}

	void updateMemoryBudgets() {
		if (std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - memoryBudget.lastUpdate).count() > 1000) {
			VkPhysicalDeviceMemoryBudgetPropertiesEXT physicalDeviceMemoryBudgetPropertiesEXT{};
			physicalDeviceMemoryBudgetPropertiesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
			VkPhysicalDeviceMemoryProperties2 physicalDeviceMemoryProperties2{};
			physicalDeviceMemoryProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
			physicalDeviceMemoryProperties2.pNext = &physicalDeviceMemoryBudgetPropertiesEXT;
			vkGetPhysicalDeviceMemoryProperties2(vulkanDevice->physicalDevice, &physicalDeviceMemoryProperties2);
			memoryBudget.heapCount = physicalDeviceMemoryProperties2.memoryProperties.memoryHeapCount;
			memcpy(memoryBudget.heapBudget, physicalDeviceMemoryBudgetPropertiesEXT.heapBudget, sizeof(VkDeviceSize) * VK_MAX_MEMORY_HEAPS);
			memcpy(memoryBudget.heapUsage, physicalDeviceMemoryBudgetPropertiesEXT.heapUsage, sizeof(VkDeviceSize) * VK_MAX_MEMORY_HEAPS);
		}
	}

	virtual void render()
	{
		if (!prepared) {
			return;
		}

		VulkanExampleBase::prepareFrame();

		buildCommandBuffer(currentBuffer);

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentBuffer]->handle;
		if (vulkanDevice->queueFamilyIndices.graphics == vulkanDevice->queueFamilyIndices.transfer) {
			// If we don't have a dedicated transfer queue, we need to make sure that the main and background threads don't use the (graphics) pipeline simultaneously
			std::lock_guard<std::mutex> guard(lock_guard);
		}
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();

		if (!paused || camera.updated)
		{
			updateCascades();
			updateUniformBuffers();
			updateUniformBufferOffscreen();
		}
		updateMemoryBudgets();
	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
		updateUniformBufferOffscreen();
		// @todo
		if (!fixFrustum) {
			frustum.update(camera.matrices.perspective * camera.matrices.view);
		}
		infiniteTerrain.viewerPosition = glm::vec2(camera.position.x, camera.position.z);
		infiniteTerrain.updateVisibleChunks(camera);
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		bool updateTerrain = false;

		/*ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
		ImGui::SetNextWindowPos(ImVec2(10, 10));
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Vulkan Example", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
		ImGui::TextUnformatted(title.c_str());
		ImGui::TextUnformatted(deviceProperties.deviceName);
		ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / lastFPS), lastFPS);

	#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 5.0f * UIOverlay.scale));
	#endif
		ImGui::PushItemWidth(110.0f * UIOverlay.scale);
		OnUpdateUIOverlay(&UIOverlay);
		ImGui::PopItemWidth();
	#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		ImGui::PopStyleVar();
	#endif*/

		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_None);
		ImGui::TextUnformatted("Vulkan infinite terrain");
		ImGui::TextUnformatted("2022 by Sascha Willems");
		ImGui::TextUnformatted(deviceProperties.deviceName);
		ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / lastFPS), lastFPS);
		if (overlay->header("Memory")) {
			for (int i = 0; i < memoryBudget.heapCount; i++) {
				const float divisor = 1024.0f * 1024.0f;
				ImGui::Text("Heap %i: %.2f / %.2f", i, (float)memoryBudget.heapUsage[i] / divisor, (float)memoryBudget.heapBudget[i] / divisor);
			}
		}
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Debugging", nullptr, ImGuiWindowFlags_None);
		overlay->checkBox("Fix frustum", &fixFrustum);
		overlay->checkBox("Wireframe", &displayWireFrame);
		overlay->checkBox("Waterplane", &displayWaterPlane);
		overlay->checkBox("Display reflection", &debugDisplayReflection);
		overlay->checkBox("Display refraction", &debugDisplayRefraction);
		overlay->checkBox("Display cascades", &cascadeDebug.enabled);
		if (cascadeDebug.enabled) {
			overlay->sliderInt("Cascade", &cascadeDebug.cascadeIndex, 0, SHADOW_MAP_CASCADE_COUNT - 1);
		}
		if (overlay->sliderFloat("Split lambda", &cascadeSplitLambda, 0.1f, 1.0f)) {
			updateCascades();
			updateUniformBuffers();
		}
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(30, 30), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Terrain", nullptr, ImGuiWindowFlags_None);
		overlay->text("%d chunks in memory", infiniteTerrain.terrainChunks.size());
		overlay->text("%d chunks visible", infiniteTerrain.getVisibleChunkCount());
		int currentChunkCoordX = round((float)infiniteTerrain.viewerPosition.x / (float)(heightMapSettings.mapChunkSize - 1));
		int currentChunkCoordY = round((float)infiniteTerrain.viewerPosition.y / (float)(heightMapSettings.mapChunkSize - 1));
		overlay->text("chunk coord x = %d / y =%d", currentChunkCoordX, currentChunkCoordY);
		overlay->text("cam x = %.2f / z =%.2f", camera.position.x, camera.position.z);
		overlay->text("cam yaw = %.2f / pitch =%.2f", camera.yaw, camera.pitch);
		ImGui::End();

		bool updateParamsReq = false;
		ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Render options", nullptr, ImGuiWindowFlags_None);
		updateParamsReq |= overlay->checkBox("Fog", &uniformDataParams.fog);
		updateParamsReq |= overlay->checkBox("Shadows", &renderShadows);
		if (updateParamsReq) {
			updateUniformParams();
		}
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Terrain layers", nullptr, ImGuiWindowFlags_None);
		for (uint32_t i = 0; i < TERRAIN_LAYER_COUNT; i++) {
			overlay->sliderFloat2(("##layer_x" + std::to_string(i)).c_str(), uboTerrain.layers[i].x, uboTerrain.layers[i].y, 0.0f, 1.0f);
		}
		ImGui::End();

		if (updateParamsReq) {
			vkQueueWaitIdle(queue);
			memcpy(uniformBuffers.params.mapped, &uniformDataParams, sizeof(UniformDataParams));
		}

		ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Terrain settings", nullptr, ImGuiWindowFlags_None);
		overlay->sliderInt("Seed", &heightMapSettings.seed, 0, 128);
		overlay->sliderFloat("Noise scale", &heightMapSettings.noiseScale, 0.0f, 128.0f);
		overlay->sliderFloat("Height scale", &heightMapSettings.heightScale, 0.1f, 64.0f);
		overlay->sliderFloat("Persistence", &heightMapSettings.persistence, 0.0f, 10.0f);
		overlay->sliderFloat("Lacunarity", &heightMapSettings.lacunarity, 0.0f, 10.0f);
		
		if (ImGui::ColorEdit4("Water color", heightMapSettings.waterColor)) {
			uboWaterPlane.color.r = heightMapSettings.waterColor[0];
			uboWaterPlane.color.g = heightMapSettings.waterColor[1];
			uboWaterPlane.color.b = heightMapSettings.waterColor[2];
		}

		if (ImGui::ColorEdit4("Fog color", heightMapSettings.fogColor)) {
			uniformDataParams.fogColor.r = heightMapSettings.fogColor[0];
			uniformDataParams.fogColor.g = heightMapSettings.fogColor[1];
			uniformDataParams.fogColor.b = heightMapSettings.fogColor[2];
			updateUniformParams();
		}

		overlay->comboBox("Tree type", &heightMapSettings.treeModelIndex, treeModels);
		overlay->sliderInt("Tree density", &heightMapSettings.treeDensity, 1, 64);
		overlay->sliderFloat("Min. tree size", &heightMapSettings.minTreeSize, 0.1f, heightMapSettings.maxTreeSize);
		overlay->sliderFloat("Max. tree size", &heightMapSettings.maxTreeSize, heightMapSettings.minTreeSize, 5.0f);
		//overlay->sliderInt("LOD", &heightMapSettings.levelOfDetail, 1, 6);
		if (overlay->button("Update heightmap")) {
			updateHeightmap(false);
		}
		if (overlay->comboBox("Load preset", &presetIndex, presets)) {
			heightMapSettings.loadFromFile(getAssetPath() + "presets/" + presets[presetIndex] + ".txt");
			loadSkySphere(heightMapSettings.skySphere);
			memcpy(uboTerrain.layers, heightMapSettings.textureLayers, sizeof(glm::vec4) * TERRAIN_LAYER_COUNT);
			infiniteTerrain.clear();
			updateHeightmap(false);
			viewChanged();
			updateUniformParams();
			uboWaterPlane.color.r = heightMapSettings.waterColor[0];
			uboWaterPlane.color.g = heightMapSettings.waterColor[1];
			uboWaterPlane.color.b = heightMapSettings.waterColor[2];
		}
		ImGui::End();
	}

	virtual void mouseMoved(double x, double y, bool& handled)
	{
		ImGuiIO& io = ImGui::GetIO();
		handled = io.WantCaptureMouse;
	}

	virtual void keyPressed(uint32_t key)
	{
		float m = (GetKeyState(VK_SHIFT) & 0x8000) ? -1.0f : 1.0f;
		if (key == 88) {
			camera.setPosition(camera.position + glm::vec3(m * 240.0f, 0.0f, 0.0f));
			viewChanged();
		}
		if (key == 89) {
			camera.setPosition(camera.position + glm::vec3(0.0f, 0.0f, m * 240.0f));
			viewChanged();
		}
	}

};

VULKAN_EXAMPLE_MAIN()
