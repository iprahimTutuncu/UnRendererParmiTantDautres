#pragma once
#include <SDL3/SDL_gpu.h>
#include <glm/mat4x4.hpp>

struct GBufferTextures
{
    std::shared_ptr<SDL_GPUTexture> position;
    std::shared_ptr<SDL_GPUTexture> normal;
    std::shared_ptr<SDL_GPUTexture> albedo;
    std::shared_ptr<SDL_GPUTexture> depth;
};

struct Camera 
{
    // View parameters
    glm::vec3 position = { 0.0f, 0.0f, 15.0f };
    glm::vec3 target = { 0.0f, 0.0f, 0.0f };
    glm::vec3 up = { 0.0f, 1.0f, 0.0f };

    // Projection parameters
    float fov = 45.0f;
    float aspect = 16.0f / 9.0f;
    float nearClip = 0.1f;
    float farClip = 100.0f;
};


//there's GTS::Sprite out there, eventually use the one in sprite
struct Sprite
{
    glm::vec3 position{ 0.0f };
    glm::vec4 texCoords{ 0.0f, 0.0f, 1.0f, 1.0f };
    glm::vec2 scale{ 1.0f };
    float rotation = 0.0f;
    glm::vec4 color{ 1.0f };       // RGBA tint (default: white)
    glm::vec4 keyColor{ 0.0f };    // Color-key transparency (default: transparent black)
    bool flipX = false;
    bool flipY = false;
    bool visible = true;
};

struct UBO
{
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 model;
};
