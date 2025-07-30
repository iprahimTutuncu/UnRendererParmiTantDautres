#pragma once

#include "Mesh.hpp"
#include "ShaderProgram.hpp"
#include <Eigen/Dense>

struct alignas(16) Particle {
    glm::vec3 position;
    float _p1;
};

class ParticleRenderer : Mesh {
public:
    ParticleRenderer();
    ~ParticleRenderer() override;

    void render() const override;
    void init(ShaderProgram* shader, unsigned int nb_particles);
    void deinit() override;
    void update_particles(std::vector<Eigen::Vector3f>& positions);

private:
    ShaderProgram* m_shader = nullptr;
    void* m_bufferPointer = nullptr;

    GLint m_bufferLocation;
    GLint m_bindingPoint;
    int m_particleCount = 0;
};
