#include "imguisdl.h"

#include "../camera.h"
#include "graphics.h"
#include "../controls/controls.h"

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

    ImGui::SliderFloat3("Camera", &state.camera->position.x, -100.0f, 100.0f);

    ImGui::Checkbox("Enable SSAO", &state.graphics->ssaoEnabled);

    if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Camera Control")) {

            // Camera options table
            if (ImGui::BeginTable("CameraTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                // Lock state row
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Camera Lock");
                ImGui::TableSetColumnIndex(1);
                if (state.controls->isCameraLocked) {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Locked");
                } else {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Unlocked");
                }

                // Camera position row (read-only)
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Position");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("(%.2f, %.2f, %.2f)", state.camera->position.x, state.camera->position.y, state.camera->position.z);

                // Camera target row
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Target");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("(%.2f, %.2f, %.2f)", state.controls->cameraTarget.x, state.controls->cameraTarget.y, state.controls->cameraTarget.z);

                // Camera FOV (optional, for perspective zoom)
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("FOV");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f deg", state.camera->fov * (180.0f / 3.14159265f));

                // Reset button row
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Reset");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Reset Camera")) {
                    state.controls->cameraTarget = { 0.f, 0.f, 0.f };
                    state.camera->position = { 0.f, 0.f, 100.f };
                    state.camera->rotation = { 1.f, 0.f, 0.f, 0.f };
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
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
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
