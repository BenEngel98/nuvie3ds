/*
 *  n3ds_platform.cpp - Nintendo 3DS glue for Nuvie.
 *
 *  Copyright (C) 2026  The Nuvie Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifdef __3DS__

#include "n3ds_platform.h"
#include "n3ds_kbd.h"

#include <3ds.h>
#include <malloc.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cerrno>

#include "nuvieDefs.h"
#undef clamp    // nuvieDefs.h macro vs std::clamp
#include "Screen.h"
#include "Game.h"
#include "ViewManager.h"
#include "TileManager.h"
#include "InventoryView.h"

/*
 *  Controls
 *
 *    Circle pad / D-pad   arrow keys (walk, pick a direction, move the cursor)
 *    C-stick              mouse pointer         L      left mouse button
 *    A                    Enter (do it)         R      right mouse button
 *    B                    Space (cancel/pass)   ZL     T (talk)
 *    X                    I (inventory)         ZR     U (use)
 *    Y                    L (look)              Start  Esc (game menu)
 *    Select               swap the game between the two screens
 */

extern "C" {
extern unsigned int __stacksize__;
extern u32          __ctru_heap_size;
extern u32          __ctru_linear_heap_size;

// SDL's 3DS backend busy-waits in places; give the other threads a chance.
int sched_yield(void) {
	svcSleepThread(1000000);
	return 0;
}
}

