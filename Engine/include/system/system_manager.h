#pragma once
#include <memory>

namespace Thsan 
{
    class Window;
    class ControlSetting;

    class SystemManager
    {
    public:
        void init(std::shared_ptr<ControlSetting> controlSetting);
        void update();
        void close();

        std::shared_ptr<Window> getWindow();

    private:
        std::shared_ptr<Window> pWindow;
        std::shared_ptr<ControlSetting> pControlSetting;
    };
}

