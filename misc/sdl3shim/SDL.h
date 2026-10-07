/*
 *  Nuvie was written against SDL 1.2 / SDL 2.  This header sits in front of
 *  SDL 3: SDL's own old-names table handles the plain renames, and the few
 *  things below are the ones whose shape changed.
 */
#ifndef NUVIE_SDL3_SHIM_H
#define NUVIE_SDL3_SHIM_H

#define SDL_ENABLE_OLD_NAMES 1
#include <SDL3/SDL.h>

/* The old key description struct; built from an SDL 3 key event. */
typedef struct SDL_Keysym {
	SDL_Scancode scancode;
	SDL_Keycode  sym;
	Uint16       mod;
	Uint32       unused;
} SDL_Keysym;

static inline SDL_Keysym nuvie_keysym(const SDL_Event* e) {
	SDL_Keysym k;
	k.scancode = e->key.scancode;
	k.sym      = e->key.key;
	k.mod      = (Uint16)e->key.mod;
	k.unused   = 0;
	return k;
}

#define SDLK_QUOTE SDLK_APOSTROPHE
#define SDLK_BACKQUOTE SDLK_GRAVE

/* ---- surfaces ---- */
#define SDL_SWSURFACE 0
#define SDL_HWSURFACE 0
#define SDL_SRCCOLORKEY 1

static inline SDL_Surface* SDL_CreateRGBSurface(
		Uint32 flags, int w, int h, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
	(void)flags;
	SDL_Surface* s = SDL_CreateSurface(w, h, SDL_GetPixelFormatForMasks(depth, Rmask, Gmask, Bmask, Amask));
	if (s && depth == 8)
		SDL_CreateSurfacePalette(s);    /* SDL 2 gave 8-bit surfaces a palette */
	return s;
}

static inline SDL_Surface* SDL_CreateRGBSurfaceFrom(
		void* pixels, int w, int h, int depth, int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask) {
	SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_GetPixelFormatForMasks(depth, Rmask, Gmask, Bmask, Amask), pixels, pitch);
	if (s && depth == 8)
		SDL_CreateSurfacePalette(s);
	return s;
}

#ifdef __cplusplus
/* Old 3-argument form: the flags are meaningless now. */
static inline SDL_Surface* SDL_ConvertSurface(SDL_Surface* src, SDL_PixelFormat fmt, Uint32 flags) {
	(void)flags;
	return SDL_ConvertSurface(src, fmt);
}
#endif

/* Pixel-format details for a surface (what the old surface->format pointed at). */
static inline const SDL_PixelFormatDetails* nuvie_fmt(const SDL_Surface* s) {
	return SDL_GetPixelFormatDetails(s->format);
}

/* ---- threads ---- */
#define SDL_mutexP SDL_LockMutex
#define SDL_mutexV SDL_UnlockMutex

#endif /* NUVIE_SDL3_SHIM_H */
