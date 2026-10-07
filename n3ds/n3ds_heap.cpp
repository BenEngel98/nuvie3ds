/*
 *  n3ds_heap.cc - Memory set-up for the Nintendo 3DS port.
 *
 *  Copyright (C) 2026  The Nuvie Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifdef __3DS__

#	include <3ds.h>

extern "C" {

extern char* fake_heap_start;
extern char* fake_heap_end;
extern u32   __ctru_heap;
extern u32   __ctru_linear_heap;
extern u32   __ctru_heap_size;
extern u32   __ctru_linear_heap_size;

// Nuvie needs more stack than the 3DS default (Lua scripts,
// pathfinding, deep C++ call chains). The stack is carved out of the heap.
unsigned int __stacksize__ = 4 * 1024 * 1024;

// libctru reserves 32 MB of "linear" memory for GPU-side buffers by default.
// Nuvie only needs the framebuffers and audio buffers there, so keep most of
// the RAM for the game itself.
u32 __ctru_linear_heap_size = 8 * 1024 * 1024;

/*
 *  Replaces libctru's weak __system_allocateHeaps(). The stock version asks
 *  the kernel for every last byte the commit limit allows and panics
 *  (svcBreak) if the kernel says no - which it does on real hardware when
 *  the request fills the whole application memory region. Here we ask for a
 *  little less and, if that still fails, back off in 4 MB steps.
 */
void __system_allocateHeaps(void) {
	Handle reslimit = 0;
	if (R_FAILED(svcGetResourceLimit(&reslimit, CUR_PROCESS_HANDLE))) {
		svcBreak(USERBREAK_PANIC);
	}
	s64               maxCommit = 0;
	s64               curCommit = 0;
	ResourceLimitType type      = RESLIMIT_COMMIT;
	svcGetResourceLimitLimitValues(&maxCommit, reslimit, &type, 1);
	svcGetResourceLimitCurrentValues(&curCommit, reslimit, &type, 1);
	svcCloseHandle(reslimit);
	const u32 remaining = static_cast<u32>(maxCommit - curCommit) & ~0xFFFu;

	// Linear (physically contiguous) memory first: screens and sound.
	if (__ctru_linear_heap_size == 0 || __ctru_linear_heap_size > remaining / 4) {
		__ctru_linear_heap_size = (remaining / 4) & ~0xFFFu;
	}
	Result rc = svcControlMemory(
			&__ctru_linear_heap, 0, 0, __ctru_linear_heap_size, MEMOP_ALLOC_LINEAR,
			MEMPERM_READWRITE);
	if (R_FAILED(rc)) {
		svcBreak(USERBREAK_PANIC);
	}

	// Application heap: as much as the kernel will actually give us.
	u32 want = remaining - __ctru_linear_heap_size;
	if (want > 4u * 1024 * 1024) {
		want -= 4u * 1024 * 1024;    // headroom for kernel objects / threads
	}
	want &= ~0xFFFu;
	for (;;) {
		__ctru_heap = OS_HEAP_AREA_BEGIN;
		rc          = svcControlMemory(
                &__ctru_heap, OS_HEAP_AREA_BEGIN, 0, want, MEMOP_ALLOC, MEMPERM_READWRITE);
		if (R_SUCCEEDED(rc)) {
			break;
		}
		if (want <= 16u * 1024 * 1024) {
			svcBreak(USERBREAK_PANIC);
		}
		want -= 4u * 1024 * 1024;
	}
	__ctru_heap_size = want;

	mappableInit(OS_MAP_AREA_BEGIN, OS_MAP_AREA_END);

	fake_heap_start = reinterpret_cast<char*>(__ctru_heap);
	fake_heap_end   = fake_heap_start + __ctru_heap_size;
}

// True on a New 3DS / New 2DS (the extra CPU speed and the 124 MB of
// application memory are what this port needs).
bool n3ds_is_new_3ds(void) {
	bool is_new = false;
	if (R_FAILED(APT_CheckNew3DS(&is_new))) {
		return false;
	}
	return is_new;
}

}    // extern "C"

#endif    // __3DS__