namespace {

Screen* g_screen = nullptr;

// ---- pointer driven by the C-stick ----
float  stick_x = 0.f, stick_y = 0.f;
float  cur_x = 200.f, cur_y = 120.f;
Uint32 btn_state = 0;
// What the game last saw (touch or synthetic), window coordinates.
float  seen_x = 200.f, seen_y = 120.f;
Uint32 seen_buttons = 0;

SDL_Window* game_window() {
	if (SDL_Window* w = n3ds_get_game_window()) {
		return w;
	}
	int          count   = 0;
	SDL_Window** windows = SDL_GetWindows(&count);
	SDL_Window*  w       = (windows && count > 0) ? windows[0] : nullptr;
	SDL_free(windows);
	return w;
}

void push_mouse_motion(Uint64 ts) {
	SDL_Window* w = game_window();
	SDL_Event   ev;
	SDL_zero(ev);
	ev.type             = SDL_EVENT_MOUSE_MOTION;
	ev.motion.timestamp = ts;
	ev.motion.windowID  = w ? SDL_GetWindowID(w) : 0;
	ev.motion.which     = 0;
	ev.motion.state     = btn_state;
	ev.motion.x         = cur_x;
	ev.motion.y         = cur_y;
	SDL_PushEvent(&ev);
}

void push_mouse_button(Uint8 button, bool down, Uint64 ts) {
	SDL_Window* w = game_window();
	SDL_Event   ev;
	SDL_zero(ev);
	ev.type             = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
	ev.button.timestamp = ts;
	ev.button.windowID  = w ? SDL_GetWindowID(w) : 0;
	ev.button.which     = 0;
	ev.button.button    = button;
	ev.button.down      = down;
	ev.button.clicks    = 1;
	ev.button.x         = cur_x;
	ev.button.y         = cur_y;
	const Uint32 mask   = SDL_BUTTON_MASK(button);
	btn_state           = down ? (btn_state | mask) : (btn_state & ~mask);
	SDL_PushEvent(&ev);
}

void push_key(SDL_Keycode key, bool down, Uint64 ts, bool repeat = false) {
	SDL_Window* w = game_window();
	SDL_Event   ev;
	SDL_zero(ev);
	ev.type          = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
	ev.key.timestamp = ts;
	ev.key.windowID  = w ? SDL_GetWindowID(w) : 0;
	ev.key.key       = key;
	ev.key.scancode  = SDL_GetScancodeFromKey(key, nullptr);
	ev.key.mod       = SDL_KMOD_NONE;
	ev.key.down      = down;
	ev.key.repeat    = repeat;
	SDL_PushEvent(&ev);
}

// ---- walking keys with auto-repeat (the game takes one step per key event) ----
constexpr int DIR_COUNT = 4;    // up, down, left, right
const SDL_Keycode dir_keys[DIR_COUNT] = {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
bool   dir_pad[DIR_COUNT]   = {false, false, false, false};    // D-pad
bool   dir_stick[DIR_COUNT] = {false, false, false, false};    // circle pad
bool   dir_sent[DIR_COUNT]  = {false, false, false, false};
Uint64 dir_next[DIR_COUNT]  = {0, 0, 0, 0};
constexpr Uint64 REPEAT_FIRST = 250;
constexpr Uint64 REPEAT_NEXT  = 120;

// Runs from the timer thread: pushes events only (thread safe).
Uint32 SDLCALL pointer_tick(void* userdata, SDL_TimerID id, Uint32 interval) {
	(void)userdata;
	(void)id;
	const Uint64 now = SDL_GetTicks();

	// Direction keys.
	for (int i = 0; i < DIR_COUNT; i++) {
		const bool want = dir_pad[i] || dir_stick[i];
		if (want && !dir_sent[i]) {
			push_key(dir_keys[i], true, SDL_GetTicksNS());
			dir_sent[i] = true;
			dir_next[i] = now + REPEAT_FIRST;
		} else if (want && now >= dir_next[i]) {
			push_key(dir_keys[i], true, SDL_GetTicksNS(), true);
			dir_next[i] = now + REPEAT_NEXT;
		} else if (!want && dir_sent[i]) {
			push_key(dir_keys[i], false, SDL_GetTicksNS());
			dir_sent[i] = false;
		}
	}

	// Pointer.
	const float dead = 0.18f;
	float       ax   = stick_x;
	float       ay   = stick_y;
	const float mag  = std::sqrt(ax * ax + ay * ay);
	if (mag < dead) {
		return interval;
	}
	const float speed = (mag - dead) / (1.f - dead);
	const float px    = (speed * speed * 9.f + 1.f) * (ax / mag);
	const float py    = (speed * speed * 9.f + 1.f) * (ay / mag);
	int         ww    = 400;
	int         wh    = 240;
	if (SDL_Window* w = game_window()) {
		SDL_GetWindowSize(w, &ww, &wh);
	}
	cur_x = std::min(std::max(cur_x + px, 0.f), static_cast<float>(ww - 1));
	cur_y = std::min(std::max(cur_y + py, 0.f), static_cast<float>(wh - 1));
	push_mouse_motion(SDL_GetTicksNS());
	return interval;
}

SDL_Keycode button_key(SDL_GamepadButton b) {
	switch (b) {
	case SDL_GAMEPAD_BUTTON_EAST:    // A (right-hand button)
		return SDLK_RETURN;
	case SDL_GAMEPAD_BUTTON_SOUTH:    // B (bottom)
		return SDLK_SPACE;
	case SDL_GAMEPAD_BUTTON_NORTH: {    // X (top): the Avatar's inventory, or back to the party list
		Game*        game = Game::get_game();
		ViewManager* vm   = game ? game->get_view_manager() : nullptr;
		if (vm && vm->get_current_view() == static_cast<View*>(vm->get_inventory_view())) {
			return SDLK_SLASH;    // party_view
		}
		return SDLK_F1;    // inventory 1
	}
	case SDL_GAMEPAD_BUTTON_WEST:    // Y (left)
		return SDLK_L;
	case SDL_GAMEPAD_BUTTON_START:
		return SDLK_ESCAPE;
	default:
		return SDLK_UNKNOWN;
	}
}

bool SDLCALL gamepad_watch(void* userdata, SDL_Event* event) {
	(void)userdata;
	if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
		const float v = event->gaxis.value / 32767.f;
		switch (event->gaxis.axis) {
		case SDL_GAMEPAD_AXIS_RIGHTX:
			stick_x = v;
			break;
		case SDL_GAMEPAD_AXIS_RIGHTY:
			stick_y = v;
			break;
		case SDL_GAMEPAD_AXIS_LEFTX:
			dir_stick[2] = v < -0.4f;
			dir_stick[3] = v > 0.4f;
			break;
		case SDL_GAMEPAD_AXIS_LEFTY:
			dir_stick[0] = v < -0.4f;
			dir_stick[1] = v > 0.4f;
			break;
		case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
		case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: {
			// ZL / ZR arrive as triggers: treat as digital keys.
			static bool held[2] = {false, false};
			const int   idx     = event->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? 0 : 1;
			const bool  down    = event->gaxis.value > 16000;
			if (down != held[idx]) {
				held[idx] = down;
				push_key(idx == 0 ? SDLK_T : SDLK_U, down, event->gaxis.timestamp);
			}
			break;
		}
		default:
			break;
		}
		return true;
	}
	if (event->type != SDL_EVENT_GAMEPAD_BUTTON_DOWN && event->type != SDL_EVENT_GAMEPAD_BUTTON_UP) {
		return true;
	}
	const SDL_GamepadButton gb   = static_cast<SDL_GamepadButton>(event->gbutton.button);
	const bool              down = event->gbutton.down;
	const Uint64            ts   = event->gbutton.timestamp;
	switch (gb) {
	case SDL_GAMEPAD_BUTTON_DPAD_UP:
		dir_pad[0] = down;
		return true;
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
		dir_pad[1] = down;
		return true;
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
		dir_pad[2] = down;
		return true;
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
		dir_pad[3] = down;
		return true;
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
		push_mouse_button(SDL_BUTTON_LEFT, down, ts);
		return true;
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		push_mouse_button(SDL_BUTTON_RIGHT, down, ts);
		return true;
	case SDL_GAMEPAD_BUTTON_BACK:    // Select
		if (down) {
			n3ds_request_screen_swap();
		}
		return true;
	default:
		break;
	}
	const SDL_Keycode key = button_key(gb);
	if (key != SDLK_UNKNOWN) {
		push_key(key, down, ts);
	}
	return true;
}

// Drops events meant for the keyboard window; remembers where the pointer is.
bool SDLCALL event_filter(void* userdata, SDL_Event* event) {
	(void)userdata;
	if (n3ds_kbd_handle_event(event)) {
		return false;
	}
	switch (event->type) {
	case SDL_EVENT_MOUSE_MOTION:
		seen_x = event->motion.x;
		seen_y = event->motion.y;
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		seen_x = event->button.x;
		seen_y = event->button.y;
		const Uint32 mask = SDL_BUTTON_MASK(event->button.button);
		seen_buttons      = event->button.down ? (seen_buttons | mask) : (seen_buttons & ~mask);
		break;
	}
	default:
		break;
	}
	return true;
}

