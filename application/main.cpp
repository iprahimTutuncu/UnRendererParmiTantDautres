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

    void onInput(Olaf::Options& options, const double& deltaTime, const std::vector<Olaf::InputAction>& inputActions) override {
        (void)options;
        (void)deltaTime;
        (void)inputActions;

     
    }

    void onUpdate(Olaf::Options& options, const double& deltaTime) override {
        (void)options;
        (void)deltaTime;

         imgui->newFrame();
         imgui->render();

        //   SDL_Event event;
         auto events = window->pollEvent();
         for (const auto& e : events) {
             imgui->processEvent(&e.sdlEvent); 
         }
    }

    void onDraw(Olaf::Options& options, Olaf::GraphicsManager& graphicsManager, const double& deltaTime) override {
        (void)options;
        (void)graphicsManager;
        (void)deltaTime;
       
    }
};

int main() {
    Neige engine;
    engine.init(); 
    engine.start();
    return 0;
}
