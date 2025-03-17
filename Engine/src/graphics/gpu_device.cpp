#include "pch.h"
#include "graphics/gpu_device.h"
#include "graphics/graphic_api.h"
#include "platform/vulkan/vk_gpu_device.h"
#include "system/log.h"

namespace Thsan
{
    std::shared_ptr<GpuDevice> Thsan::GpuDevice::create()
    {
        if (get_graphic_API() == GraphicAPI::Vulkan)
        {
            return std::make_shared<VkGpuDevice>();
        }

        TS_ASSERT("In Thsan::GpuDevice::create(), device wasn't created.", true);
        return nullptr;
    }
}