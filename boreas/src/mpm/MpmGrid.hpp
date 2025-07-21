#pragma once

#include "../libs/eigen.hpp"
#include "MpmGridNode.hpp"

struct MpmGrid {
    double spacing; // h
    int width;
    int height;
    int depth;
    vec3 origin; // world space origin of the grid
    std::vector<MpmGridNode> nodes;
    std::vector<std::uint32_t> active_nodes;

    MpmGrid() = default;

    MpmGrid(vec3 origin, double size_x, double size_y, double size_z, double spacing)
        : origin { origin }
        , spacing { spacing }
        , width { static_cast<int>(std::ceil(size_x / spacing)) + 1 }
        , height { static_cast<int>(std::ceil(size_y / spacing)) + 1 }
        , depth { static_cast<int>(std::ceil(size_z / spacing)) + 1 } {
        size_t nb_nodes = width * height * depth;
        nodes.resize(nb_nodes, MpmGridNode());
    }

    void reset_nodes() {
        nodes.assign(nodes.size(), MpmGridNode());
        active_nodes.clear();
    }

    inline vec3i get_local_pos_from_index(size_t index) const {
        return {
            static_cast<int>(index % width),
            static_cast<int>((index / width) % height),
            static_cast<int>(index / (width * height)),
        };
    }

    inline size_t get_node_id_from_local(vec3i pos) const {
        return static_cast<size_t>(pos.x()) + static_cast<size_t>(pos.y()) * width + static_cast<size_t>(pos.z()) * width * height;
    }

    inline size_t get_node_id_from_local(int x, int y, int z) const {
        return static_cast<size_t>(x + y * width + z * width * height);
    }


    MpmGridNode* get_node_from_local(int x, int y, int z) {
        if (x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= depth) [[unlikely]] {
            return nullptr;
        }

        return &nodes[get_node_id_from_local(x, y, z)];
    }

    MpmGridNode const* get_node_from_local(int x, int y, int z) const {
        if (x < 0 || x >= width || y < 0 || y >= height || z < 0 || z >= depth) [[unlikely]] {
            return nullptr;
        }

        return &nodes[get_node_id_from_local(x, y, z)];
    }

    vec3 get_node_world_coords(vec3i local_pos) const {
        return vec3(
            origin.x() + local_pos.x() * spacing,
            origin.y() + local_pos.y() * spacing,
            origin.z() + local_pos.z() * spacing);
    }
    vec3 get_node_world_coords(int x, int y, int z) const {
        return vec3(
            origin.x() + x * spacing,
            origin.y() + y * spacing,
            origin.z() + z * spacing);
    }

    vec3 get_node_world_coords_from_index(size_t index) const {
        return vec3(
            origin.x() + static_cast<double>(index % width) * spacing,
            origin.y() + static_cast<double>((index / width) % height) * spacing,
            origin.z() + static_cast<double>(index / (width * height)) * spacing);
    }
};
