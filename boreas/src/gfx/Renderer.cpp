#include "gfx/Renderer.hpp"

const glm::vec3 camera_pos = glm::vec3(1.0f,0.5f,1.0f);
const glm::vec3 camera_at = glm::vec3(0.0);


bool Renderer::init(int width, int height, int nb_particles) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    if (!init_shaders()) {
        return false;
    }

    m_camera = std::make_shared<CameraPerspective>(width, height, camera_pos, camera_at);

    m_floorRenderer = std::make_unique<FloorRenderer>(0.0f, 20.0f);
    m_floorRenderer->init(m_floorShader.get());
    
    m_particleRenderer = std::make_unique<ParticleRenderer>();
    m_particleRenderer->init(m_particleShader.get(), nb_particles);

    return true;
}

bool Renderer::init_shaders() {
    const std::string directory = SHADERS_DIR;

    // Particles
    bool particleShaderSuccess = true;
    m_particleShader = std::make_unique<ShaderProgram>();
    particleShaderSuccess &= 
        m_particleShader->addShaderFromSource(GL_VERTEX_SHADER, directory + "particleShader.vert");
    particleShaderSuccess &= 
        m_particleShader->addShaderFromSource(GL_FRAGMENT_SHADER, directory + "particleShader.frag");
    particleShaderSuccess &= m_particleShader->link();
    if (!particleShaderSuccess) {
        std::cerr << "ERROR: Failed to load particle shader!" << std::endl;
        return false;
    }

    // Floor
    bool floorShaderSuccess = true;
    m_floorShader = std::make_unique<ShaderProgram>();
    floorShaderSuccess &= 
        m_floorShader->addShaderFromSource(GL_VERTEX_SHADER, directory + "floorShader.vert");
    floorShaderSuccess &= 
        m_floorShader->addShaderFromSource(GL_FRAGMENT_SHADER, directory + "floorShader.frag");
    floorShaderSuccess &= m_floorShader->link();
    if (!floorShaderSuccess) {
        std::cerr << "ERROR: Failed to load floor shader!" << std::endl;
        return false;
    }

    return true;
}

void Renderer::render_scene() {
    m_floorShader->bind();
    m_floorShader->setMat4(m_floorShader->uniformLocation("uProjMatrix"), m_camera->get_proj_matrix());
    m_floorShader->setMat4(m_floorShader->uniformLocation("uViewMatrix"), m_camera->get_view_matrix());
    m_floorShader->setMat4(m_floorShader->uniformLocation("uModelMatrix"), glm::mat4(1.0f));
    m_floorRenderer->render();
}

void Renderer::render_particles() {
    m_particleShader->bind();
    m_particleShader->setMat4(m_particleShader->uniformLocation("uProjMatrix"), m_camera->get_proj_matrix());
    m_particleShader->setMat4(m_particleShader->uniformLocation("uViewMatrix"), m_camera->get_view_matrix());
    m_particleRenderer->render();
}

void Renderer::clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::update_particles(std::vector<vec3>& positions) {
    m_particleRenderer->update_particles(positions);
}

void Renderer::resize(int width, int height) {
    m_camera->set_aspect_ratio(width, height);
}

void Renderer::process_input(double dt) {
    m_camera->process_input(dt);
}

void Renderer::process_wheel(int y) {
    m_camera->process_wheel(y);
}

void Renderer::process_mouse(int x, int y) {
    m_camera->process_mouse(x, y);
}
