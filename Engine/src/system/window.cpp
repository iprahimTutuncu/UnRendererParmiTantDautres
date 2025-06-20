#include "window.h"
#include "log.h"

#include "../platform/system/window_sdl3.h"

namespace GTS {
    std::shared_ptr<Window> GTS::Window::create(WindowAPI windowAPI) {
        if (windowAPI == WindowAPI::SDL3)
            return std::make_shared<WindowSDL3>();

        GTS_ASSERT("In GTS::Window::create(WindowAPI windowAPI), no windowAPI seem to be available.", true);
        return nullptr;
    }
}
