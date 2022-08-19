/*
 * Vulkan Playground
 *
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "HeightMapSettings.h"

HeightMapSettings heightMapSettings{};

void HeightMapSettings::loadFromFile(const std::string filename)
{
	std::ifstream file;
	file.open(filename);
	assert(file.is_open());
	std::string line;
	std::string key;
	float value;
	std::map<std::string, float> settings{};
	while (file.good()) {
		getline(file, line);
		std::istringstream ss(line);
		ss >> key >> value;
		settings[key] = value;
	}
	file.close();
	if (settings.find("noiseScale") != settings.end()) {
		noiseScale = settings["noiseScale"];
	}
	if (settings.find("seed") != settings.end()) {
		seed = (int)settings["seed"];
	}
	if (settings.find("heightScale") != settings.end()) {
		heightScale = settings["heightScale"];
	}
	if (settings.find("persistence") != settings.end()) {
		persistence = settings["persistence"];
	}
	if (settings.find("persistence") != settings.end()) {
		persistence = settings["persistence"];
	}
	if (settings.find("treeDensity") != settings.end()) {
		treeDensity = (int)settings["treeDensity"];
	}
	if (settings.find("grassDensity") != settings.end()) {
		grassDensity = (int)settings["grassDensity"];
	}
	if (settings.find("treeModelIndex") != settings.end()) {
		treeModelIndex = (int)settings["treeModelIndex"];
	}
	if (settings.find("minTreeSize") != settings.end()) {
		minTreeSize = settings["minTreeSize"];
	}
	if (settings.find("maxTreeSize") != settings.end()) {
		maxTreeSize = settings["maxTreeSize"];
	}
	if (settings.find("waterColor.r") != settings.end()) {
		waterColor[0] = settings["waterColor.r"] / 255.0f;
	}
	if (settings.find("waterColor.g") != settings.end()) {
		waterColor[1] = settings["waterColor.g"] / 255.0f;
	}
	if (settings.find("waterColor.b") != settings.end()) {
		waterColor[2] = settings["waterColor.b"] / 255.0f;
	}
	if (settings.find("fogColor.r") != settings.end()) {
		fogColor[0] = settings["fogColor.r"] / 255.0f;
	}
	if (settings.find("fogColor.g") != settings.end()) {
		fogColor[1] = settings["fogColor.g"] / 255.0f;
	}
	if (settings.find("fogColor.b") != settings.end()) {
		fogColor[2] = settings["fogColor.b"] / 255.0f;
	}
	if (settings.find("skySphere") != settings.end()) {
		const int idx = (int)settings["skySphere"];
		skySphere = "skysphere" + std::to_string(idx) + ".ktx";
	}
	for (int i = 0; i < TERRAIN_LAYER_COUNT; i++) {
		const std::string id = "textureLayers[" + std::to_string(i) + "]";
		if (settings.find(id + ".start") != settings.end()) {
			textureLayers[i].x = settings[id + ".start"];
		}
		if (settings.find(id + ".range") != settings.end()) {
			textureLayers[i].y = settings[id + ".range"];
		}
	}
}