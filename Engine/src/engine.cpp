#include "pch.h"
#include "engine.h"
#include "system/window.h"
#include "system/event.h"
#include "system/control_setting.h"

#include "graphics/graphic_api.h"
#include <system/log_manager.h>

std::chrono::duration<double> frameDuration;

namespace Olaf {
    Engine::Engine()
        : graphicsManager(std::make_shared<GraphicsManager>())
        , systemManager(std::make_shared<SystemManager>()) {
    }

    Engine::~Engine() {
    }

    void Engine::init() {
        using namespace std::placeholders;
        isRunning = true;

        options.windowOptions.screenWidth = 1280;
        options.windowOptions.screenHeight = 768;

        prevWidth = options.windowOptions.screenWidth;
        prevHeight = options.windowOptions.screenHeight;

        controlSetting = std::make_shared<ControlSetting>();

        systemManager->init(options.windowOptions, controlSetting, [this](Olaf::Options& options, const double& deltaTime, const std::vector<Olaf::InputAction>& inputActions) {
            this->onInput(options, deltaTime, inputActions);
        });

        window = systemManager->getWindow();
        window->setSize(options.windowOptions.screenWidth, options.windowOptions.screenHeight);

        graphicsManager->init(options, window, [this](Olaf::Options& options, Olaf::GraphicsManager& graphicsManager, const double& deltaTime) {
            this->onDraw(options, graphicsManager, deltaTime);
        });

        onInit();
    }

    void Engine::start() {
        onStart();
        run();
    }

    void Engine::run() {
        frameDuration = std::chrono::duration<double>(1.0 / targetFrameRate);

        auto currentTime = std::chrono::high_resolution_clock::now();
        auto accumulator = std::chrono::duration<double>(0);

        while (isRunning) {
            auto newTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> frameTime = newTime - currentTime;
            currentTime = newTime;

            double deltaTime = frameTime.count();

            int w = options.windowOptions.screenWidth;
            int h = options.windowOptions.screenHeight;

            if (prevWidth != w || prevHeight != h) {
                window->setSize(w, h);
            }

            prevWidth = w;
            prevHeight = h;

            accumulator += frameTime;
            while (accumulator >= frameDuration) {
                onUpdate(options, deltaTime);
                systemManager->update(options, deltaTime);
                graphicsManager->update(options, deltaTime);

                accumulator -= frameDuration;
            }

            isRunning.store(window->isRunning(), std::memory_order_relaxed);
        }

        onExit();
        graphicsManager->close();
        systemManager->close();
    }

}
