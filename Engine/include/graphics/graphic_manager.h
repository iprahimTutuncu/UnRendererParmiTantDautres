#pragma once
#include "options.h"

#include <functional>
#include <memory>

struct SDL_GPUDevice;

namespace Olaf {
    class Window;

    class GraphicsManager {

    public:
        void init(const Options& options, std::shared_ptr<Window> window, std::function<void(Options&, GraphicsManager&, const double&)> onDrawCallback);
        void update(Options& options, double dt);
        void close();

    private:
        std::function<void(Options&, GraphicsManager&, const double&)> onDraw;
        std::shared_ptr<Window> pWindow;
        SDL_GPUDevice* gpu;
    };
}
