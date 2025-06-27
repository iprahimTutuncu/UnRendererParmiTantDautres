#include "deferred_gbuffer_renderer.h"

#include "../camera.h"
#include "graphics.h"
#include "shaders.h"
#include "texture.h"

#include <SDL3/SDL_log.h>

#include <stddef.h>

SDL_AppResult deferred_gbuffer_init(AppState& state) {
    deferred_gbuffer_create_pipelines(state);
    deferred_gbuffer_create_box_geometry(state);
    deferred_gbuffer_create_sphere_geometry(state);

    if (createSolidColorTextureRGBA8(state, DefaultWhite, 32, 32, 1.f, 1.f, 1.f, 1.f) == SDL_APP_FAILURE) [[unlikely]] {
        return SDL_APP_FAILURE;
    }

    deferred_gbuffer_update_particles(state);

    return SDL_APP_CONTINUE;
}

void deferred_gbuffer_update_particles(AppState& state) {
    std::vector<float*> positions;

    for (auto& p : state.graphics->particles) {
        positions.push_back(p.position);
    }

    if (!state.graphics->buffers[ParticlePositionBuffer]) {
        SDL_GPUBufferCreateInfo bufferInfo;
        bufferInfo.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        bufferInfo.size = static_cast<std::uint32_t>(state.graphics->particles.size() * sizeof(Particle::position));
        state.graphics->buffers[ParticlePositionBuffer] = SDL_CreateGPUBuffer(state.device, &bufferInfo);
    }

    // Create transfer buffer
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = static_cast<std::uint32_t>(state.graphics->particles.size() * sizeof(Particle::color));
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(state.device, &transferInfo);

    // Map and fill transfer buffer
    void* mapped = SDL_MapGPUTransferBuffer(state.device, transferBuffer, false);
    SDL_memcpy(mapped, positions.data(), transferInfo.size);
    SDL_UnmapGPUTransferBuffer(state.device, transferBuffer);

    // Upload to GPU buffer
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(state.device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTransferBufferLocation transferLoc = { transferBuffer, 0 };
    SDL_GPUBufferRegion bufferRegion = { state.graphics->buffers[ParticlePositionBuffer], 0, transferInfo.size };
    SDL_UploadToGPUBuffer(copyPass, &transferLoc, &bufferRegion, true);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);
}

