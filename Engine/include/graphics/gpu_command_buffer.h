#pragma once

#include "graphics/gpu_device.h"
#include "graphics/gpu_type.h"

namespace Thsan
{
    class GpuCommandBuffer
    {
    public:
        virtual ~GpuCommandBuffer() = default;

        virtual void init(QueueType::Enum type, u32 buffer_size, u32 submit_size, bool baked) = 0;
        virtual void terminate() = 0;

        virtual void setGpuDevice(GpuDevice* device) = 0;
        virtual void setHandle(u32 handle) = 0;

        //virtual void bind_pipeline(PipelineHandle handle) = 0;
        //virtual void bind_vertex_buffer(BufferHandle handle, u32 binding, u32 offset) = 0;
        //virtual void bind_index_buffer(BufferHandle handle, u32 offset, IndexType index_type) = 0;
        //virtual void bind_descriptor_set(DescriptorSetHandle* handles, u32 num_lists, u32* offsets, u32 num_offsets) = 0;

        //virtual void set_viewport(const Viewport* viewport) = 0;
        //virtual void set_scissor(const Rect2DInt* rect) = 0;

        virtual void clear(f32 red, f32 green, f32 blue, f32 alpha) = 0;
        virtual void clear_depth_stencil(f32 depth, u8 stencil) = 0;

        virtual void draw(TopologyType::Enum topology, u32 first_vertex, u32 vertex_count, u32 first_instance, u32 instance_count) = 0;
        virtual void draw_indexed(TopologyType::Enum topology, u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance) = 0;
        //virtual void draw_indirect(BufferHandle handle, u32 offset, u32 stride) = 0;
        //virtual void draw_indexed_indirect(BufferHandle handle, u32 offset, u32 stride) = 0;

        //virtual void dispatch(u32 group_x, u32 group_y, u32 group_z) = 0;
        //virtual void dispatch_indirect(BufferHandle handle, u32 offset) = 0;

        //virtual void barrier(const ExecutionBarrier& barrier) = 0;

        //virtual void fill_buffer(BufferHandle buffer, u32 offset, u32 size, u32 data) = 0;

        //virtual void push_marker(const char* name) = 0;
        //virtual void pop_marker() = 0;

        virtual void reset() = 0;

    protected:
        GpuDevice* device;
        //RenderPass* current_render_pass;
        //Pipeline* current_pipeline;
        bool is_recording;
        u32 handle;
        u32 current_command;
        //ResourceHandle resource_handle;
        QueueType::Enum type = QueueType::Enum::Graphics;
        u32 buffer_size = 0;
        bool baked = false;
    };
}
