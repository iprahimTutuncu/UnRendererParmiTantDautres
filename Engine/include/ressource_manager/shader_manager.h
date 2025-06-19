#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include <SDL3/SDL_gpu.h>

namespace GTS
{
    class Window;
}

namespace Ressource
{

    class ShaderManager
    {
    public:

        static std::shared_ptr<SDL_GPUShader> get(SDL_GPUDevice* device,
            const std::string& shaderFilename,
            Uint32 samplerCount,
            Uint32 uniformBufferCount,
            Uint32 storageBufferCount,
            Uint32 storageTextureCount);

        static void releaseUnused();
        static void init(std::shared_ptr<GTS::Window> window);

    private:
        static std::shared_ptr<GTS::Window> window;
        static std::unordered_map<std::string, std::shared_ptr<SDL_GPUShader>> p_shaders;

        static SDL_GPUShader* loadShader(SDL_GPUDevice* device,
            const std::string& shaderFilename,
            Uint32 samplerCount,
            Uint32 uniformBufferCount,
            Uint32 storageBufferCount,
            Uint32 storageTextureCount);

        static std::string getFullPath(const std::string& shaderFilename, SDL_GPUShaderFormat& outFormat, const char*& outEntrypoint);
        ShaderManager() = delete;
    };
};
