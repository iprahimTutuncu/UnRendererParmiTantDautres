#include "ParticleRenderer.hpp"

#include <iostream>

ParticleRenderer::ParticleRenderer() { }

ParticleRenderer::~ParticleRenderer() {
    deinit();
}

void ParticleRenderer::render() const {
    if (m_particleCount == 0) {
        return;
    }

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_bindingPoint, m_ebo);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_POINTS, 0, m_particleCount);
}

void ParticleRenderer::init(ShaderProgram* shader) { }

void ParticleRenderer::init(ShaderProgram* shader, unsigned int nb_particles) {
    m_shader = shader;
    m_shader->bind();

    glCreateVertexArrays(1, &m_vao);

    // Instance data
    glCreateBuffers(1, &m_ebo);
    glNamedBufferStorage(
        m_ebo,
        sizeof(Particle) * nb_particles,
        nullptr,
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    GLenum init_error = glGetError();
    if (init_error != GL_NO_ERROR) {
        std::cerr << "ERROR: Failed to create OpenGL buffer: " << init_error << std::endl;
    }

    m_bufferLocation = glGetProgramResourceIndex(shader->programId(), GL_SHADER_STORAGE_BLOCK, "particleInstances");
    if (m_bufferLocation == GL_INVALID_INDEX) {
        std::cerr << "ERROR: ParticleRenderer SSBO block 'particleInstances' not found in shader!" << std::endl;
    }

    GLenum prop = GL_BUFFER_BINDING;
    GLsizei length = 0;
    GLint tmp_binding = 0;
    glGetProgramResourceiv(
        shader->programId(),
        GL_SHADER_STORAGE_BLOCK,
        m_bufferLocation,
        1, // Count of properties
        &prop, // Property to query
        1, // Size of the buffer
        &length, // Length written
        &tmp_binding // Output buffer
    );
    m_bindingPoint = GLuint(tmp_binding);

    // Utiliser le point de binding trouvé
    if (m_bindingPoint != 0) {
        std::cerr << "WARNING: Particle SSBO binding point expected 0 but got " << m_bindingPoint << "!" << std::endl;
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ebo);

    m_bufferPointer = glMapNamedBufferRange(m_ebo, 0, sizeof(Particle) * nb_particles, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    if (m_bufferPointer == nullptr) {
        GLenum map_error = glGetError();
        std::cerr << "ERROR: Failed to map particle buffer (m_ebo: " << m_ebo << ") to pointer! OpenGL Error: " << map_error << std::endl;
        return;
    }

    m_particleCount = nb_particles;
}

void ParticleRenderer::deinit() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_bindingPoint, 0);
    Mesh::deinit();
}

void ParticleRenderer::update_particles(std::vector<vec3>& positions) {
    float* dst = static_cast<float*>(m_bufferPointer);
    for (size_t i = 0; i < positions.size(); ++i) {
        const vec3& p = positions[i];
        dst[4 * i + 0] = p.x();
        dst[4 * i + 1] = p.y();
        dst[4 * i + 2] = p.z();
        dst[4 * i + 3] = 0.0f;
    }
}
