#include "pch.h"
#include "platform/vulkan/vk_gpu_device.h"
#include "platform/vulkan/vk_gpu_command_buffer.h"
#include "platform/vulkan/vk_command_buffer_ring.h"

#include <vk_bootstrap/VkBootstrap.h>
#include <SDL3/SDL_vulkan.h>
#include <system/log.h>

constexpr bool bUseValidationLayers = true;

namespace Thsan
{
	static VkCommandBufferRing vkCommandBufferRing;

	void VkGpuDevice::init(const DeviceCreation& creation)
	{
		TS_TRACE("Initializing VkGpuDevice...");

		creationInfo = creation;

		initVulkan();
		initSwapchain();
		initCommands();
		initSyncStructures();

		isInitialized = true;
	}

	void VkGpuDevice::shutdown()
	{
		TS_TRACE("Shutting down VkGpuDevice...");

		if (isInitialized)
		{
			vkDeviceWaitIdle(device);

			vkCommandBufferRing.shutdown();

			for (int i = 0; i < FRAME_OVERLAP; i++)
				vkDestroyCommandPool(device, frames[i].commandPool, nullptr);
			
			destroySwapchain();

			vkDestroySurfaceKHR(instance, surface, nullptr);
			vkDestroyDevice(device, nullptr);

			vkb::destroy_debug_utils_messenger(instance, debug_messenger);
			vkDestroyInstance(instance, nullptr);


			TS_INFO("VkGpuDevice has been shutdown successfully, SDL_window can now be destroyed.");
		}
		else
		{
			TS_ERROR("Couldn't shutdown VkGpuDevice, device is still running.");
		}
	}

	void VkGpuDevice::createSwapchain(u32 width, u32 height)
	{
		TS_TRACE("Creating swapchain with dimensions: {}x{}...", width, height);

		vkb::SwapchainBuilder swapchainBuilder{ physical_device, device, surface };
		swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

		TS_INFO("Setting desired swapchain format: VK_FORMAT_B8G8R8A8_UNORM");

		auto swapchainResult = swapchainBuilder
			.set_desired_format(VkSurfaceFormatKHR{ .format = swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
			.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
			.set_desired_extent(width, height)
			.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
			.build();

		if (!swapchainResult)
		{
			TS_ERROR("Failed to create Vulkan swapchain");
			return;
		}

		vkb::Swapchain vkbSwapchain = swapchainResult.value();
		swapchainExtent = vkbSwapchain.extent;
		swapchain = vkbSwapchain.swapchain;

		TS_INFO("Swapchain created successfully: extent {}x{}", swapchainExtent.width, swapchainExtent.height);

		auto imagesResult = vkbSwapchain.get_images();
		if (!imagesResult)
		{
			TS_ERROR("Failed to retrieve swapchain images");
			return;
		}
		swapchainImages = imagesResult.value();

		auto imageViewsResult = vkbSwapchain.get_image_views();
		if (!imageViewsResult)
		{
			TS_ERROR("Failed to retrieve swapchain image views");
			return;
		}

		swapchainImageViews = imageViewsResult.value();

		TS_INFO("Swapchain setup completed with {} images", swapchainImages.size());
	}

	void VkGpuDevice::destroySwapchain()
	{
		vkDestroySwapchainKHR(device, swapchain, nullptr);

		for (int i = 0; i < swapchainImageViews.size(); i++)
			vkDestroyImageView(device, swapchainImageViews[i], nullptr);
	}

	GpuCommandBuffer* VkGpuDevice::getCommandBuffer(QueueType::Enum type, bool begin)
	{
		GpuCommandBuffer* cb = vkCommandBufferRing.getCommandBuffer(get_current_frame_index(), begin);
		return cb;
	}

	GpuCommandBuffer* VkGpuDevice::getInstantCommandBuffer()
	{
		GpuCommandBuffer* cb = vkCommandBufferRing.getCommandBufferInstant(get_current_frame_index(), false);
		return cb;
	}

	void VkGpuDevice::queueCommandBuffer(GpuCommandBuffer* command_buffer)
	{
		queuedCommandBuffers[numQueuedCommandBuffers++] = command_buffer;
	}

	VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask)
	{
		VkImageSubresourceRange subImage{};
		subImage.aspectMask = aspectMask;
		subImage.baseMipLevel = 0;
		subImage.levelCount = VK_REMAINING_MIP_LEVELS;
		subImage.baseArrayLayer = 0;
		subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

		return subImage;
	}

	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
	{
		VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		imageBarrier.pNext = nullptr;

		imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

		imageBarrier.oldLayout = currentLayout;
		imageBarrier.newLayout = newLayout;

		VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange = image_subresource_range(aspectMask);
		imageBarrier.image = image;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.pNext = nullptr;

		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &imageBarrier;

		vkCmdPipelineBarrier2(cmd, &depInfo);
	}

	VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
	{
		VkSemaphoreSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.semaphore = semaphore;
		submitInfo.stageMask = stageMask;
		submitInfo.deviceIndex = 0;
		submitInfo.value = 1;

		return submitInfo;
	}

	VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd)
	{
		VkCommandBufferSubmitInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		info.pNext = nullptr;
		info.commandBuffer = cmd;
		info.deviceMask = 0;

		return info;
	}

	VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
		VkSemaphoreSubmitInfo* waitSemaphoreInfo)
	{
		VkSubmitInfo2 info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		info.pNext = nullptr;

		info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
		info.pWaitSemaphoreInfos = waitSemaphoreInfo;

		info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
		info.pSignalSemaphoreInfos = signalSemaphoreInfo;

		info.commandBufferInfoCount = 1;
		info.pCommandBufferInfos = cmd;

		return info;
	}

	void VkGpuDevice::draw()
	{
		//dans le livre, d'ici jusqu'à resetPools, c'est dans la méthode new_frame

		// Fence wait and reset
		VkFence* render_complete_fence = &get_current_frame().renderFence;

		if (vkGetFenceStatus(device, *render_complete_fence) != VK_SUCCESS)
		{
			vkWaitForFences(device, 1, render_complete_fence, VK_TRUE, UINT64_MAX);
		}


		uint32_t swapchainImageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, get_current_frame().swapchainSemaphore, VK_NULL_HANDLE, &swapchainImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) 
		{
			resizeSwapchain(swapchainExtent.width, swapchainExtent.height);
		}

		vkResetFences(device, 1, render_complete_fence);

		// Command pool reset
		vkCommandBufferRing.resetPools(get_current_frame_index());

		// à partir d'ici, dans le livre c'est vulkan_create_swapchain_pass
		GpuCommandBuffer* gpu_command_buffer = this->getInstantCommandBuffer();
		VkGpuCommandBuffer* vk_gpu_command_buffer = static_cast<VkGpuCommandBuffer*>(gpu_command_buffer);
		
		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VkCommandBuffer cmd = vk_gpu_command_buffer->vkCommandBuffer;

		VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));


		//TRANSITION IMAGE
		transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

		VkClearColorValue clearValue;
		float flash = std::abs(std::sin(frameNumber / 120.f));
		clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

		VkImageSubresourceRange clearRange = image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
		vkCmdClearColorImage(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
		transition_image(cmd, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		VK_CHECK(vkEndCommandBuffer(cmd));

		VkCommandBufferSubmitInfo cmdinfo = command_buffer_submit_info(cmd);

		VkSemaphoreSubmitInfo waitInfo = semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchainSemaphore);
		VkSemaphoreSubmitInfo signalInfo = semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().renderSemaphore);

		VkSubmitInfo2 submit = submit_info(&cmdinfo, &signalInfo, &waitInfo);

		//submit command buffer to the queue and execute it.
		// _renderFence will now block until the graphic commands finish execution
		VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, get_current_frame().renderFence));


		//prepare present
		// this will put the image we just rendered to into the visible window.
		// we want to wait on the _renderSemaphore for that, 
		// as its necessary that drawing commands have finished before the image is displayed to the user
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.swapchainCount = 1;

		presentInfo.pWaitSemaphores = &get_current_frame().renderSemaphore;
		presentInfo.waitSemaphoreCount = 1;

		presentInfo.pImageIndices = &swapchainImageIndex;

		VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));

		//increase the number of frames drawn
		frameNumber++;
		TS_TRACE("FRAME NUM: {}", frameNumber);
	}

	void VkGpuDevice::resizeSwapchain(u32 width, u32 height)
	{
		destroySwapchain();
		createSwapchain(width, height);
	}

	void VkGpuDevice::initVulkan()
	{
		vkb::InstanceBuilder builder;

		// Create the Vulkan instance with basic debug features
		auto inst_ret = builder.set_app_name("Example Vulkan Application")
			.request_validation_layers(bUseValidationLayers)
			.use_default_debug_messenger()
			.require_api_version(1, 3, 0)
			.build();

		if (!inst_ret)
		{
			TS_CRITICAL("Failed to create Vulkan instance: {}", inst_ret.error().message());
			return;
		}

		vkb::Instance vkb_inst = inst_ret.value();
		instance = vkb_inst.instance;
		debug_messenger = vkb_inst.debug_messenger;

		TS_INFO("Vulkan instance created successfully.");

		// Create the surface based on the window API
		if (creationInfo.windowAPI == WindowAPI::SDL3)
		{
			if (!SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(creationInfo.window), instance, nullptr, &surface))
			{
				TS_ERROR("SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
				return;
			}
			TS_INFO("Vulkan surface created successfully.");
		}
		else
		{
			TS_ERROR("Couldn't create surface, unsupported windowAPI.");
			return;
		}

		// Vulkan 1.3 features
		VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		features.dynamicRendering = true;
		features.synchronization2 = true;

		// Vulkan 1.2 features
		VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		features12.bufferDeviceAddress = true;
		features12.descriptorIndexing = true;

		// Select a physical GPU that supports Vulkan 1.3 and our required features
		TS_TRACE("Selecting physical device...");
		vkb::PhysicalDeviceSelector selector{ vkb_inst };
		auto physDeviceResult = selector
			.set_minimum_version(1, 3)
			.set_required_features_13(features)
			.set_required_features_12(features12)
			.set_surface(surface)
			.select();

		if (!physDeviceResult)
		{
			TS_CRITICAL("Failed to select a Vulkan-compatible GPU: {}", physDeviceResult.error().message());
			return;
		}

		vkb::PhysicalDevice physicalDevice = physDeviceResult.value();
		TS_INFO("Selected Vulkan physical device: {}", physicalDevice.name);

		// Create the final Vulkan device
		TS_TRACE("Creating Vulkan logical device...");
		vkb::DeviceBuilder deviceBuilder{ physicalDevice };
		auto deviceResult = deviceBuilder.build();

		if (!deviceResult)
		{
			TS_CRITICAL("Failed to create Vulkan device: {}", deviceResult.error().message());
			return;
		}

		vkb::Device vkbDevice = deviceResult.value();
		device = vkbDevice.device;
		physical_device = physicalDevice.physical_device;

		//get graphic queue
		graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
		graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

		TS_INFO("Vulkan logical device created successfully.");
	}

	void VkGpuDevice::initSwapchain()
	{
		createSwapchain(creationInfo.width, creationInfo.height);
	}

	void VkGpuDevice::initCommands()
	{
		vkCommandBufferRing.init(this);
	}

	void VkGpuDevice::initSyncStructures()
	{
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.pNext = nullptr;

		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;
		semaphoreCreateInfo.flags = 0;

		for (int i = 0; i < FRAME_OVERLAP; i++) 
		{
			VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));

			VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].swapchainSemaphore));
			VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].renderSemaphore));
		}

	}
}