void deferred_gbuffer_render(AppState& state, SDL_GPUCommandBuffer* cmdBuf) {
    GraphicState& graphics = *state.graphics;

    if (!graphics.textures[GeometryPosition] || !graphics.textures[GeometryNormal] || !graphics.textures[GeometryAlbedo] || !graphics.textures[GeometryDepth]) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "GBuffer textures not set!");
        return;
    }

    graphics.particleUniformBuffer.time += 0.01f;

    SDL_GPUStorageBufferReadWriteBinding bufferBindings {};
    bufferBindings.buffer = graphics.buffers[ParticlePositionBuffer];

    SDL_GPUCommandBuffer* cmdBufcomp = SDL_AcquireGPUCommandBuffer(state.device);

    // BEGIN
    SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBufcomp, nullptr, 0, &bufferBindings, 1);
    SDL_BindGPUComputePipeline(computePass, graphics.computePipeline[ParticleUpdate]);

    SDL_PushGPUComputeUniformData(cmdBufcomp, 0, &graphics.particleUniformBuffer, sizeof(ParticleUpdateUniform));
    Uint32 groupCount = static_cast<std::uint32_t>((graphics.particles.size() + 63) / 64);
    SDL_DispatchGPUCompute(computePass, groupCount, 1, 1);
    SDL_EndGPUComputePass(computePass);
    SDL_SubmitGPUCommandBuffer(cmdBufcomp);
    // END

    SDL_GPUDepthStencilTargetInfo depthTarget {};
    depthTarget.texture = graphics.textures[GeometryDepth];
    depthTarget.cycle = true;
    depthTarget.clear_depth = 1;
    depthTarget.clear_stencil = 0;
    depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    depthTarget.store_op = SDL_GPU_STOREOP_STORE;
    depthTarget.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depthTarget.stencil_store_op = SDL_GPU_STOREOP_STORE;

    // Set up render targets
    SDL_GPUColorTargetInfo colorTargets[3] = {};
    colorTargets[0].texture = graphics.textures[GeometryPosition];
    colorTargets[1].texture = graphics.textures[GeometryNormal];
    colorTargets[2].texture = graphics.textures[GeometryAlbedo];

    for (auto& target : colorTargets) {
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        target.clear_color = { 0, 0, 0, 1 };
    }

    // Begin geometry pass
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmdBuf, colorTargets, 3, &depthTarget);

    SDL_GPUGraphicsPipeline* boxPipeline = graphics.rasterMode == RasterMode_Fill
        ? graphics.graphicPipeline[GeometryBufferFillPipeline]
        : graphics.graphicPipeline[GeometryBufferLinePipeline];

    SDL_BindGPUGraphicsPipeline(pass, boxPipeline);

    // Bind box geometry
    SDL_GPUBufferBinding vertexBinding {};
    vertexBinding.buffer = graphics.buffers[BoxVertexBuffer];
    vertexBinding.offset = 0;

    SDL_GPUBufferBinding indexBinding {};
    indexBinding.buffer = graphics.buffers[BoxIndexBuffer];
    indexBinding.offset = 0;

    // Bind default texture
    SDL_GPUTextureSamplerBinding samplerBinding {};
    samplerBinding.texture = graphics.textures[DefaultWhite];
    samplerBinding.sampler = graphics.samplersPreset[LinearClamp];

    SDL_BindGPUFragmentSamplers(pass, 0, &samplerBinding, 1);

    SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
    SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    // Render each box
    for (Box& box : graphics.boxes) {
        vec3 min = { box.min[0], box.min[1], box.min[2] };
        vec3 max = { box.max[0], box.max[1], box.max[2] };

        vec3 size = { max.x - min.x, max.y - min.y, max.z - min.z };
        vec3 center = { (max.x + min.x) * 0.5f, (max.y + min.y) * 0.5f, (max.z + min.z) * 0.5f };

        mat4 model = mat4::identity();

        // Apply translation
        model[3].x = center.x;
        model[3].y = center.y;
        model[3].z = center.z;

        // Apply scaling (column-major)
        model[0].x *= size.x * 0.5f;
        model[1].y *= size.y * 0.5f;
        model[2].z *= size.z * 0.5f;

        graphics.geometryBufferUniform.model = model;
        graphics.geometryBufferUniform.view = state.camera->view_matrix();
        graphics.geometryBufferUniform.proj = state.camera->projection_matrix();
        SDL_PushGPUVertexUniformData(cmdBuf, 0, &graphics.geometryBufferUniform, sizeof(graphics.geometryBufferUniform));
        SDL_DrawGPUIndexedPrimitives(pass, graphics.numBoxIndices, 1, 0, 0, 0);
    }

    SDL_GPUGraphicsPipeline* particlePipeline = graphics.rasterMode == RasterMode_Fill
        ? graphics.graphicPipeline[GeometryBufferParticleFillPipeline]
        : graphics.graphicPipeline[GeometryBufferParticleLinePipeline];

    SDL_BindGPUGraphicsPipeline(pass, particlePipeline);

    vertexBinding.buffer = graphics.buffers[SphereVertexBuffer];
    vertexBinding.offset = 0;

    indexBinding.buffer = graphics.buffers[SphereIndexBuffer];
    indexBinding.offset = 0;

    SDL_BindGPUFragmentSamplers(pass, 0, &samplerBinding, 1);

    SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
    SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    SDL_BindGPUVertexStorageBuffers(pass, 0, &graphics.buffers[ParticlePositionBuffer], 1);

    mat4 model {};
    model[0].x = 0.25f;
    model[1].y = 0.25f;
    model[2].z = 0.25f;
    model[3].z = 1.f;

    graphics.geometryBufferUniform.model = model;
    state.camera->position = vec3 { 0.f, 0.f, 15.f };

    graphics.geometryBufferUniform.view = state.camera->view_matrix();
    graphics.geometryBufferUniform.proj = state.camera->projection_matrix();
    SDL_PushGPUVertexUniformData(cmdBuf, 0, &graphics.geometryBufferUniform, sizeof(GeometryBufferUniform));
    SDL_DrawGPUIndexedPrimitives(pass, graphics.numSphereIndices, static_cast<std::uint32_t>(graphics.particles.size()), 0, 0, 0);

    SDL_EndGPURenderPass(pass);
}

