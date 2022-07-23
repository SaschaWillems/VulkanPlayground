/*
 * Vulkan Playground
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include "HeightMapSettings.h"
#include "VulkanHeightmap.hpp"
#include "VulkanBuffer.hpp"
#include "CommandBuffer.hpp"
#include "VulkanContext.h"
#include <glm/glm.hpp>

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
	float alpha = 0.0f;

	TerrainChunk(glm::ivec2 coords, int size);
	void update();
	void updateHeightMap();
	float getHeight(int x, int y);
	void updateTrees(float minHeight);
	void draw(CommandBuffer* cb);
};