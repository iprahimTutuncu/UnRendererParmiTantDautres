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

    void onInput([[maybe_unused]] Olaf::Options& options, [[maybe_unused]] const double& deltaTime, [[maybe_unused]] const std::vector<Olaf::InputAction>& inputActions) override {
    }

    void onUpdate([[maybe_unused]] Olaf::Options& options, [[maybe_unused]] const double& deltaTime) override {
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
