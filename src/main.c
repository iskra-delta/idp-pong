#include <ugpx.h>
#include <partner/conio.h>
#include <sys/bdos.h>
#include "game/game.h"

#define PARTNER_WIDTH  1024
#define PARTNER_HEIGHT 512

struct game_state game;

static int read_key(void) {
    return (int)(unsigned char)bdos(C_RAWIO, 0xFF);
}

/* Intro: draw once, redraw only when mode toggles, wait for ENTER/Q. */
static void run_intro(struct game_state *g) {
    game_render_full(g);

    while (g->screen == 0 && !g->quit) {
        int key = read_key();
        if (key)
            game_handle_key(g, key);
    }
}

/* Game: single-page update to avoid Partner page-switch instability. */
static void run_game(struct game_state *g) {
    game_render_full(g);
    game_render_draw_all(g);

    while (g->screen == 1 && !g->quit) {
        int key = read_key();
        int score_changed;
        g->prev_paddle1_y = g->paddle1_y;
        g->prev_paddle2_y = g->paddle2_y;
        g->prev_ball_x    = g->ball_x;
        g->prev_ball_y    = g->ball_y;
        g->prev_score1    = g->score1;
        g->prev_score2    = g->score2;

        if (key)
            game_handle_key(g, key);
        game_update(g);
        if (g->screen != 1 || g->quit)
            break;
        score_changed = (g->score1 != g->prev_score1 || g->score2 != g->prev_score2);

        game_render_erase(g);
        game_render_draw(g);

        if (score_changed)
            game_render_score(g);
    }

    /* Let's erae game. */

}

int main(void) {

    /* Hide text cursor and clear text screen. */
    setcursortype(NOCURSOR);
    clrscr();
    
    /* Init HIRES mode. */
    ginit(RES_1024x512);

    /* Clear screen. */
    gcls();
    
    game_init(&game, PARTNER_WIDTH, PARTNER_HEIGHT);

    while (!game.quit) {
        if (game.screen == 0)
            run_intro(&game);
        else if (game.screen == 1)
            run_game(&game);
        else if (game.screen == 2)
            game.quit = 1; /* For now. */
    }
    
    gcls();

    /* Exit HIRES mode.*/
    gexit();

    /* Restore text cursor. */
    setcursortype(NORMALCURSOR);

    /* Exit to CP/M. */
    return 0;
}
