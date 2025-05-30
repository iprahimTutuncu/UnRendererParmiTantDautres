#pragma once
#include "options.h"

#include <memory>
#include <functional>
#include <vector>
#include <glm/glm.hpp>

struct SDL_GPUDevice;
struct SDL_GPUBuffer;
struct SDL_GPUTexture;
struct SDL_GPUSampler;

namespace Olaf
{
    class Window;

    struct Box
    {
        glm::vec3 min;
        glm::vec3 max;
    };

    struct Vertex 
    {
        float position[3];  // 12 
        float normal[3];    // 12 
        float texCoord[2];  // 8 
    };

    struct UBO
    {
        glm::mat4 proj;
        glm::mat4 view;
        glm::mat4 model;
    };

    class GraphicsManager
    {
    public:

    public:
        void init(const Options& options, std::shared_ptr<Window> window, std::function<void(Options&, GraphicsManager&, const double&)> onDrawCallback);
        void update(Options& options, double dt);
        void close();
        void add(const Box* box);
        void remove(const Box* box);

        //void add(const std::Vector<Particle>* particles);
        //void remove(const std::Vector<Particle>* particles);

        //void add(const Mesh* mesh);
        //void remove(const Mesh* mesh);

    private:

        std::function<void(Options&, GraphicsManager&, const double&)> onDraw;
        std::shared_ptr<Window> pWindow;
        SDL_GPUDevice* gpu;
        SDL_GPUBuffer* vertexBuffer;
        SDL_GPUBuffer* indexBuffer;
        SDL_GPUTexture* texture;
        SDL_GPUTexture* depthTexture;
        std::vector<const Box*> boxes;

        //très temporaire, j'va jsute tester mon shader
        UBO ubo;
    };
}
