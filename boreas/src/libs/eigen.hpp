#pragma once

#include <Eigen/Dense>
#include <Eigen/SVD>

using vec3d = Eigen::Vector3d;
using vec3i = Eigen::Vector3i;

using mat3n = Eigen::Matrix<double, 3, Eigen::Dynamic, Eigen::ColMajor>;
using vec3 = Eigen::Vector3d;
using mat3 = Eigen::Matrix3d;

using quat = Eigen::Quaternion<double>;
