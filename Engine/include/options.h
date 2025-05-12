#pragma once

namespace Olaf
{
	struct WindowOptions
	{
		int screenWidth{};
		int screenHeight{};
	};

	struct RenderOptions
	{

	};

	struct Options

	{
		RenderOptions renderOptions;
		WindowOptions windowOptions;
	};
}