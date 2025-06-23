#include "imguisdl.h"
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlgpu3.h>
#include <imgui.h>


SDL_AppResult imgui_init(AppState& state, int argc, char** argv) {
    (void)argc;
    (void)argv;


    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    int width = 0, height = 0;
    SDL_GetWindowSize(state.window, &width, &height);
    io.DisplaySize = ImVec2((float)width, (float)height);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLGPU(state.window);
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = state.device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(state.device, state.window);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&init_info);
    return SDL_APP_CONTINUE;
}

void imgui_iterate(AppState& state, SDL_GPURenderPass* renderPass, SDL_GPUTexture* swapchainTexture, SDL_GPUCommandBuffer* cmdbuf) {
    (void)state;

    bool show_demo_window = true;

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (show_demo_window) {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.
        ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;

        if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags)) {
            if (ImGui::BeginTabItem("MainControl")) {
                ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)

                ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
                ImGui::ColorEdit4("clear color", (float*)new ImVec4(0.1f, 0.2f, 0.3f, 1.0f)); // Edit 3 floats representing a color

                if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
                    counter++;
                ImGui::SameLine();
                ImGui::Text("counter = %d", counter);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Util")) {
                ImGui::Text("This is the Broccoli tab!\nblah blah blah blah blah");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Logs")) {
                ImGui::Text("This is the Cucumber tab!\nblah blah blah blah blah");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    if (swapchainTexture != nullptr) {
        // Setup and start a render pass
        SDL_GPUColorTargetInfo target_info = {};
        target_info.texture = swapchainTexture;
        target_info.clear_color = SDL_FColor { 0.3f, 0.4f, 0.5f, 1.0f };
        target_info.load_op = SDL_GPU_LOADOP_LOAD;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
        target_info.mip_level = 0;
        target_info.layer_or_depth_plane = 0;
        target_info.cycle = false;

        // Render ImGui
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        if (draw_data && draw_data->DisplaySize.x > 0 && draw_data->DisplaySize.y > 0) {
            Imgui_ImplSDLGPU3_PrepareDrawData(draw_data, cmdbuf);
            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmdbuf, renderPass);
        }
    }
}

SDL_AppResult imgui_event(AppState& state, SDL_Event& event) {
    (void)state;
    ImGui_ImplSDL3_ProcessEvent(&event);
    return SDL_APP_CONTINUE;
}

void imgui_quit(AppState& state) {
    (void)state;
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
