/*
* Heightmap terrain generator
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <glm/glm.hpp>

#include "vulkan/vulkan.h"
#include "VulkanDevice.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanTexture.hpp"
#include "Noise.h"
#include <ktx.h>
#include <ktxvulkan.h>

namespace vks 
{
	struct TerrainType
	{
		std::string name;
		float height;
		glm::vec3 color;
	};

	class HeightMap
	{
	private:
		uint16_t *heightdata;
		uint32_t dim;
		uint32_t scale;

		vks::VulkanDevice *device = nullptr;
		VkQueue copyQueue = VK_NULL_HANDLE;

		float* data = nullptr;
	public:
		enum Topology { topologyTriangles, topologyQuads };

		float heightScale = 4.0f;
		float uvScale = 1.0f;

		vks::Buffer vertexBuffer;
		vks::Buffer indexBuffer;

		vks::Texture2D texture{};

		struct Vertex {
			glm::vec3 pos;
			glm::vec3 normal;
			glm::vec2 uv;
			glm::vec4 color;
			glm::vec4 pad1;
			float terrainHeight;
		};

		size_t vertexBufferSize = 0;
		size_t indexBufferSize = 0;
		uint32_t indexCount = 0;

		std::vector<TerrainType> regions;

		glm::vec3 rgb(int r, int g, int b) {
			return glm::vec3((float)r, (float)g, (float)b) / 255.f;
		}

		HeightMap(vks::VulkanDevice *device, VkQueue copyQueue)
		{
			this->device = device;
			this->copyQueue = copyQueue;
			// @todo
			regions.resize(8);

			regions[0] = { "Water Deep",	0.3f,  rgb(25, 50, 191) };
			regions[1] = { "Water Shallow", 0.4f,  rgb(54, 100, 191) };
			regions[2] = { "Sand",			0.45f, rgb(207, 207, 124) };
			regions[3] = { "Grass",			0.55f, rgb(85, 151, 25) };
			regions[4] = { "Grass 2",		0.6f,  rgb(62, 105, 20) };
			regions[5] = { "Rock",			0.7f,  rgb(88, 64, 59) };
			regions[6] = { "Rock 2",		0.9f,  rgb(66, 53, 50) };
			regions[7] = { "snow",			1.0f,  rgb(212, 212, 212) };
		};

		~HeightMap()
		{
			vertexBuffer.destroy();
			indexBuffer.destroy();
			if (data) {
				delete[] data;
			}
			delete[] heightdata;
		}

		float getHeight(uint32_t x, uint32_t y)
		{
			glm::ivec2 rpos = glm::ivec2(x, y) * glm::ivec2(scale);
			rpos.x = std::max(0, std::min(rpos.x, (int)dim - 1));
			rpos.y = std::max(0, std::min(rpos.y, (int)dim - 1));
			rpos /= glm::ivec2(scale);
			return *(heightdata+ (rpos.x + rpos.y * dim) * scale) / 65535.0f * heightScale;
		}

		float getHeight2(uint32_t x, uint32_t y)
		{
			glm::ivec2 rpos = glm::ivec2(x, y) * glm::ivec2(scale);
			rpos.x = std::max(0, std::min(rpos.x, (int)dim - 1));
			rpos.y = std::max(0, std::min(rpos.y, (int)dim - 1));
			rpos /= glm::ivec2(scale);
			float height = *(data + (rpos.x + rpos.y * dim) * scale) * heightScale;
			return height;
		}

		float inverseLerp(float xx, float yy, float value)
		{
			return (value - xx) / (yy - xx);
		}

		void generate(glm::ivec2 size, int seed, float noiseScale, int octaves, float persistence, float lacunarity, glm::vec2 offset)
		{
			dim = size.x;
			texture.width = size.x;
			texture.height = size.y;

			float maxPossibleNoiseHeight = 0;
			float amplitude = 1;
			float frequency = 1;

			std::default_random_engine prng(seed);
			std::uniform_real_distribution<float> distribution(-100000, +100000);
			std::vector<glm::vec2> octaveOffsets(octaves);
			for (int32_t i = 0; i < octaves; i++) {
				float offsetX = distribution(prng) + offset.x;
				float offsetY = distribution(prng) - offset.y;
				octaveOffsets[i] = glm::vec2(offsetX, offsetY);
				maxPossibleNoiseHeight += amplitude;
				amplitude *= persistence;
			}

			if (data) {
				delete[] data;
			}

			const uint32_t heightDataSize = texture.width * texture.height * sizeof(float);
			data = new float[heightDataSize];

			PerlinNoise perlinNoise;

			float maxNoiseHeight = std::numeric_limits<float>::min();
			float minNoiseHeight = std::numeric_limits<float>::max();

			float halfWidth = size.x / 2.0f;
			float halfHeight = size.y / 2.0f;


			for (int32_t y = 0; y < size.x; y++) {
				for (int32_t x = 0; x < size.y; x++) {

					amplitude = 1;
					frequency = 1;

					float noiseHeight = 0;

					for (uint32_t i = 0; i < octaves; i++) {
						float sampleX = ((float)x - halfWidth + octaveOffsets[i].x) / noiseScale * frequency;
						float sampleY = ((float)y - halfHeight + octaveOffsets[i].y) / noiseScale * frequency;

						float perlinValue = perlinNoise.noise(sampleX, sampleY) * 2.0f - 1.0f;
						noiseHeight += perlinValue * amplitude;

						amplitude *= persistence;
						frequency *= lacunarity;
					}

					if (noiseHeight > maxNoiseHeight) {
						maxNoiseHeight = noiseHeight;
					}
					else if (noiseHeight < minNoiseHeight) {
						minNoiseHeight = noiseHeight;
					}

					data[x + y * texture.width] = noiseHeight;
				}
			}

			// Normalize
			for (size_t y = 0; y < size.y; y++) {
				for (size_t x = 0; x < size.x; x++) {
					// Local
					//data[x + y * texture.width] = inverseLerp(minNoiseHeight, maxNoiseHeight, data[x + y * texture.width]);
//					data[x + y * texture.width] = (data[x + y * texture.width] + 1.0f) / (2.0f * maxPossibleNoiseHeight / 1.5f);
					//data[x + y * texture.width] = data[x + y * texture.width] / 0.6;
					//data[x + y * texture.width] = glm::clamp(data[x + y * texture.width], 0.0f, std::numeric_limits<float>::max());
					data[x + y * texture.width] = inverseLerp(-3.0f, 0.6f, data[x + y * texture.width]);
				}
			}

			//const uint32_t colorTextureSize = texture.width * texture.height * sizeof(glm::vec4);
			//glm::vec4* colorTextureData = new glm::vec4[colorTextureSize];
			//for (size_t y = 0; y < size.y; y++) {
			//	for (size_t x = 0; x < size.x; x++) {
			//		float currentHeight = data[x + y * texture.width];
			//		for (size_t i = 0; i < regions.size(); i++) {
			//			if (currentHeight <= regions[i].height) {
			//				colorTextureData[x + y * texture.width] = glm::vec4(regions[i].color, 1.0f);
			//				break;
			//			}
			//		}
			//		//colorTextureData[x + y * texture.width] = glm::vec3(data[x + y * texture.width]);
			//	}
			//}

			//texture.fromBuffer(
			//	colorTextureData,
			//	colorTextureSize,
			//	VK_FORMAT_R32G32B32A32_SFLOAT,
			//	//VK_FORMAT_R32_SFLOAT,
			//	size.x,
			//	size.y,
			//	device,
			//	copyQueue,
			//	//VK_FILTER_LINEAR
			//	VK_FILTER_NEAREST
			//);
			
			//delete[] colorTextureData;
		}

		void generateMesh(glm::vec3 scale, Topology topology, int levelOfDetail)
		{
			// @todo: heightcurve (see E06:LOD)
			// @todo: two buffers, current and update, switch in cb once done (signal via flag)?

			int32_t width = dim;
			int32_t height = dim;
			float topLeftX = (float)(width - 1) / -2.0f;
			float topLeftZ = (float)(height - 1) / 2.0f;

			int meshSimplificationIncrement = std::max(levelOfDetail, 1) * 2;
			int verticesPerLine = (width - 1) / meshSimplificationIncrement + 1;

			Vertex* vertices = new Vertex[verticesPerLine * verticesPerLine];
			uint32_t* triangles = new uint32_t[(verticesPerLine - 1) * (verticesPerLine - 1) * 6];
			uint32_t triangleIndex = 0;
			uint32_t vertexIndex = 0;
			indexCount = (verticesPerLine - 1) * (verticesPerLine - 1) * 6;

			auto addTriangle = [&triangleIndex, triangles](int a, int b, int c) {
				triangles[triangleIndex] = a;
				triangles[triangleIndex+1] = b;
				triangles[triangleIndex+2] = c;
				triangleIndex += 3;
			};

			auto getHeight2 = [this, scale](int x, int y) {
				if (x < 0) { x = 0; }
				if (y < 0) { y = 0; }
				if (x > dim) { x = dim; }
				if (y > dim) { y = dim; }
				float height = data[x + y * texture.width] *abs(scale.y);
				if (height < 0.0f) {
					height = 0.0f;
				}
				return height;
			};

			for (int32_t y = 0; y < width; y += meshSimplificationIncrement) {
				for (int32_t x = 0; x < height; x += meshSimplificationIncrement) {
					float currentHeight = data[x + y * width];
					if (currentHeight < 0.0f) {
						currentHeight = 0.0f;
					}
					vertices[vertexIndex].pos.x = topLeftX + (float)x;
					vertices[vertexIndex].pos.y = currentHeight;
					vertices[vertexIndex].pos.z = topLeftZ - (float)y;
					vertices[vertexIndex].pos *= scale;
					vertices[vertexIndex].pos.y += 1.75f;
					vertices[vertexIndex].uv = glm::vec2((float)x / (float)width, (float)y / (float)height);
					vertices[vertexIndex].terrainHeight = currentHeight;

					float hL = getHeight2(x - 1, y);
					float hR = getHeight2(x + 1, y);
					float hD = getHeight2(x, y + 1);
					float hU = getHeight2(x, y - 1);
					glm::vec3 normalVector = glm::normalize(glm::vec3(hL - hR, -2.0f, hD - hU));
					vertices[vertexIndex].normal = normalVector;

					if ((x < width - 1) && (y < height - 1)) {
						addTriangle(vertexIndex, vertexIndex + verticesPerLine + 1, vertexIndex + verticesPerLine);
						addTriangle(vertexIndex + verticesPerLine + 1, vertexIndex, vertexIndex + 1);
					}
					vertexIndex++;
				}
			}

			VkDeviceSize vertexBufferSize = verticesPerLine * verticesPerLine * sizeof(Vertex);
			VkDeviceSize indexBufferSize = (verticesPerLine - 1) * (verticesPerLine - 1) * 6 * sizeof(uint32_t);

			// Create staging buffers
			vks::Buffer vertexStaging, indexStaging;
			device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertexStaging, vertexBufferSize, vertices);
			device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &indexStaging, indexBufferSize, triangles);
			// Device local (target) buffer
			device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, vertexBufferSize);
			device->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer, indexBufferSize);
			// Copy from staging buffers
			VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true, VK_QUEUE_TRANSFER_BIT);
			VkBufferCopy copyRegion = {};
			copyRegion.size = vertexBufferSize;
			vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertexBuffer.buffer, 1, &copyRegion);
			copyRegion.size = indexBufferSize;
			vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indexBuffer.buffer, 1, &copyRegion);
			device->flushCommandBuffer(copyCmd, copyQueue, true, VK_QUEUE_TRANSFER_BIT);

			vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
			vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
			vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
			vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);
		}

#if defined(__ANDROID__)
		void loadFromFile(const std::string filename, uint32_t patchsize, glm::vec3 scale, Topology topology, AAssetManager* assetManager)
#else
		void loadFromFile(const std::string filename, uint32_t patchsize, glm::vec3 scale, Topology topology)
#endif
		{
			assert(device);
			assert(copyQueue != VK_NULL_HANDLE);

			ktxResult result;
			ktxTexture* ktxTexture;
#if defined(__ANDROID__)
			AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
			assert(asset);
			size_t size = AAsset_getLength(asset);
			assert(size > 0);
			void *textureData = malloc(size);
			AAsset_read(asset, textureData, size);
			AAsset_close(asset);
			result = ktxTexture_CreateFromMemory(textureData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);
			free(textureData);
#else
			result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
#endif
			assert(result == KTX_SUCCESS);
			ktx_size_t ktxSize = ktxTexture_GetImageSize(ktxTexture, 0);
			ktx_uint8_t* ktxImage = ktxTexture_GetData(ktxTexture);
			dim = ktxTexture->baseWidth;
			heightdata = new uint16_t[dim * dim];
			memcpy(heightdata, ktxImage, ktxSize);
			this->scale = dim / patchsize;
			ktxTexture_Destroy(ktxTexture);

			// Generate vertices
			Vertex * vertices = new Vertex[patchsize * patchsize * 4];

			const float wx = 2.0f;
			const float wy = 2.0f;

			for (uint32_t x = 0; x < patchsize; x++) {
				for (uint32_t y = 0; y < patchsize; y++) {
					uint32_t index = (x + y * patchsize);
					vertices[index].pos[0] = (x * wx + wx / 2.0f - (float)patchsize * wx / 2.0f) * scale.x;
					vertices[index].pos[1] = 0.0f; // -getHeight(x, y) * scale.y + 1.0f;
					vertices[index].pos[2] = (y * wy + wy / 2.0f - (float)patchsize * wy / 2.0f) * scale.z;
					vertices[index].uv = glm::vec2((float)x / patchsize, (float)y / patchsize) * uvScale;
					vertices[index].color = glm::vec4(glm::vec3(getHeight(x, y)), 1.0f);
					// Normal
					float dx = getHeight(x < patchsize - 1 ? x + 1 : x, y) - getHeight(x > 0 ? x - 1 : x, y);
					if (x == 0 || x == patchsize - 1)
						dx *= 2.0f;
					float dy = getHeight(x, y < patchsize - 1 ? y + 1 : y) - getHeight(x, y > 0 ? y - 1 : y);
					if (y == 0 || y == patchsize - 1)
						dy *= 2.0f;
					glm::vec3 A = glm::vec3(1.0f, 0.0f, dx);
					glm::vec3 B = glm::vec3(0.0f, 1.0f, dy);
					glm::vec3 normal = (glm::normalize(glm::cross(A, B)) + 1.0f) * 0.5f;
					normal = (glm::normalize(glm::cross(A, B)));
					vertices[x + y * patchsize].normal = glm::vec3(normal.x, normal.y, normal.z);
				}
			}

			// Generate indices

			const uint32_t w = (patchsize - 1);
			uint32_t *indices;

			switch (topology)
			{
			// Indices for triangles
			case topologyTriangles:
			{
				indices = new uint32_t[w * w * 6];
				for (uint32_t x = 0; x < w; x++) {
					for (uint32_t y = 0; y < w; y++) {
						uint32_t index = (x + y * w) * 6;
						indices[index] = (x + y * patchsize);
						indices[index + 1] = indices[index] + patchsize;
						indices[index + 2] = indices[index + 1] + 1;
						indices[index + 3] = indices[index + 1] + 1;
						indices[index + 4] = indices[index] + 1;
						indices[index + 5] = indices[index];
					}
				}
				indexCount = (patchsize - 1) * (patchsize - 1) * 6;
				indexBufferSize = (w * w * 6) * sizeof(uint32_t);
				break;
			}
			// Indices for quad patches (tessellation)
			case topologyQuads:
			{
				indices = new uint32_t[w * w * 4];
				for (uint32_t x = 0; x < w; x++) {
					for (uint32_t y = 0; y < w; y++) {
						uint32_t index = (x + y * w) * 4;
						indices[index] = (x + y * patchsize);
						indices[index + 1] = indices[index] + patchsize;
						indices[index + 2] = indices[index + 1] + 1;
						indices[index + 3] = indices[index] + 1;
					}
				}
				indexCount = (patchsize - 1) * (patchsize - 1) * 4;
				indexBufferSize = (w * w * 4) * sizeof(uint32_t);
				break;
			}
			}

			assert(indexBufferSize > 0);

			vertexBufferSize = (patchsize * patchsize * 4) * sizeof(Vertex);

			// Generate Vulkan buffers

			vks::Buffer vertexStaging, indexStaging;

			// Create staging buffers
			device->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&vertexStaging,
				vertexBufferSize,
				vertices);

			device->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&indexStaging,
				indexBufferSize,
				indices);

			// Device local (target) buffer
			device->createBuffer(
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&vertexBuffer,
				vertexBufferSize);

			device->createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&indexBuffer,
				indexBufferSize);

			// Copy from staging buffers
			VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			VkBufferCopy copyRegion = {};

			copyRegion.size = vertexBufferSize;
			vkCmdCopyBuffer(
				copyCmd,
				vertexStaging.buffer,
				vertexBuffer.buffer,
				1,
				&copyRegion);

			copyRegion.size = indexBufferSize;
			vkCmdCopyBuffer(
				copyCmd,
				indexStaging.buffer,
				indexBuffer.buffer,
				1,
				&copyRegion);

			device->flushCommandBuffer(copyCmd, copyQueue, true);

			vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
			vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
			vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
			vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);
		}
		void draw(VkCommandBuffer cb) {
			const VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer.buffer, offsets);
			vkCmdBindIndexBuffer(cb, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cb, indexCount, 1, 0, 0, 0);
		}
	};
}
