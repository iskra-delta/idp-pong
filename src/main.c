#include "game/game.h"
#include "platform/platform.h"

static void game_frame(int key, void *ctx) {
    struct GameState *game = (struct GameState *) ctx;
    game_handle_key(game, key);
    game_update(game);
    game_render(game);
}

static int game_running(void *ctx) {
    const struct GameState *game = (const struct GameState *) ctx;
    return !game->quit;
}

int main(void) {
    struct GameState game;

    if (!platform_init()) {
        return 1;
    }

    game_init(&game, platform_display_width(), platform_display_height());
    platform_loop(game_frame, game_running, &game, 0U);

    platform_shutdown();
    return 0;
}
