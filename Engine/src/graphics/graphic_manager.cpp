#include "pch.h"
#include "options.h"
#include "graphics/graphic_manager.h"
#include "system/window.h"
#include <SDL3/SDL_gpu.h>
#include <system/log.h>
#include <SDL3/SDL_filesystem.h>

#include <stb_image/stb_image.h>
#include <glm/ext.hpp>
#include <graphics/image.h>

static SDL_GPUGraphicsPipeline* FillPipeline;
static SDL_GPUGraphicsPipeline* LinePipeline;
static SDL_GPUGraphicsPipeline* renderPipeline;
static SDL_GPUViewport SmallViewport = { 160, 120, 320, 240, 0.1f, 1.0f };
static SDL_Rect ScissorRect = { 320, 240, 320, 240 };

static bool UseWireframeMode = true;
static bool UseSmallViewport = false;
static bool UseScissorRect = false;

static const char* BasePath = NULL;

static const char* SamplerNames[] =
{
	"PointClamp",
	"PointWrap",
	"LinearClamp",
	"LinearWrap",
	"AnisotropicClamp",
	"AnisotropicWrap",
};

SDL_GPUSampler* Samplers[SDL_arraysize(SamplerNames)];

static int CurrentSamplerIndex;

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

	ubo.proj = glm::perspective(glm::radians(50.f), (float)options.windowOptions.screenWidth / (float)options.windowOptions.screenHeight, 0.01f, 1000.f);
	
	ubo.view = glm::lookAt
	(
		glm::vec3(0.f, 0.f, 15.f),
		glm::vec3(0.f, 0.f, 0.f),
		glm::vec3(0.f, 1.f, 0.f) 
	);
	
	ubo.model = glm::mat4(1.f);

	GpuHandle handle = pWindow->getGpuDevice();
	if (get_graphic_API() == GraphicAPI::SDL3)
	{
		InitializeAssetLoader();

		Image image;

		std::string fullPath = std::string(BasePath) + "media/images/sdl.png";
		image.loadFromFile(fullPath);

		gpu = handle.as<SDL_GPUDevice>();

		// Create the shaders
		SDL_GPUShader* gBufferVertexShader = LoadShader(gpu, "deferred_gBuffer.vert", 0, 1, 0, 0);
		if (gBufferVertexShader == NULL)
		{
			OLAF_ERROR("Failed to create vertex shader!");
		}

		SDL_GPUShader* gBufferFragmentShader = LoadShader(gpu, "deferred_gBuffer.frag", 1, 0, 0, 0);
		if (gBufferFragmentShader == NULL)
		{
			OLAF_ERROR("Failed to create fragment shader!");
		}

		SDL_GPUShader* deferredVertexShader = LoadShader(gpu, "quad.vert", 0, 0, 0, 0);
		if (deferredVertexShader == NULL)
		{
			OLAF_ERROR("Failed to create vertex shader!");
		}

		SDL_GPUShader* deferredFragmentShader = LoadShader(gpu, "deferred_render.frag", 3, 1, 0, 0);
		if (deferredFragmentShader == NULL)
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

		SDL_GPUTextureCreateInfo sceneDepthTextureInfo;
		sceneDepthTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
		int width, height;
		SDL_GetWindowSize(window, &width, &height);
		sceneDepthTextureInfo.width = width;
		sceneDepthTextureInfo.height = height;
		sceneDepthTextureInfo.layer_count_or_depth = 1;
		sceneDepthTextureInfo.num_levels = 1;
		sceneDepthTextureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
		sceneDepthTextureInfo.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
		sceneDepthTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

		depthTexture = SDL_CreateGPUTexture(gpu, &sceneDepthTextureInfo);

		//g-buffer texture creation
		auto createGBufferTexture = [=](int width, int height, SDL_GPUTextureFormat format) -> SDL_GPUTexture*
			{
				SDL_GPUTextureCreateInfo createInfo = {};
				createInfo.type = SDL_GPU_TEXTURETYPE_2D;                     
				createInfo.format = format;                                   
				createInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
				createInfo.width = static_cast<Uint32>(width);
				createInfo.height = static_cast<Uint32>(height);
				createInfo.layer_count_or_depth = 1;                          
				createInfo.num_levels = 1;                                    
				createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;              
				createInfo.props = 0;                                         

				SDL_GPUTexture* texture = SDL_CreateGPUTexture(gpu, &createInfo);
				if (!texture)
				{
					OLAF_ERROR("Failed to create G-buffer texture: {}", SDL_GetError());
				}
				return texture;
			};

		int w = options.windowOptions.screenWidth;
		int h = options.windowOptions.screenHeight;

		gPosition = createGBufferTexture(w, h, SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT);
		gNormal = createGBufferTexture(w, h, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT);
		gAlbedo = createGBufferTexture(w, h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);


		SDL_GPUColorTargetDescription colorTargetDescs[3] = {};
		SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(gpu, window);

		for (int i = 0; i < 3; ++i)
			colorTargetDescs[i].format = format;

		// create les texture pour le Gbufffer ici
		// review if I did good
		SDL_GPUVertexInputState fullscreenInputState = {};
		fullscreenInputState.num_vertex_buffers = 0;
		fullscreenInputState.num_vertex_attributes = 0;

		SDL_GPUVertexInputState vertexInputState = {};
		vertexInputState.num_vertex_buffers = 1;
		vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
		vertexInputState.num_vertex_attributes = 3;
		vertexInputState.vertex_attributes = vertexAttributes;

		SDL_GPUDepthStencilState depthStencilState = {};
		depthStencilState.enable_depth_test = true;
		depthStencilState.enable_depth_write = true;
		depthStencilState.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
		depthStencilState.write_mask = 0xFF;

		SDL_GPUGraphicsPipelineCreateInfo gPipelineCreateInfo = {};
		gPipelineCreateInfo.target_info.num_color_targets = 3;
		gPipelineCreateInfo.target_info.color_target_descriptions = colorTargetDescs;
		gPipelineCreateInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
		gPipelineCreateInfo.target_info.has_depth_stencil_target = true;
		gPipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		gPipelineCreateInfo.vertex_shader = gBufferVertexShader;
		gPipelineCreateInfo.fragment_shader = gBufferFragmentShader;
		gPipelineCreateInfo.vertex_input_state = vertexInputState;
		gPipelineCreateInfo.depth_stencil_state = depthStencilState;
		gPipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
		FillPipeline = SDL_CreateGPUGraphicsPipeline(gpu, &gPipelineCreateInfo);
		if (FillPipeline == NULL)
		{
			OLAF_ERROR("Failed to create fill pipeline!");
		}

		gPipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;
		LinePipeline = SDL_CreateGPUGraphicsPipeline(gpu, &gPipelineCreateInfo);
		if (LinePipeline == NULL)
		{
			OLAF_ERROR("Failed to create line pipeline!");
		}

		SDL_GPUDepthStencilState fullscreenDepthStencilState = {};
		depthStencilState.enable_depth_test = false;
		depthStencilState.enable_depth_write = false;
		depthStencilState.compare_op = SDL_GPU_COMPAREOP_ALWAYS;

		// make the final render pass here I think
		SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.target_info.num_color_targets = 1;
		pipelineCreateInfo.target_info.color_target_descriptions = &colorTargetDescs[0];
		pipelineCreateInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
		pipelineCreateInfo.target_info.has_depth_stencil_target = false;
		pipelineCreateInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		pipelineCreateInfo.vertex_shader = deferredVertexShader;
		pipelineCreateInfo.fragment_shader = deferredFragmentShader;
		pipelineCreateInfo.vertex_input_state = fullscreenInputState;
		pipelineCreateInfo.depth_stencil_state = fullscreenDepthStencilState;
		pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
		renderPipeline = SDL_CreateGPUGraphicsPipeline(gpu, &pipelineCreateInfo);
		if (renderPipeline == NULL)
		{
			OLAF_ERROR("Failed to create render pipeline!");
		}

		// Clean up shader resources
		SDL_ReleaseGPUShader(gpu, gBufferVertexShader);
		SDL_ReleaseGPUShader(gpu, gBufferFragmentShader);
		SDL_ReleaseGPUShader(gpu, deferredVertexShader);
		SDL_ReleaseGPUShader(gpu, deferredFragmentShader);
		// Assuming Samplers is an array of SDL_GPUSampler* of size at least 6

		SDL_GPUSamplerCreateInfo info{};

		// pointClamp
		info = {};
		info.min_filter = SDL_GPU_FILTER_NEAREST;
		info.mag_filter = SDL_GPU_FILTER_NEAREST;
		info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
		info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		Samplers[0] = SDL_CreateGPUSampler(gpu, &info);

		// PointWrap
		info = {};
		info.min_filter = SDL_GPU_FILTER_NEAREST;
		info.mag_filter = SDL_GPU_FILTER_NEAREST;
		info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
		info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		Samplers[1] = SDL_CreateGPUSampler(gpu, &info);

		// LinearClamp
		info = {};
		info.min_filter = SDL_GPU_FILTER_LINEAR;
		info.mag_filter = SDL_GPU_FILTER_LINEAR;
		info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
		info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		Samplers[2] = SDL_CreateGPUSampler(gpu, &info);

		// LinearWrap
		info = {};
		info.min_filter = SDL_GPU_FILTER_LINEAR;
		info.mag_filter = SDL_GPU_FILTER_LINEAR;
		info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
		info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		Samplers[3] = SDL_CreateGPUSampler(gpu, &info);

		// AnisotropicClamp
		info = {};
		info.min_filter = SDL_GPU_FILTER_LINEAR;
		info.mag_filter = SDL_GPU_FILTER_LINEAR;
		info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
		info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		info.enable_anisotropy = true;
		info.max_anisotropy = 4;
		Samplers[4] = SDL_CreateGPUSampler(gpu, &info);

		// AnisotropicWrap
		info = {};
		info.min_filter = SDL_GPU_FILTER_LINEAR;
		info.mag_filter = SDL_GPU_FILTER_LINEAR;
		info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
		info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		info.enable_anisotropy = true;
		info.max_anisotropy = 4;
		Samplers[5] = SDL_CreateGPUSampler(gpu, &info);



		// Finally, print instructions!
		OLAF_ERROR("Press Left to toggle wireframe mode");
		OLAF_ERROR("Press Down to toggle small viewport");
		OLAF_ERROR("Press Right to toggle scissor rect");
		// Create the vertex buffer
		
		//Texture creation (to refactor to texture class)

		SDL_GPUTextureCreateInfo textureCreateInfo;
		textureCreateInfo.type = SDL_GPU_TEXTURETYPE_2D;
		textureCreateInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		textureCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
		textureCreateInfo.width = image.getWidth();
		textureCreateInfo.height = image.getHeight();
		textureCreateInfo.layer_count_or_depth = 1;
		textureCreateInfo.num_levels = 1;
		textureCreateInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

		texture = SDL_CreateGPUTexture(gpu, &textureCreateInfo);

		SDL_GPUTransferBufferCreateInfo textureTransferBufferInfo;
		textureTransferBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		textureTransferBufferInfo.size = image.getSize();
		auto textureTransferBuffer = SDL_CreateGPUTransferBuffer(gpu, &textureTransferBufferInfo);

		Uint8* textureTransferPtr = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(gpu, textureTransferBuffer, false));
		SDL_memcpy(textureTransferPtr, image.getData(), image.getSize());
		SDL_UnmapGPUTransferBuffer(gpu, textureTransferBuffer);

		SDL_GPUBufferCreateInfo vertexBufferInfo = {};
		vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		vertexBufferInfo.size = sizeof(Vertex) * 24;

		vertexBuffer = SDL_CreateGPUBuffer(gpu, &vertexBufferInfo);

		SDL_GPUBufferCreateInfo indexBufferInfo = {};
		indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
		indexBufferInfo.size = sizeof(Uint16) * 36;

		indexBuffer = SDL_CreateGPUBuffer(gpu, &indexBufferInfo);

		// Create the transfer buffer
		SDL_GPUTransferBufferCreateInfo transferBufferInfo = {};
		transferBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		transferBufferInfo.size = vertexBufferInfo.size + indexBufferInfo.size;  // Quad = 4 vertices

		SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpu, &transferBufferInfo);

		// Map the transfer buffer
		Vertex* transferData = static_cast<Vertex*>(SDL_MapGPUTransferBuffer(gpu, transferBuffer, false));

		// Define cube vertices
		Vertex cubeVertices[] = {
			// Front face
			{{-1, -1,  1}, {0, 0,  1}, {0, 1}},
			{{ 1, -1,  1}, {0, 0,  1}, {1, 1}},
			{{ 1,  1,  1}, {0, 0,  1}, {1, 0}},
			{{-1,  1,  1}, {0, 0,  1}, {0, 0}},

			// Back face
			{{ 1, -1, -1}, {0, 0, -1}, {0, 1}},
			{{-1, -1, -1}, {0, 0, -1}, {1, 1}},
			{{-1,  1, -1}, {0, 0, -1}, {1, 0}},
			{{ 1,  1, -1}, {0, 0, -1}, {0, 0}},

			// Left face
			{{-1, -1, -1}, {-1, 0, 0}, {0, 1}},
			{{-1, -1,  1}, {-1, 0, 0}, {1, 1}},
			{{-1,  1,  1}, {-1, 0, 0}, {1, 0}},
			{{-1,  1, -1}, {-1, 0, 0}, {0, 0}},

			// Right face
			{{ 1, -1,  1}, {1, 0, 0}, {0, 1}},
			{{ 1, -1, -1}, {1, 0, 0}, {1, 1}},
			{{ 1,  1, -1}, {1, 0, 0}, {1, 0}},
			{{ 1,  1,  1}, {1, 0, 0}, {0, 0}},

			// Top face
			{{-1,  1,  1}, {0, 1, 0}, {0, 1}},
			{{ 1,  1,  1}, {0, 1, 0}, {1, 1}},
			{{ 1,  1, -1}, {0, 1, 0}, {1, 0}},
			{{-1,  1, -1}, {0, 1, 0}, {0, 0}},

			// Bottom face
			{{-1, -1, -1}, {0, -1, 0}, {0, 1}},
			{{ 1, -1, -1}, {0, -1, 0}, {1, 1}},
			{{ 1, -1,  1}, {0, -1, 0}, {1, 0}},
			{{-1, -1,  1}, {0, -1, 0}, {0, 0}},
		};

		// Copy vertices to GPU buffer
		std::memcpy(transferData, cubeVertices, sizeof(cubeVertices));

		// Now write the indices right after the vertex buffer
		Uint16* indexData = reinterpret_cast<Uint16*>(&transferData[24]);

		Uint16 cubeIndices[] = {
			// Front
			0, 1, 2, 0, 2, 3,
			// Back
			4, 5, 6, 4, 6, 7,
			// Left
			8, 9,10, 8,10,11,
			// Right
			12,13,14,12,14,15,
			// Top
			16,17,18,16,18,19,
			// Bottom
			20,21,22,20,22,23
		};

		// Copy indices
		std::memcpy(indexData, cubeIndices, sizeof(cubeIndices));

		SDL_UnmapGPUTransferBuffer(gpu, transferBuffer);

		// Upload to the vertex buffer
		SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(gpu);
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

		SDL_GPUTransferBufferLocation vertexTransferLoc = {};
		vertexTransferLoc.transfer_buffer = transferBuffer;
		vertexTransferLoc.offset = 0;

		SDL_GPUBufferRegion vertexBufferRegion = {};
		vertexBufferRegion.buffer = vertexBuffer;
		vertexBufferRegion.offset = 0;
		vertexBufferRegion.size = sizeof(Vertex) * 24;

		SDL_UploadToGPUBuffer(copyPass, &vertexTransferLoc, &vertexBufferRegion, false);

		SDL_GPUTransferBufferLocation indexTransferLoc = {};
		indexTransferLoc.transfer_buffer = transferBuffer;
		indexTransferLoc.offset = sizeof(Vertex) * 24;

		SDL_GPUBufferRegion indexBufferRegion = {};
		indexBufferRegion.buffer = indexBuffer;
		indexBufferRegion.offset = 0;
		indexBufferRegion.size = sizeof(Uint16) * 36;
		 
		SDL_UploadToGPUBuffer(copyPass, &indexTransferLoc, &indexBufferRegion, false);

		SDL_GPUTextureTransferInfo transferInfo = {};
		transferInfo.transfer_buffer = textureTransferBuffer;
		transferInfo.offset = 0;

		SDL_GPUTextureRegion textureRegion = {};
		textureRegion.texture = texture;
		textureRegion.w = image.getWidth();
		textureRegion.h = image.getHeight();
		textureRegion.d = 1;

		SDL_UploadToGPUTexture(copyPass, &transferInfo, &textureRegion, false);


		SDL_EndGPUCopyPass(copyPass);
		SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
		SDL_ReleaseGPUTransferBuffer(gpu, transferBuffer);
		SDL_ReleaseGPUTransferBuffer(gpu, textureTransferBuffer);
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
			SDL_GPUDepthStencilTargetInfo depthStencilTargetInfo = { 0 };
			depthStencilTargetInfo.texture = depthTexture;
			depthStencilTargetInfo.cycle = true;
			depthStencilTargetInfo.clear_depth = 1;
			depthStencilTargetInfo.clear_stencil = 0;
			depthStencilTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
			depthStencilTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
			depthStencilTargetInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
			depthStencilTargetInfo.stencil_store_op = SDL_GPU_STOREOP_STORE;

			SDL_GPUColorTargetInfo gBuffers[3] = {};
			gBuffers[0].texture = gPosition;
			gBuffers[1].texture = gNormal;
			gBuffers[2].texture = gAlbedo;

			for (auto& g : gBuffers)
			{
				g.clear_color = { 0, 0, 0, 1 };
				g.load_op = SDL_GPU_LOADOP_CLEAR;
				g.store_op = SDL_GPU_STOREOP_STORE;
			}

			SDL_GPURenderPass* geometryPass = SDL_BeginGPURenderPass(cmdbuf, gBuffers, 3, &depthStencilTargetInfo);
			SDL_BindGPUGraphicsPipeline(geometryPass, UseWireframeMode ? LinePipeline : FillPipeline);


			if (UseSmallViewport)
			{
				SDL_SetGPUViewport(geometryPass, &SmallViewport);
			}
			if (UseScissorRect)
			{
				SDL_SetGPUScissor(geometryPass, &ScissorRect);
			}

			SDL_GPUBufferBinding binding = {};
			binding.buffer = vertexBuffer;  // vertex de la boite
			binding.offset = 0;

			SDL_GPUBufferBinding indexBinding = {};
			indexBinding.buffer = indexBuffer; // indices de la boite
			indexBinding.offset = 0;

			SDL_GPUTextureSamplerBinding samplerBinding = {};
			samplerBinding.texture = texture;
			samplerBinding.sampler = Samplers[CurrentSamplerIndex];

			SDL_BindGPUVertexBuffers(geometryPass, 0, &binding, 1);
			SDL_BindGPUIndexBuffer(geometryPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

			SDL_BindGPUFragmentSamplers(geometryPass, 0, &samplerBinding, 1);

			for (auto box : boxes)
			{
				glm::vec3 size = box->max - box->min;
				glm::vec3 center = (box->max + box->min) * 0.5f;

				ubo.model = glm::mat4(1.0f);
				ubo.model = glm::translate(ubo.model, center);
				ubo.model = glm::scale(ubo.model, size * 0.5f);
				SDL_PushGPUVertexUniformData(cmdbuf, 0, &ubo, sizeof(ubo));

				SDL_DrawGPUIndexedPrimitives(geometryPass, 36, 1, 0, 0, 0);
			}

			SDL_EndGPURenderPass(geometryPass);

			SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
			colorTargetInfo.texture = swapchainTexture;
			colorTargetInfo.clear_color = SDL_FColor{ 0.3f, 0.4f, 0.5f, 1.0f };
			colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
			colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

			SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, nullptr);
			SDL_BindGPUGraphicsPipeline(renderPass, renderPipeline);

			if (UseSmallViewport)
			{
				SDL_SetGPUViewport(renderPass, &SmallViewport);
			}
			if (UseScissorRect)
			{
				SDL_SetGPUScissor(renderPass, &ScissorRect);
			}

			// Bind the 3 G-buffer textures as samplers
			SDL_GPUTextureSamplerBinding gbufferSamplers[3];
			gbufferSamplers[0] = { gPosition, Samplers[0] };
			gbufferSamplers[1] = { gNormal, Samplers[0] };
			gbufferSamplers[2] = { gAlbedo, Samplers[0] };
			SDL_BindGPUFragmentSamplers(renderPass, 0, gbufferSamplers, 3);
			int displayMode = 1;  // 0 = Albedo, 1 = Normal, etc.
			SDL_PushGPUFragmentUniformData(cmdbuf, 0, &displayMode, sizeof(int));
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
		SDL_ReleaseGPUGraphicsPipeline(gpu, renderPipeline);

		SDL_ReleaseGPUTexture(gpu, depthTexture);
		SDL_ReleaseGPUTexture(gpu, gPosition);
		SDL_ReleaseGPUTexture(gpu, gNormal);
		SDL_ReleaseGPUTexture(gpu, gAlbedo);
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
