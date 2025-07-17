#pragma once

#include "../eigen.hpp"
#include "MpmGridNode.hpp"

struct MpmGrid {
    double spacing; // h
    vec3 origin; // world space origin of the grid
    size_t width;
    size_t height;
    size_t depth;
    std::vector<MpmGridNode> nodes;
    std::vector<MpmGridNode*> active_nodes;

    MpmGrid() {
        spacing = 0.0;
        width = 0;
        height = 0;
        depth = 0;
    }

    MpmGrid(vec3 origin, double size_x, double size_y, double size_z, double spacing)
        : origin { origin }
        , spacing { spacing }
        , width { static_cast<size_t>(std::ceil(size_x / spacing)) + 1 }
        , height { static_cast<size_t>(std::ceil(size_y / spacing)) + 1 }
        , depth { static_cast<size_t>(std::ceil(size_z / spacing)) + 1 } {
        size_t nb_nodes = width * height * depth;
        nodes.resize(nb_nodes, MpmGridNode());
        active_nodes.reserve(nb_nodes);

        for (size_t z = 0; z < depth; ++z) {
            for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x) {
                    size_t index = get_node_id_from_local(x, y, z);
                    nodes[index].index = index;
                    nodes[index].local_pos = vec3i(x, y, z);
                }
            }
        }
    }

    void reset_nodes() {
        for (MpmGridNode& node : nodes) {
            node.velocity = vec3::Zero();
            node.velocity_star = vec3::Zero();
            node.momentum = vec3::Zero();
            node.mass = 0.0;
            node.force = vec3::Zero();
            node.is_active = false;
        }

        active_nodes.clear();
    }

    size_t get_node_id_from_local(vec3i pos) const {
        return static_cast<size_t>(pos.x()) + static_cast<size_t>(pos.y()) * width + static_cast<size_t>(pos.z()) * width * height;
    }

    size_t get_node_id_from_local(size_t x, size_t y, size_t z) const {
        return static_cast<size_t>(x) + static_cast<size_t>(y) * width + static_cast<size_t>(z) * width * height;
    }

    MpmGridNode& get_node_from_local(vec3i pos) {
        return get_node_from_local(pos.x(), pos.y(), pos.z());
    }

    MpmGridNode const& get_node_from_local(vec3i pos) const {
        return get_node_from_local(pos.x(), pos.y(), pos.z());
    }

    inline MpmGridNode& get_node_from_local(int x, int y, int z) {
        return nodes[get_node_id_from_local(x, y, z)];
    }

    inline MpmGridNode const& get_node_from_local(int x, int y, int z) const {
        return nodes[get_node_id_from_local(x, y, z)];
    }

    const vec3i get_node_local_coords(const vec3& pos) const {
        vec3 relative_pos = pos - origin;
        return vec3i(
            static_cast<int>(std::floor(relative_pos.x() / spacing)),
            static_cast<int>(std::floor(relative_pos.y() / spacing)),
            static_cast<int>(std::floor(relative_pos.z() / spacing)));
    }

    const vec3 get_node_world_coords(vec3i local_pos) const {
        return origin + (vec3(local_pos.array().cast<double>()) * spacing);
    }
};
