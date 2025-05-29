#include "pch.h"
#include <engine.h>

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
    }

    void onUpdate(Olaf::Options& options, const double& deltaTime) override {
    }

    void onDraw(Olaf::Options& options, Olaf::GraphicsManager& graphicsManager, const double& deltaTime) override {
    }
};

int main(int argc, char** argv) {
    Olaf::Engine* engine = new Neige();
    engine->init();
    engine->start();
    delete engine;
    return 0;
}