void deferred_gbuffer_create_pipelines(AppState& state) {
    deferred_gbuffer_create_particles_pipeline(state);
    deferred_gbuffer_create_mesh_pipeline(state);

    // this shouldn't exist xddd
    // have a fonciton that create all the compute shader eventually
    // if ray tracing ever happen, createSoftwareRaytracingPipelines
    SDL_GPUComputePipelineCreateInfo pipelineInfo {};
    pipelineInfo.num_readwrite_storage_textures = 0;
    pipelineInfo.num_readonly_storage_buffers = 0;
    pipelineInfo.num_readwrite_storage_buffers = 1;
    pipelineInfo.num_readonly_storage_textures = 0;
    pipelineInfo.num_uniform_buffers = 1;
    pipelineInfo.num_samplers = 0;
    pipelineInfo.threadcount_x = 64;
    pipelineInfo.threadcount_y = 1;
    pipelineInfo.threadcount_z = 1;

    state.graphics->computePipeline[ParticleUpdate] = createComputePipelineFromShader(state.device, SHADER_PATH("particleUpdate.comp"), &pipelineInfo);
}

void deferred_gbuffer_create_particles_pipeline(AppState& state) {
    SDL_GPUDevice* device = state.device;

    SDL_GPUShader* vertexShader = loadShader(device, SHADER_PATH("deferred_gBuffer_particles.vert"), 0, 1, 1, 0);
    if (!vertexShader) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load vertex shader: deferred_gBuffer_particles.vert");
        return;
    }

    SDL_GPUShader* fragmentShader = loadShader(device, SHADER_PATH("deferred_gBuffer.frag"), 1, 0, 0, 0);
    if (!fragmentShader) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load fragment shader: deferred_gBuffer.frag");
        SDL_ReleaseGPUShader(device, vertexShader);
        return;
    }

    // Vertex input state
    SDL_GPUVertexBufferDescription vertexBufferDesc = {};
    vertexBufferDesc.slot = 0;
    vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBufferDesc.instance_step_rate = 0;
    vertexBufferDesc.pitch = sizeof(Vertex);

    SDL_GPUVertexAttribute vertexAttributes[3] = {};

    // position:vec3 ? la location 0
    vertexAttributes[0].buffer_slot = 0;
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].offset = offsetof(Vertex, position);

    // normal:vec3 ? la  location 1
    vertexAttributes[1].buffer_slot = 0;
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].offset = offsetof(Vertex, normal);

    // texCoord:vec2 ? la  location 2
    vertexAttributes[2].buffer_slot = 0;
    vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttributes[2].location = 2;
    vertexAttributes[2].offset = offsetof(Vertex, texCoord);

    SDL_GPUVertexInputState vertexInputState = {};
    vertexInputState.num_vertex_buffers = 1;
    vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
    vertexInputState.num_vertex_attributes = 3;
    vertexInputState.vertex_attributes = vertexAttributes;

    // Depth state
    SDL_GPUDepthStencilState depthStencilState = {};
    depthStencilState.enable_depth_test = true;
    depthStencilState.enable_depth_write = true;
    depthStencilState.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    depthStencilState.write_mask = 0xFF;

    SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(
        device,
        state.window);

    SDL_GPUColorTargetDescription colorTargets[3] {};
    colorTargets[0].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    colorTargets[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    colorTargets[2].format = swapchainFormat; // Albedo

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.target_info.num_color_targets = 3;
    pipelineInfo.target_info.color_target_descriptions = colorTargets;
    pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    pipelineInfo.target_info.has_depth_stencil_target = true;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipelineInfo.vertex_shader = vertexShader;
    pipelineInfo.fragment_shader = fragmentShader;
    pipelineInfo.vertex_input_state = vertexInputState;
    pipelineInfo.depth_stencil_state = depthStencilState;
    pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

    // Fill pipeline
    pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    state.graphics->graphicPipeline[GeometryBufferParticleFillPipeline] = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

    if (!state.graphics->graphicPipeline[GeometryBufferParticleFillPipeline]) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create particle fill pipeline: %s", SDL_GetError());
    }

    // Wireframe pipeline
    pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;
    state.graphics->graphicPipeline[GeometryBufferParticleLinePipeline] = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

    if (!state.graphics->graphicPipeline[GeometryBufferParticleLinePipeline]) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create particle wireframe pipeline: %s", SDL_GetError());
    }

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);
}

