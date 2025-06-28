#pragma once

#include <Eigen/Dense>
#include <Eigen/SVD>

const float EPSILON = 1E-12;

using vec3d = Eigen::Vector3d;
using vec3i = Eigen::Vector3i;

using mat3n = Eigen::Matrix<double, 3, Eigen::Dynamic, Eigen::ColMajor>;
using vec3 = Eigen::Vector3d;
using mat3 = Eigen::Matrix3d;

using vec4 = Eigen::Vector4f;
using mat4 = Eigen::Vector4f;
