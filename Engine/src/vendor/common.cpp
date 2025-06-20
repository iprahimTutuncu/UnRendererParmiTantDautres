#include "common.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC  SDL_malloc
#define STBI_REALLOC SDL_realloc
#define STBI_FREE    SDL_free
#define STBI_ONLY_HDR
#include <stb/stb_image.h>

int CommonInit(Context* context, SDL_WindowFlags windowFlags) {
    context->Device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
        true,
        nullptr);

    if (context->Device == nullptr) {
        SDL_Log("GPUCreateDevice failed");
        return -1;
    }

    context->Window = SDL_CreateWindow(context->ExampleName, 640, 480, windowFlags);
    if (context->Window == nullptr) {
        SDL_Log("CreateWindow failed: %s", SDL_GetError());
        return -1;
    }

    if (!SDL_ClaimWindowForGPUDevice(context->Device, context->Window)) {
        SDL_Log("GPUClaimWindow failed");
        return -1;
    }

    return 0;
}

void CommonQuit(Context* context) {
    SDL_ReleaseWindowFromGPUDevice(context->Device, context->Window);
    SDL_DestroyWindow(context->Window);
    SDL_DestroyGPUDevice(context->Device);
}

static const char* BasePath = nullptr;
void InitializeAssetLoader() {
    BasePath = SDL_GetBasePath();
}

SDL_GPUShader* LoadShader(
    SDL_GPUDevice* device,
    const char* shaderFilename,
    Uint32 samplerCount,
    Uint32 uniformBufferCount,
    Uint32 storageBufferCount,
    Uint32 storageTextureCount) {
    // Auto-detect the shader stage from the file name for convenience
    SDL_GPUShaderStage stage;
    if (SDL_strstr(shaderFilename, ".vert")) {
        stage = SDL_GPU_SHADERSTAGE_VERTEX;
    } else if (SDL_strstr(shaderFilename, ".frag")) {
        stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    } else {
        SDL_Log("Invalid shader stage!");
        return nullptr;
    }

    char fullPath[256];
    SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    const char* entrypoint;

    if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
        SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/SPIRV/%s.spv", BasePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entrypoint = "main";
    } else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
        SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/MSL/%s.msl", BasePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_MSL;
        entrypoint = "main0";
    } else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
        SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/DXIL/%s.dxil", BasePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entrypoint = "main";
    } else {
        SDL_Log("%s", "Unrecognized backend shader format!");
        return nullptr;
    }

    size_t codeSize;
    void* code = SDL_LoadFile(fullPath, &codeSize);
    if (code == nullptr) {
        SDL_Log("Failed to load shader from disk! %s", fullPath);
        return nullptr;
    }

    SDL_GPUShaderCreateInfo shaderInfo = {};
    shaderInfo.code = (Uint8*)code;
    shaderInfo.code_size = codeSize;
    shaderInfo.entrypoint = entrypoint;
    shaderInfo.format = format;
    shaderInfo.stage = stage;
    shaderInfo.num_samplers = samplerCount;
    shaderInfo.num_uniform_buffers = uniformBufferCount;
    shaderInfo.num_storage_buffers = storageBufferCount;
    shaderInfo.num_storage_textures = storageTextureCount;

    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
    if (shader == nullptr) {
        SDL_Log("Failed to create shader!");
        SDL_free(code);
        return nullptr;
    }

    SDL_free(code);
    return shader;
}

SDL_GPUComputePipeline* CreateComputePipelineFromShader(
    SDL_GPUDevice* device,
    const char* shaderFilename,
    SDL_GPUComputePipelineCreateInfo* createInfo) {
    char fullPath[256];
    SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    const char* entrypoint;

    if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
        SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/SPIRV/%s.spv", BasePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_SPIRV;
        entrypoint = "main";
    } else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
        SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/MSL/%s.msl", BasePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_MSL;
        entrypoint = "main0";
    } else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
        SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/DXIL/%s.dxil", BasePath, shaderFilename);
        format = SDL_GPU_SHADERFORMAT_DXIL;
        entrypoint = "main";
    } else {
        SDL_Log("%s", "Unrecognized backend shader format!");
        return nullptr;
    }

    size_t codeSize;
    void* code = SDL_LoadFile(fullPath, &codeSize);
    if (code == nullptr) {
        SDL_Log("Failed to load compute shader from disk! %s", fullPath);
        return nullptr;
    }

    // Make a copy of the create data, then overwrite the parts we need
    SDL_GPUComputePipelineCreateInfo newCreateInfo = *createInfo;
    newCreateInfo.code = (Uint8*)code;
    newCreateInfo.code_size = codeSize;
    newCreateInfo.entrypoint = entrypoint;
    newCreateInfo.format = format;

    SDL_GPUComputePipeline* pipeline = SDL_CreateGPUComputePipeline(device, &newCreateInfo);
    if (pipeline == nullptr) {
        SDL_Log("Failed to create compute pipeline!");
        SDL_free(code);
        return nullptr;
    }

    SDL_free(code);
    return pipeline;
}

