#include "pch.h"
#include "graphics/graphic_api.h"

namespace Thsan
{
    GraphicAPI Graphics::graphicAPI = GraphicAPI::Vulkan;

    GraphicAPI get_graphic_API()
    {
        return Graphics::graphicAPI;
    }
}