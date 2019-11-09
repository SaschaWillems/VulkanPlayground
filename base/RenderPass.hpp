/*
* Render pass abstraction class
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

class RenderPass {
private:
	VkDevice device;
	int32_t width;
	int32_t height;
	VkFramebuffer framebuffer;
	std::vector<VkAttachmentDescription> attachmentDescriptions;
	std::vector<VkSubpassDependency> subpassDependencies;
	std::vector<VkSubpassDescription> subpassDescriptions;
	std::vector<VkClearValue> clearValues;
public:
	VkRenderPass handle;
	RenderPass(VkDevice device) {
		this->device = device;
	}
	~RenderPass() {
		// @todo
	}
	void create() {
		VkRenderPassCreateInfo CI{};
		CI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		CI.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
		CI.pAttachments = attachmentDescriptions.data();
		CI.subpassCount = static_cast<uint32_t>(subpassDescriptions.size());
		CI.pSubpasses = subpassDescriptions.data();
		CI.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
		CI.pDependencies = subpassDependencies.data();
		VK_CHECK_RESULT(vkCreateRenderPass(device, &CI, nullptr, &handle));
	}
	VkRenderPassBeginInfo getBeginInfo() {
		VkRenderPassBeginInfo beginInfo = vks::initializers::renderPassBeginInfo();
		beginInfo.renderPass = handle;
		beginInfo.renderArea.offset.x = 0;
		beginInfo.renderArea.offset.y = 0;
		beginInfo.framebuffer = framebuffer;
		beginInfo.renderArea.extent.width = width;
		beginInfo.renderArea.extent.height = height;
		beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		beginInfo.pClearValues = clearValues.data();
		return beginInfo;
	}
	void setDimensions(int32_t width, int32_t height) {
		this->height = height;
		this->width = width;
	}
	void setFrameBuffer(VkFramebuffer framebuffer) {
		this->framebuffer = framebuffer;
	}
	void setColorClearValue(uint32_t index, std::array<float, 4> values) {
		if (index + 1 > clearValues.size()) {
			clearValues.resize(index + 1);
		}
		memcpy(clearValues[index].color.float32, values.data(), sizeof(float) * 4);
	}
	void setDepthStencilClearValue(uint32_t index, float depth, uint32_t stencil) {
		if (index + 1> clearValues.size()) {
			clearValues.resize(index + 1);
		}
		clearValues[index].depthStencil.depth = depth;
		clearValues[index].depthStencil.stencil = stencil;
	}
	void addAttachmentDescription(VkAttachmentDescription description) {
		attachmentDescriptions.push_back(description);
	}
	void addSubpassDependency(VkSubpassDependency dependency) {
		subpassDependencies.push_back(dependency);
	}
	void addSubpassDescription(VkSubpassDescription description) {
		subpassDescriptions.push_back(description);
	}
};