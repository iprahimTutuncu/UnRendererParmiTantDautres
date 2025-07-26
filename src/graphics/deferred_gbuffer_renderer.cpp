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

    state.graphics->bilateralBlurBufferUniform.blurScale = 2.0f;
    state.graphics->bilateralBlurBufferUniform.blurDepthFalloff = 1.0f;
    state.graphics->bilateralBlurBufferUniform.filterRadius = 10;

    deferred_gbuffer_update_particles(state);

    return SDL_APP_CONTINUE;
}

void deferred_gbuffer_update_particles(AppState& state) {

    std::vector<float> positions;

    positions.resize(state.graphics->particles.size() * 4);

    for (size_t i = 0; i < state.graphics->particles.size(); i++) {
        // Access the i-th particle
        auto& p = state.graphics->particles[i];

        size_t j = i * 4;
        positions[j] = p.position[0];
        positions[j + 1] = p.position[1];
        positions[j + 2] = p.position[2];
        positions[j + 3] = p.position[3];
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

    int width, height;
    if (!SDL_GetWindowSize(state.window, &width, &height))
        return;

    if (!graphics.textures[GeometryPosition] || !graphics.textures[GeometryNormal] || !graphics.textures[GeometryAlbedo] || !graphics.textures[GeometryDepth]) [[unlikely]] {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "GBuffer textures not set!");
        return;
    }

    // --- ParticleUpdate Compute Pass ---
    {
        graphics.particleUniformBuffer.time += 0.01f;

        SDL_GPUStorageBufferReadWriteBinding bufferBinding {};
        bufferBinding.buffer = graphics.buffers[ParticlePositionBuffer];

        SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuf, nullptr, 0, &bufferBinding, 1);
        SDL_BindGPUComputePipeline(computePass, graphics.computePipeline[ParticleUpdate]);
        SDL_PushGPUComputeUniformData(cmdBuf, 0, &graphics.particleUniformBuffer, sizeof(ParticleUpdateUniform));

        Uint32 groupCount = static_cast<Uint32>((graphics.particles.size() + 63) / 64);
        SDL_DispatchGPUCompute(computePass, groupCount, 1, 1);
        SDL_EndGPUComputePass(computePass);
    }

    // --- Setup Depth and Color Targets ---
    SDL_GPUDepthStencilTargetInfo depthTarget {};
    depthTarget.texture = graphics.textures[GeometryDepth];
    depthTarget.cycle = true;
    depthTarget.clear_depth = 1;
    depthTarget.clear_stencil = 0;
    depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    depthTarget.store_op = SDL_GPU_STOREOP_STORE;
    depthTarget.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depthTarget.stencil_store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUColorTargetInfo colorTargets[3] = {};
    colorTargets[0].texture = graphics.textures[GeometryPosition];
    colorTargets[1].texture = graphics.textures[GeometryNormal];
    colorTargets[2].texture = graphics.textures[GeometryAlbedo];

    for (auto& target : colorTargets) {
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        target.clear_color = { 0, 0, 0, 0 };
    }

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmdBuf, colorTargets, 3, &depthTarget);

    // --- Box Rendering Pass ---
    {
        SDL_GPUGraphicsPipeline* pipeline = graphics.rasterMode == RasterMode_Fill
            ? graphics.graphicPipeline[GeometryBufferFillPipeline]
            : graphics.graphicPipeline[GeometryBufferLinePipeline];

        SDL_BindGPUGraphicsPipeline(pass, pipeline);

        SDL_GPUBufferBinding vertexBinding {};
        vertexBinding.buffer = graphics.buffers[BoxVertexBuffer];

        SDL_GPUBufferBinding indexBinding {};
        indexBinding.buffer = graphics.buffers[BoxIndexBuffer];

        SDL_GPUTextureSamplerBinding samplerBinding {};
        samplerBinding.texture = graphics.textures[DefaultWhite];
        samplerBinding.sampler = graphics.samplersPreset[LinearClamp];

        SDL_BindGPUFragmentSamplers(pass, 0, &samplerBinding, 1);
        SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
        SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        for (Box& box : graphics.boxes) {
            vec3 min { box.min[0], box.min[1], box.min[2] };
            vec3 max { box.max[0], box.max[1], box.max[2] };

            vec3 size { max.x - min.x, max.y - min.y, max.z - min.z };
            vec3 center { (max.x + min.x) * 0.5f, (max.y + min.y) * 0.5f, (max.z + min.z) * 0.5f };

            mat4 model = mat4::identity();
            model[3].x = center.x;
            model[3].y = center.y;
            model[3].z = center.z;
            model[0].x *= size.x * 0.5f;
            model[1].y *= size.y * 0.5f;
            model[2].z *= size.z * 0.5f;
            model[3].w = 1.f;

            graphics.geometryBufferUniform.model = model;
            graphics.geometryBufferUniform.view = state.camera->view_matrix();
            graphics.geometryBufferUniform.proj = state.camera->projection_matrix();
            graphics.geometryBufferUniform.id = 1;

            SDL_PushGPUVertexUniformData(cmdBuf, 0, &graphics.geometryBufferUniform, sizeof(GeometryBufferUniform));
            SDL_DrawGPUIndexedPrimitives(pass, graphics.numBoxIndices, 1, 0, 0, 0);
        }
    }

    // --- Particle Rendering Pass ---
    {
        SDL_GPUGraphicsPipeline* pipeline = graphics.rasterMode == RasterMode_Fill
            ? graphics.graphicPipeline[GeometryBufferParticleFillPipeline]
            : graphics.graphicPipeline[GeometryBufferParticleLinePipeline];

        SDL_BindGPUGraphicsPipeline(pass, pipeline);

        SDL_GPUBufferBinding vertexBinding {};
        vertexBinding.buffer = graphics.buffers[SphereVertexBuffer];

        SDL_GPUBufferBinding indexBinding {};
        indexBinding.buffer = graphics.buffers[SphereIndexBuffer];

        SDL_GPUTextureSamplerBinding samplerBinding {};
        samplerBinding.texture = graphics.textures[DefaultWhite];
        samplerBinding.sampler = graphics.samplersPreset[LinearClamp];

        SDL_BindGPUFragmentSamplers(pass, 0, &samplerBinding, 1);
        SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
        SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_BindGPUVertexStorageBuffers(pass, 0, &graphics.buffers[ParticlePositionBuffer], 1);

        mat4 model {};
        model[0].x = 0.25f;
        model[1].y = 0.25f;
        model[2].z = 0.25f;
        model[3].w = 1.f;

        graphics.geometryBufferUniform.model = model;

        graphics.geometryBufferParticlesUniform.view = state.camera->view_matrix();
        graphics.geometryBufferParticlesUniform.proj = state.camera->projection_matrix();
        graphics.geometryBufferParticlesUniform.id = 2;
        graphics.geometryBufferParticlesUniform.radius = 2.f;
        graphics.geometryBufferParticlesUniform.color = { 1.f, 1.f, 1.f, 1.f };
        graphics.geometryBufferParticlesUniform.near = state.camera->near;
        graphics.geometryBufferParticlesUniform.far = state.camera->far;

        SDL_PushGPUVertexUniformData(cmdBuf, 0, &graphics.geometryBufferParticlesUniform, sizeof(GeometryBufferParticlesUniform));
        SDL_PushGPUFragmentUniformData(cmdBuf, 0, &graphics.geometryBufferParticlesUniform, sizeof(GeometryBufferParticlesUniform));

        SDL_DrawGPUIndexedPrimitives(pass, graphics.numSphereIndices, static_cast<std::uint32_t>(graphics.particles.size()), 0, 0, 0);
    }

    SDL_EndGPURenderPass(pass);

    { // Particle Bilateral Blur
        SDL_GPUStorageTextureReadWriteBinding textureBinding {};
        textureBinding.texture = state.graphics->textures[GeometryDepthModified];

        SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuf, &textureBinding, 1, nullptr, 0);

        SDL_BindGPUComputePipeline(computePass, graphics.computePipeline[ParticleBilateralBlur]);
        SDL_PushGPUComputeUniformData(cmdBuf, 0, &graphics.bilateralBlurBufferUniform, sizeof(BilateralBlurBufferUniform));

        SDL_GPUTextureSamplerBinding samplerBinding {};
        samplerBinding.texture = state.graphics->textures[GeometryDepth];
        samplerBinding.sampler = state.graphics->samplersPreset[LinearClamp];

        SDL_BindGPUComputeSamplers(computePass, 0, &samplerBinding, 1);

        Uint32 x = static_cast<Uint32>((width + 15) / 16);
        Uint32 y = static_cast<Uint32>((height + 15) / 16);
        SDL_DispatchGPUCompute(computePass, x, y, 1);
        SDL_EndGPUComputePass(computePass);
    }

    { // Particle Depth To Position/Normal/Albedo
        SDL_GPUStorageTextureReadWriteBinding textureBinding[3] {};
        textureBinding[0].texture = state.graphics->textures[GeometryPosition];
        textureBinding[1].texture = state.graphics->textures[GeometryNormal];
        textureBinding[2].texture = state.graphics->textures[GeometryAlbedo];

        SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBuf, textureBinding, 3, nullptr, 0);

        SDL_BindGPUComputePipeline(computePass, graphics.computePipeline[ParticleDepthToGBuffer]);
        SDL_PushGPUComputeUniformData(cmdBuf, 0, &graphics.geometryBufferParticlesUniform, sizeof(GeometryBufferParticlesUniform));

        SDL_GPUTextureSamplerBinding samplerBinding {};
        samplerBinding.texture = state.graphics->textures[GeometryDepthModified];
        samplerBinding.sampler = state.graphics->samplersPreset[LinearClamp];
        SDL_BindGPUComputeSamplers(computePass, 0, &samplerBinding, 1);

        Uint32 x = static_cast<Uint32>((width + 15) / 16);
        Uint32 y = static_cast<Uint32>((height + 15) / 16);
        SDL_DispatchGPUCompute(computePass, x, y, 1);
        SDL_EndGPUComputePass(computePass);
    }
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

    state.graphics->computePipeline[ParticleUpdate] = createComputePipelineFromShader(state.device, SHADER_PATH("particle_update.comp"), &pipelineInfo);

    pipelineInfo.num_readwrite_storage_textures = 1;
    pipelineInfo.num_readonly_storage_buffers = 0;
    pipelineInfo.num_readwrite_storage_buffers = 0;
    pipelineInfo.num_readonly_storage_textures = 0;
    pipelineInfo.num_uniform_buffers = 1;
    pipelineInfo.num_samplers = 1;
    pipelineInfo.threadcount_x = 16;
    pipelineInfo.threadcount_y = 16;
    pipelineInfo.threadcount_z = 1;

    state.graphics->computePipeline[ParticleBilateralBlur] = createComputePipelineFromShader(state.device, SHADER_PATH("particle_bilateral_blur.comp"), &pipelineInfo);
    pipelineInfo.num_readwrite_storage_textures = 3;
    pipelineInfo.num_readonly_storage_buffers = 0;
    pipelineInfo.num_readwrite_storage_buffers = 0;
    pipelineInfo.num_readonly_storage_textures = 0;
    pipelineInfo.num_uniform_buffers = 1;
    pipelineInfo.num_samplers = 1;
    pipelineInfo.threadcount_x = 16;
    pipelineInfo.threadcount_y = 16;
    pipelineInfo.threadcount_z = 1;

    state.graphics->computePipeline[ParticleDepthToGBuffer] = createComputePipelineFromShader(state.device, SHADER_PATH("particle_gbuffer_from_depth.comp"), &pipelineInfo);
}

