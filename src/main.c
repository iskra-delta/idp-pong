#include "game/game.h"
#include "platform/platform.h"

int main(void) {
    struct GameState game;

    if (!platform_init()) {
        return 1;
    }

    game_init(&game, platform_display_width(), platform_display_height());
    while (!game.quit) {
        int key = platform_read_key();
        game_handle_key(&game, key);
        game_update(&game);
        game_render(&game);
        platform_present();
        platform_delay(0U);
    }

    platform_shutdown();
    return 0;
}
