#include "texture.h"
#include "graphics.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

void init_sampler_presets(AppState& state) {
    SDL_GPUDevice* device = state.device;
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "init_sampler_presets: GPU device is null");
        return;
    }

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

SDL_AppResult createRenderTarget(AppState& state, TextureIndex index, int width, int height, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage) {

    SDL_GPUTextureCreateInfo createInfo = {};
    createInfo.type = SDL_GPU_TEXTURETYPE_2D;
    createInfo.format = format;
    createInfo.usage = usage;
    createInfo.width = static_cast<Uint32>(width);
    createInfo.height = static_cast<Uint32>(height);
    createInfo.layer_count_or_depth = 1;
    createInfo.num_levels = 1;
    createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTexture* texture = SDL_CreateGPUTexture(state.device, &createInfo);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create render target:", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state.graphics->textures[index] = texture;
    return SDL_APP_CONTINUE;
}

SDL_AppResult createSolidColorTextureRGBA8(AppState& state, TextureIndex index, int width, int height, const float r, const float g, const float b, const float a) {

    const size_t pixelCount = width * height;
    std::vector<uint8_t> pixelData(pixelCount * 4); // 4 bytes per pixel (RGBA8)

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

    SDL_GPUTexture* texture = SDL_CreateGPUTexture(state.device, &createInfo);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create solid color texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create and fill transfer buffer
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = (int) pixelData.size();

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(state.device, &transferInfo);
    if (!transferBuffer) {
        SDL_ReleaseGPUTexture(state.device, texture);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create transfer buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Copy data to transfer buffer
    void* transferData = SDL_MapGPUTransferBuffer(state.device, transferBuffer, false);
    if (!transferData) {
        SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
        SDL_ReleaseGPUTexture(state.device, texture);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to map transfer buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    memcpy(transferData, pixelData.data(), pixelData.size());
    SDL_UnmapGPUTransferBuffer(state.device, transferBuffer);

    // Upload to GPU texture
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(state.device);
    if (!cmdbuf) {
        SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
        SDL_ReleaseGPUTexture(state.device, texture);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
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
    SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);

    // Store in texture map
    state.graphics->textures[index] = texture;
    return SDL_APP_CONTINUE;
}
