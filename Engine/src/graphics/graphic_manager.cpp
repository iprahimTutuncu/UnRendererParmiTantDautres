#include "pch.h"
#include "options.h"
#include "graphics/graphic_manager.h"
#include "system/window.h"
#include <SDL3/SDL_gpu.h>
#include <system/log.h>
#include <SDL3/SDL_filesystem.h>

#include <glm/ext.hpp>

static SDL_GPUGraphicsPipeline* FillPipeline;
static SDL_GPUGraphicsPipeline* LinePipeline;
static SDL_GPUViewport SmallViewport = { 160, 120, 320, 240, 0.1f, 1.0f };
static SDL_Rect ScissorRect = { 320, 240, 320, 240 };

static bool UseWireframeMode = false;
static bool UseSmallViewport = false;
static bool UseScissorRect = false;

static const char* BasePath = NULL;

void InitializeAssetLoader()
{
	BasePath = SDL_GetBasePath();
}


SDL_GPUShader* LoadShader(
	SDL_GPUDevice* device,
	const char* shaderFilename,
	Uint32 samplerCount,
	Uint32 uniformBufferCount,
	Uint32 storageBufferCount,
	Uint32 storageTextureCount
) {
	// Auto-detect the shader stage from the file name for convenience
	SDL_GPUShaderStage stage;
	if (SDL_strstr(shaderFilename, ".vert"))
	{
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	}
	else if (SDL_strstr(shaderFilename, ".frag"))
	{
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	}
	else
	{
		OLAF_ERROR("Invalid shader stage!");
		return NULL;
	}

	char fullPath[256];
	SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
	SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
	const char* entrypoint;

	if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV)
	{
		std::snprintf(fullPath, sizeof(fullPath), "%smedia/shaders/compiled/SPIRV/%s.spv", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_SPIRV;
		entrypoint = "main";
	}
	else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL)
	{
		std::snprintf(fullPath, sizeof(fullPath), "%smedia/shaders/compiled/MSL/%s.msl", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_MSL;
		entrypoint = "main0";
	}
	else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL)
	{
		std::snprintf(fullPath, sizeof(fullPath), "%smedia/shaders/compiled/DXIL/%s.dxil", BasePath, shaderFilename);
		format = SDL_GPU_SHADERFORMAT_DXIL;
		entrypoint = "main";
	}
	else 
	{
		OLAF_ERROR("%s", "Unrecognized backend shader format!");
		return NULL;
	}

	size_t codeSize;
	void* code = SDL_LoadFile(fullPath, &codeSize);
	if (code == NULL)
	{
		OLAF_ERROR("Failed to load shader from disk! %s", fullPath);
		return NULL;
	}


	SDL_GPUShaderCreateInfo shaderInfo = {
		.code_size = codeSize,
		.code = static_cast<const Uint8*>(code),
		.entrypoint = entrypoint,
		.format = format,
		.stage = stage,
		.num_samplers = samplerCount,
		.num_storage_textures = storageTextureCount,
		.num_storage_buffers = storageBufferCount,
		.num_uniform_buffers = uniformBufferCount,
	};

	SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
	if (shader == NULL)
	{
		OLAF_ERROR("Failed to create shader!");
		SDL_free(code);
		return NULL;
	}

	SDL_free(code);
	return shader;
}

