#pragma once
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>

namespace GTS
{
    struct Box
    {
        glm::vec3 min;
        glm::vec3 max;
    };

    struct Rect 
    {
        float centerX;
        float centerY;
        float halfWidth;
        float halfHeight;
    };

    struct Particle 
    {
        glm::vec4 position;
        glm::vec4 color;
    };

    struct Particles 
    {
        std::vector<Particle> data;
    };

    struct Vertex
    {
        float position[3];
        float _pad1;      

        float normal[3];  
        float _pad2;      

        float texCoord[2];
        float _pad3[2];   
    };

}