#pragma once

#include "deferred_struct.h"

#include <SDL3/SDL_gpu.h>

#include <memory>
#include <vector>

namespace GTS {
    class Window;
    struct Box;

    class DeferredGBufferTerrainRenderer {
        enum RasterMode {
            Fill,
            Line
        };

    public:
        DeferredGBufferTerrainRenderer(SDL_GPUDevice* device, std::shared_ptr<Window> window);
        ~DeferredGBufferTerrainRenderer();

        // Set output G-Buffer textures
        void setGBufferOutput(const GBufferTextures& gBuffer);
        void setFillMode(RasterMode mode);
        void setCamera(const Camera& camera);

        // Get current G-Buffer output
        const GBufferTextures& getGBufferOutput() const {
            return m_gBuffer;
        }

        void render(SDL_GPUCommandBuffer* cmdBuf, const std::vector<const Box*>& boxes);

        void releaseRessouces();

    private:
        void createPipelines();
        void createBoxGeometry();

        // Render boxes to G-Buffer

        std::shared_ptr<Window> m_window;
        SDL_GPUDevice* m_gpuDevice;
        GBufferTextures m_gBuffer;

        // Geometry pipeline
        SDL_GPUGraphicsPipeline* m_gBufferFillPipeline;
        SDL_GPUGraphicsPipeline* m_gBufferLinePipeline;

        // Box resources
        SDL_GPUBuffer* m_boxVertexBuffer;
        SDL_GPUBuffer* m_boxIndexBuffer;

        std::shared_ptr<SDL_GPUTexture> default_white;

        RasterMode rasterMode { RasterMode::Fill };
    };
}
