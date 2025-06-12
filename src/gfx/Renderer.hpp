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
        const double particle_spacing = 0.015;
        const double grid_spacing = 0.120;
        const vec3 grid_size = vec3(2.0, 5.0, 2.0);
        const vec3 grid_origin = vec3(-0.5, -1.5, -0.5);
        const double simulation_dt = 1.0 / 160;

        const glm::vec3 camera_pos = glm::vec3(5.0f,0.5f,5.0f);
        const glm::vec3 camera_at = glm::vec3(0.0);

        std::shared_ptr<CameraPerspective> m_camera;

        std::unique_ptr<ShaderProgram> m_particleShader;
        std::unique_ptr<ParticleRenderer> m_particleRenderer;

        // Floor
        std::unique_ptr<ShaderProgram> m_floorShader;
        std::unique_ptr<FloorRenderer> m_floorRenderer;
};
