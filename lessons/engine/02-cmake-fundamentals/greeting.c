/*
 * greeting.c — Implementation of the greeting module.
 *
 * This is a separate translation unit from main.c.  The compiler turns each
 * .c file into its own object file (.o or .obj), and the linker combines
 * them.  If this file is missing from add_executable(), the linker will
 * report "undefined reference to get_greeting" — a classic linker error.
 *
 * SPDX-License-Identifier: Zlib
 */

#include "greeting.h"

#include <SDL3/SDL.h>

const char *get_greeting(void)
{
    /* SDL_GetVersion() returns the runtime SDL version as an integer.
     * We use it here to prove that SDL was linked successfully —
     * if target_link_libraries were missing, this call would produce
     * an "undefined reference to SDL_GetVersion" linker error. */
    static char buffer[128];
    int version = SDL_GetVersion();
    int major = version / 1000000;
    int minor = (version / 1000) % 1000;
    int patch = version % 1000;

    SDL_snprintf(buffer, sizeof(buffer),
                 "Hello from a linked module! (SDL %d.%d.%d)",
                 major, minor, patch);
    return buffer;
}

const char *get_lesson_topic(void)
{
    return "CMake Fundamentals: Targets, Properties, and Linking";
}
