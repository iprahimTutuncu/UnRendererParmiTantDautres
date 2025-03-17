#pragma once
#include "graphics/gpu_type.h"
#include "platform/vulkan/vk_gpu_command_buffer.h"

namespace Thsan 
{
	class GpuDevice;
	class VkGpuDevice;
	class GpuCommandBuffer;
	class VkGpuCommandBuffer;

	class VkCommandBufferRing
	{
	public:
		void init(GpuDevice* gpu);
		void shutdown();

		void resetPools(u32 frameIndex);

		GpuCommandBuffer* getCommandBuffer(u32 frame, bool begin);
		GpuCommandBuffer* getCommandBufferInstant(u32 frame, bool begin);

		static u16 poolFromIndex(u32 index) { return (u16)index / kBufferPerPool; }

		static const u32 kMaxSwapchainImages = 3; // à ramener dans GPU Ressources potentiellement
		static const u16 kMaxThreads = 1;
		static const u16 kMaxPools = kMaxSwapchainImages * kMaxThreads;
		static const u16 kBufferPerPool = 4;
		static const u16 kMaxBuffers = kBufferPerPool * kMaxPools;
	private:
		VkGpuDevice* vkGPU;
		VkCommandPool vulkanCommandPools[kMaxPools];
		VkGpuCommandBuffer vkGpuCommandBuffers[kMaxBuffers];
		u8 nextFreePerThreadFrame[kMaxPools];
	};
}