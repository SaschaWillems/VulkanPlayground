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

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanTexture.hpp"
#include "VulkanglTFModel.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanHeightmap.hpp"

#define ENABLE_VALIDATION false

// Offscreen frame buffer properties
#define FB_DIM 1024
#define FB_COLOR_FORMAT VK_FORMAT_R8G8B8A8_UNORM

class VulkanExample : public VulkanExampleBase
{
public:
	bool debugDisplay = false;

	vks::HeightMap* heightMap;

	enum class SceneDrawType { sceneDrawTypeRefract, sceneDrawTypeReflect, sceneDrawTypeDisplay };

	struct Quad {
		uint32_t indexCount;
		vks::Buffer vertices;
		vks::Buffer indices;
	} quad;

	struct Textures {
		vks::Texture2D heightMap;
		vks::Texture2D skySphere;
		vks::Texture2D waterNormalMap;
		vks::Texture2DArray terrainArray;
	} textures;

	struct Models {
		vkglTF::Model skysphere;
		vkglTF::Model plane;
	} models;

	struct {
		vks::Buffer vsShared;
		vks::Buffer vsMirror;
		vks::Buffer vsOffScreen;
		vks::Buffer vsDebugQuad;
		vks::Buffer terrain;
		vks::Buffer sky;
	} uniformBuffers;

	struct UBO {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 lightPos = glm::vec4(0.0f, -10.0f, 10.0f, 1.0f);
	} uboShared, uboTerrain, uboSky;

	struct UBOMirror {
		glm::mat4 projection;
		glm::mat4 model;
		glm::vec4 cameraPos;
		float time;
	} uboMirror;

	struct {
		VkPipelineLayout textured;
		VkPipelineLayout shaded;
		VkPipelineLayout terrain;
		VkPipelineLayout sky;
	} pipelineLayouts;

	struct {
		VkPipeline debug;
		VkPipeline shaded;
		VkPipeline mirror;
		VkPipeline terrain;
		VkPipeline sky;
	} pipelines;

	struct {
		VkDescriptorSet offscreen;
		VkDescriptorSet mirror;
		VkDescriptorSet model;
		VkDescriptorSet debugQuad;
		VkDescriptorSet terrain;
		VkDescriptorSet skysphere;
	} descriptorSets;

