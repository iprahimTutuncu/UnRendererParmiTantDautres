#include <olaf/engine.h>
#include <imgui/ImGuiSDLGPU.h>


class Neige : public Olaf::Engine {
public:
    Neige() = default;
    ~Neige() = default;

    ImGuiSDLGPU* imgui = nullptr;

    void onInit() override {
    }

    void onStart() override {
        Olaf::GpuHandle handle = window->getGpuDevice();
        imgui = new ImGuiSDLGPU(handle.as<SDL_GPUDevice>());
        imgui->initialize(window->getWindow().as<SDL_Window>());
    }

    void onExit() override {
        if (imgui) {
          //  imgui->shutdown();
            delete imgui;
            imgui = nullptr;
        }
    }

    void onSuspend() override {
    }

    void onResume() override {
    }

    void onInput([[maybe_unused]] Olaf::Options& options, [[maybe_unused]] const double& deltaTime, [[maybe_unused]] const std::vector<Olaf::InputAction>& inputActions) override {
    }

    void onUpdate([[maybe_unused]] Olaf::Options& options, [[maybe_unused]] const double& deltaTime) override {
         imgui->newFrame();
         imgui->render();

        //   SDL_Event event;
         auto events = window->pollEvent();
         for (const auto& e : events) {
             imgui->processEvent(&e.sdlEvent); 
         }
    }

    void onDraw([[maybe_unused]] Olaf::Options& options, [[maybe_unused]] Olaf::GraphicsManager& graphicsManager, [[maybe_unused]] const double& deltaTime) override {
    }
};

int main() {
    Neige engine;
    engine.init(); 
    engine.start();
    return 0;
}
