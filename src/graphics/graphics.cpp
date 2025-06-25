#include "graphics.h"

#include "../camera.h"
#include "components.h"
#include "imguisdl.h"
#include "shaders.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

#include <stddef.h>

static constexpr PositionColorVertex positions[] {
    { -10, -10, -10, 255, 0, 0, 255 },
    { 10, -10, -10, 255, 0, 0, 255 },
    { 10, 10, -10, 255, 0, 0, 255 },
    { -10, 10, -10, 255, 0, 0, 255 },
    { -10, -10, 10, 255, 255, 0, 255 },
    { 10, -10, 10, 255, 255, 0, 255 },
    { 10, 10, 10, 255, 255, 0, 255 },
    { -10, 10, 10, 255, 255, 0, 255 },
    { -10, -10, -10, 255, 0, 255, 255 },
    { -10, 10, -10, 255, 0, 255, 255 },
    { -10, 10, 10, 255, 0, 255, 255 },
    { -10, -10, 10, 255, 0, 255, 255 },
    { 10, -10, -10, 0, 255, 0, 255 },
    { 10, 10, -10, 0, 255, 0, 255 },
    { 10, 10, 10, 0, 255, 0, 255 },
    { 10, -10, 10, 0, 255, 0, 255 },
    { -10, -10, -10, 0, 255, 255, 255 },
    { -10, -10, 10, 0, 255, 255, 255 },
    { 10, -10, 10, 0, 255, 255, 255 },
    { 10, -10, -10, 0, 255, 255, 255 },
    { -10, 10, -10, 0, 0, 255, 255 },
    { -10, 10, 10, 0, 0, 255, 255 },
    { 10, 10, 10, 0, 0, 255, 255 },
    { 10, 10, -10, 0, 0, 255, 255 },
};

