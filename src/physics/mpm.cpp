#include "../vmath.h"

#include <cmath>
#include <limits>

// c'est une tentative de comprendre MPM en l'implémentant moi même depuis les
// référence donné par Ferat. ça va me permettre de mieux saisir quoi fait quoi et
// d'écrire le code de manière à pouvoir l'optimiser.

typedef float T;

namespace basis {

    /**
     * @brief The one-dimensional B-splines function
     *
     * @param x the input value
     * @return constexpr T the value of the B-splines function at x
     */
    static inline constexpr T N(T x) {
        if (std::abs(x) < static_cast<T>(1) - std::numeric_limits<T>::epsilon())
            //  |x|³/2 − x² + 2/3
            return std::pow(std::abs(x), static_cast<T>(3)) / 2 - std::pow(x, static_cast<T>(2)) + static_cast<T>(2) / 3;
        if (std::abs(x) < static_cast<T>(2) - std::numeric_limits<T>::epsilon())
            return -std::pow(std::abs(x), static_cast<T>(3)) / 6 + std::pow(x, static_cast<T>(2)) + static_cast<T>(4) / 3;

        return static_cast<T>(0);
    }

    /**
     * @brief The derivative of the one-dimensional B-splines function
     *
     * @param x the input value
     * @return constexpr T the value of the derivative of the B-splines function at x
     */
    static inline constexpr T dNdu(T x) {

        if (std::abs(x) > 2 - std::numeric_limits<T>::epsilon()) [[unlikely]]
            return static_cast<T>(0);

        if (x < static_cast<T>(-1) - std::numeric_limits<T>::epsilon())
            return (x * x) / 2 - (std::abs(x) + std::abs(x)) + static_cast<T>(2);
        if (x < static_cast<T>(0) - std::numeric_limits<T>::epsilon())
            return static_cast<T>(-3) * (x * x) / 2 + (std::abs(x) + std::abs(x));
        if (x < static_cast<T>(1) - std::numeric_limits<T>::epsilon())
            return static_cast<T>(3) * (x * x) / 2 - (std::abs(x) + std::abs(x));

        // 1 <= x < 2
        return (x * x) / -2 + (std::abs(x) + std::abs(x)) - static_cast<T>(2);
    }

    /**
     * @brief The second derivative of the one-dimensional B-splines function
     *
     * @param x the input value
     * @return constexpr T the value of the second derivative of the B-splines function at x
     */
    static inline constexpr T d2Ndu2(T x) {
        if (std::abs(x) < static_cast<T>(1) - std::numeric_limits<T>::epsilon())
            return 3 * std::abs(x) - 2;
        if (std::abs(x) < static_cast<T>(2) - std::numeric_limits<T>::epsilon())
            return static_cast<T>(2) - std::abs(x);
        return static_cast<T>(0);
    }

    /**
     * @brief The weight function for the grid node i with the particle p
     *
     * @param xp The displacement of the particle p in the x direction
     * @param yp The displacement of the particle p in the y direction
     * @param zp The displacement of the particle p in the z direction
     * @param xi The displacement of the grid node i in the x direction
     * @param yi The displacement of the grid node i in the y direction
     * @param zi The displacement of the grid node i in the z direction
     * @param one_over_h The inverse of the grid spacing
     * @return constexpr T the weight of the particle p on the grid node i
     */
    static inline constexpr T weight_i_p(vec3 p, vec3 i, T one_over_h) {
        return N((p.x - i.x) * one_over_h) * N((p.y - i.y) * one_over_h) * N((p.z - i.z) * one_over_h);
    }

    /**
     * @brief The gradient of the weight function for the grid node i with the particle p
     *
     * @param p The displacement of the particle p
     * @param i The displacement of the grid node i
     * @param one_over_h The inverse of the grid spacing
     * @return constexpr vec3 the gradient of the weight of the particle p on the grid node i
     */
    static inline constexpr vec3 grad_wdight_i_p(vec3 p, vec3 i, T one_over_h) {
        return {
            dNdu((p.x - i.x) * one_over_h) * N((p.y - i.y) * one_over_h) * N((p.z - i.z) * one_over_h) * one_over_h,
            N((p.x - i.x) * one_over_h) * dNdu((p.y - i.y) * one_over_h) * N((p.z - i.z) * one_over_h) * one_over_h,
            N((p.x - i.x) * one_over_h) * N((p.y - i.y) * one_over_h) * dNdu((p.z - i.z) * one_over_h) * one_over_h,
        };
    }

} // namespace basis

namespace elastic {



} // namespace elastic
