#pragma once
#include "options.h"

#include "sprite.h"

#include "graphics/deferred_lighting_renderer.h"
#include "graphics/deferred_gbuffer_renderer.h"

#include <glm/glm.hpp>
#include <memory>
#include <functional>
#include <vector>

struct SDL_GPUDevice;
struct SDL_GPUBuffer;
struct SDL_GPUTexture;
struct SDL_GPUSampler;

namespace GTS
{
    class Window;
    class DeferredLightingRenderer;
    struct Box;
    struct Particles;

    class GraphicsManager
    {
    public:
        GraphicsManager() = default;
        ~GraphicsManager() = default;
    public:
        void init(const Options& options, std::shared_ptr<Window> window, std::function<void(Options&, GraphicsManager&, const double&)> onDrawCallback);
        void update(Options& options, double dt);
        void close();

        void add(const Box* box);
        void remove(const Box* box);

        void add(const Sprite* sprite);
        void remove(const Sprite* sprite);

    private:
        std::function<void(Options&, GraphicsManager&, const double&)> onDraw;
        std::shared_ptr<Window> pWindow;
        SDL_GPUDevice* gpu;
        SDL_GPUBuffer* vertexBuffer;
        SDL_GPUBuffer* indexBuffer;
        std::shared_ptr<SDL_GPUTexture> texture;

        std::shared_ptr<SDL_GPUTexture> gPosition;
        std::shared_ptr<SDL_GPUTexture> gNormal;
        std::shared_ptr<SDL_GPUTexture> gAlbedo;
        std::shared_ptr<SDL_GPUTexture> gDepth;

        std::vector<const Box*> boxes;
        Particles* particles;
        std::unique_ptr<DeferredLightingRenderer> deferredLightingRenderer;
        std::unique_ptr<DeferredGBufferRenderer> deferredGBufferRenderer;

    };
}