void deferred_gbuffer_create_mesh_pipeline(AppState& state) {
    // Load shaders
    auto vertexShader = loadShader(state.device, SHADER_PATH("deferred_gBuffer.vert"), 0, 1, 0, 0);
    auto fragmentShader = loadShader(state.device, SHADER_PATH("deferred_gBuffer.frag"), 1, 0, 0, 0);

    // Vertex input state
    SDL_GPUVertexBufferDescription vertexBufferDesc = {};
    vertexBufferDesc.slot = 0;
    vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertexBufferDesc.instance_step_rate = 0;
    vertexBufferDesc.pitch = sizeof(Vertex);

    SDL_GPUVertexAttribute vertexAttributes[3] = {};

    // position:vec3 ? la location 0
    vertexAttributes[0].buffer_slot = 0;
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].offset = 0;

    // normal:vec3 ? la  location 1
    vertexAttributes[1].buffer_slot = 0;
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].offset = sizeof(float) * 4; // 16 bytes

    // texCoord:vec2 ? la  location 2
    vertexAttributes[2].buffer_slot = 0;
    vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttributes[2].location = 2;
    vertexAttributes[2].offset = sizeof(float) * 8; // 32 bytes

    SDL_GPUVertexInputState vertexInputState = {};
    vertexInputState.num_vertex_buffers = 1;
    vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
    vertexInputState.num_vertex_attributes = 3;
    vertexInputState.vertex_attributes = vertexAttributes;

    // Depth state
    SDL_GPUDepthStencilState depthStencilState = {};
    depthStencilState.enable_depth_test = true;
    depthStencilState.enable_depth_write = true;
    depthStencilState.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    depthStencilState.write_mask = 0xFF;

    // Color targets
    SDL_GPUColorTargetDescription colorTargets[3] = {};
    colorTargets[0].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT; // Position
    colorTargets[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT; // Normal
    colorTargets[2].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM; // Albedo

    // Pipeline creation
    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.target_info.num_color_targets = 3;
    pipelineInfo.target_info.color_target_descriptions = colorTargets;
    pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    pipelineInfo.target_info.has_depth_stencil_target = true;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipelineInfo.vertex_shader = vertexShader;
    pipelineInfo.fragment_shader = fragmentShader;
    pipelineInfo.vertex_input_state = vertexInputState;
    pipelineInfo.depth_stencil_state = depthStencilState;
    pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

    state.graphics->graphicPipeline[GeometryBufferFillPipeline] = SDL_CreateGPUGraphicsPipeline(state.device, &pipelineInfo);

    if (!state.graphics->graphicPipeline[GeometryBufferFillPipeline]) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create GBuffer pipeline: %s", SDL_GetError());
    }

    pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;
    state.graphics->graphicPipeline[GeometryBufferLinePipeline] = SDL_CreateGPUGraphicsPipeline(state.device, &pipelineInfo);

    if (!state.graphics->graphicPipeline[GeometryBufferLinePipeline]) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create GBuffer line pipeline: %s", SDL_GetError());
    }

    SDL_ReleaseGPUShader(state.device, vertexShader);
    SDL_ReleaseGPUShader(state.device, fragmentShader);
}