Uint64 last_debug = 0;

}    // namespace

// ---------------------------------------------------------------------------

void n3ds_platform_init() {
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/nuvie", 0777);
	mkdir("sdmc:/3ds/nuvie/u6_save", 0777);
	chdir("sdmc:/3ds/nuvie");
	freopen("sdmc:/3ds/nuvie/nuvie_log.txt", "w", stdout);
	freopen("sdmc:/3ds/nuvie/nuvie_err.txt", "w", stderr);
	setvbuf(stdout, nullptr, _IONBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);
	bool is_new = n3ds_is_new_3ds();
	std::printf("Nuvie 3DS: %s 3DS, heap %u KB, linear %u KB, stack %u KB\n", is_new ? "New" : "original",
			__ctru_heap_size / 1024, __ctru_linear_heap_size / 1024, __stacksize__ / 1024);
}

void n3ds_input_start(SDL_Window* win) {
	n3ds_set_game_window(win);
	SDL_InitSubSystem(SDL_INIT_GAMEPAD);
	int            count = 0;
	SDL_JoystickID* ids  = SDL_GetGamepads(&count);
	for (int i = 0; i < count; i++) {
		SDL_OpenGamepad(ids[i]);
	}
	SDL_free(ids);
	SDL_AddEventWatch(gamepad_watch, nullptr);
	SDL_SetEventFilter(event_filter, nullptr);
	SDL_AddTimer(16, pointer_tick, nullptr);
	int w = 400, h = 240;
	SDL_GetWindowSize(win, &w, &h);
	cur_x = w / 2.f;
	cur_y = h / 2.f;
	n3ds_kbd_create();
	std::printf("Nuvie 3DS: input ready, %d gamepad(s), window %dx%d\n", count, w, h);
}

Uint32 n3ds_mouse_state(float* x, float* y) {
	if (x) {
		*x = seen_x;
	}
	if (y) {
		*y = seen_y;
	}
	return seen_buttons | btn_state;
}

void n3ds_apply_screen() {
	Screen* screen = Screen::get_screen();
	if (screen == nullptr) {
		return;
	}
	n3ds_kbd_destroy();
	screen->n3ds_move_window(n3ds_game_on_bottom());
	n3ds_set_game_window(screen->get_sdl_window());
	int w = 400, h = 240;
	SDL_GetWindowSize(screen->get_sdl_window(), &w, &h);
	cur_x = w / 2.f;
	cur_y = h / 2.f;
	seen_x = cur_x;
	seen_y = cur_y;
	n3ds_kbd_create();
	std::printf("Nuvie 3DS: game now on the %s screen\n", n3ds_game_on_bottom() ? "bottom" : "top");
}

void n3ds_frame() {
	if (n3ds_take_screen_swap_request()) {
		n3ds_apply_screen();
	}
	const Uint64 now = SDL_GetTicks();
	if (now - last_debug > 30000) {
		last_debug = now;
		struct mallinfo mi = mallinfo();
		std::printf("Nuvie 3DS: heap in use %u KB, free %u KB\n", mi.uordblks / 1024, mi.fordblks / 1024);
	}
}

void n3ds_debug_tick() {
	n3ds_frame();
}

