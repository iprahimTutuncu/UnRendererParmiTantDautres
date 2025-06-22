#include "graphics.h"

#include "../camera.h"
#include "components.h"
#include "shaders.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

struct UniformBufferObject {
    mat4 uProjMatrix;
    mat4 uViewMatrix;
    mat4 uModelMatrix;
};

SDL_AppResult graphics_init(AppState& state, int argc, char** argv) {
    (void)argc;
    (void)argv;

    state.graphics = new GraphicState {}; // will be free in graphics_quit()
    GraphicState& graphics = *state.graphics;

    // create the shaders
    SDL_GPUShader* vertShader = loadShader(state.device, SHADERS_DIR "floorShader.vert.spv", 0, 1, 1, 0);
    if (vertShader == nullptr) [[unlikely]] {
        return SDL_APP_FAILURE;
    }

    SDL_GPUShader* fragShader = loadShader(
        state.device,
        SHADERS_DIR "floorShader.frag.spv",
        0,
        0,
        1,
        0);
    if (fragShader == nullptr) [[unlikely]] {
        SDL_ReleaseGPUShader(state.device, vertShader);
        return SDL_APP_FAILURE;
    }

    SDL_GPUTextureFormat depthStencilFormat;
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
        SDL_Log("Stencil formats not supported!");
        SDL_ReleaseGPUShader(state.device, fragShader);
        SDL_ReleaseGPUShader(state.device, vertShader);
        return SDL_APP_FAILURE;
    }

    static SDL_GPUColorTargetDescription colorTargetDescription[] {
        {
            .format = SDL_GPU_TEXTUREFORMAT_INVALID,
            .blend_state = {
                .src_color_blendfactor = SDL_GPUBlendFactor::SDL_GPU_BLENDFACTOR_ONE,
                .dst_color_blendfactor = SDL_GPUBlendFactor::SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .color_blend_op = SDL_GPU_BLENDOP_ADD,
                .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                .color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B,
                .enable_blend = true,
                .enable_color_write_mask = false,
                .padding1 = 0,
                .padding2 = 0,
            },
        },
    };
    colorTargetDescription[0].format = SDL_GetGPUSwapchainTextureFormat(state.device, state.window);

    static_assert(sizeof(PositionColorVertex) == 16, "by std140, the byte pitch between vec3 must be 16 bytes");
    static constexpr SDL_GPUVertexBufferDescription vertexBufferDescription[] {
        {
            .slot = 0,
            .pitch = sizeof(PositionColorVertex),
            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
            .instance_step_rate = 0,
        },
    };
    static constexpr SDL_GPUVertexAttribute vertexAttributes[] {
        {
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = 0,
        },
    };

    static SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo {
        .vertex_shader = nullptr,
        .fragment_shader = nullptr,
        .vertex_input_state = {
            .vertex_buffer_descriptions = vertexBufferDescription,
            .num_vertex_buffers = 1,
            .vertex_attributes = vertexAttributes,
            .num_vertex_attributes = 1,
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            .depth_bias_constant_factor = 0,
            .depth_bias_clamp = 0,
            .depth_bias_slope_factor = 0,
            .enable_depth_bias = false,
            .enable_depth_clip = false,
            .padding1 = 0,
            .padding2 = 0,
        },
        .multisample_state = {
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
            .sample_mask = 0,
            .enable_mask = false,
            .padding1 = 0,
            .padding2 = 0,
            .padding3 = 0,
        },
        .depth_stencil_state = {
            .compare_op = SDL_GPU_COMPAREOP_NEVER,
            .back_stencil_state = {
                .fail_op = SDL_GPU_STENCILOP_REPLACE,
                .pass_op = SDL_GPU_STENCILOP_KEEP,
                .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
                .compare_op = SDL_GPU_COMPAREOP_NEVER,
            },
            .front_stencil_state = {
                .fail_op = SDL_GPU_STENCILOP_REPLACE,
                .pass_op = SDL_GPU_STENCILOP_KEEP,
                .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
                .compare_op = SDL_GPU_COMPAREOP_NEVER,
            },
            .compare_mask = 0,
            .write_mask = 0xFF,
            .enable_depth_test = false,
            .enable_depth_write = false,
            .enable_stencil_test = true,
            .padding1 = 0,
            .padding2 = 0,
            .padding3 = 0,
        },
        .target_info = {
            .color_target_descriptions = colorTargetDescription,
            .num_color_targets = 1,
            .depth_stencil_format = depthStencilFormat,
            .has_depth_stencil_target = false,
            .padding1 = 0,
            .padding2 = 0,
            .padding3 = 0,
        },
        .props = 0,
    };

    pipelineCreateInfo.vertex_shader = vertShader;
    pipelineCreateInfo.fragment_shader = fragShader;

    graphics.pipeline = SDL_CreateGPUGraphicsPipeline(state.device, &pipelineCreateInfo);
    SDL_ReleaseGPUShader(state.device, fragShader);
    SDL_ReleaseGPUShader(state.device, vertShader);

    if (graphics.pipeline == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create the render pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // ======= Floor =======

    static constexpr PositionVertex positions[] {
        { -1.f, 0.f, -1.f },
        { 1.f, 0.f, 1.f },
        { 1.f, 0.f, -1.f },
        { 1.f, 0.f, 1.f },
        { -1.f, 0.f, -1.f },
        { -1.f, 0.f, 1.f },
    };

    static constexpr SDL_GPUBufferCreateInfo bufferCreateInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = sizeof(positions),
        .props = 0,
    };
    graphics.vertexBuffer = SDL_CreateGPUBuffer(state.device, &bufferCreateInfo);
    if (graphics.vertexBuffer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a vertex buffers: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    int width, height;
    SDL_GetWindowSizeInPixels(state.window, &width, &height);

    SDL_GPUTextureCreateInfo gpuTextureCreateInfo {
        .type = SDL_GPU_TEXTURETYPE_3D,
        .format = depthStencilFormat,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width = static_cast<Uint32>(width),
        .height = static_cast<Uint32>(height),
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = 0,
    };

    graphics.depthSencilTexture = SDL_CreateGPUTexture(state.device, &gpuTextureCreateInfo);
    if (graphics.depthSencilTexture == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a GPU Texture: %s", SDL_GetError());

        return SDL_APP_FAILURE;
    }

    SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = sizeof(positions),
        .props = 0,
    };
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(state.device, &transferBufferCreateInfo);
    if (transferBuffer == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create a GPU transfer buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    PositionVertex* transferData = reinterpret_cast<PositionVertex*>(SDL_MapGPUTransferBuffer(state.device, transferBuffer, false));
    if (transferData == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to map GPU transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
        return SDL_APP_FAILURE;
    }

    for (size_t i = 0; i < sizeof(positions) / sizeof(PositionVertex); i++) {
        transferData[i] = positions[i];
    }

    SDL_UnmapGPUTransferBuffer(state.device, transferBuffer);

    SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(state.device);
    if (uploadCmdBuf == nullptr) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to acquired GPU Command Buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
    const SDL_GPUTransferBufferLocation transfertLocation {
        .transfer_buffer = transferBuffer,
        .offset = 0,
    };
    const SDL_GPUBufferRegion bufferRegion {
        .buffer = graphics.vertexBuffer,
        .offset = 0,
        .size = transferBufferCreateInfo.size,
    };
    SDL_UploadToGPUBuffer(copyPass, &transfertLocation, &bufferRegion, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
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

    static SDL_GPUColorTargetInfo colorTargetInfo {
        .texture = nullptr,
        .mip_level = 0,
        .layer_or_depth_plane = 0,
        .clear_color = SDL_FColor { 0.3f, 0.4f, 0.5f, 1.0f },
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .resolve_texture = nullptr,
        .resolve_mip_level = 0,
        .resolve_layer = 0,
        .cycle = false,
        .cycle_resolve_texture = false,
        .padding1 = 0,
        .padding2 = 0,
    };

    colorTargetInfo.texture = swapchainTexture;
    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, nullptr);

    UniformBufferObject uniformBlob;
    uniformBlob.uProjMatrix = state.camera->projection_matrix();
    uniformBlob.uViewMatrix = state.camera->view_matrix();
    uniformBlob.uModelMatrix = mat4::identity();

inversesqrt(13.5f)

    SDL_PushGPUVertexUniformData(cmdbuf, 0, &uniformBlob, sizeof(uniformBlob));

    SDL_GPUBufferBinding vertexBinding[] {
        {
            .buffer = state.graphics->vertexBuffer,
            .offset = 0,
        }
    };
    SDL_BindGPUVertexBuffers(renderPass, 0, vertexBinding, 1);

    SDL_SetGPUStencilReference(renderPass, 1);
    SDL_BindGPUGraphicsPipeline(renderPass, state.graphics->pipeline);
    SDL_DrawGPUPrimitives(renderPass, 6, 1, 0, 0);
    SDL_EndGPURenderPass(renderPass);

    if (!SDL_SubmitGPUCommandBuffer(cmdbuf)) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Failed to submit GPU Command Buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult graphics_event(AppState& state, SDL_Event& event) {
    (void)state;
    (void)event;
    return SDL_APP_CONTINUE;
}

void graphics_quit(AppState& state) {
    if (state.graphics) {
        if (state.graphics->pipeline)
            SDL_ReleaseGPUGraphicsPipeline(state.device, state.graphics->pipeline);

        if (state.graphics->vertexBuffer)
            SDL_ReleaseGPUBuffer(state.device, state.graphics->vertexBuffer);

        if (state.graphics->depthSencilTexture)
            SDL_ReleaseGPUTexture(state.device, state.graphics->depthSencilTexture);

        delete state.graphics;
    }
}