void Olaf::GraphicsManager::init(const Options& options, std::shared_ptr<Window> window, std::function<void(Options& , GraphicsManager&, const double&)>  onDrawCallback)
{
	onDraw = onDrawCallback;
	pWindow = window;

	ubo.proj = glm::perspective(glm::radians(50.f), (float)options.windowOptions.screenWidth / (float)options.windowOptions.screenHeight, 0.000001f, 1000.f);
	
	ubo.view = glm::lookAt
	(
		glm::vec3(0.f, 0.f, 5.f),
		glm::vec3(0.f, 0.f, 0.f),
		glm::vec3(0.f, 1.f, 0.f) 
	);
	
	ubo.model = glm::mat4(1.f);

	GpuHandle handle = pWindow->getGpuDevice();
	if (get_graphic_API() == GraphicAPI::SDL3)
	{
		InitializeAssetLoader();

		gpu = handle.as<SDL_GPUDevice>();

		// Create the shaders
		SDL_GPUShader* vertexShader = LoadShader(gpu, "RawTriangle.vert", 0, 1, 0, 0);
		if (vertexShader == NULL)
		{
			OLAF_ERROR("Failed to create vertex shader!");
		}

		SDL_GPUShader* fragmentShader = LoadShader(gpu, "SolidColor.frag", 0, 0, 0, 0);
		if (fragmentShader == NULL)
		{
			OLAF_ERROR("Failed to create fragment shader!");
		}

		SDL_Window* window = pWindow->getWindow().as<SDL_Window>();

		SDL_GPUVertexBufferDescription vertexBufferDesc = {};
		vertexBufferDesc.slot = 0;
		vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
		vertexBufferDesc.instance_step_rate = 0;
		vertexBufferDesc.pitch = sizeof(Vertex); // 32 bytes

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
		vertexAttributes[1].offset = sizeof(float) * 3; // 12 bytes

		// texCoord:vec2 à la  location 2
		vertexAttributes[2].buffer_slot = 0;
		vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
		vertexAttributes[2].location = 2;
		vertexAttributes[2].offset = sizeof(float) * 6; // 24 bytes

		SDL_GPUVertexInputState vertexInputState = {};
		vertexInputState.num_vertex_buffers = 1;
		vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
		vertexInputState.num_vertex_attributes = 3;
		vertexInputState.vertex_attributes = vertexAttributes;

		SDL_GPUColorTargetDescription colorTargetDesc = {};
		colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(gpu, window);

		SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.target_info.num_color_targets = 1;
		pipelineCreateInfo.target_info.color_target_descriptions = &colorTargetDesc;
		pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pipelineCreateInfo.vertex_shader = vertexShader;
		pipelineCreateInfo.fragment_shader = fragmentShader;
		pipelineCreateInfo.vertex_input_state = vertexInputState;

		pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
		FillPipeline = SDL_CreateGPUGraphicsPipeline(gpu, &pipelineCreateInfo);
		if (FillPipeline == NULL)
		{
			OLAF_ERROR("Failed to create fill pipeline!");
		}

		pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;
		LinePipeline = SDL_CreateGPUGraphicsPipeline(gpu, &pipelineCreateInfo);
		if (LinePipeline == NULL)
		{
			OLAF_ERROR("Failed to create line pipeline!");
		}

		// Clean up shader resources
		SDL_ReleaseGPUShader(gpu, vertexShader);
		SDL_ReleaseGPUShader(gpu, fragmentShader);

		// Finally, print instructions!
		OLAF_ERROR("Press Left to toggle wireframe mode");
		OLAF_ERROR("Press Down to toggle small viewport");
		OLAF_ERROR("Press Right to toggle scissor rect");
		// Create the vertex buffer
		SDL_GPUBufferCreateInfo vertexBufferInfo = {};
		vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		vertexBufferInfo.size = sizeof(Vertex) * 3;

		vertexBuffer = SDL_CreateGPUBuffer(gpu, &vertexBufferInfo);

		// Create the transfer buffer
		SDL_GPUTransferBufferCreateInfo transferBufferInfo = {};
		transferBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		transferBufferInfo.size = sizeof(Vertex) * 3;

		SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpu, &transferBufferInfo);

		// Map the transfer buffer
		Vertex* transferData = static_cast<Vertex*>(SDL_MapGPUTransferBuffer(gpu, transferBuffer, false));

		// Fill in the vertex data (position, normal, texCoord)
		transferData[0] = Vertex
		{
			{-1.0f, -1.0f, 0.0f},    // position
			{0.0f,  0.0f, 1.0f},     // normal
			{0.0f,  0.0f}            // texCoord
		};

		transferData[1] = Vertex
		{
			{1.0f, -1.0f, 0.0f},
			{0.0f, 0.0f, 1.0f},
			{1.0f, 0.0f}
		};

		transferData[2] = Vertex
		{
			{0.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, 1.0f},
			{0.5f, 1.0f}
		};

		SDL_UnmapGPUTransferBuffer(gpu, transferBuffer);

		// Upload to the vertex buffer
		SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(gpu);
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

		SDL_GPUTransferBufferLocation transferLocation = {};
		transferLocation.transfer_buffer = transferBuffer;
		transferLocation.offset = 0;

		SDL_GPUBufferRegion bufferRegion = {};
		bufferRegion.buffer = vertexBuffer;
		bufferRegion.offset = 0;
		bufferRegion.size = sizeof(Vertex) * 3;

		SDL_UploadToGPUBuffer(copyPass, &transferLocation, &bufferRegion, false);

		SDL_EndGPUCopyPass(copyPass);
		SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
		SDL_ReleaseGPUTransferBuffer(gpu, transferBuffer);
	}
}