void deferred_gbuffer_create_particles_pipeline(AppState& state) {
    SDL_GPUDevice* device = state.device;

    SDL_GPUShader* vertexShader = loadShader(device, SHADER_PATH("deferred_gBuffer_particles.vert"), 0, 1, 1, 0);
    if (!vertexShader) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load vertex shader: deferred_gBuffer_particles.vert");
        return;
    }

    SDL_GPUShader* fragmentShader = loadShader(device, SHADER_PATH("deferred_gBuffer_particles.frag"), 1, 1, 0, 0);
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

    SDL_GPUColorTargetDescription colorTargets[3] {};
    colorTargets[0].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    colorTargets[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    colorTargets[2].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

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
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    float r = 0.5f;

    // Positions (x, y, z), normals (up), texcoords
    vertices.push_back(Vertex { { -r, -r, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } }); // bottom left
    vertices.push_back(Vertex { { r, -r, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } }); // bottom right
    vertices.push_back(Vertex { { r, r, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } }); // top right
    vertices.push_back(Vertex { { -r, r, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } }); // top left

    // Two triangles (0-1-2 and 2-3-0)
    indices = {
        0, 1, 2,
        2, 3, 0
    };

    state.graphics->numSphereIndices = static_cast<std::uint32_t>(indices.size());

    // Upload to GPU
    SDL_GPUBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    vertexBufferInfo.size = static_cast<std::uint32_t>(sizeof(Vertex) * vertices.size());
    state.graphics->buffers[SphereVertexBuffer] = SDL_CreateGPUBuffer(state.device, &vertexBufferInfo);

    SDL_GPUBufferCreateInfo indexBufferInfo = {};
    indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    indexBufferInfo.size = static_cast<std::uint32_t>(sizeof(uint16_t) * indices.size());
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
