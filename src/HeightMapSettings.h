/*
 * Vulkan Playground
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <glm/glm.hpp>

#define TERRAIN_LAYER_COUNT 6

class HeightMapSettings {
public:
	float noiseScale = 66.0f;
	int seed = 54;
	uint32_t width = 100;
	uint32_t height = 100;
	float heightScale = 28.5f;
	uint32_t octaves = 4;
	float persistence = 0.5f;
	float lacunarity = 1.87f;
	glm::vec2 offset = { 0,0 };
	int mapChunkSize = 241;
	int levelOfDetail = 1;
	int treeDensity = 30;
	int grassDensity = 256;
	float minTreeSize = 0.75f;
	float maxTreeSize = 1.5f;
	int treeModelIndex = 2;
	glm::vec4 textureLayers[TERRAIN_LAYER_COUNT];
	float waterColor[3];
	float fogColor[3] = { 0.47f, 0.5f, 0.67f };
	std::string skySphere = "skysphere01.ktx";

	int grassDim = 256;
	float grassScale = 0.5f;

	float waterPosition = 1.75f;

	float maxChunkDrawDistance = 360.0f; // 460.0f; @todo

	void loadFromFile(const std::string filename);
};

extern HeightMapSettings heightMapSettings;