bool n3ds_write_default_config(const char* path) {
	FILE* f = std::fopen(path, "w");
	if (f == nullptr) {
		return false;
	}
	std::fputs(
			"<config>\n"
			" <loadgame>ultima6</loadgame>\n"
			" <datadir>romfs:/data</datadir>\n"
			" <keys>(default)</keys>\n"
			" <input>\n"
			"  <enable_doubleclick>yes</enable_doubleclick>\n"
			"  <doubleclick_opens_containers>yes</doubleclick_opens_containers>\n"
			"  <party_view_targeting>no</party_view_targeting>\n"
			"  <new_command_bar>no</new_command_bar>\n"
			"  <enabled_dragging>yes</enabled_dragging>\n"
			"  <look_on_left_click>yes</look_on_left_click>\n"
			"  <walk_with_left_button>yes</walk_with_left_button>\n"
			"  <direction_selects_target>yes</direction_selects_target>\n"
			"  <interface>normal</interface>\n"
			" </input>\n"
			" <general>\n"
			"  <lighting>original</lighting>\n"
			"  <dither_mode>none</dither_mode>\n"
			"  <enable_cursors>yes</enable_cursors>\n"
			"  <converse_gump>default</converse_gump>\n"
			"  <use_text_gumps>no</use_text_gumps>\n"
			"  <party_formation>standard</party_formation>\n"
			"  <show_console>yes</show_console>\n"
			" </general>\n"
			" <cheats>\n"
			"  <enabled>no</enabled>\n"
			" </cheats>\n"
			" <video>\n"
			"  <game_style>original</game_style>\n"
			"  <scale_method>point</scale_method>\n"
			"  <scale_factor>1</scale_factor>\n"
			"  <fullscreen>no</fullscreen>\n"
			"  <non_square_pixels>no</non_square_pixels>\n"
			"  <screen_width>320</screen_width>\n"
			"  <screen_height>200</screen_height>\n"
			"  <game_width>320</game_width>\n"
			"  <game_height>200</game_height>\n"
			"  <game_position>center</game_position>\n"
			" </video>\n"
			" <audio>\n"
			"  <enabled>yes</enabled>\n"
			"  <enable_music>yes</enable_music>\n"
			"  <enable_sfx>yes</enable_sfx>\n"
			"  <music_volume>100</music_volume>\n"
			"  <sfx_volume>255</sfx_volume>\n"
			"  <combat_changes_music>yes</combat_changes_music>\n"
			"  <vehicles_change_music>yes</vehicles_change_music>\n"
			"  <conversations_stop_music>no</conversations_stop_music>\n"
			"  <stop_music_on_group_change>yes</stop_music_on_group_change>\n"
			" </audio>\n"
			" <ultima6>\n"
			"  <language>en</language>\n"
			"  <gamedir>./ultima6</gamedir>\n"
			"  <townsdir>./fmtowns_u6</townsdir>\n"
			"  <sounddir>./u6_sounds</sounddir>\n"
			"  <savedir>./u6_save</savedir>\n"
			"  <skip_intro>no</skip_intro>\n"
			"  <show_eggs>no</show_eggs>\n"
			"  <roof_mode>no</roof_mode>\n"
			"  <use_new_dolls>no</use_new_dolls>\n"
			"  <cb_position>default</cb_position>\n"
			"  <show_orig_style_cb>default</show_orig_style_cb>\n"
			"  <map_tile_lighting>yes</map_tile_lighting>\n"
			"  <custom_actor_tiles>default</custom_actor_tiles>\n"
			"  <converse_solid_bg>no</converse_solid_bg>\n"
			"  <music>native</music>\n"
			"  <sfx>native</sfx>\n"
			"  <enable_speech>yes</enable_speech>\n"
			"  <game_specific_keys>(default)</game_specific_keys>\n"
			"  <patch_keys>./patchkeys.txt</patch_keys>\n"
			" </ultima6>\n"
			" <martian>\n"
			"  <language>en</language>\n"
			"  <gamedir>./martian</gamedir>\n"
			"  <savedir>./martian_save</savedir>\n"
			"  <skip_intro>no</skip_intro>\n"
			"  <game_specific_keys>(default)</game_specific_keys>\n"
			"  <patch_keys>./patchkeys.txt</patch_keys>\n"
			" </martian>\n"
			" <savage>\n"
			"  <language>en</language>\n"
			"  <gamedir>./savage</gamedir>\n"
			"  <savedir>./savage_save</savedir>\n"
			"  <skip_intro>no</skip_intro>\n"
			"  <game_specific_keys>(default)</game_specific_keys>\n"
			"  <patch_keys>./patchkeys.txt</patch_keys>\n"
			" </savage>\n"
			"</config>\n",
			f);
	std::fclose(f);
	return true;
}

#endif    // __3DS__
