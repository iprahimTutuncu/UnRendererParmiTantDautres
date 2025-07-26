#include "graphics.h"

#include "deferred_gbuffer_renderer.h"
#include "deferred_lighting_renderer.h"
#include "imguisdl.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <imgui_impl_sdlgpu3.h>

#include <stddef.h>
#include <random>

static SDL_AppResult graphics_create_render_targets(AppState& state, std::uint32_t width, std::uint32_t height);
static SDL_GPUTexture* createSolidColorTextureRGBA8(SDL_GPUDevice* device, std::uint32_t width, std::uint32_t height, const float r, const float g, const float b, const float a);
static void init_sampler_presets(AppState& state);

SDL_AppResult graphics_init(AppState& state, [[maybe_unused]] int argc, [[maybe_unused]] char** argv) {

    state.graphics = new GraphicState {}; // Freed in graphics_quit()
    GraphicState& graphics = *state.graphics;

    SDL_GetHintBoolean(SDL_HINT_RENDER_VULKAN_DEBUG, true);

    graphics.staticTextures[DefaultWhite] = createSolidColorTextureRGBA8(state.device, 32u, 32u, 1.f, 1.f, 1.f, 1.f);
    if (!graphics.staticTextures[DefaultWhite]) [[unlikely]]
        return SDL_APP_FAILURE;

    if (SDL_AppResult result = graphics_create_render_targets(state, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT); result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    imgui_init(state);
    init_sampler_presets(state);

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_real_distribution<float> dist1000(-5, 5);
    std::uniform_real_distribution<float> dist255(0, 256);
    generator.seed(38);

    static constexpr std::size_t numParticles = 5000; // Total number of random particles

    // Half-extents to center the cloud at origin

    graphics.particles.resize(numParticles);
    Particle* p = graphics.particles.data();

    for (std::size_t i = 0; i < numParticles; ++i) {
        // Random position within a centered cube
        p->position[0] = dist1000(generator);
        p->position[1] = dist1000(generator);
        p->position[2] = dist1000(generator);
        p->position[3] = 1.0f;

        // Random color
        p->color[0] = dist255(generator);
        p->color[1] = dist255(generator);
        p->color[2] = dist255(generator);
        p->color[3] = 1.0f;

        ++p;
    }

    graphics.boxes.resize(1);

    graphics.boxes[0].min[0] = -0.5f;
    graphics.boxes[0].min[1] = -0.5f;
    graphics.boxes[0].min[2] = -0.5f;

    graphics.boxes[0].max[0] = 0.5f;
    graphics.boxes[0].max[1] = 0.5f;
    graphics.boxes[0].max[2] = 0.5f;

    if (SDL_AppResult result = deferred_lighting_init(state); result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    if (SDL_AppResult result = deferred_gbuffer_init(state); result != SDL_APP_CONTINUE) [[unlikely]]
        return result;

    // if (SDL_AppResult result = deferred_ssao_init(state); result != SDL_APP_CONTINUE) [[unlikely]]
    //     return result;

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

    deferred_gbuffer_render(state, cmdbuf);

    // deferred_ssao_render(state, cmdbuf);

    // ou

    /*
    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = state.textures[ssaoTexture];
    colorTarget.clear_color = SDL_FColor { 0.0f, 0.0f, 0.0f, 1.0f };
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTarget, 1, nullptr);
    deferred_ssao_render(state, renderPass, cmdbuf);
    SDL_EndGPURenderPass(renderPass);
    */

    imgui_iterate(state);
    ImDrawData* draw_data = ImGui::GetDrawData();
    Imgui_ImplSDLGPU3_PrepareDrawData(draw_data, cmdbuf);

    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = swapchainTexture;
    colorTarget.clear_color = SDL_FColor { 0.0f, 0.0f, 0.0f, 1.0f };
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTarget, 1, nullptr);

    state.graphics->displayMode = DisplayMode::Final;
    deferred_lighting_render_to_texture(state, renderPass, cmdbuf, state.graphics->displayMode); // le "to_texture" dans le nom je vais retirer
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmdbuf, renderPass);

    SDL_EndGPURenderPass(renderPass);

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return SDL_APP_CONTINUE;
}

