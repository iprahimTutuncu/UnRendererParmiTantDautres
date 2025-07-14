#pragma once

#include <GL/glew.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#include <SDL.h>
#include <SDL_opengl>
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#endif
