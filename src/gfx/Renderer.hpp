#pragma once

#include "gfx/ShaderProgram.hpp"
#include "gfx/CameraPerspective.hpp"
#include "gfx/ParticleRenderer.hpp"
#include "gfx/FloorRenderer.hpp"
#include "libs/eigen.hpp"

#include <memory>

class Renderer {
    public:
        Renderer() = default;
        ~Renderer() = default;

        bool init(int width, int height, int nb_particles = 0);
        void render_particles();
        void render_scene();
        void resize(int width, int height);
        void update_particles(std::vector<vec3>& positions);
        void clear();

        void process_input(double dt);
        void process_wheel(int y);
        void process_mouse(int x, int y);

    private:
        bool init_shaders();
        bool init_scene();
        bool init_particles(int count);

    private:

        std::shared_ptr<CameraPerspective> m_camera;

        // Particle
        std::unique_ptr<ShaderProgram> m_particleShader;
        std::unique_ptr<ParticleRenderer> m_particleRenderer;

        // Floor
        std::unique_ptr<ShaderProgram> m_floorShader;
        std::unique_ptr<FloorRenderer> m_floorRenderer;
};
