#include "pch.h"
#include "engine.h"
#include "system/window.h"
#include "system/event.h"
#include "system/control_setting.h"

#include "graphics/graphic_api.h"
#include <system/log_manager.h>

namespace Thsan
{
    Engine::Engine():
        graphicsManager(std::make_shared<GraphicsManager>()),
        systemManager(std::make_shared<SystemManager>())
    {

    }

    Engine::~Engine()
    {
        if (graphicsThread.joinable())
        {
            graphicsThread.join();
        }

        graphicsManager->close();
        systemManager->close();
    }

    void Engine::init()
    {
        isRunning = true;

        controlSetting = std::make_shared<ControlSetting>();
        systemManager->init(controlSetting);
        window = systemManager->getWindow();
        graphicsManager->init(window);


        graphicsThread = std::thread(&Engine::graphicsThreadFunction, this, &isRunning);

    }

    void Engine::start()
    {
        run();
    }


    void Engine::run()
    {
        while (isRunning && window->isRunning())
        {
            event();
            update();
            window->swapBuffers();
        }
    }

    void Engine::update()
    {
        systemManager->update();
    }

    void Engine::event()
    {
        // Poll events and handle them
        std::vector<Event> events = window->pollEvent();
        for (const auto& e : events)
        {

        }
    }

    void Engine::graphicsThreadFunction(std::atomic<bool>* isRunning)
    {
        while (*isRunning && window->isRunning())
        {
            graphicsManager->update();
        }
    }
}
