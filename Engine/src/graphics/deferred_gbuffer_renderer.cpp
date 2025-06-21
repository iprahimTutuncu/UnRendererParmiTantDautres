#include "deferred_gbuffer_renderer.h"
#include "geometry.h"

#include "../ressource_manager/sampler_manager.h"
#include "../ressource_manager/shader_manager.h"
#include "../ressource_manager/texture_manager.h"
#include "../system/log.h"
#include "../system/window.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_render.h>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>

static UBO ubo;

namespace GTS {
    SDL_GPUComputePipeline* CreateComputePipelineFromShader(SDL_GPUDevice* device, const char* shaderFilename, SDL_GPUComputePipelineCreateInfo* createInfo) {
        std::array<char, 256> fullPath {};
        SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
        SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
        const char* entrypoint = nullptr;

        if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
            SDL_snprintf(fullPath.data(), fullPath.size(), "%smedia/shaders/compiled/SPIRV/%s.spv", SDL_GetBasePath(), shaderFilename);
            format = SDL_GPU_SHADERFORMAT_SPIRV;
            entrypoint = "main";
        } else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
            SDL_snprintf(fullPath.data(), fullPath.size(), "%smedia/shaders/compiled/MSL/%s.msl", SDL_GetBasePath(), shaderFilename);
            format = SDL_GPU_SHADERFORMAT_MSL;
            entrypoint = "main0";
        } else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
            SDL_snprintf(fullPath.data(), fullPath.size(), "%smedia/shaders/compiled/DXIL/%s.dxil", SDL_GetBasePath(), shaderFilename);
            format = SDL_GPU_SHADERFORMAT_DXIL;
            entrypoint = "main";
        } else {
            SDL_Log("Unrecognized backend shader format!");
            return nullptr;
        }

        size_t codeSize = 0;
        void* code = SDL_LoadFile(fullPath.data(), &codeSize);
        if (code == nullptr) {
            SDL_Log("Failed to load compute shader from disk! %s", fullPath.data());
            return nullptr;
        }

        SDL_GPUComputePipelineCreateInfo newCreateInfo = *createInfo;
        newCreateInfo.code = (const Uint8*)code;
        newCreateInfo.code_size = codeSize;
        newCreateInfo.entrypoint = entrypoint;
        newCreateInfo.format = format;

        SDL_Log("Creating compute pipeline with shader: %s (format: %d, entrypoint: %s)",
            fullPath.data(), format, entrypoint);

        SDL_GPUComputePipeline* pipeline = SDL_CreateGPUComputePipeline(device, &newCreateInfo);
        if (pipeline == nullptr) {

            SDL_Log("Pipeline creation failed for shader: %s, format: %d, size: %zu",
                fullPath.data(), format, codeSize);

            SDL_free(code);
            return nullptr;
        }

        SDL_free(code); // SDL_CreateGPUComputePipeline copies internally, so we can free now.
        return pipeline;
    }

    DeferredGBufferRenderer::DeferredGBufferRenderer(SDL_GPUDevice* device, std::shared_ptr<Window> window)
        : m_window(window)
        , m_gpuDevice(device) {
        createPipelines();
        createBoxGeometry();
        createSphereGeometry();

        SDL_Window* sdlWindow = window->getWindow().as<SDL_Window>();
        int w, h;
        SDL_GetWindowSize(sdlWindow, &w, &h);

        ubo.proj = glm::perspective(glm::radians(50.f), (float)w / (float)h, 0.01f, 1000.f);

        ubo.view = glm::lookAt(
            glm::vec3(0.f, 0.f, 15.f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.f, 1.f, 0.f));

        ubo.model = glm::mat4(1.f);

        default_white = Ressource::TextureManager::createSolidColorTextureRGBA8("default_white", default_white_width, default_white_height, 1.f, 1.f, 1.f, 1.f);
    }

    DeferredGBufferRenderer::~DeferredGBufferRenderer() {
        releaseRessouces();
    }

    void DeferredGBufferRenderer::setGBufferOutput(const GBufferTextures& gBuffer) {
        m_gBuffer = gBuffer;
    }

    void DeferredGBufferRenderer::setFillMode(RasterMode mode) {
        rasterMode = mode;
    }

    void DeferredGBufferRenderer::setCamera(const Camera& camera) {
        ubo.proj = glm::perspective(glm::radians(camera.fov), camera.aspect, camera.nearClip, camera.farClip);

        ubo.view = glm::lookAt(
            camera.position,
            camera.target,
            camera.up);
    }

    void DeferredGBufferRenderer::setParticles(const Particles& particles) {
        std::vector<glm::vec4> positions;

        for (auto p : particles.data)
            positions.push_back(p.position);

        particleCount = positions.size();

        if (!m_particlePositionBuffer) {
            SDL_GPUBufferCreateInfo bufferInfo;
            bufferInfo.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
            bufferInfo.size = particleCount * sizeof(glm::vec4);
            m_particlePositionBuffer = SDL_CreateGPUBuffer(m_gpuDevice, &bufferInfo);
        }

        // Create transfer buffer
        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = particleCount * sizeof(glm::vec4);
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice, &transferInfo);

        // Copy particle position data into transfer buffer
        glm::vec4* mapped = static_cast<glm::vec4*>(SDL_MapGPUTransferBuffer(m_gpuDevice, transferBuffer, false));
        memcpy(mapped, positions.data(), transferInfo.size);
        SDL_UnmapGPUTransferBuffer(m_gpuDevice, transferBuffer);

        // Upload to GPU buffer
        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(m_gpuDevice);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

        SDL_GPUTransferBufferLocation transferLoc = { transferBuffer, 0 };
        SDL_GPUBufferRegion bufferRegion = { m_particlePositionBuffer, 0, transferInfo.size };
        SDL_UploadToGPUBuffer(copyPass, &transferLoc, &bufferRegion, true);

        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmdBuf);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice, transferBuffer);
    }

    void DeferredGBufferRenderer::render(SDL_GPUCommandBuffer* cmdBuf, const std::vector<const Box*>& boxes) {
        if (!m_gBuffer.position || !m_gBuffer.normal || !m_gBuffer.albedo || !m_gBuffer.depth) {
            GTS_ERROR("GBuffer textures not set!");
            return;
        }

        temporaryUniforms.time += 0.01f;

        // pour si jamais je veux un storage texture dans le compute
        // SDL_GPUStorageTextureReadWriteBinding bindings{};
        // bindings.texture = default_white.get();

        SDL_GPUStorageBufferReadWriteBinding bufferBindings {};
        bufferBindings.buffer = m_particlePositionBuffer;

        SDL_GPUCommandBuffer* cmdBufcomp = SDL_AcquireGPUCommandBuffer(m_gpuDevice);

        // BEGIN
        SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(cmdBufcomp, nullptr, 0, &bufferBindings, 1);
        SDL_BindGPUComputePipeline(computePass, computePipeline);

        SDL_PushGPUComputeUniformData(cmdBufcomp, 0, &temporaryUniforms, sizeof(TemporaryUniforms));
        Uint32 groupCount = (particleCount + 63) / 64; // Ceiling division
        SDL_DispatchGPUCompute(computePass, groupCount, 1, 1);
        SDL_EndGPUComputePass(computePass);
        SDL_SubmitGPUCommandBuffer(cmdBufcomp);
        // END

        SDL_GPUDepthStencilTargetInfo depthTarget = {};
        depthTarget.texture = m_gBuffer.depth.get();
        depthTarget.cycle = true;
        depthTarget.clear_depth = 1;
        depthTarget.clear_stencil = 0;
        depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        depthTarget.store_op = SDL_GPU_STOREOP_STORE;
        depthTarget.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
        depthTarget.stencil_store_op = SDL_GPU_STOREOP_STORE;

        // Set up render targets
        SDL_GPUColorTargetInfo colorTargets[3] = {};
        colorTargets[0].texture = m_gBuffer.position.get();
        colorTargets[1].texture = m_gBuffer.normal.get();
        colorTargets[2].texture = m_gBuffer.albedo.get();

        for (auto& target : colorTargets) {
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            target.clear_color = { 0, 0, 0, 1 };
        }

        DoSomeComputeShaderThing();

        // Begin geometry pass
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmdBuf, colorTargets, 3, &depthTarget);
        SDL_BindGPUGraphicsPipeline(pass, rasterMode == RasterMode::Fill ? m_gBufferFillPipeline : m_gBufferLinePipeline);

        // Bind box geometry
        SDL_GPUBufferBinding vertexBinding = {};
        vertexBinding.buffer = m_boxVertexBuffer;
        vertexBinding.offset = 0;

        SDL_GPUBufferBinding indexBinding = {};
        indexBinding.buffer = m_boxIndexBuffer;
        indexBinding.offset = 0;

        // Bind default texture
        SDL_GPUTextureSamplerBinding samplerBinding = {};
        samplerBinding.texture = default_white.get();
        samplerBinding.sampler = Ressource::SamplerManager::get(Ressource::SamplerManager::Preset::LinearClamp).get();
        SDL_BindGPUFragmentSamplers(pass, 0, &samplerBinding, 1);

        SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
        SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        // Render each box
        for (const Box* box : boxes) {
            glm::vec3 size = box->max - box->min;
            glm::vec3 center = (box->max + box->min) * 0.5f;

            ubo.model = glm::translate(glm::mat4(1.0f), center);
            ubo.model = glm::scale(ubo.model, size * 0.5f);

            SDL_PushGPUVertexUniformData(cmdBuf, 0, &ubo, sizeof(ubo));

            // Draw call
            SDL_DrawGPUIndexedPrimitives(pass, 36, 1, 0, 0, 0);
        }

        SDL_BindGPUGraphicsPipeline(pass, rasterMode == RasterMode::Fill ? m_gBufferParticlesFillPipeline : m_gBufferParticlesLinePipeline);

        vertexBinding = {};
        vertexBinding.buffer = m_sphereVertexBuffer;
        vertexBinding.offset = 0;

        indexBinding = {};
        indexBinding.buffer = m_sphereIndexBuffer;
        indexBinding.offset = 0;

        SDL_BindGPUFragmentSamplers(pass, 0, &samplerBinding, 1);

        SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
        SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_BindGPUVertexStorageBuffers(pass, 0, &m_particlePositionBuffer, 1);

        ubo.model = glm::mat4(1.0f);
        ubo.model = glm::scale(ubo.model, glm::vec3(0.25f));

        SDL_PushGPUVertexUniformData(cmdBuf, 0, &ubo, sizeof(ubo));
        SDL_DrawGPUIndexedPrimitives(pass, numSphereIndices, particleCount, 0, 0, 0);

        SDL_EndGPURenderPass(pass);
    }

    void DeferredGBufferRenderer::releaseRessouces() {
        if (m_gBufferFillPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_gpuDevice, m_gBufferFillPipeline);
            m_gBufferFillPipeline = nullptr;
        }

        if (m_gBufferLinePipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_gpuDevice, m_gBufferLinePipeline);
            m_gBufferLinePipeline = nullptr;
        }

        if (m_boxVertexBuffer) {
            SDL_ReleaseGPUBuffer(m_gpuDevice, m_boxVertexBuffer);
            m_boxVertexBuffer = nullptr;
        }

        if (m_boxIndexBuffer) {
            SDL_ReleaseGPUBuffer(m_gpuDevice, m_boxIndexBuffer);
            m_boxIndexBuffer = nullptr;
        }
    }

    void DeferredGBufferRenderer::createPipelines() {
        createMeshPipeline();
        createParticlesPipeline();

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

        computePipeline = CreateComputePipelineFromShader(m_gpuDevice, "particleUpdate.comp", &pipelineInfo);
        temporaryUniforms.time = 0;
    }

    void DeferredGBufferRenderer::createParticlesPipeline() {
        // Load shaders
        auto vertexShader = Ressource::ShaderManager::get(m_gpuDevice, "deferred_gBuffer_particles.vert", 0, 1, 1, 0);
        if (!vertexShader) {
            GTS_ERROR("Failed to load vertex shader: deferred_gBuffer_particles.vert");
            return;
        }

        auto fragmentShader = Ressource::ShaderManager::get(m_gpuDevice, "deferred_gBuffer.frag", 1, 0, 0, 0);
        if (!fragmentShader) {
            GTS_ERROR("Failed to load fragment shader: deferred_gBuffer.frag");
            return;
        }

        // Vertex input state
        SDL_GPUVertexBufferDescription vertexBufferDesc = {};
        vertexBufferDesc.slot = 0;
        vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDesc.instance_step_rate = 0;
        vertexBufferDesc.pitch = sizeof(Vertex);

        SDL_GPUVertexAttribute vertexAttributes[3] = {};

        // position:vec3 à la location 0
        vertexAttributes[0].buffer_slot = 0;
        vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[0].location = 0;
        vertexAttributes[0].offset = 0;

        // normal:vec3 à la  location 1
        vertexAttributes[1].buffer_slot = 0;
        vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[1].location = 1;
        vertexAttributes[1].offset = sizeof(float) * 4; // 16 bytes

        // texCoord:vec2 à la  location 2
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

        SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(m_gpuDevice, m_window->getWindow().as<SDL_Window>());

        // Color targets
        SDL_GPUColorTargetDescription colorTargets[3] = {};
        colorTargets[0].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT; // Position
        colorTargets[1].format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT; // Normal
        colorTargets[2].format = format; // SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;     // Albedo

        // Pipeline creation
        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.target_info.num_color_targets = 3;
        pipelineInfo.target_info.color_target_descriptions = colorTargets;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipelineInfo.vertex_shader = vertexShader.get();
        pipelineInfo.fragment_shader = fragmentShader.get();
        pipelineInfo.vertex_input_state = vertexInputState;
        pipelineInfo.depth_stencil_state = depthStencilState;
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

        // Create fill pipeline
        m_gBufferParticlesFillPipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice, &pipelineInfo);
        if (!m_gBufferParticlesFillPipeline) {
            GTS_ERROR("Failed to create particle fill pipeline: %s", SDL_GetError());
        }

        // Create wireframe pipeline
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;
        m_gBufferParticlesLinePipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice, &pipelineInfo);
        if (!m_gBufferParticlesLinePipeline) {
            GTS_ERROR("Failed to create particle wireframe pipeline: %s", SDL_GetError());
        }

        // Clean up shader handles
        vertexShader.reset();
        fragmentShader.reset();
        Ressource::ShaderManager::releaseUnused();
    }

    void DeferredGBufferRenderer::createMeshPipeline() {
        // Load shaders
        auto vertexShader = Ressource::ShaderManager::get(m_gpuDevice, "deferred_gBuffer.vert", 0, 1, 0, 0);
        auto fragmentShader = Ressource::ShaderManager::get(m_gpuDevice, "deferred_gBuffer.frag", 1, 0, 0, 0);

        // Vertex input state
        SDL_GPUVertexBufferDescription vertexBufferDesc = {};
        vertexBufferDesc.slot = 0;
        vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDesc.instance_step_rate = 0;
        vertexBufferDesc.pitch = sizeof(Vertex);

        SDL_GPUVertexAttribute vertexAttributes[3] = {};

        // position:vec3 à la location 0
        vertexAttributes[0].buffer_slot = 0;
        vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[0].location = 0;
        vertexAttributes[0].offset = 0;

        // normal:vec3 à la  location 1
        vertexAttributes[1].buffer_slot = 0;
        vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[1].location = 1;
        vertexAttributes[1].offset = sizeof(float) * 4; // 16 bytes

        // texCoord:vec2 à la  location 2
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
        pipelineInfo.vertex_shader = vertexShader.get();
        pipelineInfo.fragment_shader = fragmentShader.get();
        pipelineInfo.vertex_input_state = vertexInputState;
        pipelineInfo.depth_stencil_state = depthStencilState;
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

        m_gBufferFillPipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice, &pipelineInfo);
        if (!m_gBufferFillPipeline) {
            GTS_ERROR("Failed to create GBuffer pipeline: %s", SDL_GetError());
        }

        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;
        m_gBufferLinePipeline = SDL_CreateGPUGraphicsPipeline(m_gpuDevice, &pipelineInfo);
        if (m_gBufferLinePipeline == NULL) {
            GTS_ERROR("Failed to create line pipeline!");
        }

        vertexShader.reset();
        fragmentShader.reset();
        Ressource::ShaderManager::releaseUnused();
    }

    template <typename tType>
    inline void ZeroStruct(tType& aValue) {
        SDL_memset(&aValue, 0, sizeof(tType));
    }

    void DeferredGBufferRenderer::DoSomeComputeShaderThing() {
    }

    void DeferredGBufferRenderer::createBoxGeometry() {
        // Cube vertices (same as original)
        Vertex vertices[] = {
            // Front face (normal: 0,0,1)
            { { -1, -1, 1 }, 0, { 0, 0, 1 }, 0, { 0, 0 }, { 0, 0 } },
            { { 1, -1, 1 }, 0, { 0, 0, 1 }, 0, { 1, 0 }, { 0, 0 } },
            { { 1, 1, 1 }, 0, { 0, 0, 1 }, 0, { 1, 1 }, { 0, 0 } },
            { { -1, 1, 1 }, 0, { 0, 0, 1 }, 0, { 0, 1 }, { 0, 0 } },

            // Back face (normal: 0,0,-1)
            { { 1, -1, -1 }, 0, { 0, 0, -1 }, 0, { 0, 0 }, { 0, 0 } },
            { { -1, -1, -1 }, 0, { 0, 0, -1 }, 0, { 1, 0 }, { 0, 0 } },
            { { -1, 1, -1 }, 0, { 0, 0, -1 }, 0, { 1, 1 }, { 0, 0 } },
            { { 1, 1, -1 }, 0, { 0, 0, -1 }, 0, { 0, 1 }, { 0, 0 } },

            // Left face (normal: -1,0,0)
            { { -1, -1, -1 }, 0, { -1, 0, 0 }, 0, { 0, 0 }, { 0, 0 } },
            { { -1, -1, 1 }, 0, { -1, 0, 0 }, 0, { 1, 0 }, { 0, 0 } },
            { { -1, 1, 1 }, 0, { -1, 0, 0 }, 0, { 1, 1 }, { 0, 0 } },
            { { -1, 1, -1 }, 0, { -1, 0, 0 }, 0, { 0, 1 }, { 0, 0 } },

            // Right face (normal: 1,0,0)
            { { 1, -1, 1 }, 0, { 1, 0, 0 }, 0, { 0, 0 }, { 0, 0 } },
            { { 1, -1, -1 }, 0, { 1, 0, 0 }, 0, { 1, 0 }, { 0, 0 } },
            { { 1, 1, -1 }, 0, { 1, 0, 0 }, 0, { 1, 1 }, { 0, 0 } },
            { { 1, 1, 1 }, 0, { 1, 0, 0 }, 0, { 0, 1 }, { 0, 0 } },

            // Top face (normal: 0,1,0)
            { { -1, 1, 1 }, 0, { 0, 1, 0 }, 0, { 0, 0 }, { 0, 0 } },
            { { 1, 1, 1 }, 0, { 0, 1, 0 }, 0, { 1, 0 }, { 0, 0 } },
            { { 1, 1, -1 }, 0, { 0, 1, 0 }, 0, { 1, 1 }, { 0, 0 } },
            { { -1, 1, -1 }, 0, { 0, 1, 0 }, 0, { 0, 1 }, { 0, 0 } },

            // Bottom face (normal: 0,-1,0)
            { { -1, -1, -1 }, 0, { 0, -1, 0 }, 0, { 0, 0 }, { 0, 0 } },
            { { 1, -1, -1 }, 0, { 0, -1, 0 }, 0, { 1, 0 }, { 0, 0 } },
            { { 1, -1, 1 }, 0, { 0, -1, 0 }, 0, { 1, 1 }, { 0, 0 } },
            { { -1, -1, 1 }, 0, { 0, -1, 0 }, 0, { 0, 1 }, { 0, 0 } },
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

        // Create GPU buffers
        SDL_GPUBufferCreateInfo vertexBufferInfo = {};
        vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        vertexBufferInfo.size = sizeof(Vertex) * 24;
        m_boxVertexBuffer = SDL_CreateGPUBuffer(m_gpuDevice, &vertexBufferInfo);

        SDL_GPUBufferCreateInfo indexBufferInfo = {};
        indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        indexBufferInfo.size = sizeof(uint16_t) * 36;
        m_boxIndexBuffer = SDL_CreateGPUBuffer(m_gpuDevice, &indexBufferInfo);

        // Create transfer buffer
        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = vertexBufferInfo.size + indexBufferInfo.size;
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice, &transferInfo);

        // Map and fill transfer buffer
        Vertex* data = static_cast<Vertex*>(SDL_MapGPUTransferBuffer(m_gpuDevice, transferBuffer, false));

        // Copy data
        memcpy(data, vertices, sizeof(vertices));
        memcpy(data + 24, indices, sizeof(indices));
        SDL_UnmapGPUTransferBuffer(m_gpuDevice, transferBuffer);

        // Upload to GPU
        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(m_gpuDevice);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

        SDL_GPUTransferBufferLocation vertexTransfer = { transferBuffer, 0 };
        SDL_GPUBufferRegion vertexRegion = { m_boxVertexBuffer, 0, sizeof(vertices) };
        SDL_UploadToGPUBuffer(copyPass, &vertexTransfer, &vertexRegion, false);

        SDL_GPUTransferBufferLocation indexTransfer = { transferBuffer, sizeof(vertices) };
        SDL_GPUBufferRegion indexRegion = { m_boxIndexBuffer, 0, sizeof(indices) };
        SDL_UploadToGPUBuffer(copyPass, &indexTransfer, &indexRegion, false);

        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmdBuf);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice, transferBuffer);
    }

    void DeferredGBufferRenderer::createSphereGeometry() {
        constexpr uint32_t kLatitudeBands = 32;
        constexpr uint32_t kLongitudeBands = 64;

        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;

        for (uint32_t lat = 0; lat <= kLatitudeBands; ++lat) {
            float theta = lat * glm::pi<float>() / kLatitudeBands;
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            for (uint32_t lon = 0; lon <= kLongitudeBands; ++lon) {
                float phi = lon * 2.0f * glm::pi<float>() / kLongitudeBands;
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
                vertex._pad1 = 0.0f;

                vertex.normal[0] = x;
                vertex.normal[1] = y;
                vertex.normal[2] = z;
                vertex._pad2 = 0.0f;

                vertex.texCoord[0] = u;
                vertex.texCoord[1] = v;
                vertex._pad3[0] = 0.0f;
                vertex._pad3[1] = 0.0f;

                vertices.push_back(vertex);
            }
        }

        for (uint32_t lat = 0; lat < kLatitudeBands; ++lat) {
            for (uint32_t lon = 0; lon < kLongitudeBands; ++lon) {
                uint16_t first = lat * (kLongitudeBands + 1) + lon;
                uint16_t second = first + kLongitudeBands + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        numSphereIndices = indices.size();

        // Upload to GPU
        SDL_GPUBufferCreateInfo vertexBufferInfo = {};
        vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        vertexBufferInfo.size = sizeof(Vertex) * vertices.size();
        m_sphereVertexBuffer = SDL_CreateGPUBuffer(m_gpuDevice, &vertexBufferInfo);

        SDL_GPUBufferCreateInfo indexBufferInfo = {};
        indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        indexBufferInfo.size = sizeof(uint16_t) * indices.size();
        m_sphereIndexBuffer = SDL_CreateGPUBuffer(m_gpuDevice, &indexBufferInfo);

        // Transfer buffer
        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = vertexBufferInfo.size + indexBufferInfo.size;
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_gpuDevice, &transferInfo);

        Vertex* data = static_cast<Vertex*>(SDL_MapGPUTransferBuffer(m_gpuDevice, transferBuffer, false));
        memcpy(data, vertices.data(), vertexBufferInfo.size);
        memcpy(reinterpret_cast<uint8_t*>(data) + vertexBufferInfo.size, indices.data(), indexBufferInfo.size);
        SDL_UnmapGPUTransferBuffer(m_gpuDevice, transferBuffer);

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(m_gpuDevice);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

        SDL_GPUTransferBufferLocation vertexTransfer = { transferBuffer, 0 };
        SDL_GPUBufferRegion vertexRegion = { m_sphereVertexBuffer, 0, vertexBufferInfo.size };
        SDL_UploadToGPUBuffer(copyPass, &vertexTransfer, &vertexRegion, false);

        SDL_GPUTransferBufferLocation indexTransfer = { transferBuffer, vertexBufferInfo.size };
        SDL_GPUBufferRegion indexRegion = { m_sphereIndexBuffer, 0, indexBufferInfo.size };
        SDL_UploadToGPUBuffer(copyPass, &indexTransfer, &indexRegion, false);

        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmdBuf);
        SDL_ReleaseGPUTransferBuffer(m_gpuDevice, transferBuffer);
    }

}