	struct {
		VkDescriptorSetLayout textured;
		VkDescriptorSetLayout shaded;
		VkDescriptorSetLayout terrain;
		VkDescriptorSetLayout skysphere;
	} descriptorSetLayouts;

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkFramebuffer frameBuffer;		
		VkDeviceMemory mem;
		VkImage image;
		VkImageView view;
		VkDescriptorImageInfo descriptor;
	};
	struct OffscreenPass {
		int32_t width, height;
		FrameBufferAttachment reflection, refraction, depth;
		VkRenderPass renderPass;
		VkSampler sampler;
	} offscreenPass;

	glm::vec3 meshPos = glm::vec3(0.0f, -1.5f, 0.0f);
	glm::vec3 meshRot = glm::vec3(0.0f);

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Offscreen rendering";
		camera.type = Camera::CameraType::firstperson;
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 512.0f);
		camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
		//camera.setTranslation(glm::vec3(18.0f, 22.5f, 57.5f));
		camera.setTranslation(glm::vec3(0.0f, 1.0f, -6.0f));
		camera.movementSpeed = 7.5f;
		settings.overlay = true;
		camera.setRotation({ -27.0000000, 0.000000000, 0.000000000 });
		camera.setPosition({ -0.0402765162, 7.17239332, -15.7546043 });
		timerSpeed *= 0.05f;
		// The scene shader uses a clipping plane, so this feature has to be enabled
		enabledFeatures.shaderClipDistance = VK_TRUE;
		enabledFeatures.samplerAnisotropy = VK_TRUE;
	}

	~VulkanExample()
	{
		// Clean up used Vulkan resources 
		// Note : Inherited destructor cleans up resources stored in base class

		// Frame buffer

		// Color attachment
		vkDestroyImageView(device, offscreenPass.refraction.view, nullptr);
		vkDestroyImage(device, offscreenPass.refraction.image, nullptr);
		vkFreeMemory(device, offscreenPass.refraction.mem, nullptr);

		// Depth attachment
		vkDestroyImageView(device, offscreenPass.depth.view, nullptr);
		vkDestroyImage(device, offscreenPass.depth.image, nullptr);
		vkFreeMemory(device, offscreenPass.depth.mem, nullptr);

		vkDestroyRenderPass(device, offscreenPass.renderPass, nullptr);
		vkDestroySampler(device, offscreenPass.sampler, nullptr);
		//vkDestroyFramebuffer(device, offscreenPass.frameBuffer, nullptr);

		vkDestroyPipeline(device, pipelines.debug, nullptr);
		vkDestroyPipeline(device, pipelines.shaded, nullptr);
		vkDestroyPipeline(device, pipelines.mirror, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayouts.textured, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayouts.shaded, nullptr);

		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.shaded, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.textured, nullptr);

		// Uniform buffers
		uniformBuffers.vsShared.destroy();
		uniformBuffers.vsMirror.destroy();
		uniformBuffers.vsOffScreen.destroy();
		uniformBuffers.vsDebugQuad.destroy();
	}

	void createFrameBuffer(FrameBufferAttachment& target)
	{
		VkImageCreateInfo image = vks::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = FB_COLOR_FORMAT;
		image.extent.width = offscreenPass.width;
		image.extent.height = offscreenPass.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &target.image));

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, target.image, &memReqs);
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &target.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, target.image, target.mem, 0));

		VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
		colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorImageView.format = FB_COLOR_FORMAT;
		colorImageView.subresourceRange = {};
		colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorImageView.subresourceRange.baseMipLevel = 0;
		colorImageView.subresourceRange.levelCount = 1;
		colorImageView.subresourceRange.baseArrayLayer = 0;
		colorImageView.subresourceRange.layerCount = 1;
		colorImageView.image = target.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &target.view));

		target.descriptor = { offscreenPass.sampler, target.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	}

	// Setup the offscreen framebuffer for rendering the mirrored scene
	// The color attachment of this framebuffer will then be used to sample from in the fragment shader of the final pass
	void prepareOffscreen()
	{
		offscreenPass.width = FB_DIM;
		offscreenPass.height = FB_DIM;

		// Find a suitable depth format
		VkFormat fbDepthFormat;
		VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &fbDepthFormat);
		assert(validDepthFormat);

		/* Renderpass */

		std::array<VkAttachmentDescription, 2> attchmentDescriptions = {};
		// Color attachment
		attchmentDescriptions[0].format = FB_COLOR_FORMAT;
		attchmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attchmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attchmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attchmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attchmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attchmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// Depth attachment
		attchmentDescriptions[1].format = fbDepthFormat;
		attchmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attchmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attchmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attchmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attchmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attchmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;
		subpassDescription.pDepthStencilAttachment = &depthReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attchmentDescriptions.size());
		renderPassInfo.pAttachments = attchmentDescriptions.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassInfo.pDependencies = dependencies.data();
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &offscreenPass.renderPass));
		
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

		/* Color frame buffers */

		createFrameBuffer(offscreenPass.refraction);
		createFrameBuffer(offscreenPass.reflection);

		// Depth stencil attachment
		VkImageCreateInfo image = vks::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = fbDepthFormat;
		image.extent.width = offscreenPass.width;
		image.extent.height = offscreenPass.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &offscreenPass.depth.image));

		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, offscreenPass.depth.image, &memReqs);
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreenPass.depth.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, offscreenPass.depth.image, offscreenPass.depth.mem, 0));

		VkImageViewCreateInfo depthStencilView = vks::initializers::imageViewCreateInfo();
		depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		depthStencilView.format = fbDepthFormat;
		depthStencilView.flags = 0;
		depthStencilView.subresourceRange = {};
		depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		depthStencilView.subresourceRange.baseMipLevel = 0;
		depthStencilView.subresourceRange.levelCount = 1;
		depthStencilView.subresourceRange.baseArrayLayer = 0;
		depthStencilView.subresourceRange.layerCount = 1;
		depthStencilView.image = offscreenPass.depth.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &offscreenPass.depth.view));

		/* Framebuffers */

		VkImageView attachments[2];
		attachments[0] = offscreenPass.refraction.view;
		attachments[1] = offscreenPass.depth.view;

		VkFramebufferCreateInfo frameBufferCI = vks::initializers::framebufferCreateInfo();
		frameBufferCI.renderPass = offscreenPass.renderPass;
		frameBufferCI.attachmentCount = 2;
		frameBufferCI.pAttachments = attachments;
		frameBufferCI.width = offscreenPass.width;
		frameBufferCI.height = offscreenPass.height;
		frameBufferCI.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCI, nullptr, &offscreenPass.refraction.frameBuffer));

		attachments[0] = offscreenPass.reflection.view;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCI, nullptr, &offscreenPass.reflection.frameBuffer));
	}

	void drawScene(VkCommandBuffer cmdBuffer, SceneDrawType drawType)
	{
		// @todo: rename to localMat
		struct PushConst {
			glm::mat4 scale = glm::mat4(1.0f);
			glm::vec4 clipPlane = glm::vec4(0.0f);
		} pushConst;
		if (drawType == SceneDrawType::sceneDrawTypeReflect) {
			pushConst.scale = glm::scale(pushConst.scale, glm::vec3(1.0f, -1.0f, 1.0f));
		}

		switch (drawType) {
		case SceneDrawType::sceneDrawTypeRefract:
			pushConst.clipPlane = glm::vec4(0.0, 1.0, 0.0, 0.0);
			break;
		case SceneDrawType::sceneDrawTypeReflect:
			pushConst.clipPlane = glm::vec4(0.0, 1.0, 0.0, 0.0);
			break;
		}

		const VkDeviceSize offsets[1] = { 0 };
		
		// Skysphere
		if (drawType != SceneDrawType::sceneDrawTypeRefract) {
			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.sky);
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.sky, 0, 1, &descriptorSets.skysphere, 0, nullptr);
			models.skysphere.draw(cmdBuffer);
		}
		
		// Terrain
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.terrain);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.terrain, 0, 1, &descriptorSets.terrain, 0, nullptr);
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &heightMap->vertexBuffer.buffer, offsets);
		vkCmdBindIndexBuffer(cmdBuffer, heightMap->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdPushConstants(cmdBuffer, pipelineLayouts.terrain, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConst), &pushConst);
		vkCmdDrawIndexed(cmdBuffer, heightMap->indexCount, 1, 0, 0, 0);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		VkViewport viewport;
		VkRect2D scissor;
		VkDeviceSize offsets[1] = { 0 };

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			/*
				First pass: Render refraction
			*/	
			{
				VkClearValue clearValues[2];
				clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
				clearValues[1].depthStencil = { 1.0f, 0 };

				VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
				renderPassBeginInfo.renderPass = offscreenPass.renderPass;
				renderPassBeginInfo.framebuffer = offscreenPass.refraction.frameBuffer;
				renderPassBeginInfo.renderArea.extent.width = offscreenPass.width;
				renderPassBeginInfo.renderArea.extent.height = offscreenPass.height;
				renderPassBeginInfo.clearValueCount = 2;
				renderPassBeginInfo.pClearValues = clearValues;

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				VkViewport viewport = vks::initializers::viewport((float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

				VkRect2D scissor = vks::initializers::rect2D(offscreenPass.width, offscreenPass.height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				VkDeviceSize offsets[1] = { 0 };

				drawScene(drawCmdBuffers[i], SceneDrawType::sceneDrawTypeRefract);

				vkCmdEndRenderPass(drawCmdBuffers[i]);
			}

			/*
				First pass: Render refraction
			*/
			{
				VkClearValue clearValues[2];
				clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
				clearValues[1].depthStencil = { 1.0f, 0 };

				VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
				renderPassBeginInfo.renderPass = offscreenPass.renderPass;
				renderPassBeginInfo.framebuffer = offscreenPass.reflection.frameBuffer;
				renderPassBeginInfo.renderArea.extent.width = offscreenPass.width;
				renderPassBeginInfo.renderArea.extent.height = offscreenPass.height;
				renderPassBeginInfo.clearValueCount = 2;
				renderPassBeginInfo.pClearValues = clearValues;

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				VkViewport viewport = vks::initializers::viewport((float)offscreenPass.width, (float)offscreenPass.height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

				VkRect2D scissor = vks::initializers::rect2D(offscreenPass.width, offscreenPass.height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				VkDeviceSize offsets[1] = { 0 };

				drawScene(drawCmdBuffers[i], SceneDrawType::sceneDrawTypeReflect);

				vkCmdEndRenderPass(drawCmdBuffers[i]);
			}

			/*
				Final pass: Scene rendering using refraction and reflection textures
			*/
			{
				clearValues[0].color = defaultClearColor;
				clearValues[1].depthStencil = { 1.0f, 0 };

				VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
				renderPassBeginInfo.renderPass = renderPass;
				renderPassBeginInfo.framebuffer = frameBuffers[i];
				renderPassBeginInfo.renderArea.extent.width = width;
				renderPassBeginInfo.renderArea.extent.height = height;
				renderPassBeginInfo.clearValueCount = 2;
				renderPassBeginInfo.pClearValues = clearValues;

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

				VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				VkDeviceSize offsets[1] = { 0 };

				// Scene

				drawScene(drawCmdBuffers[i], SceneDrawType::sceneDrawTypeDisplay);

				// Reflection plane
				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.textured, 0, 1, &descriptorSets.mirror, 0, nullptr);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.mirror);
				models.plane.draw(drawCmdBuffers[i]);

				if (debugDisplay)
				{
					vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.textured, 0, 1, &descriptorSets.debugQuad, 0, nullptr);
					vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.debug);
					vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &quad.vertices.buffer, offsets);
					vkCmdBindIndexBuffer(drawCmdBuffers[i], quad.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(drawCmdBuffers[i], quad.indexCount, 1, 0, 0, 0);
				}

				drawUI(drawCmdBuffers[i]);

				vkCmdEndRenderPass(drawCmdBuffers[i]);
			}

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		models.skysphere.loadFromFile(getAssetPath() + "scenes/geosphere.gltf", vulkanDevice, queue);
		models.plane.loadFromFile(getAssetPath() + "scenes/plane.gltf", vulkanDevice, queue);
				
		// Textures
		std::string texFormatSuffix;
		VkFormat texFormat;
		// Get supported compressed texture format
		if (vulkanDevice->features.textureCompressionBC) {
			texFormatSuffix = "_bc3_unorm";
			texFormat = VK_FORMAT_BC3_UNORM_BLOCK;
		}
		else if (vulkanDevice->features.textureCompressionASTC_LDR) {
			texFormatSuffix = "_astc_8x8_unorm";
			texFormat = VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
		}
		else if (vulkanDevice->features.textureCompressionETC2) {
			texFormatSuffix = "_etc2_unorm";
			texFormat = VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
		}
		else {
			vks::tools::exitFatal("Device does not support any compressed texture format!", VK_ERROR_FEATURE_NOT_PRESENT);
		}
		textures.skySphere.loadFromFile(getAssetPath() + "textures/skysphere" + texFormatSuffix + ".ktx", texFormat, vulkanDevice, queue);
		textures.terrainArray.loadFromFile(getAssetPath() + "textures/terrain_texturearray" + texFormatSuffix + ".ktx", texFormat, vulkanDevice, queue);
		textures.heightMap.loadFromFile(getAssetPath() + "heightmap.ktx", VK_FORMAT_R16_UNORM, vulkanDevice, queue);
		textures.waterNormalMap.loadFromFile(getAssetPath() + "textures/water_normal_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);

		VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();

		// Setup a mirroring sampler for the height map
		vkDestroySampler(device, textures.heightMap.sampler, nullptr);
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		samplerInfo.addressModeV = samplerInfo.addressModeU;
		samplerInfo.addressModeW = samplerInfo.addressModeU;
		samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = (float)textures.heightMap.mipLevels;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &textures.heightMap.sampler));
		textures.heightMap.descriptor.sampler = textures.heightMap.sampler;

		// Setup a repeating sampler for the terrain texture layers
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
		const glm::vec3 scale = glm::vec3(0.15f * 0.25f, 4.0f, 0.15f * 0.25f);
		const uint32_t patchSize = 256;
		heightMap = new vks::HeightMap(vulkanDevice, queue);
