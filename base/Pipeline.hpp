/*
* Vulkan pipeline abstraction class
*
* Copyright (C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <vector>
#include "vulkan/vulkan.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"
#include "PipelineLayout.hpp"
#include "RenderPass.hpp"

class Pipeline {
private:
	VkDevice device = VK_NULL_HANDLE;
	VkPipeline pso = VK_NULL_HANDLE;
	VkPipelineBindPoint bindPoint;
	PipelineLayout* layout = nullptr;
	RenderPass* renderPass;
	VkGraphicsPipelineCreateInfo pipelineCI;
	VkPipelineCache cache;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	std::vector<VkShaderModule> shaderModules;
public:
	Pipeline(VkDevice device) {
		this->device = device;
	}
	~Pipeline() {
		// @todo: destroy shader modules
		vkDestroyPipeline(device, pso, nullptr);
	}
	void create() {
		assert(layout);
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.layout = layout->handle;
		pipelineCI.renderPass = renderPass->handle;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, cache, 1, &pipelineCI, nullptr, &pso));
	}
	void addShader(std::string filename) {
		size_t extpos = filename.find('.');
		size_t extend = filename.find('.', extpos + 1);
		assert(extpos != std::string::npos);
		std::string ext = filename.substr(extpos + 1, extend - extpos - 1);
		VkShaderStageFlagBits shaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		if (ext == "vert") { shaderStage = VK_SHADER_STAGE_VERTEX_BIT; }
		if (ext == "frag") { shaderStage = VK_SHADER_STAGE_FRAGMENT_BIT; }
		assert(shaderStage != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);

		VkPipelineShaderStageCreateInfo shaderStageCI{};
		shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCI.stage = shaderStage;
		shaderStageCI.pName = "main";
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		shaderStageCI.module = vks::tools::loadShader(androidApp->activity->assetManager, filename.c_str(), device);
#else
		shaderStageCI.module = vks::tools::loadShader(filename.c_str(), device);
#endif
		assert(shaderStageCI.module != VK_NULL_HANDLE);
		shaderModules.push_back(shaderStageCI.module);
		shaderStages.push_back(shaderStageCI);
	}
	void setLayout(PipelineLayout* layout) {
		this->layout = layout;
	}
	void setRenderPass(RenderPass* renderPass) {
		this->renderPass = renderPass;
	}
	void setCreateInfo(VkGraphicsPipelineCreateInfo pipelineCI) {
		this->pipelineCI = pipelineCI;
		this->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	}
	void setVertexInputState(VkPipelineVertexInputStateCreateInfo* vertexInputStateCI) {
		this->pipelineCI.pVertexInputState = vertexInputStateCI;
	}
	void setCache(VkPipelineCache cache) {
		this->cache = cache;
	}
	VkPipelineBindPoint getBindPoint() {
		return bindPoint;
	}
	VkPipeline getHandle() {
		return pso;
	}
};
