/*
 *  n3ds_kbd.cc - The Nintendo 3DS "other" screen: a touch keyboard on the
 *  bottom screen, or a mirror of the game on the top screen after Select has
 *  moved the game down to the touch screen.
 *
 *  Copyright (C) 2026  The Nuvie Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifdef __3DS__

#	include "n3ds_kbd.h"
#	include <SDL3/SDL.h>

#	include <cctype>
#	include <cstring>
#	include <string>
#	include <vector>

namespace {

	// ---------------------------------------------------------------------
	// A tiny 5x7 bitmap font (original, hand drawn). Rows top to bottom,
	// '#' = pixel on.
	// ---------------------------------------------------------------------
	struct Glyph {
		char        ch;
		const char* rows[7];
	};

	const Glyph glyphs[] = {
			{'A', {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
			{'B', {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}},
			{'C', {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}},
			{'D', {"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."}},
			{'E', {"#####", "#....", "#....", "####.", "#....", "#....", "#####"}},
			{'F', {"#####", "#....", "#....", "####.", "#....", "#....", "#...."}},
			{'G', {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"}},
			{'H', {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}},
			{'I', {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."}},
			{'J', {"..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."}},
			{'K', {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}},
			{'L', {"#....", "#....", "#....", "#....", "#....", "#....", "#####"}},
			{'M', {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}},
			{'N', {"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"}},
			{'O', {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
			{'P', {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}},
			{'Q', {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"}},
			{'R', {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}},
			{'S', {".####", "#....", "#....", ".###.", "....#", "....#", "####."}},
			{'T', {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}},
			{'U', {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}},
			{'V', {"#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."}},
			{'W', {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"}},
			{'X', {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}},
			{'Y', {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."}},
			{'Z', {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"}},
			{'0', {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}},
			{'1', {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}},
			{'2', {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}},
			{'3', {"#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."}},
			{'4', {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}},
			{'5', {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}},
			{'6', {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}},
			{'7', {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."}},
			{'8', {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}},
			{'9', {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."}},
			{'.', {".....", ".....", ".....", ".....", ".....", ".##..", ".##.."}},
			{',', {".....", ".....", ".....", ".....", ".##..", "..#..", ".#..."}},
			{'-', {".....", ".....", ".....", "#####", ".....", ".....", "....."}},
			{'\'', {".##..", "..#..", ".#...", ".....", ".....", ".....", "....."}},
			{'?', {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."}},
			{'!', {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."}},
			{'<', {"...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."}},
			{'^', {"..#..", ".#.#.", "#...#", ".....", ".....", ".....", "....."}},
			{'_', {".....", ".....", ".....", ".....", ".....", ".....", "#####"}},
	};

	const Glyph* find_glyph(char c) {
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		for (const Glyph& g : glyphs) {
			if (g.ch == c) {
				return &g;
			}
		}
		return nullptr;
	}

	// ---------------------------------------------------------------------
	// Keyboard layout
	// ---------------------------------------------------------------------
	struct Key {
		int         x, y, w, h;
		const char* label;
		SDL_Keycode key;
		char        text;    // character typed (0 = none)
		bool        is_shift;
		int         special;    // 0 = plain key, 1 = game command, 2 = MAP (touch view)
	};

	constexpr int KW  = 28;    // key width
	constexpr int KH  = 30;    // key height
	constexpr int GAP = 4;
	constexpr int X0  = 4;
	constexpr int CH  = 26;    // command row height
	constexpr int Y0  = 4 + CH + GAP;    // first main row (below the commands)

	std::vector<Key> keys;

	void add_row(int row, const char* chars) {
		int x = X0;
		for (const char* p = chars; *p; ++p) {
			static char labels[64][2];
			static int  n = 0;
			char*       l = labels[n++ % 64];
			l[0]          = *p;
			l[1]          = 0;
			SDL_Keycode k = SDLK_UNKNOWN;
			if (*p >= '0' && *p <= '9') {
				k = SDLK_0 + (*p - '0');
			} else if (*p >= 'A' && *p <= 'Z') {
				k = SDLK_A + (*p - 'A');
			} else if (*p == '.') {
				k = SDLK_PERIOD;
			} else if (*p == ',') {
				k = SDLK_COMMA;
			} else if (*p == '-') {
				k = SDLK_MINUS;
			} else if (*p == '\'') {
				k = SDLK_APOSTROPHE;
			} else if (*p == '?') {
				k = SDLK_SLASH;
			}
			keys.push_back({x, Y0 + row * (KH + GAP), KW, KH, l, k, *p, false, 0});
			x += KW + GAP;
		}
	}

	void build_layout() {
		keys.clear();
		// Ultima VI's commands across the top: each is just its letter key.
		{
			struct Cmd {
				const char* label;
				SDL_Keycode key;
			};
			static const Cmd cmds[10] = {
					{"ATK", SDLK_A},  {"CAST", SDLK_C}, {"TALK", SDLK_T}, {"LOOK", SDLK_L}, {"GET", SDLK_G},
					{"DROP", SDLK_D}, {"MOVE", SDLK_M}, {"USE", SDLK_U},  {"REST", SDLK_R}, {"CMBT", SDLK_B},
			};
			const int cw = 29;
			const int cg = 2;
			for (int i = 0; i < 10; i++) {
				keys.push_back({X0 + i * (cw + cg), 4, cw, CH, cmds[i].label, cmds[i].key, 0, false, 1});
			}
		}
		add_row(0, "1234567890");
		add_row(1, "QWERTYUIOP");
		add_row(2, "ASDFGHJKL'");
		add_row(3, "ZXCVBNM,.?");
		// Bottom row: ESC | SHIFT | SPACE | BKSP | ENTER | MAP
		const int y = Y0 + 4 * (KH + GAP);
		keys.push_back({X0, y, 40, KH, "ESC", SDLK_ESCAPE, 0, false, 0});
		keys.push_back({X0 + 44, y, 36, KH, "^", SDLK_LSHIFT, 0, true, 0});
		keys.push_back({X0 + 84, y, 72, KH, "SPACE", SDLK_SPACE, ' ', false, 0});
		keys.push_back({X0 + 160, y, 40, KH, "<", SDLK_BACKSPACE, 0, false, 0});
		keys.push_back({X0 + 204, y, 56, KH, "ENTER", SDLK_RETURN, 0, false, 0});
		keys.push_back({X0 + 264, y, 48, KH, "MAP", SDLK_UNKNOWN, 0, false, 2});
	}

	// ---------------------------------------------------------------------
	// State
	// ---------------------------------------------------------------------
	SDL_Window*  aux_window    = nullptr;
	SDL_WindowID aux_id        = 0;
	SDL_Window*  game_window   = nullptr;
	bool         shifted       = false;
	int          pressed_key   = -1;
	bool         dirty         = true;
	Uint64       last_present  = 0;
	bool         on_bottom     = false;
	bool         swap_request  = false;
	bool         aux_is_mirror = false;    // true: the aux window (top screen) mirrors the game
	bool         touch_view    = false;    // bottom screen shows the game to tap on, instead of the keyboard
	bool         touch_view_sticky = false;    // opened with MAP: stays until KEYBOARD is tapped
	Uint64       touch_view_since = 0;
	bool         touch_down    = false;
	// The touch view: the game frame shrunk to 320x192 at the top, buttons below.
	constexpr int TV_H = 192;
	constexpr float TV_SCALE = 0.8f;
	bool         text_wanted   = false;
	SDL_Surface* mirror_src    = nullptr;

	// ---------------------------------------------------------------------
	// Drawing helpers: the aux screen is a plain SDL window surface.
	// ---------------------------------------------------------------------
	void fill_rect(SDL_Surface* s, int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b) {
		const SDL_Rect rc = {x, y, w, h};
		SDL_FillSurfaceRect(s, &rc, SDL_MapSurfaceRGB(s, r, g, b));
	}

	// Draw text with the bitmap font, scale = pixel size of one font pixel.
	void draw_text(SDL_Surface* s, int x, int y, const char* text, int scale, Uint8 r, Uint8 g, Uint8 b) {
		const Uint32 col = SDL_MapSurfaceRGB(s, r, g, b);
		for (const char* p = text; *p; ++p) {
			if (const Glyph* gl = find_glyph(*p)) {
				for (int row = 0; row < 7; row++) {
					for (int c = 0; c < 5; c++) {
						if (gl->rows[row][c] == '#') {
							const SDL_Rect px = {x + c * scale, y + row * scale, scale, scale};
							SDL_FillSurfaceRect(s, &px, col);
						}
					}
				}
			}
			x += 6 * scale;
		}
	}

	int text_width(const char* text, int scale) {
		return static_cast<int>(std::strlen(text)) * 6 * scale - scale;
	}

	int key_at(float px, float py) {
		for (size_t i = 0; i < keys.size(); i++) {
			const Key& k = keys[i];
			if (px >= k.x && px < k.x + k.w && py >= k.y && py < k.y + k.h) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	void push_key(const Key& k, bool down) {
		SDL_Event ev;
		SDL_zero(ev);
		ev.type          = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
		ev.key.timestamp = SDL_GetTicksNS();
		ev.key.windowID  = game_window ? SDL_GetWindowID(game_window) : 0;
		ev.key.key       = k.key;
		ev.key.scancode  = SDL_GetScancodeFromKey(k.key, nullptr);
		ev.key.mod       = (shifted && k.special == 0) ? SDL_KMOD_SHIFT : SDL_KMOD_NONE;
		ev.key.down      = down;
		ev.key.repeat    = false;
		SDL_PushEvent(&ev);
		if (down && k.special == 1) {
			// A game command: show the game on the touch screen so the target
			// can be tapped, like tapping an icon on the command bar.
			touch_view        = true;
			touch_view_sticky = false;
			touch_view_since  = SDL_GetTicks();
			dirty             = true;
		}

		if (!down && shifted && k.special == 0 && !k.is_shift) {
			shifted = false;    // shift applies to one key, like a phone keyboard
			dirty   = true;
		}
		if (down && k.text && game_window && text_wanted) {
			// Typing into a text field (character name, save name...).
			static char bufs[16][2];
			static int  n   = 0;
			char*       buf = bufs[n++ % 16];
			buf[0] = shifted ? static_cast<char>(std::toupper(static_cast<unsigned char>(k.text)))
							 : static_cast<char>(std::tolower(static_cast<unsigned char>(k.text)));
			buf[1] = 0;
			SDL_Event te;
			SDL_zero(te);
			te.type           = SDL_EVENT_TEXT_INPUT;
			te.text.timestamp = SDL_GetTicksNS();
			te.text.windowID  = SDL_GetWindowID(game_window);
			te.text.text      = buf;
			SDL_PushEvent(&te);
		}
	}

	void draw_keyboard(SDL_Surface* s) {
		fill_rect(s, 0, 0, s->w, s->h, 24, 22, 34);
		for (size_t i = 0; i < keys.size(); i++) {
			const Key& k       = keys[i];
			const bool pressed = static_cast<int>(i) == pressed_key || (k.is_shift && shifted);
			fill_rect(s, k.x, k.y, k.w, k.h, 110, 108, 128);    // border
			if (pressed) {
				fill_rect(s, k.x + 1, k.y + 1, k.w - 2, k.h - 2, 120, 150, 230);
			} else {
				fill_rect(s, k.x + 1, k.y + 1, k.w - 2, k.h - 2, 72, 70, 88);
			}
			const int scale = (std::strlen(k.label) == 1) ? 2 : 1;
			const int tw    = text_width(k.label, scale);
			const int th    = 7 * scale;
			draw_text(s, k.x + (k.w - tw) / 2, k.y + (k.h - th) / 2, k.label, scale, 235, 235, 245);
		}
	}

	// The touch view: the game frame shrunk onto the touch screen, with a
	// KEYBOARD button and a CANCEL button underneath.
	void draw_touch_view(SDL_Surface* s) {
		fill_rect(s, 0, 0, s->w, s->h, 24, 22, 34);
		if (mirror_src) {
			SDL_Rect dst = {0, 0, static_cast<int>(mirror_src->w * TV_SCALE), static_cast<int>(mirror_src->h * TV_SCALE)};
			dst.x        = (s->w - dst.w) / 2;
			SDL_BlitSurfaceScaled(mirror_src, nullptr, s, &dst, SDL_SCALEMODE_NEAREST);
		}
		const int by = TV_H + 8;
		const int bh = s->h - TV_H - 16;
		fill_rect(s, 4, by, 150, bh, 110, 108, 128);
		fill_rect(s, 5, by + 1, 148, bh - 2, 72, 70, 88);
		draw_text(s, 4 + (150 - text_width("KEYBOARD", 2)) / 2, by + (bh - 14) / 2, "KEYBOARD", 2, 235, 235, 245);
		fill_rect(s, 166, by, 150, bh, 110, 108, 128);
		fill_rect(s, 167, by + 1, 148, bh - 2, 72, 70, 88);
		draw_text(s, 166 + (150 - text_width("CANCEL", 2)) / 2, by + (bh - 14) / 2, "CANCEL", 2, 235, 235, 245);
	}

	void push_plain_key(SDL_Keycode key, bool down) {
		SDL_Event ev;
		SDL_zero(ev);
		ev.type          = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
		ev.key.timestamp = SDL_GetTicksNS();
		ev.key.windowID  = game_window ? SDL_GetWindowID(game_window) : 0;
		ev.key.key       = key;
		ev.key.scancode  = SDL_GetScancodeFromKey(key, nullptr);
		ev.key.mod       = SDL_KMOD_NONE;
		ev.key.down      = down;
		SDL_PushEvent(&ev);
	}

	// Copy the game's latest frame to the middle of the top screen, 1:1.
	void draw_mirror(SDL_Surface* s) {
		SDL_Surface* gs = mirror_src;
		if (gs == nullptr) {
			fill_rect(s, 0, 0, s->w, s->h, 0, 0, 0);
			return;
		}
		SDL_Rect dst = {(s->w - gs->w) / 2, (s->h - gs->h) / 2, gs->w, gs->h};
		SDL_BlitSurface(gs, nullptr, s, &dst);
	}

}    // namespace

void n3ds_set_text_wanted(bool on) {
	text_wanted = on;
}

bool n3ds_text_wanted() {
	return text_wanted;
}

void n3ds_set_game_window(SDL_Window* w) {
	game_window = w;
}

void n3ds_set_mirror_surface(SDL_Surface* s) {
	mirror_src = s;
}

bool n3ds_kbd_touch_view() {
	return touch_view && !touch_view_sticky;
}

Uint64 n3ds_kbd_touch_view_since() {
	return touch_view_since;
}

void n3ds_kbd_set_touch_view(bool on) {
	if (touch_view != on) {
		touch_view = on;
		touch_down = false;
		dirty      = true;
	}
}

SDL_Window* n3ds_get_game_window() {
	return game_window;
}

bool n3ds_game_on_bottom() {
	return on_bottom;
}

void n3ds_request_screen_swap() {
	swap_request = true;
}

bool n3ds_take_screen_swap_request() {
	const bool r = swap_request;
	swap_request = false;
	if (r) {
		on_bottom = !on_bottom;
	}
	return r;
}

bool n3ds_kbd_exists() {
	return aux_window != nullptr;
}

void n3ds_kbd_create() {
	if (aux_window) {
		return;
	}
	build_layout();
	int            count    = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&count);
	SDL_DisplayID  top      = (displays && count > 0) ? displays[0] : 0;
	SDL_DisplayID  bottom   = (displays && count > 1) ? displays[1] : 0;
	SDL_free(displays);
	// Game on top -> keyboard on the bottom screen; game on bottom -> the top
	// screen mirrors the game.
	aux_is_mirror              = on_bottom;
	const SDL_DisplayID target = aux_is_mirror ? top : bottom;
	const int           aw     = aux_is_mirror ? 400 : 320;
	if (!target) {
		return;
	}
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, aux_is_mirror ? "mirror" : "keyboard");
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(target));
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(target));
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, aw);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 240);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
	aux_window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);
	if (!aux_window) {
		return;
	}
	aux_id = SDL_GetWindowID(aux_window);
	if (SDL_Surface* s = SDL_GetWindowSurface(aux_window)) {
		fill_rect(s, 0, 0, s->w, s->h, 0, 0, 0);
		SDL_UpdateWindowSurface(aux_window);
	}
	// Keep keyboard focus on the game window: the keyboard is touch only.
	if (game_window) {
		SDL_RaiseWindow(game_window);
	}
	dirty = true;
}

void n3ds_kbd_destroy() {
	if (aux_window) {
		SDL_DestroyWindow(aux_window);
		aux_window = nullptr;
	}
	aux_id      = 0;
	pressed_key = -1;
	touch_down  = false;
}

void n3ds_kbd_present() {
	if (!aux_window) {
		return;
	}
	SDL_Surface* s = SDL_GetWindowSurface(aux_window);
	if (s == nullptr) {
		return;
	}
	if (aux_is_mirror) {
		// Called right after each game present: copy the new frame up.
		draw_mirror(s);
		SDL_UpdateWindowSurface(aux_window);
		return;
	}
	if (touch_view) {
		draw_touch_view(s);
		SDL_UpdateWindowSurface(aux_window);
		return;
	}
	const Uint64 now = SDL_GetTicks();
	if (dirty || now - last_present > 500) {
		draw_keyboard(s);
		SDL_UpdateWindowSurface(aux_window);
		dirty        = false;
		last_present = now;
	}
}

void n3ds_message_screen(const char* l1, const char* l2, const char* l3, const char* l4) {
	int            count    = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&count);
	SDL_DisplayID  top      = (displays && count > 0) ? displays[0] : 0;
	SDL_free(displays);
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "message");
	if (top) {
		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(top));
		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(top));
	}
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 400);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 240);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
	SDL_Window* win = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);
	if (!win) {
		return;
	}
	const char* lines[4] = {l1, l2, l3, l4};
	const Uint64 start   = SDL_GetTicks();
	bool         done    = false;
	while (!done && SDL_GetTicks() - start < 15000) {
		if (SDL_Surface* s = SDL_GetWindowSurface(win)) {
			fill_rect(s, 0, 0, s->w, s->h, 40, 10, 10);
			int y = 60;
			for (int i = 0; i < 4; i++) {
				if (lines[i] == nullptr || !*lines[i]) {
					y += 20;
					continue;
				}
				const int scale = (i == 0) ? 3 : 2;
				const int tw    = text_width(lines[i], scale);
				draw_text(s, (s->w - tw) / 2, y, lines[i], scale, 240, 220, 220);
				y += 7 * scale + 12;
			}
			SDL_UpdateWindowSurface(win);
		}
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev.type == SDL_EVENT_FINGER_DOWN || ev.type == SDL_EVENT_QUIT) {
				done = true;
			}
		}
		SDL_Delay(50);
	}
	SDL_DestroyWindow(win);
}

bool n3ds_kbd_handle_event(SDL_Event* event) {
	// A handheld never really loses focus; SDL hands the focus to whichever
	// window was created last, which would pause the game whenever the
	// keyboard / mirror window appears.
	if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
		return true;
	}
	if (!aux_window) {
		return false;
	}
	if (event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST) {
		return event->window.windowID == aux_id;
	}
	if (aux_is_mirror) {
		return false;
	}
	switch (event->type) {
	case SDL_EVENT_FINGER_DOWN:
	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_MOTION:
	case SDL_EVENT_FINGER_CANCELED: {
		if (event->tfinger.windowID != aux_id) {
			return false;
		}
		const float px = event->tfinger.x * 320.f;
		const float py = event->tfinger.y * 240.f;
		if (touch_view) {
			// Taps on the shrunk game frame become mouse clicks on the game
			// window (which is 1:1 on the top screen); the strip below holds
			// the two buttons.
			const int   fw = mirror_src ? static_cast<int>(mirror_src->w * TV_SCALE) : 320;
			const float ox = (320 - fw) / 2.f;
			if (event->type == SDL_EVENT_FINGER_DOWN) {
				if (py < TV_H) {
					touch_down = true;
					n3ds_forward_touch((px - ox) / TV_SCALE, py / TV_SCALE, 0);
				} else if (px < 160) {
					touch_view = false;
					dirty      = true;
				} else {
					push_plain_key(SDLK_SPACE, true);    // cancel the command
					push_plain_key(SDLK_SPACE, false);
					touch_view = false;
					dirty      = true;
				}
			} else if (event->type == SDL_EVENT_FINGER_MOTION) {
				if (touch_down) {
					n3ds_forward_touch((px - ox) / TV_SCALE, py / TV_SCALE, 1);
				}
			} else if (touch_down) {    // up / cancelled
				touch_down = false;
				n3ds_forward_touch((px - ox) / TV_SCALE, py / TV_SCALE, 2);
			}
			return true;
		}
		if (event->type == SDL_EVENT_FINGER_DOWN) {
			const int i = key_at(px, py);
			if (i >= 0) {
				const Key& k = keys[i];
				if (k.is_shift) {
					shifted = !shifted;
				} else if (k.special == 2) {
					touch_view        = true;
					touch_view_sticky = true;
					touch_view_since  = SDL_GetTicks();
				} else {
					pressed_key = i;
					push_key(k, true);
				}
				dirty = true;
			}
		} else if (event->type == SDL_EVENT_FINGER_UP || event->type == SDL_EVENT_FINGER_CANCELED) {
			if (pressed_key >= 0) {
				push_key(keys[pressed_key], false);
				pressed_key = -1;
				dirty       = true;
			}
		}
		return true;    // consumed
	}
	case SDL_EVENT_MOUSE_MOTION:
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		// Mouse events synthesised from touches on the keyboard window.
		return (event->type == SDL_EVENT_MOUSE_MOTION ? event->motion.windowID : event->button.windowID) == aux_id;
	default:
		return false;
	}
}

#endif    // __3DS__
