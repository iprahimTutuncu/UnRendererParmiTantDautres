#pragma once

#include <cmath>
#include <cstdlib>

const float EPSILON = 1E-12;

inline double get_random(double min, double max, unsigned int* seed) {
    return min + (rand_r(seed) / (double)RAND_MAX) * (max - min);
}

// https://csmbrannon.net/2013/02/14/illustration-of-polar-decomposition/
template<typename T>
inline T fast_polar_decompose_R(const T& A, const int k) {
    double alpha = (A.transpose() * A).trace();
    T X = A / std::sqrt(alpha);

    for (int i = 0; i < k; ++i) {
        X = 0.5 * (X + X.inverse().transpose());
    }

    return X;
}

struct SolverCG {
    template<class Vec, class CalculateA>
    static void solve(CalculateA A, Vec& x, const Vec& b, int max_iterations, double tolerance) {
        Vec r = b - A(x);
        Vec p = r;

        double rs_old = r.squaredNorm();
        const double b_norm = b.norm();
        const double b_sn = b_norm < EPSILON ? 1.0 : b_norm * b_norm;
        const double t_sq = tolerance * tolerance;

        for (int k = 0; k < max_iterations; ++k) {
            if (rs_old / b_sn < t_sq) {
                break;
            }

            Vec Ap = A(p);
            double alpha = rs_old / p.cwiseProduct(Ap).sum();

            x += alpha * p;
            r -= alpha * Ap;

            double rs_new = r.squaredNorm();

            if (rs_new / b_sn < t_sq) {
                break;
            }

            double beta = rs_new / rs_old;
            p = r + beta * p;
            rs_old = rs_new;
        }
    }
};

struct SolverCR {
    template<class Vec, class CalculateA>
    static void solve(CalculateA A, Vec& x, const Vec& b, int max_iterations, double tolerance) {
        Vec r = b - A(x);
        Vec p = r;
        Vec Ap = A(p);

        double rAr_old = r.cwiseProduct(Ap).sum();
        const double b_norm = b.norm();
        const double b_sn = b_norm < EPSILON ? 1.0 : b_norm * b_norm;
        const double t_sq = tolerance * tolerance;

        for (int k = 0; k < max_iterations ; ++k) {
            if (r.squaredNorm() / b_sn < t_sq) {
                break;
            }

            double alpha = rAr_old / Ap.squaredNorm();

            x += alpha * p;
            r -= alpha * Ap;

            if (r.squaredNorm() / b_sn < t_sq) {
                break;
            }

            Vec Ar = A(r);
            double rAr_new = (r.cwiseProduct(Ar)).sum();
            double beta = rAr_new / rAr_old;

            p  = r  + beta * p;
            Ap = Ar + beta * Ap;
            rAr_old = rAr_new;
        }
    }
};

struct SolverPCR {
    template<class Vec, class CalculateA>
    static void solve(CalculateA A, Vec& x, const Vec& b, const Vec& M_inv, int max_iterations, double tolerance) {
        Vec r = b - A(x);
        Vec z = r.cwiseProduct(M_inv);
        Vec p = z;

        double rz_old = r.cwiseProduct(z).sum();
        const double b_norm = b.norm();
        const double b_sn = b_norm < EPSILON ? 1.0 : b_norm * b_norm;
        const double t_sq = tolerance * tolerance;

        for (int k = 0; k < max_iterations ; ++k) {
            if (r.squaredNorm() / b_sn < t_sq) {
                break;
            }

            Vec Ap = A(p);
            double alpha = rz_old / Ap.squaredNorm();

            x += alpha * p;
            r -= alpha * Ap;

            if (r.squaredNorm() / b_sn < t_sq) {
                break;
            }

            z = r.cwiseProduct(M_inv);

            double rz_new = (r.cwiseProduct(z)).sum();
            double beta = rz_new / rz_old;

            p = z + beta * p;
            rz_old = rz_new;
        }
    }
};
