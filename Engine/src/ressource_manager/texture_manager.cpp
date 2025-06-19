#include "pch.h"
#include "graphics/image.h"

#include "ressource_manager/image_manager.h"
#include "ressource_manager/texture_manager.h"

#include "system/log.h"
#include "system/window.h"

namespace Ressource
{
    std::shared_ptr<GTS::Window> TextureManager::pWindow;
    std::unordered_map<std::string, std::shared_ptr<SDL_GPUTexture>> TextureManager::m_textures;
    SDL_GPUDevice* TextureManager::gpu = nullptr;

    void TextureManager::init(std::shared_ptr<GTS::Window> window)
    {
        pWindow = window;
        gpu = window->getGpuDevice().as<SDL_GPUDevice>();
    }

    void TextureManager::releaseUnused()
    {
        for (auto it = m_textures.begin(); it != m_textures.end(); )
        {
            if (it->second.use_count() == 1)
            {
                SDL_ReleaseGPUTexture(gpu, it->second.get());
                it = m_textures.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    std::shared_ptr<SDL_GPUTexture> TextureManager::get(const std::string& texturePath, const TextureInfo& info)
    {
        auto it = m_textures.find(texturePath);
        if (it != m_textures.end())
        {
            GTS_TRACE("Texture found for {}, loading pre-existing texture", texturePath);

            return it->second;
        }

        auto texture = create(texturePath, info);
        return texture;
    }

    std::shared_ptr<SDL_GPUTexture> TextureManager::createSolidColorTextureRGBA8(
        const std::string& name_id,
        int width,
        int height,
        const float r,
        const float g,
        const float b,
        const float a)
    {
        auto it = m_textures.find(name_id);
        if (it != m_textures.end())
        {
            GTS_TRACE("Solid color texture {} already exists", name_id);
            return it->second;
        }

        if (!gpu) {
            GTS_ERROR("TextureManager not initialized!");
            return nullptr;
        }

        if (width <= 0 || height <= 0) {
            GTS_ERROR("Invalid texture dimensions {}x{}", width, height);
            return nullptr;
        }

        const size_t pixelCount = width * height;
        std::vector<uint8_t> pixelData(pixelCount * 4); // 4 bytes per pixel (RGBA8)

        const uint8_t r8 = static_cast<uint8_t>(r * 255.f);
        const uint8_t g8 = static_cast<uint8_t>(g * 255.f);
        const uint8_t b8 = static_cast<uint8_t>(b * 255.f);
        const uint8_t a8 = static_cast<uint8_t>(a * 255.f);

        for (size_t i = 0; i < pixelCount; ++i) 
        {
            pixelData[i * 4 + 0] = r8;
            pixelData[i * 4 + 1] = g8;
            pixelData[i * 4 + 2] = b8;
            pixelData[i * 4 + 3] = a8;
        }


        TextureInfo info;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.mipLevels = 1;

        SDL_GPUTextureCreateInfo createInfo = {};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = info.format;
        createInfo.usage = info.usage;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = info.mipLevels;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(gpu, &createInfo);
        if (!texture)
        {
            GTS_ERROR("Failed to create solid color texture: {}", SDL_GetError());
            return nullptr;
        }

        // Create and fill transfer buffer
        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = pixelData.size();

        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpu, &transferInfo);
        if (!transferBuffer)
        {
            SDL_ReleaseGPUTexture(gpu, texture);
            GTS_ERROR("Failed to create transfer buffer: {}", SDL_GetError());
            return nullptr;
        }

        // Copy data to transfer buffer
        void* transferData = SDL_MapGPUTransferBuffer(gpu, transferBuffer, false);
        if (!transferData)
        {
            SDL_ReleaseGPUTransferBuffer(gpu, transferBuffer);
            SDL_ReleaseGPUTexture(gpu, texture);
            GTS_ERROR("Failed to map transfer buffer: {}", SDL_GetError());
            return nullptr;
        }

        memcpy(transferData, pixelData.data(), pixelData.size());
        SDL_UnmapGPUTransferBuffer(gpu, transferBuffer);

        // Upload to GPU texture
        SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(gpu);
        if (!cmdbuf)
        {
            SDL_ReleaseGPUTransferBuffer(gpu, transferBuffer);
            SDL_ReleaseGPUTexture(gpu, texture);
            GTS_ERROR("Failed to acquire command buffer: {}", SDL_GetError());
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
        SDL_ReleaseGPUTransferBuffer(gpu, transferBuffer);

        auto sharedTexture = std::shared_ptr<SDL_GPUTexture>(
            texture,
            [](SDL_GPUTexture* tex) { if (gpu) SDL_ReleaseGPUTexture(gpu, tex); }
        );

        // Store in texture map
        m_textures[name_id] = sharedTexture;
        return sharedTexture;
    }

    std::shared_ptr<SDL_GPUTexture> TextureManager::create(
        const std::string& texturePath,
        const TextureInfo& info)
    {
        auto it = m_textures.find(texturePath);
        if (it != m_textures.end())
        {
            return it->second;
        }

        auto image = ImageManager::get(texturePath);
        if (!image || !image->isValid())
        {
            GTS_ERROR("Failed to load image for texture: {}", texturePath);
            return nullptr;
        }

        SDL_GPUTexture* texture = createTextureFromImage(*image, info);
        if (!texture)
        {
            GTS_ERROR("Failed to create texture from image: {}", texturePath);
            return nullptr;
        }

        auto sharedTexture = std::shared_ptr<SDL_GPUTexture>(
            texture,
            [](SDL_GPUTexture* tex) { if (gpu) SDL_ReleaseGPUTexture(gpu, tex); }
        );

        m_textures[texturePath] = sharedTexture;
        return sharedTexture;
    }



    std::shared_ptr<SDL_GPUTexture> TextureManager::createRenderTarget(const std::string& name_id, int width, int height, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage)
    {
        auto it = m_textures.find(name_id);
        if (it != m_textures.end())
        {
            GTS_TRACE("Texture found for {}, loading pre-existing texture", name_id);

            return it->second;
        }

        SDL_GPUTextureCreateInfo createInfo = {};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = format;
        createInfo.usage = usage;
        createInfo.width = static_cast<Uint32>(width);
        createInfo.height = static_cast<Uint32>(height);
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(gpu, &createInfo);
        if (!texture)
        {
            GTS_ERROR("Failed to create render target: {}", SDL_GetError());
            return nullptr;
        }

        return std::shared_ptr<SDL_GPUTexture>(
            texture,
            [](SDL_GPUTexture* tex) { if (gpu) SDL_ReleaseGPUTexture(gpu, tex); }
        );
    }

    SDL_GPUTexture* TextureManager::createTextureFromImage(
        const GTS::Image& image,
        const TextureInfo& info)
    {
        if (!gpu) return nullptr;

        SDL_GPUTextureCreateInfo textureCreateInfo;
        textureCreateInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureCreateInfo.format = info.format;
        textureCreateInfo.usage = info.usage;
        textureCreateInfo.width = image.getWidth();
        textureCreateInfo.height = image.getHeight();
        textureCreateInfo.layer_count_or_depth = 1;
        textureCreateInfo.num_levels = info.mipLevels;
        textureCreateInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(gpu, &textureCreateInfo);
        if (!texture)
        {
            GTS_ERROR("Failed to create GPU texture: {}", SDL_GetError());
            return nullptr;
        }

        // Create transfer buffer
        SDL_GPUTransferBufferCreateInfo transferBufferInfo;
        transferBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferBufferInfo.size = image.getSize();
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpu, &transferBufferInfo);
        if (!transferBuffer)
        {
            SDL_ReleaseGPUTexture(gpu, texture);
            GTS_ERROR("Failed to create transfer buffer: {}", SDL_GetError());
            return nullptr;
        }

        // Copy image data to transfer buffer
        Uint8* transferPtr = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(gpu, transferBuffer, false));
        if (!transferPtr)
        {
            SDL_ReleaseGPUTransferBuffer(gpu, transferBuffer);
            SDL_ReleaseGPUTexture(gpu, texture);
            GTS_ERROR("Failed to map transfer buffer: {}", SDL_GetError());
            return nullptr;
        }

        SDL_memcpy(transferPtr, image.getData(), image.getSize());
        SDL_UnmapGPUTransferBuffer(gpu, transferBuffer);

        // Upload to GPU texture
        SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(gpu);
        if (!cmdbuf)
        {
            SDL_ReleaseGPUTransferBuffer(gpu, transferBuffer);
            SDL_ReleaseGPUTexture(gpu, texture);
            GTS_ERROR("Failed to acquire command buffer: {}", SDL_GetError());
            return nullptr;
        }

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
        SDL_GPUTextureTransferInfo transferInfo = {};
        transferInfo.transfer_buffer = transferBuffer;
        transferInfo.offset = 0;

        SDL_GPUTextureRegion textureRegion = {};
        textureRegion.texture = texture;
        textureRegion.w = image.getWidth();
        textureRegion.h = image.getHeight();
        textureRegion.d = 1;

        SDL_UploadToGPUTexture(copyPass, &transferInfo, &textureRegion, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmdbuf);
        SDL_ReleaseGPUTransferBuffer(gpu, transferBuffer);

        return texture;
    }



}