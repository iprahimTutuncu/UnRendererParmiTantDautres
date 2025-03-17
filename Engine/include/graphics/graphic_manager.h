#pragma once
#include <memory>

namespace Thsan
{
    class Window;
    class GpuDevice;

    class GraphicsManager
    {

    public:
        void init(std::shared_ptr<Window> window);
        void update();
        void close();
    private:
        std::shared_ptr<Window> pWindow;
        std::shared_ptr<GpuDevice> pGpuDevice;
    };
}