#if defined(__ANDROID__)
		heightMap->loadFromFile(getAssetPath() + "heightmap.ktx", patchSize, androidApp->activity->assetManager, scale, vks::HeightMap::topologyTriangles);
#else
		heightMap->loadFromFile(getAssetPath() + "heightmap.ktx", patchSize, scale, vks::HeightMap::topologyTriangles);
#endif
	}

	void generateQuad()
	{
		std::vector<vkglTF::Model::Vertex> vertexBuffer = {
			{ { 1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
			{ { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
			{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
			{ { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } }
		};
		std::vector<uint32_t> indexBuffer = { 0,1,2, 2,3,0 };
		quad.indexCount = indexBuffer.size();
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer.size() * sizeof(vkglTF::Model::Vertex), &quad.vertices.buffer, &quad.vertices.memory, vertexBuffer.data()));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBuffer.size() * sizeof(uint32_t), &quad.indices.buffer, &quad.indices.memory, indexBuffer.data()));
	}

	void setupDescriptorPool()
	{
		// @todo
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6 * 10),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8 * 10)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo( poolSizes.size(), poolSizes.data(), 5 * 10);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	void setupDescriptorSetLayout()
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo;
		VkPipelineLayoutCreateInfo pipelineLayoutInfo;

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3)
		};

		// Shaded layouts (only use first layout binding)
		descriptorLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), 1);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &descriptorSetLayouts.shaded));

		pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.shaded, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayouts.shaded));

		// Textured layouts (use all layout bindings)
		descriptorLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &descriptorSetLayouts.textured));

		pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.textured, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayouts.textured));

		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4) + sizeof(glm::vec4), 0);

		// Terrain
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		};

		descriptorLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &descriptorSetLayouts.terrain));
		pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.terrain, 1);
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayouts.terrain));

		// Skysphere
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,1),
		};
		descriptorLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &descriptorSetLayouts.skysphere));
		pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.skysphere, 1);
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayouts.sky));
	}

	void setupDescriptorSet()
	{
		// Mirror plane descriptor set
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textured,1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.mirror));

		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.mirror, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.vsMirror.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.mirror, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &offscreenPass.refraction.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.mirror, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &offscreenPass.reflection.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.mirror, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &textures.waterNormalMap.descriptor),
		};
		vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

		// Debug quad
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.debugQuad));
		std::vector<VkWriteDescriptorSet> debugQuadWriteDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.debugQuad, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.vsDebugQuad.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.debugQuad, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &offscreenPass.reflection.descriptor),
			// @todo
			//vks::initializers::writeDescriptorSet(descriptorSets.mirror, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &offscreenPass.reflection.descriptor),
		};
		vkUpdateDescriptorSets(device, debugQuadWriteDescriptorSets.size(), debugQuadWriteDescriptorSets.data(), 0, NULL);

		// Shaded descriptor sets
		allocInfo.pSetLayouts = &descriptorSetLayouts.shaded;

		// Model
		// No texture
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.model));

		std::vector<VkWriteDescriptorSet> modelWriteDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.model, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.vsShared.descriptor)
		};
		vkUpdateDescriptorSets(device, modelWriteDescriptorSets.size(), modelWriteDescriptorSets.data(), 0, NULL);

		// Offscreen
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.offscreen));

		std::vector<VkWriteDescriptorSet> offScreenWriteDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.offscreen, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.vsOffScreen.descriptor)
		};
		vkUpdateDescriptorSets(device, offScreenWriteDescriptorSets.size(), offScreenWriteDescriptorSets.data(), 0, NULL);

		//

		// Terrain
		allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.terrain, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.terrain));
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.terrain, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.terrain.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.terrain, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.heightMap.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.terrain, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.terrainArray.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Skysphere
		allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.skysphere, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.skysphere));
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.skysphere, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.sky.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.skysphere, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.skySphere.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

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
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		// Solid rendering pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		shaderStages[0] = loadShader(getAssetPath() + "shaders/quad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/quad.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		// Vertex bindings and attributes
		const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(0, sizeof(vkglTF::Model::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
		};
		const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Model::Vertex, pos)),
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Model::Vertex, normal)),
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(vkglTF::Model::Vertex, uv))
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayouts.textured, renderPass, 0);
		pipelineCI.pVertexInputState = &vertexInputState;
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = shaderStages.size();
		pipelineCI.pStages = shaderStages.data();

		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		pipelineCI.layout = pipelineLayouts.textured;
		depthStencilState.depthTestEnable = VK_FALSE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.debug));
		depthStencilState.depthTestEnable = VK_TRUE;

		// Mirror
		shaderStages[0] = loadShader(getAssetPath() + "shaders/mirror.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/mirror.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.mirror));

		// Flip culling
		//rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

		// Phong shading pipelines
		pipelineCI.layout = pipelineLayouts.shaded;
		// Scene
		shaderStages[0] = loadShader(getAssetPath() + "shaders/phong.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/phong.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.shaded));

		// Terrain
		pipelineCI.layout = pipelineLayouts.terrain;
		shaderStages[0] = loadShader(getAssetPath() + "shaders/terrain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/terrain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.terrain));
		// Sky
		pipelineCI.layout = pipelineLayouts.sky;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		depthStencilState.depthWriteEnable = VK_FALSE;
		shaderStages[0] = loadShader(getAssetPath() + "shaders/skysphere.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/skysphere.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.sky));
		depthStencilState.depthWriteEnable = VK_TRUE;
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Mesh vertex shader uniform buffer block
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.vsShared,
			sizeof(uboShared)));

		// Mirror plane vertex shader uniform buffer block
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.vsMirror,
			sizeof(uboMirror)));

		// Offscreen vertex shader uniform buffer block 
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.vsOffScreen,
			sizeof(uboShared)));

		// Debug quad vertex shader uniform buffer block 
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.vsDebugQuad,
			sizeof(uboShared)));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.terrain, sizeof(uboShared)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.sky, sizeof(uboShared)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.vsShared.map());
		VK_CHECK_RESULT(uniformBuffers.vsMirror.map());
		VK_CHECK_RESULT(uniformBuffers.vsOffScreen.map());
		VK_CHECK_RESULT(uniformBuffers.vsDebugQuad.map());
		VK_CHECK_RESULT(uniformBuffers.terrain.map());
		VK_CHECK_RESULT(uniformBuffers.sky.map());

		updateUniformBuffers();
		updateUniformBufferOffscreen();
	}

	void updateUniformBuffers()
	{
		uboShared.projection = camera.matrices.perspective;
		uboShared.model = camera.matrices.view * glm::mat4(1.0f);

		// Mesh
		uboShared.model = glm::translate(uboShared.model, meshPos);
		uboShared.model = glm::rotate(uboShared.model, glm::radians(meshRot.y), glm::vec3(0.0f, 1.0f, 0.0f));
		memcpy(uniformBuffers.vsShared.mapped, &uboShared, sizeof(uboShared));

		// Mirror
		uboMirror.projection = camera.matrices.perspective;
		uboMirror.model = camera.matrices.view * glm::mat4(1.0f);
		uboMirror.cameraPos = glm::vec4(camera.position, 0.0f);
		uboMirror.time = sin(glm::radians(timer * 360.0f));
		memcpy(uniformBuffers.vsMirror.mapped, &uboMirror, sizeof(uboMirror));

		// Debug quad
		uboShared.projection = glm::ortho(4.0f, 0.0f, 0.0f, 4.0f*(float)height / (float)width, -1.0f, 1.0f);
		uboShared.model = glm::mat4(1.0f);
		memcpy(uniformBuffers.vsDebugQuad.mapped, &uboShared, sizeof(uboShared));

		// Terrain
		uboTerrain.projection = camera.matrices.perspective;
		uboTerrain.model = camera.matrices.view;
		uniformBuffers.terrain.copyTo(&uboTerrain, sizeof(uboTerrain));

		// Sky
		uboSky.projection = camera.matrices.perspective;
		uboSky.model = glm::mat4(glm::mat3(camera.matrices.view));
		uniformBuffers.sky.copyTo(&uboSky, sizeof(uboSky));
	}

	void updateUniformBufferOffscreen()
	{
		uboShared.projection = camera.matrices.perspective;
		uboShared.model = camera.matrices.view * glm::mat4(1.0f);
		uboShared.model = glm::rotate(uboShared.model, glm::radians(meshRot.y), glm::vec3(0.0f, 1.0f, 0.0f));
		uboShared.model = glm::scale(uboShared.model, glm::vec3(1.0f, -1.0f, 1.0f));
		uboShared.model = glm::translate(uboShared.model, meshPos);
		memcpy(uniformBuffers.vsOffScreen.mapped, &uboShared, sizeof(uboShared));
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		// Command buffer to be sumitted to the queue
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

		// Submit to queue
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VulkanExampleBase::submitFrame();
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		generateQuad();
		generateTerrain();
		prepareOffscreen();
		prepareUniformBuffers();
		setupDescriptorSetLayout();
		preparePipelines();
		setupDescriptorPool();
		setupDescriptorSet();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
		if (!paused)
		{
			meshRot.y += frameTimer * 10.0f;
			updateUniformBuffers();
			updateUniformBufferOffscreen();
		}
	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
		updateUniformBufferOffscreen();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			if (overlay->checkBox("Display render target", &debugDisplay)) {
				buildCommandBuffers();
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()
