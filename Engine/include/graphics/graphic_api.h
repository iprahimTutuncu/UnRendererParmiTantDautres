#pragma once

namespace Thsan 
{
	enum class GraphicAPI 
	{
		None	= 0,
		OpenGL	= 1,
		Vulkan	= 2,
		SDL3	= 3
	};

	GraphicAPI get_graphic_API();

	class Graphics 
	{
	public:
		Graphics() = delete;
	private:
		static GraphicAPI graphicAPI;
		friend GraphicAPI get_graphic_API();
	};
}