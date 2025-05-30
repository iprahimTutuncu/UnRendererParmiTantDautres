#include <imgui/ImGuiSDLGPU.h>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlgpu3.h> // For SDL3 GPU compatibility (make sure this backend exists)
#include <imgui.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>




ImGuiSDLGPU::ImGuiSDLGPU(SDL_GPUDevice* device)
    : window_(nullptr)
    , device_(device) {
    clear_color_ = ImVec4(0.1f, 0.2f, 0.3f, 1.0f);
}

ImGuiSDLGPU::~ImGuiSDLGPU() {
    shutdown();
}

void ImGuiSDLGPU::initialize(SDL_Window* window) {
    window_ = window;

    // Setup Dear ImGui context
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);
    //  SDL_SetGPUSwapchainParameters(sdlGPU, this->sdlWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    int width = 0, height = 0;
    SDL_GetWindowSize(window_, &width, &height);
    io.DisplaySize = ImVec2((float)width, (float)height);
   
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLGPU(window_);
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = device_;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device_, window_);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&init_info);
}

void ImGuiSDLGPU::newFrame() {
    bool show_demo_window = true;

    // Start the Dear ImGui frame
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
        //ImGui::ShowDemoWindow(&show_demo_window); // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.

    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit4("clear color", (float*)&clear_color_); // Edit 3 floats representing a color

        if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::End();
    }
}

void ImGuiSDLGPU::render() {
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);


    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device_); // Acquire a GPU command buffer

    SDL_GPUTexture* swapchain_texture;
    SDL_AcquireGPUSwapchainTexture(command_buffer, window_, &swapchain_texture, nullptr, nullptr); // Acquire a swapchain texture

    if (swapchain_texture != nullptr && !is_minimized) {
        // This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index buffer!
        Imgui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);
        // Setup and start a render pass
        SDL_GPUColorTargetInfo target_info = {};
        target_info.texture = swapchain_texture;
        target_info.clear_color = SDL_FColor { clear_color_.x, clear_color_.y, clear_color_.z, clear_color_.w };
        target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
        target_info.mip_level = 0;
        target_info.layer_or_depth_plane = 0;
        target_info.cycle = false;
        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

        // Render ImGui
        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

        SDL_EndGPURenderPass(render_pass);
    }
    // Submit the command buffer
    SDL_SubmitGPUCommandBuffer(command_buffer);
}

void ImGuiSDLGPU::processEvent(const SDL_Event* event) {
    ImGui_ImplSDL3_ProcessEvent(event);
}

void ImGuiSDLGPU::shutdown() {
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
