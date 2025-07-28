#include "imguisdl.h"

#include "../camera.h"
#include "../controls/controls.h"
#include "graphics.h"

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

void imgui_iterate(AppState& state) {
    // Start the Dear ImGui frame
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

    ImGui::InputFloat3("Camera", &state.camera->position.x);
    if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("Mouvement State: ");
        ImGui::SameLine();

        if (state.controls->isCameraCaptured)
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Locked");
        else
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Unlocked");
        ImGui::SliderFloat("Movement Speed", &state.controls->movement_speed, 0.1f, 10.0f);
        ImGui::SliderFloat("Mouse Sensitivity", &state.controls->mouse_sensitivity, 0.01f, 1.0f);
        ImGui::SliderFloat("Distance From Target", &state.controls->distanceFromTarget, 0.1f, 100.0f);
        ImGui::SliderFloat("Yaw", &state.controls->yaw, -180.0f, 180.0f);
        ImGui::SliderFloat("Pitch", &state.controls->pitch, -89.0f, 89.0f);
    }

    // --- Blur Settings Tab ---
    if (ImGui::CollapsingHeader("Blur Settings")) {
        ImGui::TextUnformatted("Bilateral Blur Settings");
        ImGui::SliderFloat("Blur Scale", &state.graphics->bilateralBlurBufferUniform.blurScale, 0.1f, 10.0f);
        ImGui::SliderFloat("Depth Falloff", &state.graphics->bilateralBlurBufferUniform.blurDepthFalloff, 0.01f, 5.0f);
        ImGui::SliderInt("Filter Radius", &state.graphics->bilateralBlurBufferUniform.filterRadius, 1, 10);
    }

    ImGui::End();
    ImGui::Render();
}

void imgui_event(AppState& state, SDL_Event& event) {
    (void)state;
    ImGui_ImplSDL3_ProcessEvent(&event);
}

void imgui_quit(AppState& state) {
    (void)state;
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();
}
