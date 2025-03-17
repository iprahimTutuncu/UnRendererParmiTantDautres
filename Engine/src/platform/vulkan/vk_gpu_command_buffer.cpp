#include "pch.h"
#include "platform/vulkan/vk_gpu_command_buffer.h"


namespace Thsan
{

	void VkGpuCommandBuffer::init(QueueType::Enum type, u32 buffer_size, u32 submit_size, bool baked)
	{
		this->type = type;
		this->buffer_size = submit_size;
		this->baked = baked;

		reset();
	}

	void VkGpuCommandBuffer::terminate()
	{
		is_recording = false;
	}

	void VkGpuCommandBuffer::clear(f32 red, f32 green, f32 blue, f32 alpha)
	{
		clears[0].color = { red, green, blue, alpha };
	}

	void VkGpuCommandBuffer::clear_depth_stencil(f32 depth, u8 stencil)
	{
		clears[1].depthStencil.depth = depth;
		clears[1].depthStencil.stencil = stencil;
	}

	void VkGpuCommandBuffer::draw(TopologyType::Enum topology, u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count)
	{
		vkCmdDraw(vkCommandBuffer, vertex_count, instance_count, first_vertex, first_instance);
	}

	void VkGpuCommandBuffer::draw_indexed(TopologyType::Enum topology, u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance)
	{
		vkCmdDrawIndexed(vkCommandBuffer, index_count, instance_count, first_index, vertex_offset, first_instance);
	}

	void VkGpuCommandBuffer::setGpuDevice(GpuDevice* device)
	{
		this->device = device;
	}

	void VkGpuCommandBuffer::setHandle(u32 handle)
	{
		this->handle = handle;
	}

	void VkGpuCommandBuffer::reset()
	{
		is_recording = false;
		//current_render_pass = nullptr;
		//current_pipeline = nullptr;
		current_command = 0;
	}

}