SDL_Surface* LoadImage(const char* imageFilename, int desiredChannels) {
    char fullPath[256];
    SDL_Surface* result;
    SDL_PixelFormat format;

    SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Images/%s", BasePath, imageFilename);

    result = SDL_LoadBMP(fullPath);
    if (result == nullptr) {
        SDL_Log("Failed to load BMP: %s", SDL_GetError());
        return nullptr;
    }

    if (desiredChannels == 4) {
        format = SDL_PIXELFORMAT_ABGR8888;
    } else {
        SDL_assert(!"Unexpected desiredChannels");
        SDL_DestroySurface(result);
        return nullptr;
    }
    if (result->format != format) {
        SDL_Surface* next = SDL_ConvertSurface(result, format);
        SDL_DestroySurface(result);
        result = next;
    }

    return result;
}

float* LoadHDRImage(const char* imageFilename, int* pWidth, int* pHeight, int* pChannels, int desiredChannels) {
    char fullPath[256];
    SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Images/%s", BasePath, imageFilename);
    return stbi_loadf(fullPath, pWidth, pHeight, pChannels, desiredChannels);
}

typedef struct ASTCHeader {
    Uint8 magic[4];
    Uint8 blockX;
    Uint8 blockY;
    Uint8 blockZ;
    Uint8 dimX[3];
    Uint8 dimY[3];
    Uint8 dimZ[3];
} ASTCHeader;

typedef struct DDS_PIXELFORMAT {
    int dwSize;
    int dwFlags;
    int dwFourCC;
    int dwRGBBitCount;
    int dwRBitMask;
    int dwGBitMask;
    int dwBBitMask;
    int dwABitMask;
} DDS_PIXELFORMAT;

typedef struct DDS_HEADER {
    int dwMagic;
    int dwSize;
    int dwFlags;
    int dwHeight;
    int dwWidth;
    int dwPitchOrLinearSize;
    int dwDepth;
    int dwMipMapCount;
    int dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    int dwCaps;
    int dwCaps2;
    int dwCaps3;
    int dwCaps4;
    int dwReserved2;
} DDS_HEADER;

typedef struct DDS_HEADER_DXT10 {
    int dxgiFormat;
    int resourceDimension;
    unsigned int miscFlag;
    unsigned int arraySize;
    unsigned int miscFlags2;
} DDS_HEADER_DXT10;

void* LoadASTCImage(const char* imageFilename, int* pWidth, int* pHeight, int* pImageDataLength) {
    char fullPath[256];
    SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Images/%s", BasePath, imageFilename);

    size_t fileSize;
    void* fileContents = SDL_LoadFile(fullPath, &fileSize);
    if (fileContents == nullptr) {
        SDL_assert(!"Could not load ASTC image!");
        return nullptr;
    }

    ASTCHeader* header = (ASTCHeader*)fileContents;
    if (header->magic[0] != 0x13 || header->magic[1] != 0xAB || header->magic[2] != 0xA1 || header->magic[3] != 0x5C) {
        SDL_assert(!"Bad magic number!");
        return nullptr;
    }

    // Get the image dimensions in texels
    *pWidth = header->dimX[0] + (header->dimX[1] << 8) + (header->dimX[2] << 16);
    *pHeight = header->dimY[0] + (header->dimY[1] << 8) + (header->dimY[2] << 16);

    // Get the size of the texture data
    unsigned int block_count_x = (*pWidth + header->blockX - 1) / header->blockX;
    unsigned int block_count_y = (*pHeight + header->blockY - 1) / header->blockY;
    *pImageDataLength = block_count_x * block_count_y * 16;

    void* data = SDL_malloc(*pImageDataLength);
    SDL_memcpy(data, (char*)fileContents + sizeof(ASTCHeader), *pImageDataLength);
    SDL_free(fileContents);

    return data;
}

