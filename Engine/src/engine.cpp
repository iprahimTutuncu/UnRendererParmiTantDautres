#include "pch.h"

#define GLM_ENABLE_EXPERIMENTAL

#include "engine.h"
#include "system/window.h"
#include "system/event.h"
#include "system/control_setting.h"

#include "graphics/graphic_api.h"
#include <system/log_manager.h>
#include <SDL3/SDL_timer.h>
#include <glm/gtx/transform.hpp>

std::chrono::duration<double> frameDuration;

namespace Olaf
{
    Engine::Engine():
        graphicsManager(std::make_shared<GraphicsManager>()),
        systemManager(std::make_shared<SystemManager>())
    {

    }

    Engine::~Engine()
    {

    }

    void Engine::init()
    {
        using namespace std::placeholders;
        isRunning = true;

        options.windowOptions.screenWidth = 1280;
        options.windowOptions.screenHeight = 768;

        prevWidth = options.windowOptions.screenWidth;
        prevHeight = options.windowOptions.screenHeight;

        controlSetting = std::make_shared<ControlSetting>();
        controlSetting->add(Key::kAcHome, InputState::isDoubleClick, InputAction::action);

        systemManager->init(options.windowOptions, controlSetting, [this](Olaf::Options& options, const double& deltaTime, const std::vector<Olaf::InputAction>& inputActions)
            {
                this->onInput(options, deltaTime, inputActions);
            });

        window = systemManager->getWindow();
        window->setSize(options.windowOptions.screenWidth, options.windowOptions.screenHeight);


        graphicsManager->init(options, window, [this](Olaf::Options& options, Olaf::GraphicsManager& graphicsManager, const double& deltaTime)
            {
                this->onDraw(options, graphicsManager, deltaTime);
            });


        boxes.push_back({ glm::vec3(-1.0f), glm::vec3(1.0f) });
        boxes.push_back({ glm::vec3(5.0f, -5.5f, -0.5f), glm::vec3(8.0f, -3.0f, 0.5f) });
        boxes.push_back({ glm::vec3(-4.0f, -1.5f, -0.3f), glm::vec3(-3.0f, 1.5f, 0.3f) });
        boxes.push_back({ glm::vec3(-0.75f, 2.0f, -0.75f), glm::vec3(0.75f, 2.5f, 0.75f) });
        boxes.push_back({ glm::vec3(1.0f, -1.0f, 4.0f), glm::vec3(3.0f, 1.0f, 4.2f) });

        for (auto& box : boxes) {
            graphicsManager->add(&box);
        }

        onInit();
    }

    void Engine::start()
    {
        onStart();
        run();
    }


    void Engine::run()
    {
        const double targetFrameMS = 1000.0 / static_cast<double>(targetFrameRate);

        Uint64 frameStartMS = SDL_GetTicks();

        while (isRunning)
        {
            Uint64 newTimeMS = SDL_GetTicks();
            double deltaTime = static_cast<double>(newTimeMS - frameStartMS) / 1000.0;
            frameStartMS = newTimeMS;

            int w = options.windowOptions.screenWidth;
            int h = options.windowOptions.screenHeight;

            if (prevWidth != w || prevHeight != h)
            {
                window->setSize(w, h);
            }

            prevWidth = w;
            prevHeight = h;

            onUpdate(options, deltaTime);
            systemManager->update(options, deltaTime);
            graphicsManager->update(options, deltaTime);

            isRunning.store(window->isRunning(), std::memory_order_relaxed);

            Uint64 frameEndMS = SDL_GetTicks();
            double elapsedMS = static_cast<double>(frameEndMS - frameStartMS);

            if (elapsedMS < targetFrameMS)
            {
                SDL_Delay(static_cast<Uint32>(targetFrameMS - elapsedMS));
            }
        }

        onExit();
        graphicsManager->close();
        systemManager->close();
    }


}
