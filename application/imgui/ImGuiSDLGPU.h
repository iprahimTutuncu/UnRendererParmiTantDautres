#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <imgui.h>

class ImGuiSDLGPU  {
public:
    ImGuiSDLGPU(SDL_GPUDevice* device, SDL_Window* window);
    ~ImGuiSDLGPU();

    void initialize() ;
    void newFrame();
    void render();
    void processEvent(const SDL_Event* event);
    void shutdown();
    void renderDrawData(SDL_GPUCommandBuffer* command_buffer, SDL_GPURenderPass* render_pass);

private:
    SDL_Window* window_;
    SDL_GPUDevice* device_;
    ImVec4 clear_color_; 
};
