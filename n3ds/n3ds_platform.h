/*
 *  n3ds_platform.h - Nintendo 3DS glue for Nuvie: start-up, logging,
 *  gamepad -> keyboard/mouse mapping, and the two-screen set-up.
 *
 *  Copyright (C) 2026  The Nuvie Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef N3DS_PLATFORM_H
#define N3DS_PLATFORM_H

#ifdef __3DS__

#include <SDL3/SDL.h>

// Before anything else in main(): make the SD card folder, chdir into it,
// send stdout/stderr to log files.
void n3ds_platform_init();

// After SDL video is up and the game window exists: gamepad mapping, the
// pointer timer, the event filter and the second screen.
void n3ds_input_start(SDL_Window* game_window);

// Called once per frame from the game loop (main thread): handles a pending
// screen swap and periodic log lines.
void n3ds_frame();

// Pointer position (window coordinates) and button mask as last seen in
// mouse events, synthetic or touch.  Replaces SDL_GetMouseState on the 3DS,
// which only knows about the touch screen.
Uint32 n3ds_mouse_state(float* x, float* y);

// Writes a nuvie.cfg with 3DS defaults; returns false if it could not.
bool n3ds_write_default_config(const char* path);

// True on a New 3DS / New 2DS.
extern "C" bool n3ds_is_new_3ds(void);

#endif    // __3DS__
#endif    // N3DS_PLATFORM_H
