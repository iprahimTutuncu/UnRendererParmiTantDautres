#pragma once
#include "graphics/gpu_type.h"
#include <memory>

namespace Thsan
{
    enum class WindowAPI
    {
        None,
        SDL3
    };

    struct DeviceCreation
    {
        uint32_t width;
        uint32_t height;
        void* window;
        WindowAPI windowAPI;

        void set_window(uint32_t w, uint32_t h, void* win, WindowAPI api)
        {
            width = w;
            height = h;
            window = win;
            windowAPI = api;
        }
    };

    class GpuCommandBuffer;

    class GpuDevice
    {
    public:
        virtual ~GpuDevice() = default;
        virtual void init(const DeviceCreation& creation) = 0;
        virtual void shutdown() = 0;
        virtual void createSwapchain(uint32_t width, uint32_t height) = 0;
        virtual void destroySwapchain() = 0;
        virtual void resizeSwapchain(uint32_t width, uint32_t height) = 0;
        virtual GpuCommandBuffer* getCommandBuffer(QueueType::Enum type, bool begin) = 0;
        virtual GpuCommandBuffer* getInstantCommandBuffer() = 0;
        virtual void queueCommandBuffer(GpuCommandBuffer* command_buffer) = 0;
        virtual void draw() = 0;

        static std::shared_ptr<GpuDevice> create();
    };
}
