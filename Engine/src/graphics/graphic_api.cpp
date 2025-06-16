#include <olaf/graphics/graphic_api.h>

namespace Olaf {
    GraphicAPI Graphics::graphicAPI = GraphicAPI::SDL3;

    GraphicAPI get_graphic_API() {
        return Graphics::graphicAPI;
    }
}
