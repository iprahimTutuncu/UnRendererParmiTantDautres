#include "pch.h"
#include "ressource_manager/sampler_manager.h"
#include "system/log.h"
#include "system/window.h"

namespace Ressource
{
    SDL_GPUDevice* SamplerManager::m_gpu = nullptr;
    std::unordered_map<SamplerManager::Preset, std::shared_ptr<SDL_GPUSampler>> SamplerManager::m_presetSamplers;
    std::unordered_map<std::string, std::shared_ptr<SDL_GPUSampler>> SamplerManager::m_customSamplers;

    void SamplerManager::init(std::shared_ptr<GTS::Window> window)
    {
        GTS::GpuHandle handle = window->getGpuDevice();
        if (GTS::get_graphic_API() != GTS::GraphicAPI::SDL3)
        {
            GTS_ERROR("SamplerManager::init() - Unsupported graphic API or null device");
            return;
        }

        m_gpu = handle.as<SDL_GPUDevice>();
        createPresetSamplers();
    }

    void SamplerManager::createPresetSamplers()
    {
        SDL_GPUSamplerCreateInfo info{};

        auto createSampler = [](const SDL_GPUSamplerCreateInfo& info)
        {
            return std::shared_ptr<SDL_GPUSampler>(
                SDL_CreateGPUSampler(m_gpu, &info),
                SamplerDeleter{ m_gpu }
            );
        };

        // PointClamp
        info = {};
        info.min_filter = SDL_GPU_FILTER_NEAREST;
        info.mag_filter = SDL_GPU_FILTER_NEAREST;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_presetSamplers[Preset::PointClamp] = createSampler(info);

        // PointWrap
        info = {};
        info.min_filter = SDL_GPU_FILTER_NEAREST;
        info.mag_filter = SDL_GPU_FILTER_NEAREST;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        m_presetSamplers[Preset::PointWrap] = createSampler(info);

        // LinearClamp
        info = {};
        info.min_filter = SDL_GPU_FILTER_LINEAR;
        info.mag_filter = SDL_GPU_FILTER_LINEAR;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_presetSamplers[Preset::LinearClamp] = createSampler(info);

        // LinearWrap
        info = {};
        info.min_filter = SDL_GPU_FILTER_LINEAR;
        info.mag_filter = SDL_GPU_FILTER_LINEAR;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        m_presetSamplers[Preset::LinearWrap] = createSampler(info);

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
        m_presetSamplers[Preset::AnisotropicClamp] = createSampler(info);

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
        m_presetSamplers[Preset::AnisotropicWrap] = createSampler(info);
    }


     std::shared_ptr<SDL_GPUSampler> SamplerManager::get(Preset preset)
    {
        auto it = m_presetSamplers.find(preset);
        if (it != m_presetSamplers.end())
            return it->second;

        GTS_ERROR("Requested unknown preset sampler!");
        return nullptr;
    }

     std::shared_ptr<SDL_GPUSampler> SamplerManager::get(const std::string& name)
    {
        auto it = m_customSamplers.find(name);
        if (it != m_customSamplers.end())
            return it->second;

        GTS_ERROR("Sampler with name {} not found!", name.c_str());
        return nullptr;
    }

     std::shared_ptr<SDL_GPUSampler> SamplerManager::create(const std::string& name, const SDL_GPUSamplerCreateInfo& info)
    {
         if (m_customSamplers.contains(name)) 
         {
             GTS_WARN("Sampler {} already exists, returning existing one.", name.c_str());
             return m_customSamplers[name];
         }

         auto sampler = std::shared_ptr<SDL_GPUSampler>(
             SDL_CreateGPUSampler(m_gpu, &info),
             SamplerDeleter{ m_gpu }
         );

         if (!sampler)
         {
             GTS_ERROR("Failed to create custom sampler {}", name.c_str());
             return nullptr;
         }

         m_customSamplers[name] = sampler;
         return sampler;
    }

     void SamplerManager::releaseUnused() 
     {
         for (auto it = m_presetSamplers.begin(); it != m_presetSamplers.end(); ) 
         {
             if (it->second.use_count() == 1)
                 it = m_presetSamplers.erase(it);
             else
                 ++it;
         }

         for (auto it = m_customSamplers.begin(); it != m_customSamplers.end(); ) 
         {
             if (it->second.use_count() == 1)
                 it = m_customSamplers.erase(it);
             else
                 ++it;
         }
     }
}