void* LoadDDSImage(const char* imageFilename, SDL_GPUTextureFormat format, int* pWidth, int* pHeight, int* pImageDataLength) {
    char fullPath[256];
    SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Images/%s", BasePath, imageFilename);

    size_t fileSize;
    void* fileContents = SDL_LoadFile(fullPath, &fileSize);
    if (fileContents == nullptr) {
        SDL_assert(!"Could not load DDS image!");
        return nullptr;
    }

    DDS_HEADER* header = (DDS_HEADER*)fileContents;
    if (header->dwMagic != 0x20534444) {
        SDL_assert(!"Bad magic number!");
        return nullptr;
    }

    bool hasDX10Header = header->ddspf.dwFlags == 0x4 && header->ddspf.dwFourCC == 0x30315844;

    *pWidth = header->dwWidth;
    *pHeight = header->dwHeight;
    *pImageDataLength = header->dwPitchOrLinearSize;

    void* data = SDL_malloc(*pImageDataLength);
    SDL_memcpy(data, (char*)fileContents + sizeof(DDS_HEADER) + (hasDX10Header ? sizeof(DDS_HEADER_DXT10) : 0), *pImageDataLength);
    SDL_free(fileContents);

    return data;
}

// Matrix Math

Matrix4x4 Matrix4x4_Multiply(Matrix4x4 matrix1, Matrix4x4 matrix2) {
    Matrix4x4 result;

    result.m11 = ((matrix1.m11 * matrix2.m11) + (matrix1.m12 * matrix2.m21) + (matrix1.m13 * matrix2.m31) + (matrix1.m14 * matrix2.m41));
    result.m12 = ((matrix1.m11 * matrix2.m12) + (matrix1.m12 * matrix2.m22) + (matrix1.m13 * matrix2.m32) + (matrix1.m14 * matrix2.m42));
    result.m13 = ((matrix1.m11 * matrix2.m13) + (matrix1.m12 * matrix2.m23) + (matrix1.m13 * matrix2.m33) + (matrix1.m14 * matrix2.m43));
    result.m14 = ((matrix1.m11 * matrix2.m14) + (matrix1.m12 * matrix2.m24) + (matrix1.m13 * matrix2.m34) + (matrix1.m14 * matrix2.m44));
    result.m21 = ((matrix1.m21 * matrix2.m11) + (matrix1.m22 * matrix2.m21) + (matrix1.m23 * matrix2.m31) + (matrix1.m24 * matrix2.m41));
    result.m22 = ((matrix1.m21 * matrix2.m12) + (matrix1.m22 * matrix2.m22) + (matrix1.m23 * matrix2.m32) + (matrix1.m24 * matrix2.m42));
    result.m23 = ((matrix1.m21 * matrix2.m13) + (matrix1.m22 * matrix2.m23) + (matrix1.m23 * matrix2.m33) + (matrix1.m24 * matrix2.m43));
    result.m24 = ((matrix1.m21 * matrix2.m14) + (matrix1.m22 * matrix2.m24) + (matrix1.m23 * matrix2.m34) + (matrix1.m24 * matrix2.m44));
    result.m31 = ((matrix1.m31 * matrix2.m11) + (matrix1.m32 * matrix2.m21) + (matrix1.m33 * matrix2.m31) + (matrix1.m34 * matrix2.m41));
    result.m32 = ((matrix1.m31 * matrix2.m12) + (matrix1.m32 * matrix2.m22) + (matrix1.m33 * matrix2.m32) + (matrix1.m34 * matrix2.m42));
    result.m33 = ((matrix1.m31 * matrix2.m13) + (matrix1.m32 * matrix2.m23) + (matrix1.m33 * matrix2.m33) + (matrix1.m34 * matrix2.m43));
    result.m34 = ((matrix1.m31 * matrix2.m14) + (matrix1.m32 * matrix2.m24) + (matrix1.m33 * matrix2.m34) + (matrix1.m34 * matrix2.m44));
    result.m41 = ((matrix1.m41 * matrix2.m11) + (matrix1.m42 * matrix2.m21) + (matrix1.m43 * matrix2.m31) + (matrix1.m44 * matrix2.m41));
    result.m42 = ((matrix1.m41 * matrix2.m12) + (matrix1.m42 * matrix2.m22) + (matrix1.m43 * matrix2.m32) + (matrix1.m44 * matrix2.m42));
    result.m43 = ((matrix1.m41 * matrix2.m13) + (matrix1.m42 * matrix2.m23) + (matrix1.m43 * matrix2.m33) + (matrix1.m44 * matrix2.m43));
    result.m44 = ((matrix1.m41 * matrix2.m14) + (matrix1.m42 * matrix2.m24) + (matrix1.m43 * matrix2.m34) + (matrix1.m44 * matrix2.m44));

    return result;
}
Matrix4x4 Matrix4x4_CreateRotationZ(float radians) {
    Matrix4x4 result = {};
    result.m11 = SDL_cosf(radians);
    result.m12 = SDL_sinf(radians);
    result.m13 = 0;
    result.m14 = 0;

    result.m21 = -SDL_sinf(radians);
    result.m22 = SDL_cosf(radians);
    result.m23 = 0;
    result.m24 = 0;

    result.m31 = 0;
    result.m32 = 0;
    result.m33 = 1;
    result.m34 = 0;

    result.m41 = 0;
    result.m42 = 0;
    result.m43 = 0;
    result.m44 = 1;

    return result;
}

