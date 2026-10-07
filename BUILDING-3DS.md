# Building Nuvie for the Nintendo 3DS

Technical notes for the `n3ds` branch. The result is `nuvie.3dsx`, run from
the Homebrew Launcher on a New 3DS / New 2DS XL.

This branch also moves Nuvie from SDL 2 to SDL 3 (see `misc/sdl3shim/`), so
it builds on a desktop with SDL 3 as well: `cmake . && make`.

## Toolchain

- devkitARM (arm-none-eabi-gcc), libctru — devkitPro
- SDL 3.4 built for the 3DS (`-DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake`),
  installed into `$DEVKITPRO/portlibs/3ds`
- `3dsxtool` and `smdhtool` from devkitPro's general-tools

## Build

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
export PATH=$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH

mkdir build-3ds && cd build-3ds
cmake -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4

# package: the data directory (Lua scripts, fonts, images) goes into romfs
mkdir -p romfs && cp -r ../data romfs/data
smdhtool --create "Nuvie (Ultima VI)" "Ultima VI engine for the New 3DS / New 2DS XL" "cherygarcia77" icon.png nuvie.smdh
3dsxtool nuvie.elf nuvie.3dsx --smdh=nuvie.smdh --romfs=romfs
```

## Where the 3DS code lives

- `n3ds/n3ds_platform.cpp` — start-up (SD card folders, logs, config
  defaults and repairs), gamepad to mouse/keyboard mapping, screen swap,
  touch forwarding
- `n3ds/n3ds_kbd.cpp/.h` — the second screen: touch keyboard with the U6
  commands, the touch view, or a mirror of the game
- `n3ds/n3ds_heap.cpp` — memory set-up
- `screen/Screen.cpp` — window creation on either screen, direct blits to the
  window surface, shrink-to-fit on the touch screen
- `misc/sdl3shim/SDL.h` — SDL 2 names on top of SDL 3

Runtime layout on the SD card is described in `README-3DS.txt`.
