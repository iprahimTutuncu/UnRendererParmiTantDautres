#pragma once
#include <SDL3/SDL_gpu.h>
#include <unordered_map>
#include <string>
#include <memory>

namespace GTS
{
    class Window;
}

namespace Ressource
{
    class SamplerManager
    {
    public:
        enum class Preset
        {
            PointClamp,
            PointWrap,
            LinearClamp,
            LinearWrap,
            AnisotropicClamp,
            AnisotropicWrap
        };

        static std::shared_ptr<SDL_GPUSampler> get(Preset preset);
        static std::shared_ptr<SDL_GPUSampler> get(const std::string& name);
        static std::shared_ptr<SDL_GPUSampler> create(const std::string& name, const SDL_GPUSamplerCreateInfo& info);

        static void releaseUnused();
        static void init(std::shared_ptr<GTS::Window> window);

    private:
        struct SamplerDeleter 
        {
            void operator()(SDL_GPUSampler* sampler) const
            {
                if (sampler && m_gpu)
                    SDL_ReleaseGPUSampler(m_gpu, sampler);
            }
            SDL_GPUDevice* m_gpu;
        };
        static SDL_GPUDevice* m_gpu;
        static std::unordered_map<Preset, std::shared_ptr<SDL_GPUSampler>> m_presetSamplers;
        static std::unordered_map<std::string, std::shared_ptr<SDL_GPUSampler>> m_customSamplers;

        static void createPresetSamplers();
        SamplerManager() = delete;
    };
}