/*
 *  n3ds_kbd.h - On-screen keyboard on the Nintendo 3DS bottom screen.
 *
 *  Copyright (C) 2026  The Nuvie Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef N3DS_KBD_H
#define N3DS_KBD_H

#ifdef __3DS__

#	include <SDL3/SDL.h>

// Create / destroy the window on the screen the game is not using: the touch
// keyboard (bottom screen) or a mirror of the game (top screen).
void n3ds_kbd_create();
void n3ds_kbd_destroy();
bool n3ds_kbd_exists();

// Redraw if needed and present; in mirror mode copies the game's latest
// frame. Call from the main thread after every game present
// (Image_window::show() does).
void n3ds_kbd_present();

// Event filter helper: returns true if the event was aimed at the keyboard
// window and has been consumed (the caller should drop it).
bool n3ds_kbd_handle_event(SDL_Event* event);

// Nuvie reads typed characters from key events, so no text-input events are
// needed; kept for API compatibility.
void n3ds_set_text_wanted(bool on);
bool n3ds_text_wanted();

// The surface the top screen should mirror while the game is on the bottom
// screen (Nuvie's software frame, before it goes to the renderer).
void n3ds_set_mirror_surface(SDL_Surface* s);

// The "touch view": the bottom screen shows the game (shrunk) so a target
// can be tapped; it opens when a command is tapped on the keyboard or MAP
// is pressed, and the platform layer closes it when the command is over.
bool   n3ds_kbd_touch_view();
Uint64 n3ds_kbd_touch_view_since();
void   n3ds_kbd_set_touch_view(bool on);

// Forward a touch on the touch view to the game window as a mouse click at
// game-window coordinates: phase 0 = down, 1 = move, 2 = up (n3ds_platform).
void n3ds_forward_touch(float x, float y, int phase);

// The window that game input should be delivered to (set by Image_window).
void        n3ds_set_game_window(SDL_Window* w);
SDL_Window* n3ds_get_game_window();

// Which screen the game is on: false = top (default), true = bottom.
bool n3ds_game_on_bottom();
void n3ds_request_screen_swap();
bool n3ds_take_screen_swap_request();

// Re-create the game window on the screen chosen by n3ds_game_on_bottom()
// and the keyboard / mirror on the other one (defined in n3ds_platform.cpp).
void n3ds_apply_screen();

// Show up to four lines of text on the top screen and wait for a button
// (or a few seconds). Used for fatal start-up messages; needs SDL video.
void n3ds_message_screen(const char* l1, const char* l2, const char* l3, const char* l4);
void n3ds_debug_tick();

#endif    // __3DS__
#endif    // N3DS_KBD_H
