#include <olaf/graphics/graphic_manager.h>

#include <olaf/options.h>
#include <olaf/system/log.h>
#include <olaf/system/window.h>

#include <SDL3/SDL_gpu.h>

void Olaf::GraphicsManager::init(
    const Options &options, std::shared_ptr<Window> window,
    std::function<void(Options &, GraphicsManager &, const double &)>
        onDrawCallback) {
    (void)options;
    onDraw = onDrawCallback;
    pWindow = window;

    GpuHandle handle = pWindow->getGpuDevice();
    if (get_graphic_API() == GraphicAPI::SDL3) {
        gpu = handle.as<SDL_GPUDevice>();
    }
}

void Olaf::GraphicsManager::update(Options &options, double dt) {
    if (get_graphic_API() == GraphicAPI::SDL3) {
        auto cmdbuf = SDL_AcquireGPUCommandBuffer(gpu);
        SDL_GPUTexture *swapchainTexture;

        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                cmdbuf, pWindow->getWindow().as<SDL_Window>(), &swapchainTexture,
                nullptr, nullptr)) {
            OLAF_ERROR("WaitAndAcquireGPUSwapchainTexture failed: %s",
                SDL_GetError());
        }

        if (swapchainTexture != NULL) {
            SDL_GPUColorTargetInfo colorTargetInfo = {};
            colorTargetInfo.texture = swapchainTexture;
            colorTargetInfo.clear_color = SDL_FColor { 0.3f, 0.4f, 0.5f, 1.0f };
            colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
            SDL_EndGPURenderPass(renderPass);
        }

        SDL_SubmitGPUCommandBuffer(cmdbuf);

        if (this->onDraw)
            this->onDraw(options, *this, dt);
    }
}

void Olaf::GraphicsManager::close() {
    if (get_graphic_API() == GraphicAPI::SDL3) {
    }
}
