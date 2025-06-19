#include "pch.h"
#include "graphics/graphic_api.h"

namespace GTS
{
    GraphicAPI Graphics::graphicAPI = GraphicAPI::SDL3;

    GraphicAPI get_graphic_API()
    {
        return Graphics::graphicAPI;
    }
}