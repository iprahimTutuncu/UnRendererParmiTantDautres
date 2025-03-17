#include "pch.h"
#include "system/window.h"
#include "system/log.h"
#include "platform/system/window_sdl3.h"
namespace Thsan
{
    std::shared_ptr<Window> Thsan::Window::create(WindowAPI windowAPI)
    {
        if (windowAPI == WindowAPI::SDL3)
            return std::make_shared<WindowSDL3>();

        TS_ASSERT("In Thsan::Window::create(WindowAPI windowAPI), no windowAPI seem to be available.", true);
        return nullptr;
    }
}
