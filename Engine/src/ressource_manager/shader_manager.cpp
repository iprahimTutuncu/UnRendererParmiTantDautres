#include "shader_manager.h"

#include "../system/log.h"
#include "../system/window.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_log.h>

extern const char* BasePath;

namespace Ressource {
    std::shared_ptr<GTS::Window> Ressource::ShaderManager::window;
    std::unordered_map<std::string, std::shared_ptr<SDL_GPUShader>> ShaderManager::p_shaders;

    std::shared_ptr<SDL_GPUShader> ShaderManager::get(SDL_GPUDevice* device,
        const std::string& shaderFilename,
        Uint32 samplerCount,
        Uint32 uniformBufferCount,
        Uint32 storageBufferCount,
        Uint32 storageTextureCount) {
        auto it = p_shaders.find(shaderFilename);
        if (it != p_shaders.end())
            return it->second;

        SDL_GPUShader* shader = loadShader(device, shaderFilename,
            samplerCount, uniformBufferCount,
            storageBufferCount, storageTextureCount);

        if (!shader) {
            GTS_ERROR(
                "ShaderManager::get() - Failed to load shader:\n"
                "  Path: {}\n"
                "  Parameters:\n"
                "    Samplers: {}\n"
                "    Uniform Buffers: {}\n"
                "    Storage Buffers: {}\n"
                "    Storage Textures: {}",
                shaderFilename,
                samplerCount,
                uniformBufferCount,
                storageBufferCount,
                storageTextureCount);
            return nullptr;
        }

        p_shaders[shaderFilename] = std::shared_ptr<SDL_GPUShader>(shader,
            [](SDL_GPUShader* shader) {
                GTS::GpuHandle handle = window->getGpuDevice();
                if (GTS::get_graphic_API() != GTS::GraphicAPI::SDL3) {
                    GTS_ERROR("in removeUnused(), no gpu device found or supported");
                }

                SDL_GPUDevice* gpu = handle.as<SDL_GPUDevice>();

                SDL_ReleaseGPUShader(gpu, shader);
            });

        return p_shaders[shaderFilename];
    }

    SDL_GPUShader* ShaderManager::loadShader(SDL_GPUDevice* device,
        const std::string& shaderFilename,
        Uint32 samplerCount,
        Uint32 uniformBufferCount,
        Uint32 storageBufferCount,
        Uint32 storageTextureCount) {
        SDL_GPUShaderStage stage;
        if (shaderFilename.find(".vert") != std::string::npos)
            stage = SDL_GPU_SHADERSTAGE_VERTEX;
        else if (shaderFilename.find(".frag") != std::string::npos)
            stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        else {
            GTS_ERROR("Invalid shader stage: %s", shaderFilename.c_str());
            return nullptr;
        }

        SDL_GPUShaderFormat format;
        const char* entrypoint;
        std::string fullPath = getFullPath(shaderFilename, format, entrypoint);

        if (format == SDL_GPU_SHADERFORMAT_INVALID) {
            GTS_ERROR("Unrecognized backend shader format!");
            return nullptr;
        }

        size_t codeSize;
        void* code = SDL_LoadFile(fullPath.c_str(), &codeSize);
        if (!code) {
            GTS_ERROR("Failed to load shader from disk! %s", fullPath.c_str());
            return nullptr;
        }

        SDL_GPUShaderCreateInfo info = {
            .code_size = codeSize,
            .code = static_cast<const Uint8*>(code),
            .entrypoint = entrypoint,
            .format = format,
            .stage = stage,
            .num_samplers = samplerCount,
            .num_storage_textures = storageTextureCount,
            .num_storage_buffers = storageBufferCount,
            .num_uniform_buffers = uniformBufferCount
        };

        SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
        SDL_free(code);

        if (!shader)
            GTS_ERROR("Failed to create shader from file: %s", fullPath.c_str());

        return shader;
    }

    std::string ShaderManager::getFullPath(const std::string& shaderFilename, SDL_GPUShaderFormat& outFormat, const char*& outEntrypoint) {
        GTS::GpuHandle handle = window->getGpuDevice();
        if (GTS::get_graphic_API() != GTS::GraphicAPI::SDL3) {
            GTS_ERROR("in removeUnused(), no gpu device found or supported");
        }

        SDL_GPUDevice* gpu = handle.as<SDL_GPUDevice>();
        SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(gpu);
        const char* BasePath = SDL_GetBasePath();
        char fullPath[256];

        if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
            std::snprintf(fullPath, sizeof(fullPath), "%smedia/shaders/compiled/SPIRV/%s.spv", BasePath, shaderFilename.c_str());
            outFormat = SDL_GPU_SHADERFORMAT_SPIRV;
            outEntrypoint = "main";
        } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
            std::snprintf(fullPath, sizeof(fullPath), "%smedia/shaders/compiled/MSL/%s.msl", BasePath, shaderFilename.c_str());
            outFormat = SDL_GPU_SHADERFORMAT_MSL;
            outEntrypoint = "main0";
        } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
            std::snprintf(fullPath, sizeof(fullPath), "%smedia/shaders/compiled/DXIL/%s.dxil", BasePath, shaderFilename.c_str());
            outFormat = SDL_GPU_SHADERFORMAT_DXIL;
            outEntrypoint = "main";
        } else {
            outFormat = SDL_GPU_SHADERFORMAT_INVALID;
            outEntrypoint = nullptr;
            return "";
        }

        return std::string(fullPath);
    }

    void ShaderManager::releaseUnused() {
        for (auto it = p_shaders.begin(); it != p_shaders.end();) {
            if (it->second.use_count() == 1) {
                GTS_TRACE("ShaderManager - Releasing unused shader: {}", it->first);
                it = p_shaders.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ShaderManager::init(std::shared_ptr<GTS::Window> window) {
        ShaderManager::window = window;
    }

}
