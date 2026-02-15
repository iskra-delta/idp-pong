# idp-pong

Atari-style Pong for Iskra Delta Partner constraints, rendered with SDL2.

## Project layout

- `src/game/` - platform-independent game logic (C99, integer-only fixed-point motion)
- `src/platform/` - platform-dependent drawing/input/timing backend
- `src/main.c` - game wiring via `platform_loop(...)`

## Build

```bash
make all
```

This creates `bin/idp-pong` and stores object files under `build/`.
SDL2 development libraries are required (`libsdl2-dev` on Debian/Ubuntu).

## Controls

- `ENTER` / `SPACE`: start game or continue from game-over
- `LEFT` / `RIGHT`: switch mode in menu (`PLAYER VS CPU` or `PLAYER VS PLAYER`)
- `W` / `S`: left paddle
- `UP` / `DOWN`: right paddle (in `PLAYER VS PLAYER` mode)
- `Q`: quit

## Implemented limitations

- Screen clear via `platform_cls`
- Vector-only draw via `platform_draw_line`
- Display size via `platform_display_width` / `platform_display_height`
- Integer-only game math using 16-bit style state/velocities (no floating-point)
- Text rendering via `platform_draw_text`
- Foreground/background erase behavior via `platform_set_color(FORE/BACK)`
- Keyboard input via `platform_read_key`
- Variable ball speeds and bounce angles
- Bounce influenced by paddle movement speed
- Player vs computer mode
