#pragma once

#include "../state.h"
#include "graphics.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>

void init_sampler_presets(AppState& state);

/*

        Petite reflexion, createRenderTarget(..) est seulement appelé dans graphics_init() si vers la fin du projet
        Il y a pas de renderer qui appelle cette méthode autant en faire une lambda dans dans graphics_init(). J'ai
        envie de dire que graphics_init() devrait être le seul qui fabrique les textures pour tous les renderer
        puisque que j'estime pas qu'il va pas avoir plus qu 6-8 render target.

        à méditer. Texture.h est assez vague dans sa raison d'être en ce moment dans ma tete. Je laisse ca de même
        pour l'instant.
*/

SDL_AppResult createRenderTarget(AppState& state, TextureIndex index, int width, int height, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage);
SDL_AppResult createSolidColorTextureRGBA8(AppState& state, TextureIndex index, std::uint32_t width, std::uint32_t height, const float r, const float g, const float b, const float a);
