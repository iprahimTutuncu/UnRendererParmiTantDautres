#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <imgui.h>

class ImGuiSDLGPU  {
public:
    ImGuiSDLGPU(SDL_GPUDevice* device);
    ~ImGuiSDLGPU();

    void initialize(SDL_Window* window) ;
    void newFrame();
    void render();
    void processEvent(const SDL_Event* event);
    void shutdown();

private:
    SDL_Window* window_;
    SDL_GPUDevice* device_;
    ImVec4 clear_color_; 
};
