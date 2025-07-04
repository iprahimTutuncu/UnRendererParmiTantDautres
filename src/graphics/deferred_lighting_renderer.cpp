#include "deferred_lighting_renderer.h"
#include "shaders.h"
#include "graphics.h"

#include <SDL3/SDL_log.h>

SDL_AppResult deferred_lighting_init(AppState& state) {
    GraphicState& gfx = *state.graphics;
    SDL_GPUDevice* device = state.device;

    SDL_GPUShader* vs = loadShader(device, SHADER_PATH("quad.vert"), 0, 0, 0, 0);
    if (vs == nullptr) [[unlikely]] {
        return SDL_APP_FAILURE;
    }
    SDL_GPUShader* fsFinal = loadShader(device, SHADER_PATH("deferred_render.frag"), 3, 0, 0, 0);
    if (fsFinal == nullptr) [[unlikely]] {
        return SDL_APP_FAILURE;
    }
    SDL_GPUShader* fsDebug = loadShader(device, SHADER_PATH("deferred_debug.frag"), 3, 1, 0, 0);
    if (fsDebug == nullptr) [[unlikely]] {
        return SDL_APP_FAILURE;
    }
    SDL_GPUVertexInputState vertexInputState = {};
    vertexInputState.num_vertex_buffers = 0;
    vertexInputState.num_vertex_attributes = 0;

    SDL_GPUColorTargetDescription colorDesc = {};
    colorDesc.format = SDL_GetGPUSwapchainTextureFormat(device, state.window);

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.target_info.num_color_targets = 1;
    pipelineInfo.target_info.color_target_descriptions = &colorDesc;
    pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    pipelineInfo.target_info.has_depth_stencil_target = false;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipelineInfo.vertex_shader = vs;
    pipelineInfo.fragment_shader = fsFinal;
    pipelineInfo.vertex_input_state = vertexInputState;
    pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

    gfx.graphicPipeline[DeferredLightingPipeline] = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

    if (!gfx.graphicPipeline[DeferredLightingPipeline])
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create DeferredLightingPipeline!");

    pipelineInfo.fragment_shader = fsDebug;
    gfx.graphicPipeline[DeferredDebugPipeline] = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
    if (!gfx.graphicPipeline[DeferredDebugPipeline])
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to create DeferredDebugPipeline!");

    SDL_ReleaseGPUShader(device, vs);
    SDL_ReleaseGPUShader(device, fsFinal);
    SDL_ReleaseGPUShader(device, fsDebug);
    
    return SDL_APP_CONTINUE;
}

void deferred_lighting_render_to_texture(
    AppState& state,
    SDL_GPURenderPass* renderPass,
    SDL_GPUCommandBuffer* cmdBuf,
    DisplayMode mode) {
    GraphicState& gfx = *state.graphics;

    if (!gfx.textures[GeometryPosition] || !gfx.textures[GeometryNormal] || !gfx.textures[GeometryAlbedo]) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Renderer: Invalid G-buffer textures");
        return;
    }

    SDL_GPUGraphicsPipeline* pipeline = (mode == DisplayMode::Final) ? gfx.graphicPipeline[DeferredLightingPipeline] : gfx.graphicPipeline[DeferredDebugPipeline];

    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

    SDL_GPUSampler* sampler = gfx.samplersPreset[PointClamp];

    SDL_GPUTextureSamplerBinding gbufferSamplers[3] = {
        { gfx.textures[GeometryPosition], sampler },
        { gfx.textures[GeometryNormal], sampler },
        { gfx.textures[GeometryAlbedo], sampler }
    };

    SDL_BindGPUFragmentSamplers(renderPass, 0, gbufferSamplers, 3);

    int displayMode = static_cast<int>(mode);
    SDL_PushGPUFragmentUniformData(cmdBuf, 0, &displayMode, sizeof(int));

    SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

}
