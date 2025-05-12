#pragma once
#include "control_setting.h"
#include "options.h"

#include <memory>
#include <functional>

namespace Olaf 
{
    class Window;
    class ControlSetting;

    class SystemManager
    {
    public:
        void init(WindowOptions& options, std::shared_ptr<ControlSetting> controlSetting, std::function<void(Options&, const double&, const std::vector<InputAction>&)> onInputCallback);
        void update(Options& option, double dt);
        void close();

        std::shared_ptr<Window> getWindow();
    private:

        std::function<void(Options&, const double&, const std::vector<InputAction>&)> onInput;
        std::shared_ptr<Window> pWindow;
        std::shared_ptr<ControlSetting> pControlSetting;
    };
}