void deferred_gbuffer_create_box_geometry(AppState& state) {
    // Cube vertices (same as original)
    Vertex vertices[] = {
        // Front face (normal: 0,0,1)
        { { -1, -1, 1 }, { 0, 0, 1 }, { 0, 0 } },
        { { 1, -1, 1 }, { 0, 0, 1 }, { 1, 0 } },
        { { 1, 1, 1 }, { 0, 0, 1 }, { 1, 1 } },
        { { -1, 1, 1 }, { 0, 0, 1 }, { 0, 1 } },

        // Back face (normal: 0,0,-1)
        { { 1, -1, -1 }, { 0, 0, -1 }, { 0, 0 } },
        { { -1, -1, -1 }, { 0, 0, -1 }, { 1, 0 } },
        { { -1, 1, -1 }, { 0, 0, -1 }, { 1, 1 } },
        { { 1, 1, -1 }, { 0, 0, -1 }, { 0, 1 } },

        // Left face (normal: -1,0,0)
        { { -1, -1, -1 }, { -1, 0, 0 }, { 0, 0 } },
        { { -1, -1, 1 }, { -1, 0, 0 }, { 1, 0 } },
        { { -1, 1, 1 }, { -1, 0, 0 }, { 1, 1 } },
        { { -1, 1, -1 }, { -1, 0, 0 }, { 0, 1 } },

        // Right face (normal: 1,0,0)
        { { 1, -1, 1 }, { 1, 0, 0 }, { 0, 0 } },
        { { 1, -1, -1 }, { 1, 0, 0 }, { 1, 0 } },
        { { 1, 1, -1 }, { 1, 0, 0 }, { 1, 1 } },
        { { 1, 1, 1 }, { 1, 0, 0 }, { 0, 1 } },

        // Top face (normal: 0,1,0)
        { { -1, 1, 1 }, { 0, 1, 0 }, { 0, 0 } },
        { { 1, 1, 1 }, { 0, 1, 0 }, { 1, 0 } },
        { { 1, 1, -1 }, { 0, 1, 0 }, { 1, 1 } },
        { { -1, 1, -1 }, { 0, 1, 0 }, { 0, 1 } },

        // Bottom face (normal: 0,-1,0)
        { { -1, -1, -1 }, { 0, -1, 0 }, { 0, 0 } },
        { { 1, -1, -1 }, { 0, -1, 0 }, { 1, 0 } },
        { { 1, -1, 1 }, { 0, -1, 0 }, { 1, 1 } },
        { { -1, -1, 1 }, { 0, -1, 0 }, { 0, 1 } },
    };

    uint16_t indices[] = {
        // Front face
        0, 1, 2,
        2, 3, 0,

        // Back face
        4, 5, 6,
        6, 7, 4,

        // Left face
        8, 9, 10,
        10, 11, 8,

        // Right face
        12, 13, 14,
        14, 15, 12,

        // Top face
        16, 17, 18,
        18, 19, 16,

        // Bottom face
        20, 21, 22,
        22, 23, 20
    };

    state.graphics->numBoxIndices = sizeof(indices) / sizeof(indices[0]);

    // Create GPU buffers
    SDL_GPUBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    vertexBufferInfo.size = sizeof(vertices);
    state.graphics->buffers[BoxVertexBuffer] = SDL_CreateGPUBuffer(state.device, &vertexBufferInfo);

    SDL_GPUBufferCreateInfo indexBufferInfo = {};
    indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    indexBufferInfo.size = sizeof(indices);
    state.graphics->buffers[BoxIndexBuffer] = SDL_CreateGPUBuffer(state.device, &indexBufferInfo);

    // Create transfer buffer
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = sizeof(vertices) + sizeof(indices);
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(state.device, &transferInfo);

    // Map and fill transfer buffer
    Vertex* data = static_cast<Vertex*>(SDL_MapGPUTransferBuffer(state.device, transferBuffer, false));

    // Copy data
    SDL_memcpy(data, vertices, sizeof(vertices));
    SDL_memcpy(data + sizeof(vertices) / sizeof(vertices[0]), indices, sizeof(indices));
    SDL_UnmapGPUTransferBuffer(state.device, transferBuffer);

    // Upload to GPU
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(state.device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTransferBufferLocation vertexTransfer = { transferBuffer, 0 };
    SDL_GPUBufferRegion vertexRegion = { state.graphics->buffers[BoxVertexBuffer], 0, sizeof(vertices) };
    SDL_UploadToGPUBuffer(copyPass, &vertexTransfer, &vertexRegion, false);

    SDL_GPUTransferBufferLocation indexTransfer = { transferBuffer, sizeof(vertices) };
    SDL_GPUBufferRegion indexRegion = { state.graphics->buffers[BoxIndexBuffer], 0, sizeof(indices) };
    SDL_UploadToGPUBuffer(copyPass, &indexTransfer, &indexRegion, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);
    SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
}

void deferred_gbuffer_create_sphere_geometry(AppState& state) {
    static constexpr std::size_t kLatitudeBands = 32;
    static constexpr std::size_t kLongitudeBands = 64;
    static constexpr float pi = 3.14159265358979323846f;

    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    for (std::size_t lat = 0; lat <= kLatitudeBands; ++lat) {
        float theta = static_cast<float>(lat) * pi / kLatitudeBands;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (std::size_t lon = 0; lon <= kLongitudeBands; ++lon) {
            float phi = static_cast<float>(lon) * (2.f * pi) / kLongitudeBands;
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            float u = 1.0f - static_cast<float>(lon) / kLongitudeBands;
            float v = 1.0f - static_cast<float>(lat) / kLatitudeBands;

            Vertex vertex = {};
            vertex.position[0] = x;
            vertex.position[1] = y;
            vertex.position[2] = z;

            vertex.normal[0] = x;
            vertex.normal[1] = y;
            vertex.normal[2] = z;

            vertex.texCoord[0] = u;
            vertex.texCoord[1] = v;

            vertices.push_back(vertex);
        }
    }

    for (std::size_t lat = 0; lat < kLatitudeBands; ++lat) {
        for (std::size_t lon = 0; lon < kLongitudeBands; ++lon) {
            std::uint16_t first = static_cast<std::uint16_t>(lat * (kLongitudeBands + 1) + lon);
            std::uint16_t second = static_cast<std::uint16_t>(first + kLongitudeBands + 1);

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    state.graphics->numSphereIndices = static_cast<std::uint32_t>(indices.size());

    // Upload to GPU
    SDL_GPUBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    vertexBufferInfo.size = static_cast<std::uint32_t>(sizeof(decltype(vertices)::value_type) * vertices.size());
    state.graphics->buffers[SphereVertexBuffer] = SDL_CreateGPUBuffer(state.device, &vertexBufferInfo);

    SDL_GPUBufferCreateInfo indexBufferInfo = {};
    indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    indexBufferInfo.size = static_cast<std::uint32_t>(sizeof(decltype(indices)::value_type) * indices.size());
    state.graphics->buffers[SphereIndexBuffer] = SDL_CreateGPUBuffer(state.device, &indexBufferInfo);

    // Transfer buffer
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = vertexBufferInfo.size + indexBufferInfo.size;
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(state.device, &transferInfo);

    Vertex* data = static_cast<Vertex*>(SDL_MapGPUTransferBuffer(state.device, transferBuffer, false));
    memcpy(data, vertices.data(), vertexBufferInfo.size);
    memcpy(reinterpret_cast<uint8_t*>(data) + vertexBufferInfo.size, indices.data(), indexBufferInfo.size);
    SDL_UnmapGPUTransferBuffer(state.device, transferBuffer);

    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(state.device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTransferBufferLocation vertexTransfer = { transferBuffer, 0 };
    SDL_GPUBufferRegion vertexRegion = { state.graphics->buffers[SphereVertexBuffer], 0, vertexBufferInfo.size };
    SDL_UploadToGPUBuffer(copyPass, &vertexTransfer, &vertexRegion, false);

    SDL_GPUTransferBufferLocation indexTransfer = { transferBuffer, vertexBufferInfo.size };
    SDL_GPUBufferRegion indexRegion = { state.graphics->buffers[SphereIndexBuffer], 0, indexBufferInfo.size };
    SDL_UploadToGPUBuffer(copyPass, &indexTransfer, &indexRegion, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);
    SDL_ReleaseGPUTransferBuffer(state.device, transferBuffer);
}