void Olaf::GraphicsManager::update(Options& options, double dt)
{

	if (get_graphic_API() == GraphicAPI::SDL3)
	{
		auto cmdbuf = SDL_AcquireGPUCommandBuffer(gpu);
		if (cmdbuf == NULL)
		{
			OLAF_ERROR("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
		}

		SDL_GPUTexture* swapchainTexture;
	
		if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, pWindow->getWindow().as<SDL_Window>(), &swapchainTexture, nullptr, nullptr))
		{
			OLAF_ERROR("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
		}
		
		if (swapchainTexture != NULL)
		{
			SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
			colorTargetInfo.texture = swapchainTexture;
			colorTargetInfo.clear_color = SDL_FColor{ 0.3f, 0.4f, 0.5f, 1.0f };
			colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
			colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;


			SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
			SDL_BindGPUGraphicsPipeline(renderPass, UseWireframeMode ? LinePipeline : FillPipeline);

			if (UseSmallViewport)
			{
				SDL_SetGPUViewport(renderPass, &SmallViewport);
			}
			if (UseScissorRect)
			{
				SDL_SetGPUScissor(renderPass, &ScissorRect);
			}

			// vertex attribute - per vertex

			// uniform data

			float rotationSpeedDegPerSec = 90.0f;
			float angleRadians = glm::radians(rotationSpeedDegPerSec * static_cast<float>(dt));
			glm::vec3 rotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);

			ubo.model = glm::rotate(ubo.model, angleRadians, rotationAxis);

			SDL_GPUBufferBinding binding = {};
			binding.buffer = vertexBuffer;
			binding.offset = 0;

			SDL_BindGPUVertexBuffers(renderPass, 0, &binding, 1);
			SDL_PushGPUVertexUniformData(cmdbuf, 0, &ubo, sizeof(ubo));
			SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
			SDL_EndGPURenderPass(renderPass);
		}
		SDL_SubmitGPUCommandBuffer(cmdbuf);

		if (this->onDraw) this->onDraw(options, *this, dt);
	}
}

void Olaf::GraphicsManager::close()
{
	if (get_graphic_API() == GraphicAPI::SDL3)
	{
		SDL_ReleaseGPUGraphicsPipeline(gpu, FillPipeline);
		SDL_ReleaseGPUGraphicsPipeline(gpu, LinePipeline);
	}
}

void Olaf::GraphicsManager::add(const Box* box)
{
	boxes.push_back(box);
}

void Olaf::GraphicsManager::remove(const Box* box)
{
	auto it = std::find(boxes.begin(), boxes.end(), box);
	if (it != boxes.end())
	{
		boxes.erase(it);
	}
}
