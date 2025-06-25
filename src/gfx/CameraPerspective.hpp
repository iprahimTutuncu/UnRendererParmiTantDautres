#pragma once

#include <glm/glm.hpp>

const float DEFAULT_YAW			=  -90.0f;
const float DEFAULT_PITCH		=  0.0f;
const float DEFAULT_SPEED		=  2.5f;
const float DEFAULT_SENSITIVITY		=  0.1f;
const float DEFAULT_ZOOM_MAX		=  120.0f;
const float DEFAULT_ZOOM_MIN		=  1.0f;
const float DEFAULT_ZOOM		=  45.0f;
const float DEFAULT_NEAR		=  0.1f;
const float DEFAULT_FAR                 =  100.0f;
const float DEFAULT_FOV                 =  glm::radians(90.0f);
const glm::vec3 DEFAULT_WORLD_UP	=  glm::vec3(0.0f, 1.0f, 0.0f);


class CameraPerspective {
    public:
        CameraPerspective() = delete;
        CameraPerspective(int width, int height, glm::vec3 position, glm::vec3 at);
        CameraPerspective(int width, int height, glm::vec3 position, glm::vec3 at, float yaw, float pitch);

        ~CameraPerspective() = default;

        glm::mat4 get_view_matrix();
        glm::mat4 get_proj_matrix();
        glm::vec3 get_position();
        void process_input(double delta_time);
        void process_mouse(float x, float y);
        void process_wheel(float y);

        const float get_aspect_ratio() const;
        void set_aspect_ratio(int width, int height);

private:
        void update();

    private:
        float _aspect_ratio = 1.0;
        bool _first_mouse = true;

        float movement_speed = DEFAULT_SPEED;
        float mouse_sensitivity = DEFAULT_SENSITIVITY;
        float zoom = DEFAULT_ZOOM;
        float yaw = DEFAULT_YAW;
        float pitch = DEFAULT_PITCH;

        float _near = DEFAULT_NEAR;
        float _far = DEFAULT_FAR;
        float _fov = DEFAULT_FOV;

        glm::vec3 world_up = DEFAULT_WORLD_UP;
        glm::vec3 position;
        glm::vec3 front;
        glm::vec3 right;
        glm::vec3 up;

};
