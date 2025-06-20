#pragma once

#include <SDL3/SDL_gpu.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace GTS {
    class Window;
    class Image;
}

namespace Ressource {
    class TextureManager {
    public:
        struct TextureInfo {
            SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            SDL_GPUTextureUsageFlags usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            int mipLevels = 1;
        };

        static void init(std::shared_ptr<GTS::Window> window);
        static void releaseUnused();

        static std::shared_ptr<SDL_GPUTexture> get(const std::string& texturePath, const TextureInfo& info = {});

        static std::shared_ptr<SDL_GPUTexture> createSolidColorTextureRGBA8(const std::string& name_id, int width, int height, const float r, const float g, const float b, const float a);

        static std::shared_ptr<SDL_GPUTexture> createRenderTarget(
            const std::string& name_id,
            int width,
            int height,
            SDL_GPUTextureFormat format,
            SDL_GPUTextureUsageFlags usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);

    private:
        static std::shared_ptr<SDL_GPUTexture> create(const std::string& texturePath, const TextureInfo& info);
        static std::shared_ptr<GTS::Window> pWindow;
        static std::unordered_map<std::string, std::shared_ptr<SDL_GPUTexture>> m_textures;
        static SDL_GPUDevice* gpu;

        static SDL_GPUTexture* createTextureFromImage(const GTS::Image& image, const TextureInfo& info);

        TextureManager() = delete;
    };
}
