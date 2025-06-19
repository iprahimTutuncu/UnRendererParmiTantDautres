#pragma once

#include <SDL3/SDL_gpu.h>
#include <memory>

#include "deferred_struct.h"

namespace GTS 
{
    class Window;
          
    class DeferredLightingRenderer 
    {
    public:
        enum class DisplayMode
        {
            Final = 0,
            Position = 1,
            Normal = 2,
            Albedo = 3,
            Depth = 4
        };

        DeferredLightingRenderer(SDL_GPUDevice* device, std::shared_ptr<Window> window);
        ~DeferredLightingRenderer();

        void setGBufferTextures(const GBufferTextures& gBuffer);

        void renderToTexture(SDL_GPUCommandBuffer* cmdBuf,
            SDL_GPUTexture* targetTexture,
            DisplayMode mode = DisplayMode::Final);

        // Cleanup resources
        void cleanup();

    private:
        void createPipelines();
        std::shared_ptr<Window> m_pWindow;
        SDL_GPUDevice* m_device;
        GBufferTextures m_gBuffer;

        // Pipelines
        SDL_GPUGraphicsPipeline* m_finalRenderPipeline;
        SDL_GPUGraphicsPipeline* m_debugPipeline;

        // Fullscreen quad resources
        SDL_GPUBuffer* m_fullscreenQuadVB;
        SDL_GPUBuffer* m_fullscreenQuadIB;

    };

}