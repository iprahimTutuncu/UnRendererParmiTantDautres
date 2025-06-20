#define GLM_ENABLE_EXPERIMENTAL

#include "engine.h"
#include "ressource_manager/sampler_manager.h"
#include "ressource_manager/texture_manager.h"
#include "ressource_manager/shader_manager.h"
#include "ressource_manager/sampler_manager.h"

#include <SDL3/SDL_timer.h>
#include <glm/gtx/transform.hpp>

#include <memory>

namespace GTS {
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
        controlSetting->add(Key::kAcHome, InputState::isDoubleClick, InputAction::action);

        systemManager->init(options.windowOptions, controlSetting, [this](GTS::Options& options, const double& deltaTime, const std::vector<GTS::InputAction>& inputActions) {
            this->onInput(options, deltaTime, inputActions);
        });

        window = systemManager->getWindow();
        window->setSize(options.windowOptions.screenWidth, options.windowOptions.screenHeight);

        Ressource::ShaderManager::init(window);
        Ressource::SamplerManager::init(window);
        Ressource::TextureManager::init(window);

        graphicsManager->init(options, window, [this](GTS::Options& options, GTS::GraphicsManager& graphicsManager, const double& deltaTime) {
            this->onDraw(options, graphicsManager, deltaTime);
        });

        // Add initial manually specified boxes
        boxes.push_back({ glm::vec3(-1.0f), glm::vec3(1.0f) });

        // Register all boxes with the graphics manager
        for (auto& box : boxes) {
            graphicsManager->add(&box);
        }
        onInit();
    }

    void Engine::start() {
        onStart();
        run();
    }

    void Engine::run() {
        const double targetFrameMS = 1000.0 / static_cast<double>(targetFrameRate);

        Uint64 frameStartMS = SDL_GetTicks();

        while (isRunning) {
            Uint64 newTimeMS = SDL_GetTicks();
            double deltaTime = static_cast<double>(newTimeMS - frameStartMS) / 1000.0;
            frameStartMS = newTimeMS;

            int w = options.windowOptions.screenWidth;
            int h = options.windowOptions.screenHeight;

            if (prevWidth != w || prevHeight != h) {
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

            if (elapsedMS < targetFrameMS) {
                SDL_Delay(static_cast<Uint32>(targetFrameMS - elapsedMS));
            }
        }

        onExit();
        graphicsManager->close();
        systemManager->close();
    }

}
