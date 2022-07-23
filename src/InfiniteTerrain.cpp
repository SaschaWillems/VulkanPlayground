/*
 * Vulkan Playground
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "InfiniteTerrain.h"

InfiniteTerrain::InfiniteTerrain() {
	chunkSize = heightMapSettings.mapChunkSize - 1;
	chunksVisibleInViewDistance = round(heightMapSettings.maxChunkDrawDistance / chunkSize);
}

void InfiniteTerrain::updateViewDistance(float viewDistance) {
	chunksVisibleInViewDistance = round(viewDistance / chunkSize);
}

bool InfiniteTerrain::chunkPresent(glm::ivec2 coords) {
	for (auto& chunk : terrainChunks) {
		if (chunk->position.x == coords.x && chunk->position.y == coords.y) {
			return true;
		}
	}
	return false;
}

TerrainChunk* InfiniteTerrain::getChunk(glm::ivec2 coords) {
	for (auto& chunk : terrainChunks) {
		if (chunk->position.x == coords.x && chunk->position.y == coords.y) {
			return chunk;
		}
	}
	return nullptr;
}

int InfiniteTerrain::getVisibleChunkCount() {
	int count = 0;
	for (auto& chunk : terrainChunks) {
		if (chunk->visible) {
			count++;
		}
	}
	return count;
}

int InfiniteTerrain::getVisibleTreeCount() {
	int count = 0;
	for (auto& chunk : terrainChunks) {
		if (chunk->hasValidMesh && chunk->visible) {
			count += chunk->treeInstanceCount;
		}
	}
	return count;
}

bool InfiniteTerrain::updateVisibleChunks(vks::Frustum& frustum) {

	bool res = false;
	int currentChunkCoordX = (int)round(viewerPosition.x / (float)chunkSize);
	int currentChunkCoordY = (int)round(viewerPosition.y / (float)chunkSize);
	for (int yOffset = -chunksVisibleInViewDistance; yOffset <= chunksVisibleInViewDistance; yOffset++) {
		for (int xOffset = -chunksVisibleInViewDistance; xOffset <= chunksVisibleInViewDistance; xOffset++) {
			glm::ivec2 viewedChunkCoord = glm::ivec2(currentChunkCoordX + xOffset, currentChunkCoordY + yOffset);
			if (chunkPresent(viewedChunkCoord)) {
				TerrainChunk* chunk = getChunk(viewedChunkCoord);
				chunk->visible = true;
			}
			else {
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

void InfiniteTerrain::updateChunks() {
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

void InfiniteTerrain::clear() {
	vkQueueWaitIdle(VulkanContext::copyQueue);
	vkQueueWaitIdle(VulkanContext::graphicsQueue);
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

// @todo
void InfiniteTerrain::update(float deltaTime) {
	for (auto& chunk : terrainChunks) {
		if ((chunk->hasValidMesh) && (chunk->alpha < 1.0f)) {
			chunk->alpha += 2.0f * deltaTime;
		}
	}
}
