#include "FloorRenderer.hpp"

FloorRenderer::~FloorRenderer() {
    deinit();
}

void FloorRenderer::init(ShaderProgram* shader) {
    m_shader = shader;
    m_shader->bind();

    float half_size = m_size / 2.0f;

    m_positions = {
        glm::vec3(-half_size, m_y_pos, -half_size),
        glm::vec3(half_size, m_y_pos, -half_size),
        glm::vec3(half_size, m_y_pos, half_size),
        glm::vec3(-half_size, m_y_pos, half_size)
    };

    m_indices = {
        glm::uvec3(0, 2, 1),
        glm::uvec3(2, 0, 3)
    };

    glCreateVertexArrays(1, &m_vao);
    glCreateBuffers(VertexBufferId::NumVertexBuffers, m_vbo);
    glCreateBuffers(1, &m_ebo);

    glNamedBufferData(
        m_vbo[VertexBufferId::Position],
        m_positions.size() * sizeof(glm::vec3),
        m_positions.data(),
        GL_STATIC_DRAW);

    glNamedBufferData(
        m_ebo,
        m_indices.size() * sizeof(glm::uvec3),
        m_indices.data(),
        GL_STATIC_DRAW);

    int vPositionLocation = shader->attributeLocation("vPosition");
    glVertexArrayAttribFormat(m_vao, vPositionLocation, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayVertexBuffer(m_vao, vPositionLocation, m_vbo[VertexBufferId::Position], 0, sizeof(glm::vec3));
    glEnableVertexArrayAttrib(m_vao, vPositionLocation);
    glVertexArrayAttribBinding(m_vao, vPositionLocation, vPositionLocation);

    glVertexArrayElementBuffer(m_vao, m_ebo);

    m_initialized = true;
}

void FloorRenderer::render() const {
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indices.size() * 3, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void FloorRenderer::deinit() {
    Mesh::deinit();
    m_initialized = false;
}
