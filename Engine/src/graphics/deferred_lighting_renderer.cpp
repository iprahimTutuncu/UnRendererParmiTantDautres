#include "deferred_lighting_renderer.h"

#include "../ressource_manager/sampler_manager.h"
#include "../ressource_manager/shader_manager.h"
#include "../system/log.h"
#include "../system/window.h"

#include <glm/glm.hpp>

namespace GTS {

    DeferredLightingRenderer::DeferredLightingRenderer(SDL_GPUDevice* device, std::shared_ptr<Window> window)
        : m_pWindow(window)
        , m_device(device) {
        memset(&m_gBuffer, 0, sizeof(GBufferTextures));
        m_finalRenderPipeline = nullptr;
        m_debugPipeline = nullptr;
        m_fullscreenQuadVB = nullptr;
        m_fullscreenQuadIB = nullptr;

        createPipelines();
    }

    DeferredLightingRenderer::~DeferredLightingRenderer() {
        cleanup();
    }

    void DeferredLightingRenderer::setGBufferTextures(const GBufferTextures& gBuffer) {
        m_gBuffer = gBuffer;
    }

    void DeferredLightingRenderer::renderToTexture(SDL_GPUCommandBuffer* cmdBuf,
        SDL_GPUTexture* targetTexture,
        DisplayMode mode) {
        if (!targetTexture || !m_gBuffer.position || !m_gBuffer.normal || !m_gBuffer.albedo) {
            GTS_ERROR("Renderer: Invalid textures provided");
            return;
        }

        SDL_GPUColorTargetInfo colorTarget = {};
        colorTarget.texture = targetTexture;
        colorTarget.clear_color = SDL_FColor { 0.0f, 0.0f, 0.0f, 1.0f };
        colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTarget, 1, nullptr);

        if (mode == DisplayMode::Final) {
            SDL_BindGPUGraphicsPipeline(renderPass, m_finalRenderPipeline);
        } else {
            SDL_BindGPUGraphicsPipeline(renderPass, m_debugPipeline);
        }

        // Bind G-Buffer textures

        SDL_GPUSampler* sampler = Ressource::SamplerManager::get(Ressource::SamplerManager::Preset::PointClamp).get();

        SDL_GPUTextureSamplerBinding gbufferSamplers[3] = {
            { m_gBuffer.position.get(), sampler },
            { m_gBuffer.normal.get(), sampler },
            { m_gBuffer.albedo.get(), sampler }
        };

        SDL_BindGPUFragmentSamplers(renderPass, 0, gbufferSamplers, 3);

        // Push display mode uniform
        int displayMode = static_cast<int>(mode);
        SDL_PushGPUFragmentUniformData(cmdBuf, 0, &displayMode, sizeof(int));

        // Draw fullscreen quad, it's on the vertex shader, no stress man
        SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

        SDL_EndGPURenderPass(renderPass);
    }

    void DeferredLightingRenderer::cleanup() {
        if (m_finalRenderPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_device, m_finalRenderPipeline);
            m_finalRenderPipeline = nullptr;
        }
        if (m_debugPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_device, m_debugPipeline);
            m_debugPipeline = nullptr;
        }
        if (m_fullscreenQuadVB) {
            SDL_ReleaseGPUBuffer(m_device, m_fullscreenQuadVB);
            m_fullscreenQuadVB = nullptr;
        }
        if (m_fullscreenQuadIB) {
            SDL_ReleaseGPUBuffer(m_device, m_fullscreenQuadIB);
            m_fullscreenQuadIB = nullptr;
        }
    }

    void DeferredLightingRenderer::createPipelines() {
        // Load shaders (simplified - you'd want proper error handling)
        std::shared_ptr<SDL_GPUShader> vs = Ressource::ShaderManager::get(m_device, "quad.vert", 0, 0, 0, 0);
        std::shared_ptr<SDL_GPUShader> fsFinal = Ressource::ShaderManager::get(m_device, "deferred_render.frag", 3, 0, 0, 0);
        std::shared_ptr<SDL_GPUShader> fsDebug = Ressource::ShaderManager::get(m_device, "deferred_debug.frag", 3, 1, 0, 0);

        // Final render pipeline
        SDL_GPUVertexInputState vertexInputState = {};
        vertexInputState.num_vertex_buffers = 0;
        vertexInputState.num_vertex_attributes = 0;

        SDL_GPUColorTargetDescription colorDesc = {};
        WindowHandle window = m_pWindow->getWindow();
        SDL_Window* sdlWindow = window.as<SDL_Window>();
        colorDesc.format = SDL_GetGPUSwapchainTextureFormat(m_device, sdlWindow);

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorDesc;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
        pipelineInfo.target_info.has_depth_stencil_target = false;
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipelineInfo.vertex_shader = vs.get();
        pipelineInfo.fragment_shader = fsFinal.get();
        pipelineInfo.vertex_input_state = vertexInputState;
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

        m_finalRenderPipeline = SDL_CreateGPUGraphicsPipeline(m_device, &pipelineInfo);
        if (m_finalRenderPipeline == nullptr) {
            GTS_ERROR("Failed to create render pipeline!");
        }
        // Debug pipeline (same except fragment shader)
        pipelineInfo.fragment_shader = fsDebug.get();
        m_debugPipeline = SDL_CreateGPUGraphicsPipeline(m_device, &pipelineInfo);

        // Release shaders (pipelines retain their own copies)
        Ressource::ShaderManager::releaseUnused();
    }

}
