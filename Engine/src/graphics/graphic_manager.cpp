#include "graphic_manager.h"

#include "geometry.h"
#include "image.h"

#include "../options.h"
#include "../ressource_manager/sampler_manager.h"
#include "../ressource_manager/shader_manager.h"
#include "../ressource_manager/texture_manager.h"
#include "../system/log.h"
#include "../system/window.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gpu.h>
#include <glm/ext.hpp>
#include <stb_image/stb_image.h>

#include <random>

namespace GTS {

    void GraphicsManager::init(const Options& options, std::shared_ptr<Window> window, std::function<void(Options&, GraphicsManager&, const double&)> onDrawCallback) {
        onDraw = onDrawCallback;
        pWindow = window;

        GpuHandle handle = pWindow->getGpuDevice();
        if (get_graphic_API() == GraphicAPI::SDL3) {
            gpu = handle.as<SDL_GPUDevice>();
            SDL_Window* window = pWindow->getWindow().as<SDL_Window>();

            int width, height;
            SDL_GetWindowSize(window, &width, &height);

            int w = options.windowOptions.screenWidth;
            int h = options.windowOptions.screenHeight;

            gPosition = Ressource::TextureManager::createRenderTarget("gPosition", w, h, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
            gNormal = Ressource::TextureManager::createRenderTarget("gNormal", w, h, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT);
            gAlbedo = Ressource::TextureManager::createRenderTarget("gAlbedo", w, h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
            gDepth = Ressource::TextureManager::createRenderTarget("gDepth", w, h, SDL_GPU_TEXTUREFORMAT_D24_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);

            deferredGBufferRenderer = std::make_unique<DeferredGBufferRenderer>(gpu, pWindow);
            deferredLightingRenderer = std::make_unique<DeferredLightingRenderer>(gpu, pWindow);

            // Set up GBuffer textures for the renderer
            GBufferTextures gBuffer;
            gBuffer.position = gPosition;
            gBuffer.normal = gNormal;
            gBuffer.albedo = gAlbedo;
            gBuffer.depth = gDepth;

            deferredGBufferRenderer->setGBufferOutput(gBuffer);
            deferredLightingRenderer->setGBufferTextures(gBuffer);

            const int GRID_SIZE = 8; // Number of particles per dimension
            const float SPACING = 2.0f; // Distance between particles in the grid
            const int NUM_PARTICLES = GRID_SIZE * GRID_SIZE * GRID_SIZE;

            particles = new Particles {};
            particles->data.reserve(NUM_PARTICLES);

            // Random number generator only for colors now
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);

            for (int x = 0; x < GRID_SIZE; ++x) {
                for (int y = 0; y < GRID_SIZE; ++y) {
                    for (int z = 0; z < GRID_SIZE; ++z) {
                        Particle p;

                        // Grid position centered around origin
                        p.position = glm::vec4(
                            (x - GRID_SIZE / 2) * SPACING,
                            (y - GRID_SIZE / 2) * SPACING,
                            (z - GRID_SIZE / 2) * SPACING,
                            1.f);

                        // Random color
                        p.color = glm::vec4(
                            colorDist(gen),
                            colorDist(gen),
                            colorDist(gen),
                            1.f);

                        particles->data.emplace_back(p);
                    }
                }
            }

            Camera cam;
            cam.fov = 50.f;
            deferredGBufferRenderer->setCamera(cam);
            deferredGBufferRenderer->setParticles(*particles);
        }
    }

    void GraphicsManager::update(Options& options, double dt) {

        if (get_graphic_API() == GraphicAPI::SDL3) {
            auto cmdbuf = SDL_AcquireGPUCommandBuffer(gpu);
            if (cmdbuf == NULL) {
                GTS_ERROR("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
            }

            SDL_GPUTexture* swapchainTexture;

            if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, pWindow->getWindow().as<SDL_Window>(), &swapchainTexture, nullptr, nullptr)) {
                GTS_ERROR("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
            }

            if (swapchainTexture != NULL) {
                deferredGBufferRenderer->setParticles(*particles);
                deferredGBufferRenderer->render(cmdbuf, boxes);
                deferredLightingRenderer->renderToTexture(cmdbuf, swapchainTexture, DeferredLightingRenderer::DisplayMode::Position);
            }
            SDL_SubmitGPUCommandBuffer(cmdbuf);

            if (this->onDraw) this->onDraw(options, *this, dt);
        }
    }

    void GraphicsManager::close() {
    }

    void GraphicsManager::add(const Box* box) {
        boxes.push_back(box);
    }

    void GraphicsManager::remove(const Box* box) {
        auto it = std::find(boxes.begin(), boxes.end(), box);
        if (it != boxes.end()) {
            boxes.erase(it);
        }
    }

    void GraphicsManager::add(const Sprite* sprite) {
    }

    void GraphicsManager::remove(const Sprite* sprite) {
    }

};
