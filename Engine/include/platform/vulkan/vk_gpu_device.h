#pragma once
#include "graphics/gpu_device.h"
#include "graphics/gpu_command_buffer.h"
#include "graphics/gpu_type.h"

#include <cstdint>
#include <vector>

namespace Thsan
{

    struct FrameData
    {
        VkCommandPool commandPool;
        VkCommandBuffer mainCommandBuffer;
        VkSemaphore swapchainSemaphore, renderSemaphore;
        VkFence renderFence;
    };

    constexpr unsigned int FRAME_OVERLAP = 2;

    class VkGpuDevice: public GpuDevice
    {
    public:
        VkGpuDevice() = default;
        ~VkGpuDevice() = default;

        virtual void init(const DeviceCreation& creation) override;
        virtual void shutdown() override;

        // Swapchain
        virtual void createSwapchain(u32 width, u32 height) override;
        virtual void destroySwapchain() override;
        virtual void resizeSwapchain(u32 width, u32 height) override;


        // Command Buffers
        virtual GpuCommandBuffer* getCommandBuffer(QueueType::Enum type, bool begin) override;
        virtual GpuCommandBuffer* getInstantCommandBuffer() override;

        virtual void queueCommandBuffer(GpuCommandBuffer* command_buffer) override;

        // rendering
        virtual void draw() override;

    private:
        void initVulkan();
        void initSwapchain();
        void initCommands();
        void initSyncStructures();
        DeviceCreation creationInfo;

        bool isInitialized{ false };
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        //Command buffer
        GpuCommandBuffer** queuedCommandBuffers = nullptr;
        uint32_t numAllocatedCommandBuffers = 0;
        uint32_t numQueuedCommandBuffers = 0;
        VkAllocationCallbacks* vulkanAllocationCallbacks{ nullptr };

        //swapchain related
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkFormat swapchainImageFormat;

        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        VkExtent2D swapchainExtent;

        //debug related
        VkDebugUtilsMessengerEXT debug_messenger;

        u32 frameNumber;

        FrameData frames[FRAME_OVERLAP];
        FrameData& get_current_frame() { return frames[frameNumber % FRAME_OVERLAP]; };
        u32 get_current_frame_index() { return frameNumber % FRAME_OVERLAP; };

        VkQueue graphicsQueue;
        uint32_t graphicsQueueFamily;

        friend class VkCommandBufferRing;

    };
}