SDL_AppResult graphics_init(AppState& state, int argc, char** argv) {

    state.graphics = new GraphicState {}; // will be free in graphics_quit()
    GraphicState& graphics = *state.graphics;

    auto result = imgui_init(state, argc, argv);
    if (result != SDL_APP_CONTINUE) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize ImGui: %s", SDL_GetError());
        return result;
    }

    // create the shaders
    SDL_GPUShader* vertShader = loadShader(state.device, SHADER_PATH("PositionColor.vert"), 0, 1, 1, 0);
    if (vertShader == nullptr) [[unlikely]] {
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader* fragShader = loadShader(state.device, SHADER_PATH("SolidColor.frag"), 0, 0, 0, 0);
    if (fragShader == nullptr) [[unlikely]] {
        SDL_ReleaseGPUShader(state.device, vertShader);
        return SDL_APP_FAILURE;
    }

    // Graphics pipeline creation
    SDL_GPUTextureFormat depthStencilFormat;
    {
        if (SDL_GPUTextureSupportsFormat(
                state.device,
                SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
                SDL_GPU_TEXTURETYPE_2D,
                SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
            depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        } else if (SDL_GPUTextureSupportsFormat(
                       state.device,
                       SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
                       SDL_GPU_TEXTURETYPE_2D,
                       SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
            depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No supported depth-stencil format found: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        SDL_GPUColorTargetDescription colorTargetDescription {};
        colorTargetDescription.format = SDL_GetGPUSwapchainTextureFormat(state.device, state.window);

        SDL_GPUVertexBufferDescription vertexBufferDescriptions[NumBuffers] {};
        vertexBufferDescriptions[VertexBuffer].slot = 0;
        vertexBufferDescriptions[VertexBuffer].pitch = sizeof(PositionColorVertex);
        vertexBufferDescriptions[VertexBuffer].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute vertexAttributes[NumVertexAttributes] {};
        vertexAttributes[Position].location = VertexAttributeLocation::Position;
        vertexAttributes[Position].buffer_slot = 0;
        vertexAttributes[Position].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[Position].offset = offsetof(PositionColorVertex, x);
        vertexAttributes[Color].location = VertexAttributeLocation::Color;
        vertexAttributes[Color].buffer_slot = 0;
        vertexAttributes[Color].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
        vertexAttributes[Color].offset = offsetof(PositionColorVertex, r);

        SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo {};
        pipelineCreateInfo.target_info.has_depth_stencil_target = true;
        pipelineCreateInfo.target_info.depth_stencil_format = depthStencilFormat;
        pipelineCreateInfo.target_info.color_target_descriptions = &colorTargetDescription;
        pipelineCreateInfo.target_info.num_color_targets = 1;

        pipelineCreateInfo.vertex_shader = vertShader;
        pipelineCreateInfo.fragment_shader = fragShader;

        pipelineCreateInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDescriptions;
        pipelineCreateInfo.vertex_input_state.num_vertex_buffers = NumBuffers;
        pipelineCreateInfo.vertex_input_state.vertex_attributes = vertexAttributes;
        pipelineCreateInfo.vertex_input_state.num_vertex_attributes = NumVertexAttributes;

        // Depth stencil for masking
        pipelineCreateInfo.depth_stencil_state.enable_depth_test = true;
        pipelineCreateInfo.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_NEVER;
        pipelineCreateInfo.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_REPLACE;
        pipelineCreateInfo.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        pipelineCreateInfo.depth_stencil_state.back_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pipelineCreateInfo.depth_stencil_state.back_stencil_state.compare_op = SDL_GPU_COMPAREOP_NEVER;
        pipelineCreateInfo.depth_stencil_state.back_stencil_state.fail_op = SDL_GPU_STENCILOP_REPLACE;
        pipelineCreateInfo.depth_stencil_state.back_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        pipelineCreateInfo.depth_stencil_state.back_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pipelineCreateInfo.depth_stencil_state.write_mask = 0xFF;

        graphics.pipeline[MaskPipeline] = SDL_CreateGPUGraphicsPipeline(state.device, &pipelineCreateInfo);

        // Depth stencil for rendering
        pipelineCreateInfo.depth_stencil_state.enable_stencil_test = true,
        pipelineCreateInfo.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_EQUAL;
        pipelineCreateInfo.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        pipelineCreateInfo.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        pipelineCreateInfo.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pipelineCreateInfo.depth_stencil_state.back_stencil_state.compare_op = SDL_GPU_COMPAREOP_NEVER;
        pipelineCreateInfo.depth_stencil_state.back_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        pipelineCreateInfo.depth_stencil_state.back_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        pipelineCreateInfo.depth_stencil_state.back_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pipelineCreateInfo.depth_stencil_state.compare_mask = 0xFF,
        pipelineCreateInfo.depth_stencil_state.write_mask = 0;

        graphics.pipeline[Pipeline] = SDL_CreateGPUGraphicsPipeline(state.device, &pipelineCreateInfo);
    }

    SDL_ReleaseGPUShader(state.device, fragShader);
    SDL_ReleaseGPUShader(state.device, vertShader);
    if (graphics.pipeline[MaskPipeline] == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create the mask pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (graphics.pipeline[Pipeline] == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create the pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create the textures
    {
        int width, height;
        SDL_GetWindowSize(state.window, &width, &height);

        SDL_GPUTextureCreateInfo textureCreateInfo {};
        textureCreateInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureCreateInfo.format = depthStencilFormat;
        textureCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        textureCreateInfo.width = static_cast<Uint32>(width);
        textureCreateInfo.height = static_cast<Uint32>(height);
        textureCreateInfo.layer_count_or_depth = 1;
        textureCreateInfo.num_levels = 1;
        textureCreateInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        graphics.textures[DepthTexture] = SDL_CreateGPUTexture(state.device, &textureCreateInfo);
        if (graphics.textures[DepthTexture] == nullptr) [[unlikely]] {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create the depth texture: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    // ======= Floor =======

    // Create the VertexBuffer
    {
        SDL_GPUBufferCreateInfo bufferCreateInfo {};
        bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bufferCreateInfo.size = sizeof(positions);

        graphics.buffers[VertexBuffer] = SDL_CreateGPUBuffer(state.device, &bufferCreateInfo);
        if (graphics.buffers[VertexBuffer] == nullptr) [[unlikely]] {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a vertex buffers: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo {};
    transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferBufferCreateInfo.size = sizeof(positions);

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(state.device, &transferBufferCreateInfo);
    if (transferBuffer == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a GPU transfer buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    void* transferData = SDL_MapGPUTransferBuffer(state.device, transferBuffer, false);
    if (transferData == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to map GPU transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
        return SDL_APP_FAILURE;
    }

    // Copy the vertex data to the transfer buffer
    SDL_memcpy(transferData, positions, sizeof(positions));
    SDL_UnmapGPUTransferBuffer(state.device, transferBuffer);

    SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(state.device);
    if (uploadCmdBuf == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to acquired GPU Command Buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
        return SDL_APP_FAILURE;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
    SDL_GPUTransferBufferLocation transfertLocation;
    transfertLocation.transfer_buffer = transferBuffer;
    transfertLocation.offset = 0;

    SDL_GPUBufferRegion bufferRegion;
    bufferRegion.buffer = graphics.buffers[VertexBuffer];
    bufferRegion.offset = 0;
    bufferRegion.size = sizeof(positions);

    SDL_UploadToGPUBuffer(copyPass, &transfertLocation, &bufferRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    if (!SDL_SubmitGPUCommandBuffer(uploadCmdBuf)) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to submit GPU Command Buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
        return SDL_APP_FAILURE;
    }

    SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);

    return SDL_APP_CONTINUE;
}

SDL_AppResult graphics_iterate(AppState& state) {
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(state.device);
    if (cmdbuf == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to acquired GPU Command Buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture* swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            cmdbuf, state.window, &swapchainTexture, nullptr, nullptr)) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to acquire GPU Swapchain Texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (swapchainTexture == nullptr) [[unlikely]] {
        // the window is minimized
        if (!SDL_CancelGPUCommandBuffer(cmdbuf)) [[unlikely]] {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to cancel GPU Command Buffer: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        return SDL_APP_CONTINUE;
    }

    UniformBuffer ubo;
    ubo.model = mat4::identity();
    ubo.view = state.camera->view_matrix();
    ubo.proj = state.camera->projection_matrix();
    SDL_PushGPUVertexUniformData(cmdbuf, 0, &ubo, sizeof(ubo));

    SDL_GPUColorTargetInfo colorTargetInfo {};
    colorTargetInfo.texture = swapchainTexture;
    colorTargetInfo.clear_color = SDL_FColor { 0.2f, 0.5f, 0.4f, 1.0f };
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

    // BUG: the screen is black if we pass depthStencilTargetInfo into SDL_BeginGPURenderPass.
    [[maybe_unused]] SDL_GPUDepthStencilTargetInfo depthStencilTargetInfo {};
    depthStencilTargetInfo.texture = state.graphics->textures[DepthTexture];
    depthStencilTargetInfo.cycle = true;
    depthStencilTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    depthStencilTargetInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depthStencilTargetInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depthStencilTargetInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
        cmdbuf,
        &colorTargetInfo,
        1u,
        nullptr);

    SDL_GPUBufferBinding vertexBinding[] {
        {
            .buffer = state.graphics->buffers[VertexBuffer],
            .offset = 0,
        }
    };
    SDL_BindGPUVertexBuffers(renderPass, 0, vertexBinding, sizeof(vertexBinding) / sizeof(SDL_GPUBufferBinding));
    SDL_SetGPUStencilReference(renderPass, 0);
    SDL_BindGPUGraphicsPipeline(renderPass, state.graphics->pipeline[MaskPipeline]);
    SDL_DrawGPUPrimitives(renderPass, sizeof(positions) / sizeof(PositionColorVertex), 1, 0, 0);

    imgui_iterate(state, renderPass, cmdbuf);
    SDL_EndGPURenderPass(renderPass);

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return SDL_APP_CONTINUE;
}

SDL_AppResult graphics_event(AppState& state, SDL_Event& event) {
    auto result = imgui_event(state, event);
    if (result != SDL_APP_CONTINUE) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to process ImGui event: %s", SDL_GetError());
        return result;
    }

    return SDL_APP_CONTINUE;
}

void graphics_quit(AppState& state) {
    imgui_quit(state);
    if (state.graphics) {
        for (std::size_t i = 0; i < NumPipelines; i++) {
            if (state.graphics->pipeline[i])
                SDL_ReleaseGPUGraphicsPipeline(state.device, state.graphics->pipeline[i]);
        }

        for (std::size_t i = 0; i < NumBuffers; i++) {
            if (state.graphics->buffers[i])
                SDL_ReleaseGPUBuffer(state.device, state.graphics->buffers[i]);
        }

        for (std::size_t i = 0; i < NumTextures; i++) {
            if (state.graphics->textures[i])
                SDL_ReleaseGPUTexture(state.device, state.graphics->textures[i]);
        }

        delete state.graphics;
    }
}
