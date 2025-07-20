#include "ssao_renderer.h"
#include "graphics.h"
#include "shaders.h"
#include <SDL3/SDL_gpu.h>

SDL_AppResult deferred_ssao_init(AppState& state) {
    GraphicState& graphics = *state.graphics;

    // Release previous SSAO texture if it exists
    if (graphics.textures[GeometrySSAO]) {
        SDL_ReleaseGPUTexture(state.device, graphics.textures[GeometrySSAO]);
        graphics.textures[GeometrySSAO] = nullptr;
    }

    SDL_GPUTextureCreateInfo createInfo = {};
    createInfo.type = SDL_GPU_TEXTURETYPE_2D;
    createInfo.format = SDL_GPU_TEXTUREFORMAT_R16_UNORM;
    createInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    createInfo.width = INITIAL_WINDOW_WIDTH;
    createInfo.height = INITIAL_WINDOW_HEIGHT;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    graphics.textures[GeometrySSAO] = SDL_CreateGPUTexture(state.device, &createInfo);
    if (!graphics.textures[GeometrySSAO]) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create SSAO render target: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (SDL_AppResult result = deferred_ssao_create_pipeline(state); result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    return SDL_APP_CONTINUE;
}

SDL_AppResult deferred_ssao_create_pipeline(AppState& state) {
    SDL_GPUDevice* device = state.device;
    GraphicState& graphics = *state.graphics;

    // Load SSAO shaders (explicit arguments, similar to GBuffer)
    SDL_GPUShader* vertexShader = loadShader(device, SHADER_PATH("quad.vert"), 0, 1, 0, 0);
    if (!vertexShader) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load SSAO vertex shader: ssao.vert");
        return SDL_APP_FAILURE;
    }
    SDL_GPUShader* fragmentShader = loadShader(device, SHADER_PATH("ssao.frag"), 3, 0, 0, 0);
    if (!fragmentShader) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load SSAO fragment shader: ssao.frag");
        SDL_ReleaseGPUShader(device, vertexShader);
        return SDL_APP_FAILURE;
    }

    // Vertex input state
    SDL_GPUVertexBufferDescription vertexBufferDesc = {};
    vertexBufferDesc.slot = 0;
    vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBufferDesc.instance_step_rate = 0;
    vertexBufferDesc.pitch = sizeof(Vertex);

    SDL_GPUVertexAttribute vertexAttributes[3] = {};
    vertexAttributes[0].buffer_slot = 0;
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].offset = offsetof(Vertex, position);

    vertexAttributes[1].buffer_slot = 0;
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].offset = offsetof(Vertex, normal);

    vertexAttributes[2].buffer_slot = 0;
    vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttributes[2].location = 2;
    vertexAttributes[2].offset = offsetof(Vertex, texCoord);

    SDL_GPUVertexInputState vertexInputState = {};
    vertexInputState.num_vertex_buffers = 1;
    vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
    vertexInputState.num_vertex_attributes = 3;
    vertexInputState.vertex_attributes = vertexAttributes;

    // Color target
    SDL_GPUColorTargetDescription colorTarget = {};
    colorTarget.format = SDL_GPU_TEXTUREFORMAT_R16_UNORM; // SSAO output format

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.target_info.num_color_targets = 1;
    pipelineInfo.target_info.color_target_descriptions = &colorTarget;
    pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    pipelineInfo.target_info.has_depth_stencil_target = false;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipelineInfo.vertex_shader = vertexShader;
    pipelineInfo.fragment_shader = fragmentShader;
    pipelineInfo.vertex_input_state = vertexInputState;
    pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

    graphics.graphicPipeline[SSAOPipeline] = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
    if (!graphics.graphicPipeline[SSAOPipeline]) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create SSAO graphics pipeline: %s", SDL_GetError());
        SDL_ReleaseGPUShader(device, vertexShader);
        SDL_ReleaseGPUShader(device, fragmentShader);
        return SDL_APP_FAILURE;
    }

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    return SDL_APP_CONTINUE;
}

void deferred_ssao_render(AppState& state, SDL_GPUCommandBuffer* cmdbuf) {
    GraphicState& graphics = *state.graphics;
    if (!graphics.ssaoEnabled) return; // SSAO DISABLED

    if (!graphics.textures[GeometrySSAO]) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "SSAO texture not initialized!");
        return;
    }

    if (!graphics.graphicPipeline[SSAOPipeline]) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "SSAO graphics pipeline not initialized!");
        return;
    }

    // Bind SSAO render target
    SDL_GPUColorTargetInfo ssaoTarget = {};
    ssaoTarget.texture = graphics.textures[GeometrySSAO];
    ssaoTarget.clear_color = SDL_FColor { 1.0f, 1.0f, 1.0f, 1.0f }; // White = no occlusion
    ssaoTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    ssaoTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* ssaoPass = SDL_BeginGPURenderPass(cmdbuf, &ssaoTarget, 1, nullptr);

    // Bind SSAO pipeline
    SDL_BindGPUGraphicsPipeline(ssaoPass, graphics.graphicPipeline[SSAOPipeline]);

    // Bind G-buffer textures as samplers
    SDL_GPUTextureSamplerBinding gbufferSamplers[3] = {
        { graphics.textures[GeometryPosition], graphics.samplersPreset[LinearClamp] },
        { graphics.textures[GeometryNormal], graphics.samplersPreset[LinearClamp] },
        { graphics.textures[GeometryDepth], graphics.samplersPreset[LinearClamp] }
    };
    SDL_BindGPUFragmentSamplers(ssaoPass, 0, gbufferSamplers, 3);

    // Draw fullscreen quad (3 vertices for 1 triangles)
    SDL_DrawGPUPrimitives(ssaoPass, 3, 1, 0, 0);

    SDL_EndGPURenderPass(ssaoPass);
}
