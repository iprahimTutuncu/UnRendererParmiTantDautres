#include "pch.h"
#include <engine.h>

class Neige : public GTS::Engine
{
public:
	Neige() = default;
	~Neige() = default;

	void onInit() override
	{
	}

	void onStart() override
	{
	}

	void onExit() override
	{
	}

	void onSuspend() override
	{
	}

	void onResume() override
	{
	}

	void onInput(GTS::Options& options, const double& deltaTime, const std::vector<GTS::InputAction>& inputActions) override
	{
	}

	void onUpdate(GTS::Options& options, const double& deltaTime) override
	{
	}

	void onDraw(GTS::Options& options, GTS::GraphicsManager& graphicsManager, const double& deltaTime) override
	{

	}

};


int main(int argc, char** argv) 
{
	GTS::Engine* engine = new Neige();
	engine->init();
	engine->start();
	delete engine;
	return 0;
}

/*
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <glm/glm.hpp>

struct Sprite {
    float x, y, z;
    float width, height;

    Sprite(float x, float y, float z, float w, float h)
        : x(x), y(y), z(z), width(w), height(h) {}
};

glm::vec3 camPosition(0.0f, 0.0f, 0.0f);

float distanceSquared(const Sprite& sprite, const glm::vec3& camPos) {
    float dx = sprite.x - camPos.x;
    float dy = sprite.y - camPos.y;
    float dz = sprite.z - camPos.z;
    return dx * dx + dy * dy + dz * dz;
}

int main() {
    const int NUM_SPRITES = 1000;
    const int BENCHMARK_RUNS = 100;
    std::vector<Sprite> sprites;
    std::vector<long> durations;
    durations.reserve(BENCHMARK_RUNS);

    // Initialize random engine once
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-100.0f, 100.0f);
    std::uniform_real_distribution<float> sizeDist(1.0f, 10.0f);

    // Generate sprites once (same set for all benchmarks)
    sprites.reserve(NUM_SPRITES);
    for (int i = 0; i < NUM_SPRITES; ++i) {
        sprites.emplace_back(
            posDist(gen), posDist(gen), posDist(gen),
            sizeDist(gen), sizeDist(gen)
        );
    }

    // Warm-up run (not measured)
    std::vector<Sprite> warmup = sprites;
    std::sort(warmup.begin(), warmup.end(), [](const Sprite& a, const Sprite& b) {
        return distanceSquared(a, camPosition) > distanceSquared(b, camPosition);
        });

    // Benchmark loop
    for (int run = 0; run < BENCHMARK_RUNS; ++run) {
        auto copy = sprites; // Copy original for each run

        auto start = std::chrono::high_resolution_clock::now();

        std::sort(copy.begin(), copy.end(), [](const Sprite& a, const Sprite& b) {
            return distanceSquared(a, camPosition) > distanceSquared(b, camPosition);
            });

        auto end = std::chrono::high_resolution_clock::now();
        durations.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
        );
    }

    // Calculate statistics
    long total = std::accumulate(durations.begin(), durations.end(), 0L);
    double average = static_cast<double>(total) / BENCHMARK_RUNS;

    auto min = *std::min_element(durations.begin(), durations.end());
    auto max = *std::max_element(durations.begin(), durations.end());

    // Output results
    std::cout << "Benchmark results (100 runs sorting " << NUM_SPRITES << " sprites):\n";
    std::cout << "Average time: " << average << " μs\n";
    std::cout << "Minimum time: " << min << " μs\n";
    std::cout << "Maximum time: " << max << " μs\n";
    std::cout << "First sorted sprite position: ("
        << sprites[0].x << ", " << sprites[0].y << ", " << sprites[0].z << ")\n";

    return 0;
}
*/