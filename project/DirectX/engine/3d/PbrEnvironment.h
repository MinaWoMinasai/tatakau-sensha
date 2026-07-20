#pragma once

#include <cstdint>
#include <string>

#include "Struct.h"
#include "TextureManager.h"

class PbrEnvironment
{
public:
	enum class SourceType {
		ProceduralSky,
		TextureFile,
		SolidColor,
	};

	struct Settings {
		SourceType sourceType = SourceType::ProceduralSky;
		std::string texturePath;
		Vector3 solidColor = { 0.74f, 0.78f, 0.82f };
	};

	bool Apply(const Settings& settings)
	{
		if (applied_ && IsSameSettings(settings_, settings)) {
			return loadedFromSource_;
		}

		TextureManager* textureManager = TextureManager::GetInstance();
		bool loadedFromSource = false;
		switch (settings.sourceType) {
		case SourceType::TextureFile:
			loadedFromSource = textureManager->CreatePbrEnvironmentTexturesFromCubeMap(settings.texturePath);
			sourceLabel_ = loadedFromSource ? "texture convolution" : "texture rejected, procedural fallback";
			break;
		case SourceType::SolidColor:
			textureManager->CreatePbrSolidColorEnvironmentTextures(
				settings.solidColor.x,
				settings.solidColor.y,
				settings.solidColor.z);
			sourceLabel_ = "solid color";
			break;
		case SourceType::ProceduralSky:
		default:
			textureManager->CreatePbrEnvironmentTexturesFromCubeMap("");
			sourceLabel_ = "procedural clean sky";
			break;
		}

		environmentSrvIndex_ = textureManager->GetSrvIndex(
			TextureManager::GetPbrEnvironmentTexturePath(),
			TextureManager::TextureColorSpace::LinearData);
		settings_ = settings;
		loadedFromSource_ = loadedFromSource;
		applied_ = true;
		return loadedFromSource_;
	}

	uint32_t GetEnvironmentSrvIndex() const { return environmentSrvIndex_; }
	bool IsLoadedFromSource() const { return loadedFromSource_; }
	const std::string& GetSourceLabel() const { return sourceLabel_; }

private:
	static bool IsSameSettings(const Settings& lhs, const Settings& rhs)
	{
		return lhs.sourceType == rhs.sourceType &&
			lhs.texturePath == rhs.texturePath &&
			lhs.solidColor.x == rhs.solidColor.x &&
			lhs.solidColor.y == rhs.solidColor.y &&
			lhs.solidColor.z == rhs.solidColor.z;
	}

	Settings settings_;
	uint32_t environmentSrvIndex_ = 0;
	bool loadedFromSource_ = false;
	bool applied_ = false;
	std::string sourceLabel_ = "not applied";
};
