#pragma once
#include <string>

namespace GTS
{
	struct WindowOptions
	{
		int screenWidth{};
		int screenHeight{};
	};

	struct TerrainOptions
	{
		float position[3];
		float normalBlurr[2];

		int terrainSize{};

		int normalBlurIteration{};
		float LOD{};
		float LODStartAt{};
		float angle{};
		float horizon{};
		float scaleHeight{};
		float spriteRenderDistance{};
		float terrainRenderDistance{};
		float shininess{};
		int blurType{};

		std::string albedoImagePath;
		std::string heightImagePath;
		std::string collisionHeightImagePath;
		std::string collisionMaskImagePath;
	};

	struct RenderOptions
	{
		TerrainOptions options;
	};

	struct Options

	{
		RenderOptions renderOptions;
		WindowOptions windowOptions;
	};
}