// PUBLIC API HEADER
#pragma once

/**
 * Return values for optional main callbacks.
 *
 * Returning MPM_APP_SUCCESS or MPM_APP_FAILURE from MPM_AppInit,
 * MPM_AppEvent, or MPM_AppIterate will terminate the program and report
 * success/failure to the operating system. What that means is
 * platform-dependent. On Unix, for example, on success, the process error
 * code will be zero, and on failure it will be 1. This interface doesn't
 * allow you to return specific exit codes, just whether there was an error
 * generally or not.
 */
typedef enum MPM_AppResult {
    MPM_APP_CONTINUE, /**< Value that requests that the app continue from the main callbacks. */
    MPM_APP_SUCCESS, /**< Value that requests termination with success from the main callbacks. */
    MPM_APP_FAILURE /**< Value that requests termination with error from the main callbacks. */
} MPM_AppResult;

struct Material {
    float critical_compression; // theta_c
    float critical_stretch; // theta_s
    float hardening_coefficient; // xi

    float initial_youngs_modulus; // E_0
    float poisson_ratio; // nu
    float density; // rho_0
    float volume; // V_0

    // calculated
    float lambda_0;
    float mu_0;
};

struct MpmParams {
    float deltaT;
    float radius; // h

    float compression;// theta_c
    float stretch;// theta_s
    float hardening;// xi
    float young;// E_0
    float poisson;// nu
    float alpha;
    float density; // ρ

    int num_particles;

    int grid_size;
    float gravity[3];

    float get_lambda() const {
        return (poisson * young) / ((1 + poisson) * (1 - 2 * poisson));
    }

    float get_mu() const {
        return young / (2 * (1 + poisson));
    }

    float get_mass() const {
        return density * radius * radius * radius / num_particles;
    }
};

/**
 * @brief Use the same interface API as SDL3 Callback' api
 *
 * @param appstate A place where the app can store a pointer to its state.
 * @param argc the standard ANSI C main's argc; number of command line arguments.
 * @param argv the standard ANSI C main's argv; array of command line arguments.
 * @return MPM_AppResult MPM_APP_CONTINUE to continue, MPM_APP_SUCCESS to terminate with success, or MPM_APP_FAILURE to terminate with error.
 */
MPM_AppResult mpm_init(void** appstate, MpmParams const& params);
MPM_AppResult mpm_iterate(void* appstate);
// MPM_AppResult mpm_event(void* appstate, SDL_Event& event);
void mpm_quit(void* appstate);
