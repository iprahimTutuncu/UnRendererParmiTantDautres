#include "gfx/Mesh.hpp"

Mesh::~Mesh() 
{
    deinit();
}

void Mesh::deinit()
{
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
    }
    if (m_ebo) {
        glDeleteBuffers(1, &m_ebo);
    }
    for (GLuint& vbo : m_vbo) {
        if (vbo) {
            glDeleteBuffers(1, &vbo);
        }
    }
}
