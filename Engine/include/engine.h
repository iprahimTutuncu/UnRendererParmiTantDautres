#pragma once

#include "system/window.h"
#include "system/system_manager.h"
#include "graphics/graphic_api.h"
#include "graphics/graphic_manager.h"

#include <memory>
#include <vector>
#include <thread>

namespace Thsan
{
    class Engine
    {
    public:
        Engine();
        ~Engine();

        void init();
        void start();

    private:

        void run();
        void update();
        void event();

        void graphicsThreadFunction(std::atomic<bool>* isRunning);

        std::shared_ptr<Window> window;
        std::atomic<bool> isRunning{ false };
        std::shared_ptr<ControlSetting> controlSetting;
        std::shared_ptr<GraphicsManager> graphicsManager;
        std::shared_ptr<SystemManager> systemManager;
        std::thread graphicsThread;

    };
}