Matrix4x4 Matrix4x4_CreateTranslation(float x, float y, float z) {
    Matrix4x4 result = {};
    result.m11 = 1;
    result.m12 = 0;
    result.m13 = 0;
    result.m14 = 0;
    result.m21 = 0;
    result.m22 = 1;
    result.m23 = 0;
    result.m24 = 0;
    result.m31 = 0;
    result.m32 = 0;
    result.m33 = 1;
    result.m34 = 0;
    result.m41 = x;
    result.m42 = y;
    result.m43 = z;
    result.m44 = 1;
    return result;
}

Matrix4x4 Matrix4x4_CreateOrthographicOffCenter(
    float left,
    float right,
    float bottom,
    float top,
    float zNearPlane,
    float zFarPlane) {
    Matrix4x4 result = {};
    result.m11 = 2.0f / (right - left);
    result.m22 = 2.0f / (top - bottom);
    result.m33 = 1.0f / (zNearPlane - zFarPlane);
    result.m41 = (left + right) / (left - right);
    result.m42 = (top + bottom) / (bottom - top);
    result.m43 = zNearPlane / (zNearPlane - zFarPlane);
    result.m44 = 1.0f;
    return result;
}

Matrix4x4 Matrix4x4_CreatePerspectiveFieldOfView(
    float fieldOfView,
    float aspectRatio,
    float nearPlaneDistance,
    float farPlaneDistance) {
    float num = 1.0f / SDL_tanf(fieldOfView * 0.5f);
    Matrix4x4 result = {};
    result.m11 = num / aspectRatio;
    result.m22 = num;
    result.m33 = farPlaneDistance / (nearPlaneDistance - farPlaneDistance);
    result.m34 = -1;
    result.m43 = (nearPlaneDistance * farPlaneDistance) / (nearPlaneDistance - farPlaneDistance);
    return result;
}

Matrix4x4 Matrix4x4_CreateLookAt(
    Vector3 cameraPosition,
    Vector3 cameraTarget,
    Vector3 cameraUpVector) {
    Vector3 targetToPosition = {
        cameraPosition.x - cameraTarget.x,
        cameraPosition.y - cameraTarget.y,
        cameraPosition.z - cameraTarget.z
    };

    Vector3 vectorA = Vector3_Normalize(targetToPosition);
    Vector3 vectorB = Vector3_Normalize(Vector3_Cross(cameraUpVector, vectorA));
    Vector3 vectorC = Vector3_Cross(vectorA, vectorB);

    Matrix4x4 result = {};
    result.m11 = vectorB.x;
    result.m12 = vectorC.x;
    result.m13 = vectorA.x;
    result.m14 = 0;
    result.m21 = vectorB.y;
    result.m22 = vectorC.y;
    result.m23 = vectorA.y;
    result.m24 = 0;
    result.m31 = vectorB.z;
    result.m32 = vectorC.z;
    result.m33 = vectorA.z;
    result.m34 = 0;
    result.m41 = -Vector3_Dot(vectorB, cameraPosition);
    result.m42 = -Vector3_Dot(vectorC, cameraPosition);
    result.m43 = -Vector3_Dot(vectorA, cameraPosition);
    result.m44 = 1;
    return result;
}

Vector3 Vector3_Normalize(Vector3 vec) {
    float magnitude = SDL_sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
    Vector3 result = {};
    result.x = vec.x / magnitude;
    result.y = vec.y / magnitude;
    result.z = vec.z / magnitude;
    return result;
}

float Vector3_Dot(Vector3 vecA, Vector3 vecB) {
    return (vecA.x * vecB.x) + (vecA.y * vecB.y) + (vecA.z * vecB.z);
}

Vector3 Vector3_Cross(Vector3 vecA, Vector3 vecB) {
    Vector3 result = {};
    result.x = vecA.y * vecB.z - vecB.y * vecA.z;
    result.y = -(vecA.x * vecB.z - vecB.x * vecA.z);
    result.z = vecA.x * vecB.y - vecB.x * vecA.y;
    return result;
}
