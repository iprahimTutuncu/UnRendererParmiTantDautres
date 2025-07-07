#pragma once

#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/SVD>

using vec3d = Eigen::Vector3d;
using vec3i = Eigen::Vector3i;

using mat3n = Eigen::Matrix<double, 3, Eigen::Dynamic, Eigen::ColMajor>;
using vec3 = Eigen::Vector3d;
using mat3 = Eigen::Matrix3d;

using vec4 = Eigen::Vector4f;
using mat4 = Eigen::Vector4f;

using quat = Eigen::Quaternion<double>;
