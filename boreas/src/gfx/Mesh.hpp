#pragma once

#include "ShaderProgram.hpp"

enum VertexBufferId : unsigned int {
    Position,
    Normal,
    Tangent,
    TexCoord,
    NumVertexBuffers
};

class Mesh {
public:
    virtual ~Mesh();

    GLuint vao() const {
        return m_vao;
    }
    const GLuint* vbo() const {
        return m_vbo;
    }
    GLuint ebo() const {
        return m_ebo;
    }

protected:
    GLuint m_vao = 0;
    GLuint m_vbo[VertexBufferId::NumVertexBuffers] = { 0 };
    GLuint m_ebo = 0;

    bool m_initialized = false;

    virtual void render() const = 0;
    virtual void init(ShaderProgram* shader) = 0;
    virtual void deinit();
};
