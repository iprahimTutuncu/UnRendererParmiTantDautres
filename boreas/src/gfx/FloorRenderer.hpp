#pragma once

#include "Mesh.hpp"
#include "ShaderProgram.hpp"

#include <vector>

class FloorRenderer : public Mesh {
public:
    FloorRenderer(float y_pos = 0.0f, float size = 50.0f)
        : m_y_pos { y_pos }
        , m_size { size } { }

    ~FloorRenderer() override;

    void render() const override;
    void init(ShaderProgram* shader) override;
    void deinit() override;

private:
    ShaderProgram* m_shader = nullptr;

    float m_y_pos;
    float m_size;

    std::vector<glm::vec3> m_positions;
    std::vector<glm::uvec3> m_indices;
};
