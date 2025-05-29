#include "pch.h"
#include "options.h"
#include "graphics/graphic_manager.h"
#include "system/window.h"
#include <SDL3/SDL_gpu.h>
#include <system/log.h>

static SDL_GPUGraphicsPipeline *FillPipeline;
static SDL_GPUGraphicsPipeline *LinePipeline;
static SDL_GPUViewport SmallViewport = { 160, 120, 320, 240, 0.1f, 1.0f };
static SDL_Rect ScissorRect = { 320, 240, 320, 240 };

static bool UseWireframeMode = false;
static bool UseSmallViewport = false;
static bool UseScissorRect = false;

void Olaf::GraphicsManager::init(
    const Options &options, std::shared_ptr<Window> window,
    std::function<void(Options &, GraphicsManager &, const double &)>
        onDrawCallback) {
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
            SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
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
