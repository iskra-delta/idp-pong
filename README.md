# idp-pong

`Namizni tenis` for the Iskra Delta Partner graphics mode.

The game now has:

- a vector splash screen with a boxed instruction panel
- one-player mode against the computer
- two-player mode on the same keyboard
- a game-over screen with replay prompt
- double-buffered gameplay with page flipping

## Project layout

- `src/engine/` - game state, physics, input handling, splash/game-over rendering, gameplay rendering
- `src/glyphs/` - splash vector data and Partner font data
- `src/main.c` - Partner-specific startup, page management, main loop, keyboard polling
- `src/Makefile` - SDCC build inside the docker toolchain

## Build

```bash
make
```

This does the following:

- downloads and extracts the latest `iskra-delta/idp-sdk` release into `.deps/idp-sdk/`
- builds this project inside `wischner/sdcc-z80-idp:latest`
- links with the SDK runtime (`crt0cpm3-z80.rel`, `libsdk.lib`, `libcpm3-z80.lib`, `libsdcc-z80.lib`) plus `ugpx`
- creates `bin/ntenis.com`
- creates `bin/ntenis.img`

Useful targets:

- `make` - build the program and disk image
- `make clean` - remove local build outputs
- `make docker-pull` - pull the Partner toolchain image
- `make fix-perms` - repair ownership if Docker created root-owned outputs

## Controls

### Splash screen

- `1`, `ENTER`, or `SPACE` - start one-player game against the computer
- `2` - start two-player game
- `Q` or `ESC` - quit

### During game

- `W` - left paddle up
- `S` - left paddle down
- `I` - right paddle up in two-player mode
- `K` - right paddle down in two-player mode
- `Q` or `ESC` - quit immediately

The paddle motion accelerates with repeated presses in the same direction.

### Game over

- `D` - start a new match
- `N`, `Q`, or `ESC` - exit

## Notes

- Resolution is `1024x512`.
- The splash screen is drawn on the hidden page and then displayed at once.
- Gameplay uses page flipping and redraws only the moving objects and score changes.
- Input uses raw CP/M polling through `bdos(C_RAWIO, 0xFF)`.
- The splash vector data and font live in code space, not copied into RAM data by startup.