SDL_AppResult graphics_event(AppState& state, SDL_Event& event) {
    imgui_event(state, event);

    if (event.type == SDL_EVENT_WINDOW_RESIZED) [[unlikely]] {
        int& width = event.window.data1;
        int& height = event.window.data2;
        SDL_AppResult result = graphics_create_render_targets(state, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
        if (result != SDL_APP_CONTINUE) [[unlikely]]
            return result;
    }

    return SDL_APP_CONTINUE;
}

void graphics_quit(AppState& state) {
    imgui_quit(state);
    if (state.graphics) {
        for (std::size_t i = 0; i < NumGraphicPipelines; i++) {
            if (state.graphics->graphicPipeline[i])
                SDL_ReleaseGPUGraphicsPipeline(state.device, state.graphics->graphicPipeline[i]);
        }

        for (std::size_t i = 0; i < NumComputePipelines; i++) {
            if (state.graphics->computePipeline[i])
                SDL_ReleaseGPUComputePipeline(state.device, state.graphics->computePipeline[i]);
        }

        for (std::size_t i = 0; i < NumBuffers; i++) {
            if (state.graphics->buffers[i])
                SDL_ReleaseGPUBuffer(state.device, state.graphics->buffers[i]);
        }

        for (std::size_t i = 0; i < NumTextures; i++) {
            if (state.graphics->textures[i])
                SDL_ReleaseGPUTexture(state.device, state.graphics->textures[i]);
        }

        for (std::size_t i = 0; i < NumStaticTextures; i++) {
            if (state.graphics->staticTextures[i])
                SDL_ReleaseGPUTexture(state.device, state.graphics->staticTextures[i]);
        }

        for (std::size_t i = 0; i < NumSamplers; i++) {
            if (state.graphics->samplersPreset[i])
                SDL_ReleaseGPUSampler(state.device, state.graphics->samplersPreset[i]);
        }

        delete state.graphics;
    }
}

static SDL_AppResult graphics_create_render_targets(AppState& state, std::uint32_t width, std::uint32_t height) {

    for (SDL_GPUTexture*& texture : state.graphics->textures) {
        if (texture) [[likely]] {
            SDL_ReleaseGPUTexture(state.device, texture);
            texture = nullptr;
        }
    }

    struct RenderTargetParams {
        const TextureIndex index;
        const SDL_GPUTextureFormat format;
        const SDL_GPUTextureUsageFlags usage;
    };

    constexpr RenderTargetParams params[] = {
        { GeometryPosition, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE },
        { GeometryNormal, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE },
        { GeometryAlbedo, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE },
        { GeometryDepth, SDL_GPU_TEXTUREFORMAT_D24_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET },
        { GeometryDepthModified, SDL_GPU_TEXTUREFORMAT_R32_FLOAT, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE },
    };

    SDL_GPUTextureCreateInfo createInfo = {};
    createInfo.type = SDL_GPU_TEXTURETYPE_2D;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    for (const auto& param : params) {
        createInfo.format = param.format;
        createInfo.usage = param.usage;

        state.graphics->textures[param.index] = SDL_CreateGPUTexture(state.device, &createInfo);
        if (!state.graphics->textures[param.index]) [[unlikely]] {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create render target %d: %s", param.index, SDL_GetError());
            return SDL_APP_FAILURE;
        }
    }

    return SDL_APP_CONTINUE;
}

static SDL_GPUTexture* createSolidColorTextureRGBA8(SDL_GPUDevice* device, std::uint32_t width, std::uint32_t height, const float r, const float g, const float b, const float a) {

    const std::uint32_t pixelCount = width * height;
    std::vector<uint8_t> pixelData(pixelCount * 4); // 4 bytes per pixel (RGBA8)

    // Otherwise, fill each pixel with the specified color
    const uint8_t r8 = static_cast<uint8_t>(r * 255.f);
    const uint8_t g8 = static_cast<uint8_t>(g * 255.f);
    const uint8_t b8 = static_cast<uint8_t>(b * 255.f);
    const uint8_t a8 = static_cast<uint8_t>(a * 255.f);

    for (size_t i = 0; i < pixelCount; ++i) {
        pixelData[i * 4 + 0] = r8;
        pixelData[i * 4 + 1] = g8;
        pixelData[i * 4 + 2] = b8;
        pixelData[i * 4 + 3] = a8;
    }

    SDL_GPUTextureCreateInfo createInfo = {};
    createInfo.type = SDL_GPU_TEXTURETYPE_2D;
    createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    createInfo.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &createInfo);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create solid color texture: %s", SDL_GetError());
        return nullptr;
    }

    // Create and fill transfer buffer
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = static_cast<std::uint32_t>(pixelData.size());

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    if (!transferBuffer) {
        SDL_ReleaseGPUTexture(device, texture);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create transfer buffer: %s", SDL_GetError());
        return nullptr;
    }

    // Copy data to transfer buffer
    void* transferData = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    if (!transferData) {
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to map transfer buffer: %s", SDL_GetError());
        return nullptr;
    }

    SDL_memcpy(transferData, pixelData.data(), pixelData.size());
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    // Upload to GPU texture
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(device);
    if (!cmdbuf) {
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        SDL_ReleaseGPUTexture(device, texture);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to acquire command buffer: %s", SDL_GetError());
        return nullptr;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
    SDL_GPUTextureTransferInfo textureTransfer = {};
    textureTransfer.transfer_buffer = transferBuffer;
    textureTransfer.offset = 0;

    SDL_GPUTextureRegion textureRegion = {};
    textureRegion.texture = texture;
    textureRegion.w = width;
    textureRegion.h = height;
    textureRegion.d = 1;

    SDL_UploadToGPUTexture(copyPass, &textureTransfer, &textureRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdbuf);
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

    // Store in texture map
    return texture;
}

static void init_sampler_presets(AppState& state) {
    assert(state.device != nullptr && "Device must be initialized before creating samplers");
    SDL_GPUDevice* device = state.device;

    SDL_GPUSamplerCreateInfo info {};
    GraphicState& gfx = *state.graphics;

    auto set_sampler = [&](SamplerPreset preset) {
        if (gfx.samplersPreset[preset]) {
            SDL_ReleaseGPUSampler(device, gfx.samplersPreset[preset]);
            gfx.samplersPreset[preset] = nullptr;
        }
        gfx.samplersPreset[preset] = SDL_CreateGPUSampler(device, &info);
    };

    // PointClamp
    info = {};
    info.min_filter = SDL_GPU_FILTER_NEAREST;
    info.mag_filter = SDL_GPU_FILTER_NEAREST;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    set_sampler(PointClamp);

    // PointWrap
    info = {};
    info.min_filter = SDL_GPU_FILTER_NEAREST;
    info.mag_filter = SDL_GPU_FILTER_NEAREST;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    set_sampler(PointWrap);

    // LinearClamp
    info = {};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    set_sampler(LinearClamp);

    // LinearWrap
    info = {};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    set_sampler(LinearWrap);

    // AnisotropicClamp
    info = {};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.enable_anisotropy = true;
    info.max_anisotropy = 4;
    set_sampler(AnisotropicClamp);

    // AnisotropicWrap
    info = {};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    info.enable_anisotropy = true;
    info.max_anisotropy = 4;
    set_sampler(AnisotropicWrap);
}
