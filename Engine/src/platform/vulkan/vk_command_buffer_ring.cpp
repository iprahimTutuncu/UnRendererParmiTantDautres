#include "pch.h"
#include "platform/vulkan/vk_command_buffer_ring.h"
#include "platform/vulkan/vk_gpu_command_buffer.h"
#include "platform/vulkan/vk_gpu_device.h"
#include "graphics/gpu_type.h"

namespace Thsan
{
	void VkCommandBufferRing::init(GpuDevice* gpu)
	{
        this->vkGPU = static_cast<VkGpuDevice*>(gpu);

        for (uint32_t i = 0; i < kMaxPools; i++)
        {
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = vkGPU->graphicsQueueFamily;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            VK_CHECK(vkCreateCommandPool(vkGPU->device, &poolInfo, vkGPU->vulkanAllocationCallbacks, &vulkanCommandPools[i]));
        }

        for (uint32_t i = 0; i < kMaxBuffers; i++)
        {
            VkCommandBufferAllocateInfo cmd = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
            const uint32_t poolIndex = poolFromIndex(i);
            cmd.commandPool = vulkanCommandPools[poolIndex];
            cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmd.commandBufferCount = 1;
            VK_CHECK(vkAllocateCommandBuffers(vkGPU->device, &cmd, &vkGpuCommandBuffers[i].vkCommandBuffer));

            vkGpuCommandBuffers[i].setGpuDevice(gpu);
            vkGpuCommandBuffers[i].setHandle(i);
            vkGpuCommandBuffers[i].reset();
        }
	}

    void VkCommandBufferRing::shutdown()
    {
        for (u32 i = 0; i < kMaxSwapchainImages * kMaxThreads; i++) 
        {
            vkDestroyCommandPool(vkGPU->device, vulkanCommandPools[i], vkGPU->vulkanAllocationCallbacks);
        }
    }

    void VkCommandBufferRing::resetPools(u32 frameIndex)
    {
        // ex: max Thread is 2 and triple buffering
        // [0] -> Frame 0, Thread 0 | 0 * 2 + 0 = 0
        // [1] -> Frame 0, Thread 1 | 0 * 2 + 1 = 1
        // [2] -> Frame 1, Thread 0 | 1 * 2 + 0 = 2
        // [3] -> Frame 1, Thread 1 | 1 * 2 + 1 = 3
        // [4] -> Frame 2, Thread 0 | 2 * 2 + 0 = 4
        // [5] -> Frame 2, Thread 1 | 2 * 2 + 1 = 5

        for (u32 i = 0; i < kMaxThreads; i++) 
        {
            VK_CHECK(vkResetCommandPool(vkGPU->device, vulkanCommandPools[frameIndex * kMaxThreads + i], 0));
        }
    }

    GpuCommandBuffer* VkCommandBufferRing::getCommandBuffer(u32 frame, bool begin)
    {
        // TODO: take in account threads
        VkGpuCommandBuffer* cb = &vkGpuCommandBuffers[frame * kBufferPerPool];

        if (begin)
        {
            cb->reset();

            VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK(vkBeginCommandBuffer(cb->vkCommandBuffer, &beginInfo));
        }

        return cb;
    }

    GpuCommandBuffer* VkCommandBufferRing::getCommandBufferInstant(u32 frame, bool begin)
    {
        // will return the command before that doesn't interfer with the main draw commands
        // atleast to my understanding.
        // todo: evaluate if I actually need that or maybe refactor the name?
        VkGpuCommandBuffer* cb = &vkGpuCommandBuffers[frame * kBufferPerPool + 1];
        return cb;
    }
}