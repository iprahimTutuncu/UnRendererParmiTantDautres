#pragma once

#include "../state.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>

// Initialization & cleanup
SDL_AppResult deferred_gbuffer_init(AppState& state);

// Configuration
void deferred_gbuffer_update_particles(AppState& state);

// Rendering
void deferred_gbuffer_render(AppState& state, SDL_GPUCommandBuffer* cmdBuf);

// Internal creation helpers (optional)
void deferred_gbuffer_create_pipelines(AppState& state);
void deferred_gbuffer_create_mesh_pipeline(AppState& state);
void deferred_gbuffer_create_particles_pipeline(AppState& state);
void deferred_gbuffer_create_box_geometry(AppState& state);
void deferred_gbuffer_create_sphere_geometry(AppState& state);
