#include "graphics/graphic_api.h"
#include "pch.h"

namespace Olaf {
    GraphicAPI Graphics::graphicAPI = GraphicAPI::SDL3;

    GraphicAPI get_graphic_API() {
        return Graphics::graphicAPI;
    }
}
