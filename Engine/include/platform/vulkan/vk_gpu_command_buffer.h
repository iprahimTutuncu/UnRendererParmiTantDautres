#pragma once

#include <vulkan/vulkan.h>
#include <graphics/gpu_type.h>
#include "graphics/gpu_command_buffer.h"
namespace Thsan
{
    class VkGpuCommandBuffer : public GpuCommandBuffer
    {
    public:
        virtual void init(QueueType::Enum type, u32 buffer_size, u32 submit_size, bool baked) override;
        virtual void terminate();

        virtual void clear(f32 red, f32 green, f32 blue, f32 alpha) override;
        virtual void clear_depth_stencil(f32 depth, u8 stencil) override;

        virtual void draw(TopologyType::Enum topology, u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count) override;
        virtual void draw_indexed(TopologyType::Enum topology, u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance) override;

        virtual void setGpuDevice(GpuDevice* device) override;
        virtual void setHandle(u32 handle) override;

        virtual void reset() override;

        VkCommandBuffer vkCommandBuffer;

    private:
        QueueType::Enum type;
        u32 buffer_size;
        bool baked;
        bool is_recording = false;
        u32 current_command = 0;

        VkClearValue clears[2];

        friend class VkCommandBufferRing;
    };
}
