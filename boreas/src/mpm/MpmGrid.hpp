#pragma once

#include "MpmGridNode.hpp"
#include "libs/eigen.hpp"


struct MpmGrid {
    double spacing; // h
    vec3 origin; // world space origin of the grid
    int width;
    int height;
    int depth;
    std::vector<MpmGridNode> nodes;
    std::vector<MpmGridNode*> active_nodes;

    const vec3 box_min;
    const vec3 box_max;
    const double box_eps; // boundary collision margin
    const double box_mu; // friction of grid boundary

    MpmGrid(vec3 origin, double size_x, double size_y, double size_z, double spacing) :
        origin{origin}, 
        spacing{spacing}, 
        width{static_cast<int>(std::ceil(size_x / spacing)) + 1},
        height{static_cast<int>(std::ceil(size_y / spacing)) + 1},
        depth{static_cast<int>(std::ceil(size_z / spacing)) + 1},
        // TODO: bounding box
        box_min{origin},
        box_max{origin + vec3(width-1, height-1, depth-1) * spacing},
        box_eps{EPSILON * spacing},
        box_mu{0.5}
    {
        int nb_nodes = static_cast<int>(width * height * depth);
        nodes.resize(nb_nodes, MpmGridNode());
        active_nodes.reserve(nb_nodes);

        for (int z = 0; z < depth; ++z) {
        for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t index = get_node_id_from_local({x, y, z});
            nodes[index].index = index;
            nodes[index].local_pos = vec3i(x, y, z);
        }}}
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

    size_t get_node_id_from_local(vec3i pos) {
        return static_cast<size_t>(pos.x()) + 
            static_cast<size_t>(pos.y()) * width + 
            static_cast<size_t>(pos.z()) * width * height;
    }

    MpmGridNode* get_node_from_local(vec3i pos) {
        return get_node_from_local(pos.x(), pos.y(), pos.z());
    }

    MpmGridNode* get_node_from_local(int x, int y, int z) {
        if (x < 0 || x >= width ||
            y < 0 || y >= height ||
            z < 0 || z >= depth ) {
            return nullptr;
        }

        return &nodes[get_node_id_from_local({x, y, z})];
    }

    vec3i get_node_local_coords(const vec3& pos) const {
        vec3 relative_pos = pos - origin;
        return vec3i(
            static_cast<int>(std::floor(relative_pos.x() / spacing)),
            static_cast<int>(std::floor(relative_pos.y() / spacing)),
            static_cast<int>(std::floor(relative_pos.z() / spacing))
        );
    }

    vec3 get_node_world_coords(vec3i local_pos) const {
        return origin + (vec3(local_pos.array().cast<double>()) * spacing);
    }
};
