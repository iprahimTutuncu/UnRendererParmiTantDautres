#pragma once

#include "options.h"
#include "system/window.h"
#include "system/system_manager.h"
#include "graphics/geometry.h"
#include "graphics/graphic_api.h"
#include "graphics/graphic_manager.h"

#include <memory>
#include <vector>
#include <thread>

namespace GTS
{
    class Engine
    {
    public:
        Engine();
        ~Engine();

        void init();
        void start();

        virtual void onInit() = 0;
        virtual void onStart() = 0;
        virtual void onExit() = 0;
        virtual void onSuspend() = 0;
        virtual void onResume() = 0;

        virtual void onInput(Options& options, const double& deltaTime, const std::vector<InputAction>& inputActions) = 0;
        virtual void onUpdate(Options& options, const double& deltaTime) = 0;
        virtual void onDraw(Options& options, GraphicsManager& graphicsManager, const double& deltaTime) = 0;

    private:

        void run();

        std::shared_ptr<Window> window;
        std::atomic<bool> isRunning{ false };
        std::shared_ptr<ControlSetting> controlSetting;
        std::shared_ptr<GraphicsManager> graphicsManager;
        std::shared_ptr<SystemManager> systemManager;
        std::thread graphicsThread;
        std::vector<Box> boxes;
        double targetFrameRate{ 60.0 };

        Options options;

        int prevWidth;
        int prevHeight;

    };
    
}
