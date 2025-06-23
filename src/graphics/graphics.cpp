#include "graphics.h"

#include "components.h"
#include "imguisdl.h"
#include "shaders.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

#include <cstring>
#include <stddef.h>

SDL_AppResult graphics_init(AppState& state, int argc, char** argv) {
    (void)argc;
    (void)argv;

    state.graphics = new GraphicState {}; // will be free in graphics_quit()
    GraphicState& graphics = *state.graphics;

    // create the shaders
    SDL_GPUShader* vertShader = loadShader(state.device, SHADER_PATH("PositionColor.vert"), 0, 0, 1, 1);
    if (vertShader == nullptr) [[unlikely]] {
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader* fragShader = loadShader(state.device, SHADER_PATH("SolidColor.frag"), 0, 0, 0, 0);
    if (fragShader == nullptr) [[unlikely]] {
        SDL_ReleaseGPUShader(state.device, vertShader);
        return SDL_APP_FAILURE;
    }

    // Graphics pipeline creation
    {
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
        pipelineCreateInfo.target_info.color_target_descriptions = &colorTargetDescription;
        pipelineCreateInfo.target_info.num_color_targets = 1;
        pipelineCreateInfo.vertex_shader = vertShader;
        pipelineCreateInfo.fragment_shader = fragShader;
        pipelineCreateInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDescriptions;
        pipelineCreateInfo.vertex_input_state.num_vertex_buffers = NumBuffers;
        pipelineCreateInfo.vertex_input_state.vertex_attributes = vertexAttributes;
        pipelineCreateInfo.vertex_input_state.num_vertex_attributes = NumVertexAttributes;

        // Target info
        graphics.pipeline[Pipeline] = SDL_CreateGPUGraphicsPipeline(state.device, &pipelineCreateInfo);
    }

    SDL_ReleaseGPUShader(state.device, fragShader);
    SDL_ReleaseGPUShader(state.device, vertShader);
    if (graphics.pipeline[Pipeline] == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create the pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // ======= Floor =======

    static constexpr PositionColorVertex positions[] {
        { -1, -1, 0, 255, 0, 0, 255 },
        { 1, -1, 0, 0, 255, 0, 255 },
        { 0, 1, 0, 0, 0, 255, 255 },
    };

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
    std::memcpy(transferData, positions, sizeof(positions));
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

    SDL_GPUColorTargetInfo colorTargetInfo {};
    colorTargetInfo.texture = swapchainTexture;
    colorTargetInfo.clear_color = SDL_FColor { 0.f, 0.f, 0.f, 1.0f };
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

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
    SDL_BindGPUGraphicsPipeline(renderPass, state.graphics->pipeline[Pipeline]);
    SDL_BindGPUVertexBuffers(renderPass, 0, vertexBinding, sizeof(vertexBinding) / sizeof(SDL_GPUBufferBinding));
    SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

    SDL_EndGPURenderPass(renderPass);
    imgui_iterate(state, renderPass, swapchainTexture, cmdbuf);

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return SDL_APP_CONTINUE;
}

SDL_AppResult graphics_event(AppState& state, SDL_Event& event) {
    (void)state;
    (void)event;
    return SDL_APP_CONTINUE;
}

void graphics_quit(AppState& state) {
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
