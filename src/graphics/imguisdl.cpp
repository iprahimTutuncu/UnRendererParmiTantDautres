#include "imguisdl.h"

#include "../camera.h"
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

void imgui_iterate(AppState& state, SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmdbuf) {
    // Start the Dear ImGui frame
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

    if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("MainControl")) {
            ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Camera Control")) {

            // Camera options table
            if (ImGui::BeginTable("CameraTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                // Lock state row
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Camera Lock");
                ImGui::TableSetColumnIndex(1);
                if (state.camera->locked) {
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
                ImGui::Text("(%.2f, %.2f, %.2f)", state.camera->target.x, state.camera->target.y, state.camera->target.z);

                // Reset button row
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Reset");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button("Reset Camera")) {
                    state.camera->position = { 0.0f, 0.0f, state.camera->distance };
                    state.camera->target = { 0.0f, 0.0f, 0.0f };
                    state.camera->rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
                    state.camera->locked = false;
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }






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
