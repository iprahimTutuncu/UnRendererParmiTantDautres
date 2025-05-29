#include <olaf/system/system_manager.h>

#include <olaf/options.h>
#include <olaf/system/event.h>
#include <olaf/system/log_manager.h>
#include <olaf/system/window.h>

namespace Olaf {
    void Olaf::SystemManager::init(WindowOptions& options, std::shared_ptr<ControlSetting> controlSetting, std::function<void(Options&, const double&, const std::vector<InputAction>&)> onInputCallback) {
        LogManager::init();

        pWindow = Window::create(WindowAPI::SDL3);

        if (!pWindow->init(options.screenWidth, options.screenHeight, "Olaf Engine")) {
            return;
        }
        pControlSetting = controlSetting;
        this->onInput = onInputCallback;

        pWindow->enableEventForHUD();
    }

    void Olaf::SystemManager::update(Options& options, double dt) {
        std::vector<Event> events = pWindow->pollEvent();

        for (Event e : events)
            pControlSetting->handleInput(e);

        pControlSetting->updateInput();

        if (onInput) onInput(options, dt, pControlSetting->getInput());
    }

    void SystemManager::close() {
        if (pWindow)
            pWindow->close();

        LogManager::shutdown();
    }

    std::shared_ptr<Window> SystemManager::getWindow() {
        return pWindow;
    }
}
