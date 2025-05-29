#include <olaf/system/window.h>

#include <olaf/platform/system/window_sdl3.h>
#include <olaf/system/log.h>
namespace Olaf {
    std::shared_ptr<Window> Olaf::Window::create(WindowAPI windowAPI) {
        if (windowAPI == WindowAPI::SDL3)
            return std::make_shared<WindowSDL3>();

        OLAF_ASSERT("In Olaf::Window::create(WindowAPI windowAPI), no windowAPI seem to be available.", true);
        return nullptr;
    }
}
