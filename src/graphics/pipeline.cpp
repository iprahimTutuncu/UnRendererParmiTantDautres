#include "pipeline.h"

#include "components.h"

#include <stddef.h>

enum VertexAttributesLocationOrder {
    Position,
    NumVertexAttributes // must be last
};

enum VertexBuffers {
    PositionBuffer,
    NumVertexBuffers // must be last
};

SDL_GPUGraphicsPipeline* createScenePipeline(
    SDL_GPUDevice* device,
    SDL_Window* window,
    SDL_GPUTextureFormat depthStencilFormat,
    SDL_GPUShader* vertShader,
    SDL_GPUShader* fragShader) {

    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo {};
    pipelineCreateInfo.vertex_shader = vertShader;
    pipelineCreateInfo.fragment_shader = fragShader;

    // Vertex input state
    SDL_GPUVertexBufferDescription vertexBufferDescriptions[NumVertexBuffers] {};
    vertexBufferDescriptions[PositionBuffer].slot = 0;
    vertexBufferDescriptions[PositionBuffer].pitch = sizeof(PositionColorVertex);
    vertexBufferDescriptions[PositionBuffer].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute vertexAttributes[NumVertexAttributes] {};
    vertexAttributes[Position].location = VertexAttributesLocationOrder::Position;
    vertexAttributes[Position].buffer_slot = 0;
    vertexAttributes[Position].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[Position].offset = offsetof(PositionColorVertex, x);

    pipelineCreateInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDescriptions;
    pipelineCreateInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineCreateInfo.vertex_input_state.vertex_attributes = vertexAttributes;
    pipelineCreateInfo.vertex_input_state.num_vertex_attributes = 1;

    // Depth stencil state
    pipelineCreateInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    pipelineCreateInfo.depth_stencil_state.write_mask = 0xFF;
    pipelineCreateInfo.depth_stencil_state.enable_depth_write = true;
    pipelineCreateInfo.depth_stencil_state.enable_stencil_test = true;

    // Target info
    SDL_GPUColorTargetDescription colorTargetDescription[1] {};
    colorTargetDescription[0].format = SDL_GetGPUSwapchainTextureFormat(device, window);
    pipelineCreateInfo.target_info.color_target_descriptions = colorTargetDescription;
    pipelineCreateInfo.target_info.num_color_targets = 1;
    pipelineCreateInfo.target_info.depth_stencil_format = depthStencilFormat;
    pipelineCreateInfo.target_info.has_depth_stencil_target = true;

    return SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);
}
