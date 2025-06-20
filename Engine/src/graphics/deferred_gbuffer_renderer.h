#pragma once

#include "deferred_struct.h"

#include <SDL3/SDL_gpu.h>

#include <memory>
#include <vector>

namespace GTS {
    class Window;
    struct Box;
    struct Particles;

    class DeferredGBufferRenderer {
        enum RasterMode {
            Fill,
            Line
        };

    public:
        DeferredGBufferRenderer(SDL_GPUDevice* device, std::shared_ptr<Window> window);
        ~DeferredGBufferRenderer();

        // Set output G-Buffer textures
        void setGBufferOutput(const GBufferTextures& gBuffer);
        void setFillMode(RasterMode mode);
        void setCamera(const Camera& camera);
        void setParticles(const Particles& particles);

        // Get current G-Buffer output
        const GBufferTextures& getGBufferOutput() const {
            return m_gBuffer;
        }

        void render(SDL_GPUCommandBuffer* cmdBuf, const std::vector<const Box*>& boxes);

        void releaseRessouces();

    private:
        void createPipelines();
        void createParticlesPipeline();
        void createMeshPipeline();
        void DoSomeComputeShaderThing();

        void createBoxGeometry();
        void createSphereGeometry();

        // Render boxes to G-Buffer
        int particleCount { 100 };

        std::shared_ptr<Window> m_window;
        SDL_GPUDevice* m_gpuDevice;
        GBufferTextures m_gBuffer;

        // Geometry pipeline
        SDL_GPUGraphicsPipeline* m_gBufferFillPipeline;
        SDL_GPUGraphicsPipeline* m_gBufferLinePipeline;

        SDL_GPUGraphicsPipeline* m_gBufferParticlesFillPipeline;
        SDL_GPUGraphicsPipeline* m_gBufferParticlesLinePipeline;

        // Box resources
        SDL_GPUBuffer* m_boxVertexBuffer;
        SDL_GPUBuffer* m_boxIndexBuffer;
        std::shared_ptr<SDL_GPUTexture> default_white;
        int default_white_width { 32 };
        int default_white_height { 32 };

        // Particle resources
        SDL_GPUBuffer* m_particlePositionBuffer { nullptr };
        SDL_GPUBuffer* m_particlesVertexBuffer { nullptr };
        SDL_GPUBuffer* m_particlesIndexBuffer { nullptr };

        uint16_t numSphereIndices { 0 };
        SDL_GPUBuffer* m_sphereVertexBuffer { nullptr };
        SDL_GPUBuffer* m_sphereIndexBuffer { nullptr };

        // goal modify the cube
        SDL_GPUComputePipeline* computePipeline;

        struct TemporaryUniforms {
            float time;
        };

        TemporaryUniforms temporaryUniforms;

        RasterMode rasterMode { RasterMode::Fill };
    };
}
