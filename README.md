# idp-pong

Pong for the Iskra Delta Partner graphics mode.

## Project layout

- `src/game/` - game rules, physics, and drawing helpers
- `src/main.c` - Partner-specific loop, page flipping, and input polling
- `src/Makefile` - SDCC build that links `ugpx` and `libpartner`

## Build

```bash
make
```

This creates:

- `bin/ntenis.com`
- `bin/ntenis.img`

Build artifacts are stored under `build/`. The top-level `Makefile` now follows
the same pattern as the `aids` asteroids project:

- build `../libpartner` first
- build this project inside `wischner/sdcc-z80-idp:latest`
- package a bootable CP/M disk image with `cpmdisk`

Useful targets:

- `make` - build `ntenis.com` and `ntenis.img`
- `make clean` - remove local build outputs
- `make docker-pull` - pull the Partner toolchain image
- `make fix-perms` - repair ownership if Docker created root-owned outputs

## Controls

- `ENTER` / `SPACE`: start game or continue from game-over
- `A` / `D`: switch mode in menu
- `W` / `S`: left paddle
- `I` / `K`: right paddle in two-player mode
- `Q`: quit

## Notes

- Uses integer-only movement and collision logic.
- Uses page flipping plus erase/redraw of only the moving objects during play.
- Uses raw CP/M keyboard polling through `bdos(C_RAWIO, 0xFF)`, matching the
  approach used in `aids`.
