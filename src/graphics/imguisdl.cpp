#include "imguisdl.h"

#include "../camera.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

void imgui_init(AppState& state) {
     // Setup Dear ImGui context
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLGPU(state.window);
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = state.device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(state.device, state.window);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&init_info);
}

void imgui_iterate(AppState& state, SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmdbuf) {
    // Start the Dear ImGui frame
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

    ImGui::BeginGroup();

    ImGui::SliderFloat3("Camera", &state.camera->position.x, -100.0f, 100.0f);

    ImGui::EndGroup();

    ImGui::End();
    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();
    Imgui_ImplSDLGPU3_PrepareDrawData(draw_data, cmdbuf);
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmdbuf, renderPass);
}

void imgui_event(AppState& state, SDL_Event& event) {
    (void)state;
    ImGui_ImplSDL3_ProcessEvent(&event);
}

void imgui_quit(AppState& state) {
    (void)state;
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
