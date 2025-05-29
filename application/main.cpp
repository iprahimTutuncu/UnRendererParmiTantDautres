#include <olaf/engine.h>

class Neige : public Olaf::Engine {
public:
    Neige() = default;
    ~Neige() = default;

    void onInit() override {
    }

    void onStart() override {
    }

    void onExit() override {
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
